# SO_Project README

# Client's code
This code is a client that sends HTTP GET requests to a server, given a server IP address, a port number, and the number of requests. The client creates multiple child processes to handle the requests, and each child sends one request to the server.

The original code was adapted by Pedro Sobral on 11/02/13 from Nigel Griffiths' code, and was further adapted by Karol Henriques on 17/04/23.

## Getting Started

To compile the client's code, use the following command: gcc client.c -o client
The program accepts four arguments:
Server IP Address: The IP address of the server to which the client will send the requests.
Listening Port: The port number of the server to which the client will connect.
Number of Requests: The total number of requests that the client will send to the server.
Batch Size: The number of requests that each child process will send.
Example usage:
./client 127.0.0.1 8080 10 2
The above command will send 10 requests in batches of 2 to the server at IP address 127.0.0.1, port number 8080.

## Code Structure

The main function is responsible for parsing the command-line arguments and creating child processes to send the requests. Each child process creates a socket, connects to the server, sends an HTTP GET request, receives the response, and extracts the HTTP response code. The response code is written to a pipe, which is read by the parent process. The data is analyzed and a little report is done: the total time of the requests, the average time to complete the request, and the minimum and maximum for a request to be completely handled in printed to the client.

The code uses the fork() system call to create child processes. The pipe() system call is used to create a pipe for inter-process communication. It also uses readN and writeN POSIX syscalls to read data from the pipe and to write this data to a shared file.

The TIMER_START() and TIMER_STOP() functions are used to measure the time taken by each request.

The handle_signal() function is used to handle the SIGPIPE and SIGINT signals.

# Tiny Web Server

This is a simple implementation of a web server that handles only GET requests, based on the HTTP 1.1 specification. It serves static files and logs all requests and errors to a file called tws.log.

The original code was adapted by Pedro Sobral on 11/02/13 from Nigel Griffiths' code, and was further adapted by Karol Henriques on 17/04/23.

## Getting Started

To compile the server's code, use the following command: gcc -o tws tws.c

Example usage:
./tws 8080 ./webdir

The above command will launch the server in the 8080 port and it will start listening to the client's requests.

## Code Structure

This is a C program implementing a simple static file web server that serves files in response to HTTP GET requests. It includes functions for handling error messages, logging requests and responses to disk, and dealing with signals for child processes that end abruptly. The program reads incoming HTTP requests and sends responses back to the client. It also includes a list of file extensions and their corresponding MIME types for serving different types of files.

- Only handles GET requests

- Serves static files

- Logs all requests and errors to tws.log

- Handles SIGCHLD signal to restart child processes that terminate abruptly

## Limitations

This server is not intended to be used in production environments, as it lacks many important features of a real web server, such as support for POST requests, dynamic content generation, and security features. It is meant to be used for educational purposes and as a starting point for building more advanced web servers.

## License

This code is licensed under the UFP license.


