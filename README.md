# my_screen

Usage:

* server:
    ```shell
    $ go run cmd/server.go 
    ```
    for daemonized run
    ```shell
    $ go run cmd/server.go -d
    ```
* client
    ```shell
     $ go run cmd/client.go list
    ```
    ```shell
     $ go run cmd/client.go new [id]
    ```
    ```shell
     $ go run cmd/client.go attach <id>
    ```
    ```shell
     $ go run cmd/client.go kill <id>
    ```
  
  To detach from session use Ctrl+C
