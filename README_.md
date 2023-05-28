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

The client.c file contains C code for a client application that communicates with a web server
using TCP/IP sockets. The code is responsible for sending HTTP requests to the server and analyzing
the response times of the requests. 

The main function initializes variables, sets up the SIGINT signal handler, validates command-line
arguments, establishes a connection to the server, sends HTTP requests, and manages threads for
handling requests. It also handles the termination of the program when the SIGINT signal is
received.

# Tiny Web Server

This is a simple implementation of a web server that handles only GET requests, based on the HTTP
1.1 specification. It serves static files and logs all requests and errors to a file called tws.log.

This server can handle multiple client requests using different methods: sequential, forking child
processes, or creating a pool of child processes.

The code is written in C and the web() function is responsible for handling the requests. The
server listens for incoming requests on a socket and then uses the accept() function to accept the
connection request. The incoming socket is passed to the web() function to handle the client's
request. After the request is handled, the socket is closed and the server goes back to listening
for incoming requests.

The original code was adapted by Pedro Sobral on 02-22-13 from Nigel Griffiths' code and was
further adapted by Karol Henriques on 05-07-23.

## Getting Started

To compile the server's code, use the following command: gcc -o tws tws.c

Example usage:
./tws 8080 ./webdir

The above command will launch the server in the 8080 port and it will start listening to the client's requests.

## Code Structure

The code is divided into three sections:

### 1. Sequential

The first section uses a sequential approach, which means that the server handles requests one by
one. This approach is not suitable for handling multiple client requests simultaneously as it can
cause the server to become unresponsive or slow. 

### 2. Multithreaded Web Server

This implementation is a multithreaded web server that can handle multiple client requests
concurrently. The server creates a new thread to handle each incoming client request, so that it
can accept new connections while serving existing ones.

To prevent multiple threads from accessing the same resources concurrently, a mutex is used to
ensure thread safety. 

### 3. Multithreaded Producer-Consumer Problem Solution 

The program creates a semaphore can_produce and can_consume to control the number of items in the
buffer. can_produce is initialized with the number of products that the buffer can hold, and
can_consume is initialized with zero, meaning that there are no items in the buffer at the
beginning.

There are two mutexes, mutexP and mutexC, to protect the access to the buffer. mutexP is used by
the producer, and mutexC is used by the consumers. The buffer is an array of size PRODUCTS.

### 4. FSM

The code provided is an implementation of a web server using a Finite State Machine (FSM) approach.
The FSM is used to handle each client request received by the server. Below is an overview of the
FSM implementation in the code:

1. ready_state(state_t* current_state): A function representing the READY state of the FSM. It
    initializes the buffer variables and sets the current state to READY.

2. receiving_state(int fd, int hit, state_t* current_state, char* buf): A function representing the
    RECEIVING state of the FSM. It receives the client request, stores it in the buffer, and
    transitions to the PROCESSING state.

3. processing_state(int fd, int hit, char* buffer, state_t* current_state): A function representing the PROCESSING state of the FSM. It processes the client request by calling the web function to handle the request and transitions to the SENDING state.

4. sending_state(int fd, int hit, char* buffer, int file_fd, state_t* current_state): A function representing the SENDING state of the FSM. It sends the response to the client and transitions to the END state.

5. end_state(int fd, state_t* current_state): A function representing the END state of the FSM. It closes the client connection and transitions back to the READY state.

To ensure thread safety, the code uses mutexes and semaphores to control access to shared
resources, such as the request buffer and file operations. Mutexes are used to protect critical
sections of code where multiple threads might access shared variables simultaneously. Semaphores
are used to synchronize the producer and consumer threads and control access to the request buffer.

The FSM implementation in this code is relatively simple and handles only a basic sequence of
states for each client request. More complex FSMs might include additional states and transitions
to handle more intricate request processing scenarios.

## Limitations

This server is not intended to be used in production environments, as it lacks many important
features of a real web server, such as support for POST requests, dynamic content generation, and
security features. It is meant to be used for educational purposes and as a starting point for
building more advanced web servers.

## License

This code is licensed under the UFP license.

