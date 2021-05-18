package main

import (
	"bsse-linux-team-tasks/screen"
	"fmt"
	"os"
)

func main() {
	args := os.Args[1:]
	if len(args) < 1 {
		fmt.Println(screen.USAGE)
		return
	}
	conn, err := screen.ConnectToServer(screen.KServerPort)
	if err != nil {
		fmt.Println("Failed to connect to the server:", err)
		return
	}
	c := screen.NewClient(conn)
	c.Do(args)
}
