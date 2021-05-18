package screen

import (
	"bytes"
	"io"
	"log"
	"net"
	"os"
	"strings"
	"sync"
	"syscall"
	"testing"
	"time"
)

const TestServerPort = 9090
const (
	hello = "echo hello\n"
	cycle = `for i in 1 2 3
do
echo $i
done`
)

func fakeClient(conn *net.TCPConn, reader io.Reader, writer io.Writer) *Client {
	return &Client{conn: NewTCPConnWrapper(conn), reader: reader, writer: writer, sigs: make(chan os.Signal)}
}

func fakeServer(listener *net.TCPListener, sigs chan os.Signal) *Server {
	return &Server{
		listener:    listener,
		sessions:    make(map[string]*Session),
		notifyChan:  make(chan string, 1),
		sessionSubs: make(map[string][]chan bool), // false means "terminate"
		notifySync:  &sync.Mutex{},
		sigs:        sigs,
		redirectErr: false,
	}
}

func assertOutput(t *testing.T, expected string, actualBytes []byte) {
	actual := string(actualBytes)
	if expected != actual {
		t.Errorf("Server MUST return:\n %s, but:\n %s given",
			expected, actual)
	}
}

func assertListOutput(t *testing.T, expected map[string]bool, actualBytes []byte) {
	actual := make(map[string]bool)
	for _, v := range strings.Split(string(actualBytes), "\n")[1:] {
		if len(v) != 0 {
			actual[v] = true
		}
	}
	for k, _ := range actual {
		if _, ok := expected[k]; !ok {
			t.Errorf("Unexpected session id: %s\n", k)
		}
	}
	for k, _ := range expected {
		if _, ok := actual[k]; !ok {
			t.Errorf("Expected session id %s wasn't detected\n", k)
		}
	}
}

func createClient(input string, writer io.Writer, args []string, port int) *Client {
	reader := strings.NewReader(input)
	conn, _ := ConnectToServer(port)
	c := fakeClient(conn, reader, writer)

	go c.Do(args)
	time.Sleep(time.Millisecond * 10)
	return c
}

func stopClient(c *Client) {
	c.sigs <- os.Interrupt
	time.Sleep(time.Millisecond * 10)
}

func quiet() func() {
	null, _ := os.Open(os.DevNull)
	sout := os.Stdout
	serr := os.Stderr
	os.Stdout = null
	os.Stderr = null
	log.SetOutput(null)
	return func() {
		defer null.Close()
		os.Stdout = sout
		os.Stderr = serr
		log.SetOutput(os.Stderr)
	}
}

func createServer(port int) chan os.Signal {
	listener, _ := net.ListenTCP("tcp", &net.TCPAddr{Port: port})
	sigs := make(chan os.Signal, 1)
	server := fakeServer(listener, sigs)
	go func() {
		_ = server.ListenAndServe()
	}()
	return sigs
}
func Test_new(t *testing.T) {
	defer quiet()()
	port := TestServerPort + 0
	sSigs := createServer(port)

	t.Run("echo hello to new session", func(t *testing.T) {
		writer := bytes.NewBuffer(make([]byte, 0, 4096))
		stopClient(createClient(hello, writer, []string{"new", "id1"}, port))
		assertOutput(t, "hello\n", writer.Bytes())
	})

	t.Run("id is already taken", func(t *testing.T) {
		writer := bytes.NewBuffer(make([]byte, 0, 4096))
		stopClient(createClient("", bytes.NewBuffer(make([]byte, 0, 4096)), []string{"new", "id2"}, port))
		stopClient(createClient("", writer, []string{"new", "id2"}, port))
		assertOutput(t, "Operation failed: id is already taken\n", writer.Bytes())
	})

	sSigs <- syscall.SIGINT
	time.Sleep(time.Millisecond * 100)
}

