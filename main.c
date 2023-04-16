//
//  SO_Project_1version.c
//
//
//  Created by Karol Henriques on 04/04/2023.
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

//#include "aux_functions_SO_Project.h"

#define SOCK_PATH "/tmp/socket"
#define MAX_LINE_SIZE 1024
#define BUFSIZE 4096
#define TIMER_START() gettimeofday(&tv1, NULL)
#define TIMER_STOP() \
gettimeofday(&tv2, NULL); \
timersub(&tv2, &tv1, &tv); \
time_delta = (float)tv.tv_sec + tv.tv_usec / 1000000.0

#define IP_MAX_LENGTH 16
#define URL_MAX_LENGTH 40
#define REQUEST_MAX 1000000

//Global variables

int pipefd[2];
volatile bool flagCreatChildren = true;
long* childrenPids;

int pexit(char* msg){
    perror(msg);
    exit(1);
}

struct Record {
    pid_t pid;
    int bsn;
    int rsn;
    int rc;
    double t;
};

struct Node {
    struct Record record;
    struct Node* next;
};

struct Node* insertRecord(struct Node** head, struct Record record) {
    struct Node* new_node = (struct Node*)malloc(sizeof(struct Node));
    if (new_node == NULL) {
        pexit("Unable to allocate memory");
    }
    new_node->record = record;
    new_node->next = *head;
    
    if (*head == NULL) {
            // If the linked list is empty, set the head pointer to the new node
            *head = new_node;
        } else {
            // Otherwise, update the new node's next pointer to point to the current head
            new_node->next = *head;
            // Update the head pointer to point to the new node
            *head = new_node;
        }

    //the insertions are happening in the head. So, we are returning the new head
    printf("New node created with PID %d, next pointer is %p.\n", new_node->record.pid, new_node->next);
    printf("Head pointer is %p.\n", *head);
    return new_node;
}

void printLinkedListElements(struct Node** head){
    struct Node* current = *head;
    
    while (current != NULL) {
        printf("PID: %d, BSN: %d, RSN: %d, RC: %d, T: %f\n", current->record.pid, current->record.bsn, current->record.rsn, current->record.rc, current->record.t);
        current = current->next;
    }
}

void pipeToFile(int pipefd[], char* fileName, bool writeIt) {
    close(pipefd[1]);
    //int fd = open(fileName, O_RDONLY); this permition is to read the data if was the child that wrote that
    int fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        pexit("Unable to open file");
    }
    
    int pid, bsn, rsn, rc;
    float t;
    char buf[BUFSIZE];
    ssize_t count = read(pipefd[0], buf, sizeof(buf));
    
    if (count == -1) {
        pexit("read from pipe");
    }
    
    printf("Parent read message from pipe:\n%.*s\n", (int)count, buf);
  
    if(writeIt){
        if(write(fd, buf, strlen(buf)) < 0){
            pexit("writing to shared file error (parent)");
        }
    }
    close(pipefd[0]);
    close(fd);
}

/*void readFromFile(char* fileName, struct Node** head){
    
    printf("head = %p\n", head);
    
    char buf[MAX_LINE_SIZE];
    char c;
    int n = 0;
    size_t len = 0, string_len = 0;
    int fd = open(fileName, O_RDONLY);
    if (fd == -1) {
        pexit("Unable to open file");
    }
    
    printf("Parent reading from file:\n");
    
    struct Record record;
    
    while ((n = read(fd, buf, MAX_LINE_SIZE)) > 0) {
        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                // End of line
                //printf("%.*s\n", (int)len, buf);
                //Create a new Record:
                printf("Create a new struc:\n");
                
                //struct Record record;

                printf("Input string: %.*s\n", (int)len, buf);
                record.pid = atoi(strtok(buf, ";"));
                printf("PID: %d\n", record.pid);
                
                //record.bsn = atoi(strtok(NULL, ";"));
                //printf("BSN: %d\n", record.bsn);
                char* token = strtok(NULL, ";");

                if (token != NULL) {
                    printf("BSN: %d\n", record.bsn);
                    record.bsn = atoi(token);

                } else {
                    printf("%d", record.bsn);
                    pexit("Strtok problem");
                }
                
                record.rsn = atoi(strtok(NULL, ";"));
                printf("RSN: %d\n", record.rsn);
                record.rc = atoi(strtok(NULL, ";"));
                printf("RC: %d\n", record.rc);
                record.t = atof(strtok(NULL, ";"));
                printf("T: %f\n", record.t);
                
                //Insert at linked list
                //insertRecord(head, record);
                *head = insertRecord(head, record);
                
                len = 0;
            } else {
                // Append the character to the current line
                if (len == MAX_LINE_SIZE - 1) {
                    pexit("Line too long");
                }
                buf[len] = buf[i];
                len++;
            }
        }
    }
    
    if(n == 0){ //end of file
        if (len > 0) {
            // Print the last line
            printf("%.*s\n", (int) len, buf);
        }
        //close(fd);
    }
    if (n == -1) {
        pexit("Unable to read file");
        exit(EXIT_FAILURE);
    }
    close(fd);
}*/

