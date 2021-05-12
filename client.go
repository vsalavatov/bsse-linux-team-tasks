package main

import (
	"bufio"
	"errors"
	"fmt"
	"net"
	"os"
	"os/signal"
	"syscall"
)

const USAGE = `usage:  command [argument]

commands:
	kill <id>
	new [<id>]
	list
	attach <id>`

type Client struct {
	conn *net.TCPConn
}

func parseArguments(args []string) (Message, error) {
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
		arg = args[1] // TODO if len(argv) < 3 ...
	case "kill":
		command = KILL
		arg = args[1] // TODO if len(argv) < 3 ...
	default:
		return Message{}, errors.New(USAGE)
	}
	return Message{CLIENT, command, arg, SUCCESS}, nil
}

func (c *Client) Do(args []string) {
	sigs := make(chan os.Signal, 1)
	end := make(chan bool, 1)

	signal.Notify(sigs, syscall.SIGINT)

	go func() {
		<-sigs
		end <- true
	}()

	go func() {
		msg, err := parseArguments(args)
		if err != nil {
			fmt.Println(err)
			end <- true
			return
		}
		err = sendMessage(msg, c.conn)
		if err != nil {
			fmt.Println("Failed to send data:", err)
			end <- true
			return
		}
		reader := bufio.NewReader(os.Stdin)
		for {
			s, err := reader.ReadString('\n')
			if err != nil {
				_ = c.conn.CloseWrite()
				break
			}
			err = sendMessage(Message{CLIENT, SHELL, s, SUCCESS}, c.conn)
			if err != nil {
				fmt.Println("Failed to send data:", err)
				break
			}
		}
	}()

	go func() {
		msg, err := receiveMessage(c.conn)
		if err != nil {
			fmt.Println("Failed to send data:", err)
			end <- true
			return
		}
		if msg.Status == FAILURE {
			fmt.Println(msg.Argument)
			end <- true
			return
		}
		for {
			msg, err := receiveMessage(c.conn)
			if err != nil {
				break
			}
			if msg.Sender != SERVER {
				fmt.Println("sender is not server")
				continue
			}

			if msg.Command == SHELL {
				if _, err := os.Stdout.Write([]byte(msg.Argument)); err != nil {
					break
				}
			}
		}
	}()

	<-end
	err := sendMessage(Message{CLIENT, DETACH, "", SUCCESS}, c.conn)
	if err != nil {
		fmt.Println("Failed to send data:", err)
	}
	fmt.Println()
	fmt.Println("exiting")

}
func main() {
	args := os.Args[1:]
	if len(args) < 1 {
		fmt.Println(USAGE)
		return
	}

	conn, err := openConnection(CLIENT, "")
	if err != nil {
		panic(err)
	}

	c := Client{conn: conn}
	c.Do(args)
}
