package main

import (
	"bufio"
	"fmt"
	"os"
	"os/signal"
	"syscall"
)

func main() {
	conn, err := openConnection(CLIENT, "")
	if err != nil {
		panic(err)
	}

	sigs := make(chan os.Signal, 1)
	end := make(chan bool, 1)

	signal.Notify(sigs, syscall.SIGINT)

	go func() {
		<-sigs
		end <- true
	}()

	isAttached := AtomBool{0}

	go func() {
		reader := bufio.NewReader(os.Stdin)
		for {
			s, err := reader.ReadString('\n')
			if err != nil {
				_ = conn.CloseWrite()
				break
			}

			var c Command
			arg := ""

			if isAttached.Get() {
				c = SHELL
				arg = s
			} else {
				argv := processArguments(s)
				if len(argv) < 2 || argv[0] != "my_screen" {
					continue
				}
				switch argv[1] {
				case "list":
					c = LIST
				case "new":
					c = NEW
					if len(argv) > 2 {
						arg = argv[2]
					}
				case "attach":
					c = ATTACH
					arg = argv[2] // TODO if len(argv) < 3 ...
				case "kill":
					c = KILL
					arg = argv[2] // TODO if len(argv) < 3 ...
				default:
					continue
				}
			}
			err = sendMessage(Message{CLIENT, c, arg}, conn)
			if err != nil {
				fmt.Println("Failed to send data:", err)
				break
			}
		}
	}()

	go func() {
		for {
			msg, err := receiveMessage(conn)
			if err != nil {
				break
			}
			if msg.Sender != SERVER {
				fmt.Println("sender is not server")
				continue
			}
			switch msg.Command {
			case SHELL:
				if _, err := os.Stdout.Write([]byte(msg.Argument)); err != nil {
					break
				}
			// server sends message if command is successfully executed
			case NEW, ATTACH:
				isAttached.Set(true)
			case DETACH:
				isAttached.Set(false)
			}
		}
	}()

	<-end
	fmt.Println()
	fmt.Println("exiting")
}