void readFromFile(char* fileName, struct Node** head){
    printf("head = %p\n", head);
    char buf[MAX_LINE_SIZE];
    char c;
    int n = 0, remaining_len = 0;
    size_t len = 0, string_len = 0;
    int fd = open(fileName, O_RDONLY);
    if (fd == -1) {
        pexit("Unable to open file");
    }
    printf("Parent reading from file:\n");
    struct Record record;
    static char remaining_buf[MAX_LINE_SIZE];
    while ((n = read(fd, buf, MAX_LINE_SIZE)) > 0) {
        if (remaining_len > 0) {
            // If there's remaining buffer from the previous iteration, copy it to the beginning of the new buffer
            memcpy(buf, remaining_buf, remaining_len);
            len = remaining_len;
        }
        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                // End of line
                string_len = len + i + 1; // Include the newline character
                memcpy(remaining_buf, buf + i + 1, n - i - 1); // Copy remaining buffer to the static variable
                remaining_len = n - i - 1; // Set the remaining buffer length
                //Create a new Record:
                //printf("Create a new struc:\n");
                //struct Record record;
                printf("Input string: %.*s\n", (int)string_len, buf);
                record.pid = atoi(strtok(buf, ";"));
                char* token = strtok(NULL, ";");

                if (token != NULL) {
                    printf("BSN: %d\n", record.bsn);
                    record.bsn = atoi(token);

                } else {
                    printf("%d", record.bsn);
                    pexit("Strtok problem");
                }
                
                record.rsn = atoi(strtok(NULL, ";"));
                printf("RSN: %d\n", record.rsn);
                record.rc = atoi(strtok(NULL, ";"));
                printf("RC: %d\n", record.rc);
                record.t = atof(strtok(NULL, ";"));
                printf("T: %f\n", record.t);
                *head = insertRecord(head, record);
                len = 0;
                break;
            }
        }
        len += n;
    }
    // If there's remaining buffer at the end of the file, create a new record with it
    if (remaining_len > 0) {
        string_len = len + remaining_len;
        memcpy(buf, remaining_buf, remaining_len);
        printf("Create a new struc:\n");
        printf("Input string: %.*s\n", (int)string_len, buf);
        record.pid = atoi(strtok(buf, ";"));
        printf("PID: %d\n", record.pid);
        
        char* token = strtok(NULL, ";");

        if (token != NULL) {
            printf("BSN: %d\n", record.bsn);
            record.bsn = atoi(token);

        } else {
            printf("%d", record.bsn);
            pexit("Strtok problem");
        }
        
        record.rsn = atoi(strtok(NULL, ";"));
        printf("RSN: %d\n", record.rsn);
        record.rc = atoi(strtok(NULL, ";"));
        printf("RC: %d\n", record.rc);
        record.t = atof(strtok(NULL, ";"));
        printf("T: %f\n", record.t);
        
        //Insert at linked list
        //insertRecord(head, record);
        *head = insertRecord(head, record);
    }
    close(fd);
}


