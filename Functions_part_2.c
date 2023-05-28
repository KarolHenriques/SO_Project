//
//  client.c
//
//
//  Adapted by Pedro Sobral on 11/02/13.
//  Credits to Nigel Griffiths
//
// Adapted by Karol Henriques on 05-02-23

/**
 * @file client.c
 * @brief This file contains the implementation of a client program that sends HTTP requests to a web server.
 *
 * The client program connects to a specified IP address and port number and sends a GET request to the server.
 * It then receives the HTTP response and saves the response code and timing information to a shared file.
 * The program can handle multiple concurrent requests using threads.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

// Function prototypes

/****client.c/

/**
 * @brief Prints an error message and exits the program.
 *
 * This function prints the specified error message using perror() and then terminates the program.
 *
 * @param msg The error message to print.
 */
int pexit(char *msg);

/**
 * @brief Structure for passing arguments to a thread.
 *
 * This structure is used to pass arguments to a thread. It contains the IP address, port number, and other variables needed by the thread.
 */
typedef struct {
    char *IP;      /**< The IP address. */
    char *Port;    /**< The port number. */
    int i;         /**< Integer variable i. */
    int j;         /**< Integer variable j. */
    int file_fd;   /**< The file descriptor of the shared file. */
} ARGS;

/**
 * @brief Structure for storing record information.
 *
 * This structure represents a record and stores the thread ID, bsn, rsn, rc, and t values.
 */
struct Record {
    pthread_t ID; /**< The thread ID. */
    int bsn;      /**< The bsn value. */
    int rsn;      /**< The rsn value. */
    int rc;       /**< The rc value. */
    float t;      /**< The t value. */
};

/**
 * @brief Structure for storing a node in a linked list.
 *
 * This structure represents a node in a linked list and contains a record and a pointer to the next node.
 */
struct Node {
    struct Record record; /**< The record stored in the node. */
    struct Node *next;    /**< Pointer to the next node. */
};

/**
 * @brief Structure for storing data analysis information.
 *
 * This structure represents the data analysis and stores average time, minimum time, maximum time, and total time values.
 */
struct DataAnalysis {
    double avrgTime;    /**< The average time value. */
    double minTime;     /**< The minimum time value. */
    double maxTime;     /**< The maximum time value. */
    float totalTime;    /**< The total time value. */
};

/**
 * @brief Inserts a record at the end of a linked list.
 *
 * This function inserts a record at the end of a linked list.
 *
 * @param head Pointer to the head of the linked list.
 * @param record The record to insert.
 */
void insertRecord(struct Node **head, struct Record record);

/**
 * @brief Prints the contents of a linked list.
 *
 * This function prints the contents of a linked list.
 *
 * @param head Pointer to the head of the linked list.
 */
void printLinkedList(struct Node *head);


/**
 * @brief Handles the SIGINT signal.
 *
 * This function is called when the program receives the SIGINT signal (e.g., when the user presses Ctrl+C).
 * It cancels all running threads and prints the contents of the shared file to the console.
 *
 * @param signal The signal number.
 */
void handle_signal(int signal);

/**
 * @brief Handles the HTTP request.
 *
 * This function is executed by each thread and handles the HTTP request to the web server.
 *
 * @param arg The arguments passed to the thread.
 * @return Void pointer.
 */
void* handle_request(void* arg);

/**
 * @brief Writes the specified number of bytes to a file descriptor.
 *
 * This function writes the specified number of bytes from the buffer to the file descriptor.
 * It is a wrapper around the write() system call to handle interrupted writes.
 *
 * @param fd The file descriptor.
 * @param buf The buffer containing the data to write.
 * @param n The number of bytes to write.
 * @return The number of bytes written on success, or -1 on failure.
 */
ssize_t writen(int fd, const void *buf, size_t n);

/**
 * @brief Reads the specified number of bytes from a file descriptor.
 *
 * This function reads the specified number of bytes from the file descriptor into the buffer.
 * It is a wrapper around the read() system call to handle interrupted reads.
 *
 * @param fd The file descriptor.
 * @param buf The buffer to store the read data.
 * @param n The number of bytes to read.
 * @return The number of bytes read on success, or -1 on failure.
 */
ssize_t readn(int fd, void *buf, size_t n);

/**
 * @brief Saves the record information from a buffer to a struct.
 *
 * This function parses the fields in the buffer and creates a struct with the record information.
 *
 * @param buf The buffer containing the record information.
 * @return The struct containing the record information.
 */
struct Record saveToStruct(char buf[]);

/**
 * @brief Reads records from a file and inserts them into a linked list.
 *
 * This function reads records from the specified file and inserts them into a linked list.
 *
 * @param fileName The name of the file to read from.
 * @param head Pointer to the head of the linked list.
 */
/**
 * @brief Reads data from a file and inserts records into a linked list.
 *
 * This function reads data from the specified file and inserts records into a linked list. Each line in the file represents a record that is saved to the linked list.
 *
 * @param fileName The name of the file to read.
 * @param head Pointer to the head of the linked list.
 */
