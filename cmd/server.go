package main

import (
	"bsse-linux-team-tasks/screen"
	"flag"
	"fmt"
	"net"
	"os"
	"syscall"
	"time"
)

const (
	kLogFilename = "myscreend.log"
	kPidFilename = "myscreend.pid"
)

func main() {
	daemonize := flag.Bool("d", false, "daemonizes the server when set")
	daemonized := flag.Bool("ddddd", false, "please do not use this haha")
	flag.Parse()
	if *daemonize {
		executable, err := os.Executable()
		if err != nil {
			fmt.Println("Cannot determine absolute path of this executable")
			return
		}
		cwd, err := os.Getwd()
		if err != nil {
			fmt.Println("Cannot determine current working directory")
			return
		}
		pidFile, err := os.OpenFile(kPidFilename, os.O_CREATE | syscall.O_RDWR, 0644)
		if err != nil {
			fmt.Println("Failed to create pid lock-file:", err)
			return
		}
		if err = syscall.Flock(int(pidFile.Fd()), syscall.LOCK_EX | syscall.LOCK_NB); err != nil {
			fmt.Println("Failed to lock pid lock-file:", err)
			return
		}
		logf, err := os.OpenFile(kLogFilename, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0644)
		if err != nil {
			fmt.Println("Failed to create the log file")
			return
		}
		attrs := &os.ProcAttr{
			Dir: cwd,
			Env: os.Environ(),
			Files: []*os.File{
				nil,  // stdin
				logf, // stdout
				logf, // stderr
				pidFile, // pid lock-file
			},
			Sys: &syscall.SysProcAttr{
				Setsid: true, // decouple from tty ????
			},
		}
		daemonProc, err := os.StartProcess(executable, []string{executable, "-ddddd"}, attrs) // fork/exec underneath
		if err != nil {
			fmt.Println("Failed to run daemon child:", err)
			return
		}
		fmt.Println("Daemon PID:", daemonProc.Pid)
		if _, err = pidFile.Write([]byte(fmt.Sprint(daemonProc.Pid))); err != nil {
			fmt.Println("Failed to write pid to pid lock-file:", err)
		}
		time.Sleep(time.Second / 3)
		return
	}
	runServer(*daemonized)
}

func runServer(daemonized bool) {
	if daemonized {
		pidFile := os.NewFile(3, kPidFilename)
		defer func() {
			if err := syscall.Flock(int(pidFile.Fd()), syscall.LOCK_UN); err != nil {
				fmt.Println("Failed to unlock pid lock-file:", err)
				return
			}
			pidFile.Close()
			os.Remove(kPidFilename)
		}()
	}

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
