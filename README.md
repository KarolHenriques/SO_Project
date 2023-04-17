# SO_Project README

# Client's code
This code is a client that sends HTTP GET requests to a server, given a server IP address, a port number, and the number of requests. The client creates multiple child processes to handle the requests, and each child sends one request to the server.

## Getting Started

To compile the client's code, use the following command: gcc client.c -o client

The program accepts four arguments:

1. Server IP Address: The IP address of the server to which the client will send the requests.
2. Listening Port: The port number of the server to which the client will connect.
3. Number of Requests: The total number of requests that the client will send to the server.
4. Batch Size: The number of requests that each child process will send.

Example usage:

./client 127.0.0.1 8080 10 2

The above command will send 10 requests in batches of 2 to the server at IP address 127.0.0.1, port number 8080.

## Code Structure

The main function is responsible for parsing the command-line arguments and creating child processes to send the requests. Each child process creates a socket, connects to the server, sends an HTTP GET request, receives the response, and extracts the HTTP response code. The response code is written to a pipe, which is read by the parent process. The data is analysed and a little report is done: the total time of the requests, the average time to complete the request and the minimum and maximum for a request to completely handled in printed to the client. 

The code uses the fork() system call to create child processes. The pipe() system call is used to create a pipe for inter-process communication. It also uses readn and writen POSIX syscalls to read data from the pipe and to write this data to a shared file. 

The TIMER_START() and TIMER_STOP() functions are used to measure the time taken by each request.

The handle_signal() function is used to handle the SIGPIPE and SIGINT signals.

# Server's code

The server creates a child do handle the client's requests and, as a improvement of this implementatio, creates a pool of processes to handle the client's requests. 

## Getting Started

To compile the server's code, use the following command: gcc -o tws tws.c

Example usage:

./tws 8080 ./webdir

The above command will launch the server in 8080 port and it will start listen the client's requests. 

## Code Structure

The main code is responsible for listen the requests of the clients and keep a pool of child processes available to handle this requests. 

The code uses the fork() system call to create child processes. 

The handle_signal() function is used to handle the SIGCHLD signal.

## License

This code is licensed under the UFP license.


