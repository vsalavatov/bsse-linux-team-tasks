package screen

import (
	"encoding/json"
	"net"
	"sync/atomic"
)

const KServerPort = 8998

type Command int
const (
	NEW Command = iota
	LIST
	ATTACH
	DETACH
	KILL

	DATA
)

type Status bool
const (
	FAILURE Status = false
	SUCCESS        = true
)

type Message struct {
	Command Command
	Data    map[string]interface{}
	Status  Status
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
	enc := json.NewEncoder(conn)
	err := enc.Encode(msg)
	return err
}

func receiveMessage(conn *net.TCPConn) (Message, error) {
	var msg Message
	enc := json.NewDecoder(conn)
	err := enc.Decode(&msg)
	return msg, err
}
