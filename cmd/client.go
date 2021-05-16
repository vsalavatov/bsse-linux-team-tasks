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
	conn, err := screen.ConnectToServer()
	if err != nil {
		panic(err)
	}
	c := screen.NewClient(conn)
	c.Do(args)
}
