# SO_Project_2_Part README

# Main 
This code is a client that sends HTTP GET requests to a server, given a server IP address, a port number, and the number of requests. CHANGE. The code is written in C.

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



# Tiny Web Server

This is a simple implementation of a web server that handles only GET requests, based on the HTTP 1.1 specification. It serves static files and logs all requests and errors to a file called tws.log.

This server can handle multiple client requests using different methods: sequential, forking child processes, or creating a pool of child processes.

The code is written in C and the web() function is responsible for handling the requests. The server listens for incoming requests on a socket and then uses the accept() function to accept the connection request. The incoming socket is passed to the web() function to handle the client's request. After the request is handled, the socket is closed and the server goes back to listening for incoming requests.

The original code was adapted by Pedro Sobral on 02-22-13 from Nigel Griffiths' code and was further adapted by Karol Henriques on 05-07-23.

## Getting Started

To compile the server's code, use the following command: gcc -o tws tws.c

Example usage:
./tws 8080 ./webdir

The above command will launch the server in the 8080 port and it will start listening to the client's requests.

## Code Structure

The code is divided into three sections:

### 1. Sequential

The first section uses a sequential approach, which means that the server handles requests one by one. This approach is not suitable for handling multiple client requests simultaneously as it can cause the server to become unresponsive or slow. 

### 2. Multithreaded Web Server

This implementation is a multithreaded web server that can handle multiple client requests concurrently. The server creates a new thread to handle each incoming client request, so that it can accept new connections while serving existing ones.

To prevent multiple threads from accessing the same resources concurrently, a mutex is used to ensure thread safety. 

### 3. Multithreaded Producer-Consumer Problem Solution 

The program creates a semaphore can_produce and can_consume to control the number of items in the buffer. can_produce is initialized with the number of products that the buffer can hold, and can_consume is initialized with zero, meaning that there are no items in the buffer at the beginning.

There are two mutexes, mutexP and mutexC, to protect the access to the buffer. mutexP is used by the producer, and mutexC is used by the consumers. The buffer is an array of size PRODUCTS.

### 4. FSM

## Limitations

This server is not intended to be used in production environments, as it lacks many important features of a real web server, such as support for POST requests, dynamic content generation, and security features. It is meant to be used for educational purposes and as a starting point for building more advanced web servers.

## License

This code is licensed under the UFP license.

