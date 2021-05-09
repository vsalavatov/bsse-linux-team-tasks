package main

import (
	"fmt"
	"net"
)

func main() {
	listener, err := net.ListenTCP("tcp", &net.TCPAddr{Port: 8998})
	if err != nil {
		fmt.Println(err)
		return
	}

	ch := make(chan SessionData)
	server := Server{
		listener: listener,
		sChannel: &ch,
	}

	err = server.ListenAndServe()
	if err != nil {
		fmt.Println(err)
		return
	}
}
