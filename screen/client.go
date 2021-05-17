package screen

import (
	"bufio"
	"encoding/base64"
	"errors"
	"fmt"
	"net"
	"os"
	"os/signal"
)

const USAGE = `usage:  command [argument]

commands:
	kill <id>
	new [<id>]
	list
	attach <id>`

type Client struct {
	conn *TCPConnWrapper
}

func NewClient(conn *net.TCPConn) *Client {
	return &Client{conn: NewTCPConnWrapper(conn)}
}

type Config struct {
	command  Command
	argument string
}

func parseArguments(args []string) (Config, error) {
	var command Command
	arg := ""

	switch args[0] {
	case "list":
		command = LIST
	case "new":
		command = NEW
		if len(args) > 1 {
			arg = args[1]
		}
	case "attach":
		command = ATTACH
		if len(args) < 2 {
			return Config{}, errors.New(USAGE)
		}
		arg = args[1]
	case "kill":
		command = KILL
		if len(args) < 2 {
			return Config{}, errors.New(USAGE)
		}
		arg = args[1]
	default:
		return Config{}, errors.New(USAGE)
	}
	return Config{
		command:  command,
		argument: arg,
	}, nil
}

func (c *Client) doSession(end chan bool) {
	stopReader := make(chan bool, 1)
	inputs := make(chan []byte, 10)

	go func() {
		reader := bufio.NewReader(os.Stdin)
		var buf [4096]byte
		for {
			n, err := reader.Read(buf[:])
			if err != nil {
				close(inputs)
				return
			}
			inputs <- buf[:n]
		}
	}()

	go func() {
		for {
			select {
			case <-stopReader:
				return
			case s, more := <-inputs:
				if !more {return}
				data := make(map[string]interface{})
				data["data"] = base64.StdEncoding.EncodeToString(s)
				err := c.conn.SendMessage(Message{
					Command: DATA,
					Data:    data,
					Status:  SUCCESS,
				})
				if err != nil {
					fmt.Println("Failed to send data:", err)
					break
				}
			}
		}
	}()

	go func() {
		var lastPrintedPos int = 0
		for {
			msg, err := c.conn.RecvMessage()
			if err != nil {
				fmt.Println("Failed to receive message:", err)
				break
			}
			if msg.Command != DATA {
				fmt.Println("Unexpected message:", msg)
				break
			}
			startPosR, ok := msg.Data["start_pos"]
			startPosF, ok2 := startPosR.(float64)
			if !ok || !ok2 {
				fmt.Println("Ill-formed response:", msg)
				break
			}
			startPos := int(startPosF)
			data, err := retrieveByteArray(msg.Data, "data")
			if err != nil {
				fmt.Println("Ill-formed response:", err)
				break
			}

			if startPos+len(data) > lastPrintedPos {
				toPrintFrom := lastPrintedPos - startPos
				if toPrintFrom < 0 {
					toPrintFrom = 0
				}
				toPrint := data[toPrintFrom:]
				lastPrintedPos = startPos + len(data)
				if _, err := os.Stdout.Write(toPrint); err != nil {
					break
				}
			}
		}
	}()

	<-end
	stopReader <- true
	err := c.conn.SendMessage(Message{
		Command: DETACH,
		Data:    make(map[string]interface{}),
		Status:  SUCCESS,
	})
	if err != nil {
		fmt.Println("Failed to send data:", err)
	}
	fmt.Println()
	fmt.Println("exiting")
}

func (c *Client) Do(args []string) {
	sigs := make(chan os.Signal, 1)
	end := make(chan bool, 2)

	signal.Notify(sigs, os.Interrupt)

	go func() {
		<-sigs
		end <- true
	}()

	conf, err := parseArguments(args)
	if err != nil {
		fmt.Println(err)
		return
	}

	switch conf.command {
	case LIST:
		err = c.conn.SendMessage(Message{
			Command: LIST,
			Data:    make(map[string]interface{}),
			Status:  SUCCESS,
		})
		if err != nil {
			fmt.Println("Failed to send LIST:", err)
			return
		}
		resp, err := c.conn.RecvMessage()
		if err != nil {
			fmt.Println("Failed to receive response:", err)
			return
		}
		sessions, err := retrieveStringArray(resp.Data, "sessions")
		if err != nil {
			fmt.Println("Ill-formed LIST response:", err, resp)
			return
		}
		fmt.Println(len(sessions), "sessions:")
		for _, sess := range sessions {
			fmt.Println(sess)
		}
	case NEW:
		fallthrough
	case ATTACH:
		data := make(map[string]interface{})
		if len(conf.argument) > 0 {
			data["id"] = conf.argument
		}
		err = c.conn.SendMessage(Message{
			Command: conf.command,
			Data:    data,
			Status:  SUCCESS,
		})
		if err != nil {
			fmt.Println("Failed to send", conf.command, ":", err)
			return
		}
		resp, err := c.conn.RecvMessage()
		if err != nil {
			fmt.Println("Failed to receive response:", err)
			return
		}
		if resp.Command != conf.command {
			fmt.Println("Expected to receive response to", conf.command, ", but got:", resp)
			return
		}
		if resp.Status != SUCCESS {
			fmt.Println("Operation failed:", resp)
			return
		}
		c.doSession(end)
	case KILL:
		data := make(map[string]interface{})
		data["id"] = conf.argument
		err = c.conn.SendMessage(Message{
			Command: KILL,
			Data:    data,
			Status:  SUCCESS,
		})
		if err != nil {
			fmt.Println("Failed to send LIST:", err)
			return
		}
		resp, err := c.conn.RecvMessage()
		if err != nil {
			fmt.Println("Failed to receive response:", err)
			return
		}
		if resp.Command != KILL {
			fmt.Println("Expected to receive response to KILL, but got:", resp)
			return
		}
		if resp.Status == SUCCESS {
			fmt.Println("OK!")
			return
		}
		fmt.Println("Failed to kill the session:", resp)
	}
}

func ConnectToServer() (*net.TCPConn, error) {
	return net.DialTCP("tcp", nil, &net.TCPAddr{Port: KServerPort})
}
