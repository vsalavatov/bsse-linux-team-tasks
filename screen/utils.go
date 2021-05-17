package screen

import (
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"errors"
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

type TCPConnWrapper struct {
	Conn *net.TCPConn
}

func NewTCPConnWrapper(conn *net.TCPConn) *TCPConnWrapper {
	return &TCPConnWrapper{Conn: conn}
}

func (c *TCPConnWrapper) RecvMessage() (Message, error) {
	var szbuf [4]byte
	n, err := c.Conn.Read(szbuf[:])
	if err != nil {
		return Message{}, err
	}
	if n != 4 {
		return Message{}, errors.New("protocol error")
	}
	sz := int(binary.LittleEndian.Uint32(szbuf[:]))
	data := make([]byte, sz)
	n, err = c.Conn.Read(data[:])
	if err != nil {
		return Message{}, err
	}
	if n != sz {
		return Message{}, errors.New("protocol error")
	}
	var msg Message
	err = json.Unmarshal(data[:], &msg)
	return msg, err
}

func (c *TCPConnWrapper) SendMessage(m Message) error {
	data, err := json.Marshal(m)
	if err != nil {
		return err
	}
	var szbuf [4]byte
	binary.LittleEndian.PutUint32(szbuf[:], uint32(len(data)))
	_, err = c.Conn.Write(szbuf[:])
	if err != nil {
		return nil
	}
	_, err = c.Conn.Write(data[:])
	return err
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
	data, ok := m[key]
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

func retrieveStringArray(m map[string]interface{}, key string) ([]string, error) {
	dataR, ok := m[key]
	if !ok {
		return nil, fmt.Errorf("no %v field", key)
	}
	if dataR == nil {
		return nil, nil
	}
	dataRA, ok := dataR.([]interface{})
	if !ok {
		return nil, fmt.Errorf("object is not an array")
	}
	data := make([]string, len(dataRA))
	for i := range dataRA {
		data[i], ok = dataRA[i].(string)
		if !ok {
			return nil, fmt.Errorf("element is not a string")
		}
	}
	return data, nil
}
