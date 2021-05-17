package screen

import (
	"encoding/base64"
	"errors"
	"fmt"
	"io"
	"math/rand"
	"net"
	"os"
	"os/exec"
	"os/signal"
	"sync"
	"time"
)

const kOutputBufferSize = 64 * 1024
const kIdLength = 6

var kIdAlphabet = []string{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f"}

type Session struct {
	id                  string
	cmd                 *exec.Cmd
	output              []byte
	outputStartPosition int
	outputSync          *sync.Mutex
	inputChan           chan []byte
}

type Server struct {
	listener *net.TCPListener
	sessions map[string]*Session

	notifyChan  chan string
	sessionSubs map[string][]chan bool
	notifySync  *sync.Mutex
}

func NewServer(listener *net.TCPListener) *Server {
	return &Server{
		listener:    listener,
		sessions:    make(map[string]*Session),
		notifyChan:  make(chan string, 5),
		sessionSubs: make(map[string][]chan bool), // false means "terminate"
		notifySync:  &sync.Mutex{},
	}
}

func (s *Server) ListenAndServe() error {
	sigs := make(chan os.Signal, 1)
	end := make(chan bool, 5)
	term := make(chan bool, 1)

	signal.Notify(sigs, os.Interrupt)

	go func() {
		<-sigs
		fmt.Println("Caught SIGINT")
		end <- true
		end <- true
	}()

	go func() {
		<-end
		fmt.Println("Terminating sessions...")
		s.notifySync.Lock()
		for _, subs := range s.sessionSubs {
			for _, sub := range subs {
				sub <- false
			}
		}
		s.notifySync.Unlock()
		fmt.Println("Sessions are notified about termination")
		time.Sleep(1 * time.Second)
		term <- true
	}()

	go func() {
		for id := range s.notifyChan {
			fmt.Println("Notifying session", id)
			s.notifySync.Lock()
			for _, sub := range s.sessionSubs[id] {
				fmt.Println("Notifying session sub", sub)
				sub <- true
			}
			s.notifySync.Unlock()
		}
	}()

	var err error
	var conn *net.TCPConn
loop:
	for {
		select {
		case <-end:
			fmt.Println("Finishing listening...")
			break loop
		default:
			_ = s.listener.SetDeadline(time.Now().Add(time.Millisecond * 100))
			conn, err = s.listener.AcceptTCP()
			if errors.Is(err, os.ErrDeadlineExceeded) {
				err = nil
				break // breaks from select
			}
			if err != nil {
				end <- true
				break loop
			}
			go s.serveClient(conn)
		}
	}

	<-term
	fmt.Println("Terminated.")
	return err
}

func (s *Server) serveClient(conn *net.TCPConn) {
	wconn := NewTCPConnWrapper(conn)
loop:
	for {
		msg, err := wconn.RecvMessage()
		if err != nil {
			fmt.Println("Failed to receive message:", err, conn)
			break
		}
		switch msg.Command {
		case LIST:
			var sessions []string
			for id := range s.sessions {
				sessions = append(sessions, id)
			}
			data := make(map[string]interface{})
			data["sessions"] = sessions
			err = wconn.SendMessage(Message{
				Command: LIST,
				Data:    data,
				Status:  SUCCESS,
			})
			if err != nil {
				fmt.Println("Failed to send confirmation:", err)
			}
			fmt.Println("LIST ", s.sessionSubs)
			break loop
		case NEW:
			var id string
			if _, ok := msg.Data["id"]; ok {
				id, err = retrieveString(msg.Data, "id")
				if err != nil {
					fmt.Println("Ill-formed NEW message:", err)
					break loop
				}
				if _, exists := s.sessions[id]; exists {
					data := make(map[string]interface{})
					data["reason"] = "id is already taken"
					err = wconn.SendMessage(Message{
						Command: NEW,
						Data:    data,
						Status:  FAILURE,
					})
					if err != nil {
						fmt.Println("Failed to send error:", err)
					}
					break loop
				}
			} else {
				for {
					id = generateId()
					if _, exists := s.sessions[id]; !exists {
						break
					}
				}
			}
			session, err := createSession(id, s.notifyChan)
			if err != nil {
				fmt.Println("Failed to create new session:", err)
				break loop
			}
			s.notifySync.Lock()
			fmt.Println("Adding", session, " with id", id)
			s.sessions[id] = session
			clientNotifyChan := make(chan bool, 1)
			s.sessionSubs[id] = []chan bool{clientNotifyChan}
			fmt.Println("NEW NEW NEW")
			fmt.Println("sessions", s.sessions)
			fmt.Println("subs", s.sessionSubs)
			s.notifySync.Unlock()

			err = wconn.SendMessage(Message{
				Command: NEW,
				Data:    make(map[string]interface{}),
				Status:  SUCCESS,
			})
			if err != nil {
				fmt.Println("Failed to send confirmation:", err)
			}

			s.attachToSession(wconn, session, clientNotifyChan)
			return
		case ATTACH:
			id, err := retrieveString(msg.Data, "id")
			if err != nil {
				fmt.Println("Ill-formed ATTACH message:", err)
				break loop
			}
			session, exists := s.sessions[id]
			if !exists {
				data := make(map[string]interface{})
				data["reason"] = "such a session does not exist"
				err = wconn.SendMessage(Message{
					Command: ATTACH,
					Data:    data,
					Status:  FAILURE,
				})
				if err != nil {
					fmt.Println("Failed to send error:", err)
				}
				break loop
			}

			s.notifySync.Lock()
			clientNotifyChan := make(chan bool, 1)
			s.sessionSubs[id] = append(s.sessionSubs[id], clientNotifyChan)
			s.notifySync.Unlock()

			err = wconn.SendMessage(Message{
				Command: ATTACH,
				Data:    make(map[string]interface{}),
				Status:  SUCCESS,
			})
			if err != nil {
				fmt.Println("Failed to send confirmation:", err)
			}

			s.attachToSession(wconn, session, clientNotifyChan)
			s.notifyChan <- id
			return
		case KILL:
			id, err := retrieveString(msg.Data, "id")
			if err != nil {
				fmt.Println("Ill-formed KILL message:", err)
				break loop
			}
			session, exists := s.sessions[id]
			if !exists {
				data := make(map[string]interface{})
				data["reason"] = "such a session does not exist"
				err = wconn.SendMessage(Message{
					Command: KILL,
					Data:    data,
					Status:  FAILURE,
				})
				if err != nil {
					fmt.Println("Failed to send error:", err)
				}
				break loop
			}
			err = session.cmd.Process.Kill()
			if err != nil {
				fmt.Println("Failed to kill session:", err)
			}
			close(session.inputChan)
			delete(s.sessions, session.id)
			fmt.Println("Killed session", id)
			err = wconn.SendMessage(Message{
				Command: KILL,
				Data:    make(map[string]interface{}),
				Status:  SUCCESS,
			})
			if err != nil {
				fmt.Println("Failed to send confirmation:", err)
			}
			break loop
		case DETACH:
			fallthrough
		default:
			data := make(map[string]interface{})
			data["reason"] = "not implemented"
			err = wconn.SendMessage(Message{
				Command: msg.Command,
				Data:    data,
				Status:  FAILURE,
			})
			if err != nil {
				fmt.Println("Failed to send error:", err)
			}
			break loop
		}
	}
	_ = conn.Close()
}

func generateId() string {
	result := ""
	for i := 0; i < kIdLength; i++ {
		result += kIdAlphabet[rand.Int()%len(kIdAlphabet)]
	}
	return result
}

func createSession(id string, notifyChan chan<- string) (*Session, error) {
	shell := exec.Command("sh", "-i")
	shellStdin, err := shell.StdinPipe()
	if err != nil {
		return nil, err
	}
	shellStdout, err := shell.StdoutPipe()
	if err != nil {
		return nil, err
	}
	shellStderr, err := shell.StderrPipe()
	if err != nil {
		return nil, err
	}
	if err := shell.Start(); err != nil {
		return nil, err
	}

	session := &Session{
		id:                  id,
		cmd:                 shell,
		output:              nil,
		outputStartPosition: 0,
		outputSync:          &sync.Mutex{},
		inputChan:           make(chan []byte, 5),
	}

	inputToShell := func() {
		for {
			msg, stillOpen := <-session.inputChan
			fmt.Println("ITS", msg, stillOpen)
			if !stillOpen {
				err := shellStdin.Close()
				if err != nil {
					fmt.Println("Failed to close shell's stdin:", err)
				}
				break
			}
			if _, err = shellStdin.Write(msg); err != nil {
				fmt.Println("Failed to write data to shell:", err)
				break
			}
		}
	}

	shellToBuffer := func(pipe io.ReadCloser) {
		buf := make([]byte, 4096)
		for {
			n, err := pipe.Read(buf)
			if err != nil {
				if err != io.EOF {
					fmt.Println("Failed to read from shell's out pipe:", pipe, err)
				}
				break
			}
			session.outputSync.Lock()
			session.output = append(session.output, buf[:n]...)
			if len(session.output) > kOutputBufferSize {
				diff := len(session.output) - kOutputBufferSize
				session.outputStartPosition += diff
				session.output = session.output[diff:]
			}
			session.outputSync.Unlock()
			fmt.Println("output notify", session.id)
			notifyChan <- session.id
		}
	}

	go shellToBuffer(shellStdout)
	go shellToBuffer(shellStderr)
	go inputToShell()

	fmt.Println("Started session", id, "with PID", shell.Process.Pid)

	return session, nil
}

func (s *Server) attachToSession(clientConn *TCPConnWrapper, session *Session, notifyChan chan bool) {
	fmt.Println("Attaching", clientConn, "to", session.id, "(notify chan", notifyChan, ")")
	detach := make(chan bool, 1)

	rmSub := func() {
		fmt.Println("Removing subscription to session", session.id, "conn", clientConn)
		s.notifySync.Lock()
		defer s.notifySync.Unlock()
		subs := s.sessionSubs[session.id]
		var nsubs []chan bool
		for _, sub := range subs {
			if sub != notifyChan {
				nsubs = append(nsubs, sub)
			}
		}
		s.sessionSubs[session.id] = nsubs
	}

	go func() {
		fmt.Println("Starting input-coro...")
		closed := false
		for {
			fmt.Println("AAA", clientConn)
			msg, err := clientConn.RecvMessage()
			if err != nil {
				fmt.Println("Failed to receive message:", err)
				break
			}
			fmt.Println("Got", msg)
			if msg.Command == DETACH {
				detach <- true
				break
			}
			if msg.Command != DATA {
				fmt.Println("Expected DATA message, but got:", msg)
				break
			}
			dataBytes, err := retrieveByteArray(msg.Data, "data")
			if err != nil {
				fmt.Println("DATA message is ill-formed: ", msg)
				break
			}
			fmt.Println("OOOOO", dataBytes)
			if len(dataBytes) > 0 && !closed {
				session.inputChan <- dataBytes
			}
			if msg.Status == FAILURE && !closed {
				close(session.inputChan)
				closed = true
			}
		}
		fmt.Println("Closing input-coro for session", session.id, "conn", clientConn)
		_ = clientConn.Conn.Close()
		rmSub()
	}()

	go func() {
		fmt.Println("Starting output-coro...")
	loop:
		for {
			fmt.Println("BBB")
			select {
			case <-detach:
				fmt.Println("Client detached. session", session.id, "conn", clientConn)
				break loop
			case status, more := <-notifyChan:
				fmt.Println(session.id, "notify", status, more)
				if !status { // terminate
					err := session.cmd.Process.Kill()
					if err != nil {
						fmt.Println("Failed to kill session:", err)
					}
					fmt.Println("Killed session", session.id)
					break loop
				}
				session.outputSync.Lock()
				data := make(map[string]interface{})
				data["start_pos"] = session.outputStartPosition
				data["data"] = base64.StdEncoding.EncodeToString(session.output)
				session.outputSync.Unlock()
				msg := Message{
					Command: DATA,
					Data:    data,
					Status:  SUCCESS,
				}
				err := clientConn.SendMessage(msg)
				if err != nil {
					fmt.Println("Failed to send DATA:", err)
					break loop
				}
			}
		}
		fmt.Println("Closing output-coro for session", session.id, "conn", clientConn)
		_ = clientConn.Conn.Close()
		rmSub()
	}()
}
