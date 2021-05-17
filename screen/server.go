package screen

import (
	"encoding/base64"
	"fmt"
	"io"
	"math/rand"
	"net"
	"os"
	"os/exec"
	"os/signal"
	"sync"
	"syscall"
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
		notifyChan:  make(chan string, 1),
		sessionSubs: make(map[string][]chan bool), // false means "terminate"
		notifySync:  &sync.Mutex{},
	}
}

func (s *Server) ListenAndServe() error {
	sigs := make(chan os.Signal, 1)
	end := make(chan bool, 5)
	term := make(chan bool, 1)

	signal.Notify(sigs, syscall.SIGINT)

	go func() {
		<-sigs
		end <- true
		end <- true
	}()

	go func() {
		<-end
		s.notifySync.Lock()
		for _, subs := range s.sessionSubs {
			for _, sub := range subs {
				sub <- false
			}
		}
		s.notifySync.Unlock()
		time.Sleep(1 * time.Second)
		term <- true
	}()

	go func() {
		for id := range s.notifyChan {
			s.notifySync.Lock()
			for _, sub := range s.sessionSubs[id] {
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
			break loop
		default:
			conn, err = s.listener.AcceptTCP()
			if err != nil {
				end <- true
				break loop
			}
			go s.serveClient(conn)
		}
	}

	<-term
	return err
}

func (s *Server) serveClient(conn *net.TCPConn) {
loop:
	for {
		msg, err := receiveMessage(conn)
		if err != nil {
			fmt.Println("Failed to receive message:", err, conn)
			break
		}
		switch msg.Command {
		case LIST:
			var sessions []string
			for id, _ := range s.sessions {
				sessions = append(sessions, id)
			}
			data := make(map[string]interface{})
			data["sessions"] = sessions
			err = sendMessage(Message{
				Command: LIST,
				Data:    data,
				Status:  SUCCESS,
			}, conn)
			if err != nil {
				fmt.Println("Failed to send confirmation:", err)
			}
			break loop
		case NEW:
			var id string
			idRaw, ok := msg.Data["id"]
			if ok {
				id, ok = idRaw.(string)
				if !ok {
					fmt.Println("Malformed NEW message: id is not a string")
					break loop
				}
				if _, exists := s.sessions[id]; exists {
					data := make(map[string]interface{})
					data["reason"] = "id is already taken"
					err = sendMessage(Message{
						Command: NEW,
						Data:    data,
						Status:  FAILURE,
					}, conn)
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
			s.sessions[id] = session

			s.notifySync.Lock()
			clientNotifyChan := make(chan bool, 1)
			s.sessionSubs[id] = []chan bool{clientNotifyChan}
			s.notifySync.Unlock()

			err = sendMessage(Message{
				Command: NEW,
				Data:    make(map[string]interface{}),
				Status:  SUCCESS,
			}, conn)
			if err != nil {
				fmt.Println("Failed to send confirmation:", err)
			}

			s.attachToSession(conn, session, clientNotifyChan)
			return
		case ATTACH:
			var id string
			idRaw, ok := msg.Data["id"]
			if !ok {
				fmt.Println("Ill-formed ATTACH message: no id field")
				break loop
			}
			id, ok = idRaw.(string)
			if !ok {
				fmt.Println("Malformed ATTACH message: id is not a string")
				break loop
			}
			session, exists := s.sessions[id]
			if !exists {
				data := make(map[string]interface{})
				data["reason"] = "such a session does not exist"
				err = sendMessage(Message{
					Command: ATTACH,
					Data:    data,
					Status:  FAILURE,
				}, conn)
				if err != nil {
					fmt.Println("Failed to send error:", err)
				}
				break loop
			}

			s.notifySync.Lock()
			clientNotifyChan := make(chan bool, 1)
			s.sessionSubs[id] = append(s.sessionSubs[id], clientNotifyChan)
			s.notifySync.Unlock()

			err = sendMessage(Message{
				Command: ATTACH,
				Data:    make(map[string]interface{}),
				Status:  SUCCESS,
			}, conn)
			if err != nil {
				fmt.Println("Failed to send confirmation:", err)
			}

			s.attachToSession(conn, session, clientNotifyChan)
			s.notifyChan <- id
			return
		case KILL:
			var id string
			idRaw, ok := msg.Data["id"]
			if !ok {
				fmt.Println("Ill-formed KILL message: no id field")
				break loop
			}
			id, ok = idRaw.(string)
			if !ok {
				fmt.Println("Malformed KILL message: id is not a string")
				break loop
			}
			session, exists := s.sessions[id]
			if !exists {
				data := make(map[string]interface{})
				data["reason"] = "such a session does not exist"
				err = sendMessage(Message{
					Command: KILL,
					Data:    data,
					Status:  FAILURE,
				}, conn)
				if err != nil {
					fmt.Println("Failed to send error:", err)
				}
				break loop
			}
			err := session.cmd.Process.Kill()
			if err != nil {
				fmt.Println("Failed to kill session:", err)
			}
			close(session.inputChan)
			delete(s.sessions, session.id)
			fmt.Println("Killed session", id)
			err = sendMessage(Message{
				Command: KILL,
				Data:    make(map[string]interface{}),
				Status:  SUCCESS,
			}, conn)
			if err != nil {
				fmt.Println("Failed to send confirmation:", err)
			}

			break loop
		case DETACH:
			fallthrough
		default:
			data := make(map[string]interface{})
			data["reason"] = "not implemented"
			err = sendMessage(Message{
				Command: msg.Command,
				Data:    data,
				Status:  FAILURE,
			}, conn)
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
			fmt.Println("")
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
			notifyChan <- session.id
		}
	}

	go shellToBuffer(shellStdout)
	go shellToBuffer(shellStderr)
	go inputToShell()

	fmt.Println("Started session", id, "with PID", shell.Process.Pid)

	return session, nil
}

func (s *Server) attachToSession(clientConn *net.TCPConn, session *Session, notifyChan chan bool) {
	detach := make(chan bool, 1)

	rmSub := func() {
		s.notifySync.Lock()
		defer s.notifySync.Unlock()
		subs := s.sessionSubs[session.id]
		nsubs := []chan bool{}
		for _, sub := range subs {
			if sub != notifyChan {
				nsubs = append(nsubs, sub)
			}
		}
		s.sessionSubs[session.id] = nsubs
	}

	go func() {
		for {
			msg, err := receiveMessage(clientConn)
			if err != nil {
				fmt.Println("Failed to receive message:", err)
				break
			}
			if msg.Command == DETACH {
				detach <- true
				break
			}
			if msg.Command != DATA {
				fmt.Println("Expected DATA message, but got:", msg)
				break
			}
			data, ok := msg.Data["data"]
			if !ok {
				fmt.Println("DATA message is ill-formed: ", msg)
				break
			}
			dataS, ok := data.(string)
			if !ok {
				fmt.Println("DATA message is ill-formed: ", msg)
				break
			}
			dataBytes, err := base64.StdEncoding.DecodeString(dataS)
			if err != nil {
				fmt.Println("DATA message is ill-formed: ", msg)
				break
			}
			if len(dataBytes) > 0 {
				session.inputChan <- dataBytes
			}
			if msg.Status == FAILURE {
				close(session.inputChan)
			}
		}
		_ = clientConn.Close()
		rmSub()
	}()

	go func() {
	loop:
		for {
			select {
			case <-detach:
				break loop
			case status := <-notifyChan:
				if !status { // terminate
					err := session.cmd.Process.Kill()
					if err != nil {
						fmt.Println("Failed to kill session:", err)
					}
					break loop
				}
				session.outputSync.Lock()
				data := make(map[string]interface{})
				data["start_pos"] = session.outputStartPosition
				data["data"] = base64.StdEncoding.EncodeToString(session.output)
				msg := Message{
					Command: DATA,
					Data:    data,
					Status:  SUCCESS,
				}
				err := sendMessage(msg, clientConn)
				session.outputSync.Unlock()
				if err != nil {
					fmt.Println("Failed to send DATA:", err)
					break loop
				}
			}
		}
		_ = clientConn.Close()
		rmSub()
	}()
}
