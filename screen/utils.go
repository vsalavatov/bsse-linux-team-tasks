package screen

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"net"
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

func retrieveString(m map[string]interface{}, key string) (string, error) {
	var val string
	valRaw, ok := m[key]
	if !ok {
		return "", fmt.Errorf("no %v field", key)
	}
	val, ok = valRaw.(string)
	if !ok {
		return "", fmt.Errorf("%v is not a string", key)
	}
	return val, nil
}

func retrieveByteArray(m map[string]interface{}, key string) ([]byte, error) {
	data, ok := m["data"]
	if !ok {
		return nil, fmt.Errorf("no %v field", key)
	}
	dataS, ok := data.(string)
	if !ok {
		return nil, fmt.Errorf("byte array is not base64-encoded")
	}
	dataBytes, err := base64.StdEncoding.DecodeString(dataS)
	return dataBytes, err
}