func Test_attach(t *testing.T) {
	defer quiet()()
	port := TestServerPort + 1
	sSigs := createServer(port)

	t.Run("attach to session", func(t *testing.T) {
		writer := bytes.NewBuffer(make([]byte, 0, 4096))
		stopClient(createClient(cycle, bytes.NewBuffer(make([]byte, 0, 4096)), []string{"new", "id1"}, port))
		stopClient(createClient("", writer, []string{"attach", "id1"}, port))
		assertOutput(t, "1\n2\n3\n", writer.Bytes())
	})

	t.Run("no such session", func(t *testing.T) {
		writer := bytes.NewBuffer(make([]byte, 0, 4096))
		stopClient(createClient("", writer, []string{"attach", "id2"}, port))
		assertOutput(t, "Operation failed: such a session does not exist\n", writer.Bytes())
	})

	t.Run("failed to attach to the same session twice", func(t *testing.T) {
		c1 := createClient("", bytes.NewBuffer(make([]byte, 0, 4096)), []string{"new", "id3"}, port)

		writer2 := bytes.NewBuffer(make([]byte, 0, 4096))
		stopClient(createClient("", writer2, []string{"attach", "id3"}, port))
		stopClient(c1)
		assertOutput(t, "Operation failed: there's another client attached to this session\n", writer2.Bytes())
	})

	sSigs <- syscall.SIGINT
	time.Sleep(time.Millisecond * 100)
}

func Test_kill(t *testing.T) {
	defer quiet()()
	port := TestServerPort + 2
	sSigs := createServer(port)

	t.Run("create and kill session", func(t *testing.T) {
		stopClient(createClient("", bytes.NewBuffer(make([]byte, 0, 4096)), []string{"new", "id1"}, port))
		writer := bytes.NewBuffer(make([]byte, 0, 4096))
		stopClient(createClient("", writer, []string{"list"}, port))
		assertListOutput(t, map[string]bool{"id1": true}, writer.Bytes())
		stopClient(createClient("", bytes.NewBuffer(make([]byte, 0, 4096)), []string{"kill", "id1"}, port))
		writer = bytes.NewBuffer(make([]byte, 0, 4096))
		stopClient(createClient("", writer, []string{"list"}, port))
		assertListOutput(t, map[string]bool{}, writer.Bytes())
	})

	t.Run("no such session", func(t *testing.T) {
		writer := bytes.NewBuffer(make([]byte, 0, 4096))
		stopClient(createClient("", writer, []string{"kill", "id2"}, port))
		assertOutput(t, "Failed to kill the session: such a session does not exist\n", writer.Bytes())
	})

	t.Run("killed by another client", func(t *testing.T) {
		c1 := createClient(hello, bytes.NewBuffer(make([]byte, 0, 4096)), []string{"new", "id3"}, port)
		writer2 := bytes.NewBuffer(make([]byte, 0, 4096))
		stopClient(createClient("", writer2, []string{"kill", "id3"}, port))
		stopClient(c1)
		assertOutput(t, "OK!\n", writer2.Bytes())

		writerList := bytes.NewBuffer(make([]byte, 0, 4096))
		stopClient(createClient("", writerList, []string{"list"}, port))
		assertListOutput(t, map[string]bool{}, writerList.Bytes())
	})

	sSigs <- syscall.SIGINT
	time.Sleep(time.Millisecond * 100)
}

func Test_list(t *testing.T) {
	defer quiet()()
	port := TestServerPort + 3
	sSigs := createServer(port)

	t.Run("empty session's list", func(t *testing.T) {
		writer := bytes.NewBuffer(make([]byte, 0, 4096))
		stopClient(createClient("", writer, []string{"list"}, port))
		assertListOutput(t, map[string]bool{}, writer.Bytes())
	})

	t.Run("list sessions", func(t *testing.T) {
		stopClient(createClient("", bytes.NewBuffer(make([]byte, 0, 4096)), []string{"new", "id1"}, port))
		stopClient(createClient("", bytes.NewBuffer(make([]byte, 0, 4096)), []string{"new", "id2"}, port))
		writer := bytes.NewBuffer(make([]byte, 0, 4096))
		stopClient(createClient("", writer, []string{"list"}, port))
		assertListOutput(t, map[string]bool{"id1": true, "id2": true}, writer.Bytes())
	})

	sSigs <- syscall.SIGINT
	time.Sleep(time.Millisecond * 100)
}
