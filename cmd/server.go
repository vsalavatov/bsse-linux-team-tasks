package main

import (
	"bsse-linux-team-tasks/screen"
	"fmt"
	"net"
	"os"
)

func main() {
	fmt.Println("Server PID:", os.Getpid())
	listener, err := net.ListenTCP("tcp", &net.TCPAddr{Port: screen.KServerPort})
	if err != nil {
		fmt.Println(err)
		return
	}

	server := screen.NewServer(listener)

	err = server.ListenAndServe()
	if err != nil {
		fmt.Println(err)
		return
	}
}