void readFromFile(char *fileName, struct Node **head);

/**
 * @brief Counts the number of records in a linked list.
 *
 * This function counts the number of records in the specified linked list and returns the count.
 *
 * @param head Pointer to the head of the linked list.
 * @return The number of records in the linked list.
 */
int countRecords(struct Node *head);

/**
 * @brief Calculates statistics for a linked list.
 *
 * This function calculates statistics for the specified linked list, including the average time, minimum time, maximum time, and total time.
 *
 * @param head Pointer to the head of the linked list.
 * @param totalTime The total time value.
 * @return A pointer to a DataAnalysis structure containing the calculated statistics.
 */
struct DataAnalysis *calculateStats(struct Node *head, float totalTime);

/**
 * @brief Prints a data analysis report based on a linked list and time delta.
 *
 * This function generates a data analysis report based on the specified linked list and time delta. It calculates the statistics using the calculateStats function and prints the total time, average time, minimum time, and maximum time.
 *
 * @param head Pointer to the head of the linked list.
 * @param time_delta The time delta value.
 */
void DataAnalysisReport(struct Node *head, float time_delta);

/**
 * @brief Handles a request in a thread.
 *
 * This function is the entry point for a thread that handles a request. It creates a socket, connects to the specified IP address and port, sends an HTTP request, receives the response, extracts the response code, and writes the information to a shared file. It also calculates the time delta and includes it in the written information.
 *
 * @param arg Pointer to an ARGS structure containing the necessary thread information.
 * @return NULL
 */
void *handle_request(void *arg);

/**
 * @brief Handles a signal.
 *
 * This function handles the SIGINT signal, which is used to cancel all threads and print the content available in the shared file. It cancels all threads, reads the shared file, and writes the content to the standard output.
 *
 * @param signal The signal number.
 */
void handle_signal(int signal);

/****tws_2.c/
 
 /**
  * @brief Exits the program after displaying an error message using perror.
  *
  * @param msg The error message to display.
  */
 int pexit(char *msg);

 /**
  * @brief Logs information based on the given type and parameters.
  *
  * @param type       The type of log (ERROR, FORBIDDEN, NOTFOUND, LOG).
  * @param s1         The first string parameter.
  * @param s2         The second string parameter.
  * @param socket_fd  The socket file descriptor.
  */
 void logger(int type, char *s1, char *s2, int socket_fd);

 /**
  * @brief Handles the web request and sends the appropriate response.
  *
  * @param fd   The file descriptor for the socket.
  * @param hit  The hit count.
  *
  * @return Returns 0 on success, 1 on failure.
  */
 int web(int fd, int hit);

/**
 * @brief Handles a request by invoking the web function with the provided socket file descriptor and hit count.
 *
 * @param arg  A pointer to the argument containing the socket file descriptor and hit count.
 *
 * @return Always returns 0.
 */
void* handle_request(void* arg);

/**
 * @brief Represents the consumer thread function that consumes socket file descriptors from the buffer and processes them.
 *
 * @return Always returns NULL.
 */
void* toConsume();

/**
 * @brief Represents the thread function that prints the current state of the buffer.
 *
 * @return Always returns NULL.
 */
void* toPrint();

/**
 * @brief Sets the current state to RECEIVING.
 *
 * @param current_state A pointer to the current state variable.
 */
void ready_state(state_t* current_state);

/**
 * @brief Processes the received data from the socket file descriptor and sets the current state to PROCESSING.
 *
 * @param fd The socket file descriptor.
 * @param hit The hit count.
 * @param current_state A pointer to the current state variable.
 * @param buffer The buffer to store the received data.
 *
 * @return The processed data stored in the buffer. Returns NULL if there is an error reading the data.
 */
char* receiving_state(int fd, int hit, state_t* current_state, char* buffer);

/**
 * @brief Processes the received request by checking the request type, file extension, and opening the file for reading. Sets the current state to SENDING.
 *
 * @param fd The socket file descriptor.
 * @param hit The hit count.
 * @param buffer The buffer containing the received request.
 * @param current_state A pointer to the current state variable.
 *
 * @return The file descriptor of the opened file. Returns 1 if there is an error or the request is invalid.
 */
int processing_state(int fd, int hit, char* buffer, state_t* current_state);

/**
 * @brief Sends the response header and file data to the client. Sets the current state to END.
 *
 * @param fd The socket file descriptor.
 * @param hit The hit count.
 * @param buffer The buffer containing the response header.
 * @param file_fd The file descriptor of the opened file.
 * @param current_state A pointer to the current state variable.
 */
void sending_state(int fd, int hit, char* buffer, int file_fd, state_t* current_state);

/**
 * @brief Handles the end state by closing the connection and setting the current state to READY.
 *
 * @param fd The socket file descriptor.
 * @param current_state A pointer to the current state variable.
 */
void end_state(int fd, state_t* current_state);

/**
 * @brief Represents a worker thread that processes requests from the request buffer.
 *
 * @param args A pointer to the request buffer.
 *
 * @return Always returns NULL.
 */
void* pool(void* args);


