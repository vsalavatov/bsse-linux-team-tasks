package main

import (
	"encoding/gob"
	"net"
	"regexp"
	"sync/atomic"
)

type Sender int
const (
	SESSION Sender = iota
	CLIENT
	SERVER
)

type Command int
const (
	CONNECT Command = iota
	NEW
	LIST
	SHELL
	ATTACH
	DETACH
	KILL
)

type Message struct {
	Sender Sender
	Command Command
	Argument string
}

type AtomBool struct {flag int32}

func (b *AtomBool) Set(value bool) {
	var i int32 = 0
	if value {i = 1}
	atomic.StoreInt32(&(b.flag), int32(i))
}

func (b *AtomBool) Get() bool {
	if atomic.LoadInt32(&(b.flag)) != 0 {return true}
	return false
}

func sendMessage(msg Message, conn *net.TCPConn) error {
	enc := gob.NewEncoder(conn)
	err := enc.Encode(msg)
	return err
}

func receiveMessage(conn *net.TCPConn) (Message, error) {
	var msg Message
	enc := gob.NewDecoder(conn)
	err := enc.Decode(&msg)
	return msg, err
}

func processArguments(s string) []string {
	var argv []string
	for _, v := range regexp.MustCompile(`[\s\n]+`).Split(s, -1) {
		if len(v) != 0 {
			argv = append(argv, v)
		}
	}
	return argv
}

func openConnection(s Sender, arg string) (*net.TCPConn, error) {
	conn, err := net.DialTCP("tcp", &net.TCPAddr{}, &net.TCPAddr{Port: 8998})
	if err != nil {
		return nil, err
	}
	return conn, sendMessage(Message{Sender: s, Command: CONNECT, Argument: arg}, conn)
}
