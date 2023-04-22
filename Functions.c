//
//  Functions.c
//  
//
//  Created by Karol Henriques on 22/04/2023.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <limits.h>
#include <signal.h>

/**
 * Represents a record of data containing information about a process.
 */
struct Record {
    pid_t pid; /**< The process ID. */
    int bsn; /**< The block sequence number. */
    int rsn; /**< The record sequence number. */
    int rc; /**< The return code. */
    double t; /**< The time elapsed. */
};

/**
 * Represents a node in a linked list of records.
 */
struct Node {
    struct Record record; /**< The record stored in the node. */
    struct Node* next; /**< A pointer to the next node in the list. */
};

/**
 * Contains statistical data about a set of records.
 */
struct DataAnalysis {
    double avrgTime; /**< The average time elapsed across all records. */
    double minTime; /**< The minimum time elapsed across all records. */
    double maxTime; /**< The maximum time elapsed across all records. */
    float totalTime; /**< The total time elapsed across all records. */
};

/**
 * @brief Prints an error message using perror and exits the program with a return code of 1.
 *
 * @param msg The error message to print using perror.
 */
int pexit(char* msg);

/**
 * @brief Reads n bytes from a file descriptor
 *
 * This function reads n bytes from the file descriptor fd and stores them in the buffer pointed to by buf.
 * If the read operation is interrupted by a signal, the function will attempt to read again.
 * If an error occurs, the function returns -1.
 *
 * @param fd    The file descriptor to read from.
 * @param buf   A pointer to the buffer to store the read data in.
 * @param n     The number of bytes to read.
 *
 * @return The number of bytes read, or -1 if an error occurs.
 */
ssize_t readn(int fd, void *buf, size_t n);

/**
 * @brief Writes n bytes to a file descriptor
 *
 * This function writes n bytes from the buffer pointed to by buf to the file descriptor fd.
 * If the write operation is interrupted by a signal, the function will attempt to write again.
 * If an error occurs, the function returns -1.
 *
 * @param fd    The file descriptor to write to.
 * @param buf   A pointer to the buffer containing the data to write.
 * @param n     The number of bytes to write.
 *
 * @return The number of bytes written, or -1 if an error occurs.
 */
ssize_t writen(int fd, const void *buf, size_t n);

/**
 * @brief Inserts a record into a linked list of nodes.
 *
 * @param head A pointer to a pointer to the head node of the linked list.
 * @param record The record to be inserted.
 */
void insertRecord(struct Node** head, struct Record record);

/**
 * Prints the contents of a linked list of Nodes.
 *
 * @param head Pointer to the head of the linked list.
 */
void printLinkedList(struct Node* head);

/**
 * Reads data from a pipe and writes it to a file.
 *
 * @param pipefd An array of integers representing the file descriptors for the read and write ends of the pipe.
 * @param fileName A string representing the name of the file to write the data to.
 */
void pipeToFile(int pipefd[], char* fileName);

/**
 * @brief Parse a character array and populate a Record struct.
 *
 * This function takes a character array `buf` and parses it to fill the fields
 * of a `Record` struct. The character array should contain exactly five fields
 * separated by semicolons, in the following order: `pid`, `bsn`, `rsn`, `rc`, `t`.
 *
 * @param buf The character array to parse.
 *
 * @return A `Record` struct with the fields filled in from the parsed data.
 */
struct Record saveToStruct(char buf[]);

/**
 * @brief Read data from a file and insert into a linked list.
 *
 * This function reads data from a file with the given `fileName`, parses the data
 * into `Record` structs, and inserts the structs into a linked list pointed to by
 * `head`. The file should contain one record per line, where each record consists
 * of five fields separated by semicolons: `pid`, `bsn`, `rsn`, `rc`, `t`.
 *
 * @param fileName The name of the file to read.
 * @param head A pointer to the head of the linked list to insert records into.
 */
void readFromFile(char* fileName, struct Node** head);

/**
 * @brief Terminate child processes created by the parent process.
 *
 * This function iterates over the array `childrenPids` and sends a `SIGTERM` signal
 * to each non-zero process ID in the array. It then waits for each child process
 * to exit, and checks its status. If the child process exited due to a signal, the
 * function prints a message indicating which signal terminated the child. If the
 * child process exited normally, the function prints a message indicating the exit
 * status. Finally, for each child process that has not yet terminated, the function
 * sends a `SIGKILL` signal to force termination.
 */
void terminateChildren();

/**
 * @brief Count the number of records in a linked list.
 *
 * This function traverses the linked list pointed to by `head` and counts the number
 * of nodes in the list. Each node in the list represents a record, so the count is
 * equivalent to the number of records in the list.
 *
 * @param head A pointer to the head of the linked list.
 *
 * @return The number of records in the linked list.
 */
int countRecords(struct Node* head);

/**
 * @brief Calculate statistics for a linked list of records.
 *
 * This function calculates the average, minimum, and maximum time values for a linked
 * list of records, as well as the total time passed in as an argument. It returns a
 * dynamically allocated struct containing these statistics.
 *
 * @param head A pointer to the head of the linked list.
 * @param totalTime The total time to include in the statistics.
 *
 * @return A pointer to a dynamically allocated struct containing the calculated statistics.
 *         Returns NULL if the input linked list is empty.
 */
struct DataAnalysis* calculateStats(struct Node* head, float totalTime);

/**
 * @brief Calculates statistics on a linked list of nodes and prints a report to stdout.
 *
 * @param head A pointer to the head node of the linked list.
 * @param time_delta The time delta used for calculating the statistics.
 */
void DataAnalysisReport(struct Node* head, float time_delta);

/**
 * @brief Signal handler function for handling different signals.
 *
 * @param signal The signal number.
 */
void handle_signal(int signal);