void terminateChildren(){
    int status;
    for(long i = 0; i < sizeof(childrenPids); i++){
        kill(SIGINT, childrenPids[i]);
        waitpid(childrenPids[i], &status, 0);
        if (WIFSIGNALED(status)) {
            printf("Child %ld terminated by signal %d\n", childrenPids[i], WTERMSIG(status));
        } else {
            printf("Child %ld terminated with status %d\n", childrenPids[i], WEXITSTATUS(status));
            kill(SIGKILL, childrenPids[i]);
        }
    }
}

void handle_signal(int signal) {
    printf("Received signal %d\n", signal);
    if(signal == 13){
        printf("Something was wrong with the channel\n");
        exit(EXIT_FAILURE);
    }
    if(signal == SIGINT){
        printf("Ending the child processes\n");
        
        /*Code to clean and close everything*/
        flagCreatChildren = false; //prevent the creation of more children proccesses
        
        //terminate all existenting children
        //terminateChildren();
        
        //read availa
        pipeToFile(pipefd, 0, 1);
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char *argv[], char** envp){
    
    int i, sockfd, batch_size, n_batches, j, bytes_received, total_bytes_received;
    long n_requests;
    char buffer[BUFSIZE], request[BUFSIZE], response_code[4];
    static struct sockaddr_in serv_addr;
    struct timeval tv1, tv2, tv;
    float time_delta;
    
    //Pipe variables
    /*int pipefd[2];
     int *pipefdP = pipefd[0];*/
    
    //child variables
    pid_t pid;
    
    //File variables
    char* fileName = "sharedTextFile.txt";
    
    signal(SIGPIPE, handle_signal);
    signal(SIGINT, handle_signal);
    
    
    if (argc != 4 && argc != 5) {
        printf("Usage: ./client <SERVER IP ADDRESS> <LISTENING PORT> <N REQUESTS> <BATCH SIZE>\n");
        printf("Example: ./client 127.0.0.1 8141 10 2\n");
        exit(1);
    }
    
    //Get the number of requests
    char *requests;
    n_requests = strtol(argv[3], &requests, 10); //10 stands for the decimal system
    
    if (*requests != '\0') {
        pexit("Invalid number of requests");
    }
    
    //Alloc memory to the childrenPids
    childrenPids = (long*)malloc(n_requests * sizeof(long));
    
    /*n_requests = atoi(argv[3]);*/
    /*batch_size = atoi(argv[4]);
     */
    
    if(n_requests > REQUEST_MAX){
        pexit("Max number of requests exceeded");
    }
    
    char *batch;
    n_batches = strtol(argv[4], &batch, 10); //10 stands for the decimal system
    
    if (*batch != '\0') {
        pexit("Invalid number of batches");
    }
    
    batch_size = (n_requests + n_batches - 1) / n_batches;  // Round up division
    
    if(n_batches > n_requests){
        pexit("Batches should be < than requests");
    }
    
    //open File - we open the file here onluy if is the child to write to it
    /*int fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
    if(fd == -1){
        pexit("file opening/creation error");
    }*/
    
    //The pipe should be opened by the parent process; otherwise, the children wouldn't have this channel in this descriptor
    if (pipe(pipefd) == -1) {
        pexit("pipe creation");
        exit(EXIT_FAILURE);
    }
    
    //Start to count the total time of the code
    TIMER_START();
    for (i = 0; i < n_batches; i++) {
        for (j = 0; j < batch_size && (i * batch_size + j) < n_requests && flagCreatChildren; j++) {
            pid = fork();
            if (pid == -1) {
                pexit("fork");
            } else if(pid == 0){
                //close the pipe channel that won't be used (child only writes to the pipe)
                close(pipefd[0]);
                
                //start count the time
                TIMER_START();
                
                //socket
                if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                    pexit("socket() failed");
                }
                memset(&serv_addr, 0, sizeof(serv_addr));
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
                serv_addr.sin_port = htons(atoi(argv[2]));
                if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
                    pexit("connect() failed");
                }
                
                printf("client connected to IP = %s PORT = %s\n", argv[1], argv[2]);
                //Get url
                char ip_addr[IP_MAX_LENGTH];
                strcpy(ip_addr, argv[1]);
                int port = atoi(argv[2]);
                char url[URL_MAX_LENGTH];
                
                sprintf(url, "http://%s:%d", ip_addr, port);/**/
                
                //Send HTTP request
                snprintf(request, BUFSIZE, "GET / HTTP/1.1\r\nHost: %s\r\n\r\n", url);
                if (send(sockfd, request, strlen(request), 0) < 0) {
                    pexit("Failed to send HTTP request");
                    return 1;
                }
                //Get HTTP answer
                total_bytes_received = 0;
                bytes_received = 0;
                do {
                    bytes_received = recv(sockfd, buffer + total_bytes_received, BUFSIZE - total_bytes_received, 0);
                    if (bytes_received < 0){
                        pexit("Failed to receive HTTP response");
                        return 1;
                    }
                    if(bytes_received == 0){
                        break;
                    }
                    //add if to handle if we have buffer overflow
                    total_bytes_received += bytes_received;
                } while (bytes_received > 0);
                
                // Extract the HTTP response code
                char* http_start = strstr(buffer, "HTTP/1.1");
                if (http_start == NULL) {
                    printf("Invalid HTTP response\n");
                    return 1;
                }
                char* code_start = http_start + 9; //Skip "HTTP/1.1 "
                char* code_end = strchr(code_start, ' '); // Response code ends with a space
                
                if (code_end == NULL) {
                    printf("Invalid HTTP response\n");
                    return 1;
                }
                
                size_t code_len = code_end - code_start;
                memcpy(response_code, code_start, code_len);
                response_code[code_len] = '\0';
                
                printf("HTTP response code: %s\n", response_code);
                
                //Stop counting the time and prepare to write in a shared file and in a pipe or file
                TIMER_STOP();
                
                char toFile[MAX_LINE_SIZE];
                int pid_ = getpid();
                sprintf(toFile, "%d;%d;%d;%s;%.2f\n", pid_, j, i, response_code, time_delta);
                
                //child writing to the file
                /*if(write(fd, toFile, strlen(toFile)) < 0){
                 pexit("writing to shared file error (child %d)", getpid());
                 }*/
                if(write(STDOUT_FILENO, toFile, strlen(toFile)) < 0){
                    pexit("writing to STDOUT_FILENO error");
                }
                close(sockfd);
                
                if(write(pipefd[1], toFile, strlen(toFile)) < 0){
                    pexit("pipe writing");
                }
                close(pipefd[1]);
                exit(EXIT_SUCCESS);
            }
            else{
                //parent code
                //should wait each child from each batch finishes
                //childrenPids[i] = pid;
                for(int i = 0; i < batch_size; i++){
                    //Add pids in a int array to end all children if necessary
                    waitpid(pid, NULL, 0);
                }
            }
        }
    }
    for(int i = 0; i < n_requests / batch_size; i++){ //i < requests/batch
        waitpid(pid, NULL, 0);
    }
    //wait children to end
    pid_t wpid;
    int status;
    while ((wpid = wait(&status)) > 0) {
        if (WIFEXITED(status)) {
            printf("Child exited with status %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child terminated by signal %d\n", WTERMSIG(status));
        }
    }/**/
    //terminateChildren();
    //now the father will read from the pipe
    
    //close channel that won't be used by parent process (writing channel)
    
    
    /*char buf[BUFSIZE];
     ssize_t count = read(pipefd[0], buf, sizeof(buf));
     
     if (count == -1) {
     pexit("read");
     exit(EXIT_FAILURE);
     }
     
     printf("Parent read message: %.*s\n", (int)count, buf);
     
     close(pipefd[0]);*/
    
    
    pipeToFile(pipefd, fileName, 1);
    close(sockfd);
    //Linked list problem:
    struct Node* head = NULL;
    readFromFile(fileName, &head);
    

    //printLinkedListElements(&head);
    
    //Parent writing to the file
    /*if(write(fd, buf, strlen(buf)) < 0){
     pexit("writing to shared file error (parent)");
     }*/
    
    
    
    TIMER_STOP();
    
    fprintf(stderr, "%f secs\n", time_delta);
    
    //close(fd);
}



