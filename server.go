package main

import (
	"fmt"
	"io"
	"net"
	"os"
	"os/exec"
	"os/signal"
	"strconv"
	"syscall"
)

type SessionData struct {
	id   string
	conn *net.TCPConn
}

type Server struct {
	listener *net.TCPListener
	sChannel *chan SessionData
}

func (s *Server) ListenAndServe() error {
	for {
		conn, err := s.listener.AcceptTCP()
		if err != nil {
			return err
		}

		msg, err := receiveMessage(conn)
		if err != nil {
			return err
		}
		if msg.Sender == SESSION {
			*s.sChannel <- SessionData{id: msg.Argument, conn: conn}
		} else {
			go s.serveClient(conn)
		}
	}
}

func (s *Server) serveClient(conn *net.TCPConn) {
	ss := make(map[string]SessionData)

	for {
		msg, err := receiveMessage(conn)
		if err != nil {
			fmt.Println(err)
			break
		}
		if msg.Sender != CLIENT {
			fmt.Println("sender is not client")
			continue
		}
		switch msg.Command {
		case NEW:
			id := msg.Argument // TODO: check if id is taken
			if msg.Argument == "" {
				id = "id" + strconv.Itoa(len(ss)) // TODO
			}
			err = createSession(id)
			session := <-*s.sChannel
			ss[session.id] = session
			err = sendMessage(Message{SERVER, NEW, ""}, conn)
			if err != nil {
				fmt.Println(err)
				break
			}
			attachToSession(session.conn, conn)

		case LIST:
		case SHELL:
		case ATTACH:
		case DETACH:
		case KILL:
		}
	}
}

func createSession(id string) error {
	conn, err := openConnection(SESSION, id)
	if err != nil {
		return err
	}

	shell := exec.Command("sh")
	shellStdin, err := shell.StdinPipe()
	if err != nil {
		return err
	}
	shellStdout, err := shell.StdoutPipe()
	if err != nil {
		return err
	}
	shellStderr, err := shell.StderrPipe()
	if err != nil {
		return err
	}
	if err := shell.Start(); err != nil {
		return err
	}

	remoteToShell := func() {
		for {
			msg, err := receiveMessage(conn)
			if err != nil {
				err = shellStdin.Close()
				if err != nil {
					fmt.Println("failed to close shell's stdin")
				}
				break
			}
			if !(msg.Sender == SERVER && msg.Command == SHELL) {
				continue
			}
			if _, err = shellStdin.Write([]byte(msg.Argument)); err != nil {
				fmt.Println("failed to write data to shell", err)
				_ = conn.CloseRead()
				break
			}
		}
	}

	shellToRemote := func(pipe io.ReadCloser) {
		buf := make([]byte, 4096)
		for {
			n, err := pipe.Read(buf)
			if err != nil {
				err = conn.CloseWrite()
				if err != nil {
					fmt.Println("failed to close connection write pipe")
				}
				break
			}

			if err = sendMessage(Message{SESSION, SHELL, string(buf[:n])}, conn); err != nil {
				fmt.Println("failed to write data to connection", err)
				_ = pipe.Close()
				break
			}
		}
	}

	go remoteToShell()
	go shellToRemote(shellStdout)
	go shellToRemote(shellStderr)
	return nil
}

func attachToSession(sConn *net.TCPConn, cConn *net.TCPConn) {
	// TODO: last N

	sigs := make(chan os.Signal, 1)
	end := make(chan bool, 1)

	signal.Notify(sigs, syscall.SIGINT)

	go func() {
		<-sigs
		end <- true
	}()

	redirect := func(conn1 *net.TCPConn, conn2 *net.TCPConn, s1 Sender) {
		for {
			msg, err := receiveMessage(conn1)
			if err != nil {
				break
			}
			if msg.Sender != s1 {
				fmt.Println("wrong sender")
				continue
			}
			if err := sendMessage(Message{SERVER, msg.Command, msg.Argument}, conn2); err != nil {
				break
			}
		}
	}

	go redirect(sConn, cConn, SESSION)
	go redirect(cConn, sConn, CLIENT)

	<-end
}
