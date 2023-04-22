# SO_Project README

# Main 
This code is a client that sends HTTP GET requests to a server, given a server IP address, a port number, and the number of requests. The client creates multiple child processes to handle the requests, and each child sends one request to the server. The code is written in C.

The code was adapted by Pedro Sobral on 02-22-13 from Nigel Griffiths' code and was further adapted by Karol Henriques on 04-22-23.

## Getting Started

To compile the client's code, use the following command: gcc client.c -o client

The program accepts four arguments:

- Server IP Address: The IP address of the server to which the client will send the requests.

- Listening Port: The port number of the server to which the client will connect.

- Number of Requests: The total number of requests that the client will send to the server.

- Batch Size: The number of requests that each child process will send.

Example usage:
./client 127.0.0.1 8080 10 2

The above command will send 10 requests in batches of 2 to the server at IP address 127.0.0.1, port number 8080.

## Code Structure

The main function is responsible for parsing the command-line arguments and creating child processes to send the requests. Each child process creates a socket, connects to the server, sends an HTTP GET request, receives the response, and extracts the HTTP response code. The response code is written to a pipe, which is read by the parent process. The data is analyzed and a little report is done: the total time of the requests, the average time to complete the request, and the minimum and maximum for a request to be completely handled in printed to the client.

The code uses the fork() system call to create child processes. The pipe() system call is used to create a pipe for inter-process communication. It also uses readN and writeN POSIX syscalls to read data from the pipe and to write this data to a shared file.

The TIMER_START() and TIMER_STOP() functions are used to measure the time taken by each request.

The handle_signal() function is used to handle the SIGPIPE and SIGINT signals.

The code was adapted by Pedro Sobral on 02-22-13 from Nigel Griffiths' code and was further adapted by Karol Henriques on 04-22-23.

# Tiny Web Server

This is a simple implementation of a web server that handles only GET requests, based on the HTTP 1.1 specification. It serves static files and logs all requests and errors to a file called tws.log.

This server can handle multiple client requests using different methods: sequential, forking child processes, or creating a pool of child processes.

The code is written in C and the web() function is responsible for handling the requests. The server listens for incoming requests on a socket and then uses the accept() function to accept the connection request. The incoming socket is passed to the web() function to handle the client's request. After the request is handled, the socket is closed and the server goes back to listening for incoming requests.

The original code was adapted by Pedro Sobral on 02-22-13 from Nigel Griffiths' code and was further adapted by Karol Henriques on 04-22-23.

## Getting Started

To compile the server's code, use the following command: gcc -o tws tws.c

Example usage:
./tws 8080 ./webdir

The above command will launch the server in the 8080 port and it will start listening to the client's requests.

## Code Structure

The code is divided into three sections:

### 1. Sequential

The first section uses a sequential approach, which means that the server handles requests one by one. This approach is not suitable for handling multiple client requests simultaneously as it can cause the server to become unresponsive or slow. 

### 2. Forking Child Processes

The second section uses the forking method to handle client requests. When a client request arrives, a child process is created to handle it while the parent process continues to listen for new incoming requests. This method allows the server to handle multiple requests simultaneously. However, creating a new child process for each request can be resource-intensive, and it may result in the server becoming slow or unresponsive if there are too many requests.

### 3. A pool of Child Processes

The third section uses a pool of child processes to handle client requests. When a request arrives, the parent process assigns it to an available child process in the pool. The child process handles the request, and when it's done, it notifies the parent process, which assigns the next available request to the child process. This method is more efficient than the forking method as it uses a fixed number of child processes, reducing the overhead of creating new processes for each request.

## Limitations

This server is not intended to be used in production environments, as it lacks many important features of a real web server, such as support for POST requests, dynamic content generation, and security features. It is meant to be used for educational purposes and as a starting point for building more advanced web servers.

## License

This code is licensed under the UFP license.

