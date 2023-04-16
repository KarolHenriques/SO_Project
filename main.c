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
#define RESPONSE_SIZE 4

//Global variables

int pipefd[2];
volatile bool flagCreatChildren = true;
long* childrenPids;
//File variables
char* fileName = "sharedTextFile.txt";

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

struct DataAnalysis {
    double avrgTime;
    double minTime;
    double maxTime;
    float totalTime;
};

void insertRecord(struct Node** head, struct Record record) {
    struct Node* new_node = (struct Node*)malloc(sizeof(struct Node));
    
    if (new_node == NULL) {
        pexit("Unable to allocate memory");
    }
    
    new_node->record = record;
    new_node->next = NULL;
    
    if (*head == NULL) {
        *head = new_node;
    } else {
        struct Node* tail = *head;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = new_node;
    }
    printf("New node created with PID %d, next pointer is %p.\n", new_node->record.pid, new_node->next);
    printf("Head pointer is %p.\n", *head);
}

void printLinkedList(struct Node* head) {
    
    struct Node* current = head;
    
    while (current != NULL) {
        printf("pid: %d, bsn: %d, rsn: %d, rc: %d, t: %.2f\n", current->record.pid, current->record.bsn, current->record.rsn, current->record.rc, current->record.t);
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

struct Record saveToStruct(char buf[]){
    struct Record record;
    
    char *field = strtok(buf, ";");
    record.pid = atoi(field);
    field = strtok(NULL, ";");
    record.bsn = atoi(field);
    field = strtok(NULL, ";");
    record.rsn = atoi(field);
    field = strtok(NULL, ";");
    record.rc = atoi(field);
    field = strtok(NULL, ";");
    record.t = atof(field);
    
    printf("New stuct made\n");
    return record;
    
}

void readFromFile(char* fileName, struct Node** head) {
    int fd = open(fileName, O_RDONLY);
    if (fd == -1) {
        pexit("Unable to open file");
    }
    
    struct Record record;
    char buf[MAX_LINE_SIZE + 1];  // add space for null terminator
    int n = 0;
    size_t len = 0;
    char* token;
    int count = 0;
    char c;
    
    while ((n = read(fd, &c, 1)) > 0) {
        if (c == '\n') {
            buf[len] = '\0';
            record = saveToStruct(buf);
            insertRecord(head, record);
            len = 0;
            count++;
        } else {
            buf[len++] = c;
        }
    }
    
    /*if (len > 0) {
        buf[len] = '\0';
        record = saveToStruct(buf);
        insertRecord(head, record);
        count++;
    }*/
    
    printf("Count (number of lines read from file): %d\n", count);
    close(fd);
}

void terminateChildren(){
    int status;
    for(long i = 0; i < sizeof(childrenPids); i++){
        kill(SIGTERM, childrenPids[i]);
        waitpid(childrenPids[i], &status, 0);
        if (WIFSIGNALED(status)) {
            //printf("Child %ld terminated by signal %d\n", childrenPids[i], WTERMSIG(status));
        } else {
            //printf("Child %ld terminated with status %d\n", childrenPids[i], WEXITSTATUS(status));
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
        terminateChildren();
        
        //read available data
        pipeToFile(pipefd, fileName, true);
        
        exit(EXIT_SUCCESS);
    }
}

int countRecords(struct Node* head) {
    int count = 0;
    struct Node* current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

struct DataAnalysis* calculateStats(struct Node* head, float totalTime) {
    if (head == NULL) {
        return NULL;
    }

    double sum = 0.0;
    double max_time = head->record.t;
    double min_time = head->record.t;

    struct Node* current = head;
    while (current != NULL) {
        double t = current->record.t;
        if (t > max_time) {
            max_time = t;
        }
        if (t < min_time) {
            min_time = t;
        }
        sum += t;
        current = current->next;
    }

    double avg_time = sum / countRecords(head);
    
    struct DataAnalysis* dataRecord = (struct DataAnalysis*)malloc(sizeof(struct DataAnalysis));;
    dataRecord->avrgTime = avg_time;
    dataRecord->minTime = min_time;
    dataRecord->maxTime = max_time;
    dataRecord->totalTime = totalTime;

    return dataRecord;
}


int main(int argc, char *argv[], char** envp){
    
    int i, sockfd, batch_size, n_batches, j, bytes_received, total_bytes_received;
    long n_requests;
    char buffer[BUFSIZE], request[BUFSIZE], response_code[RESPONSE_SIZE];
    static struct sockaddr_in serv_addr;
    struct timeval tv1, tv2, tv;
    float time_delta;
    
    //child variables
    pid_t pid;
    
    signal(SIGPIPE, handle_signal);
    signal(SIGINT, handle_signal);
    
    if (argc != 4 && argc != 5) {
        printf("Usage: ./client <SERVER IP ADDRESS> <LISTENING PORT> <N REQUESTS> <BATCH SIZE>\n");
        printf("Example: ./client 127.0.0.1 8141 10 2\n");
        pexit("Wrong request input");
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
    for (i = 0; i < n_batches && flagCreatChildren; i++) {
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
                //parent code - should wait each child from each batch finishes
                
                //Add pids in a int array to end all children if necessary
                childrenPids[i] = pid;
                for(int i = 0; i < batch_size; i++){
                    waitpid(pid, NULL, 0);
                }
            }
        }
        //close(fd); //if is the children that writes to the file
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
    
    //time for all processes
    TIMER_STOP();
    
    pipeToFile(pipefd, fileName, true);
    close(sockfd);
    //Linked list problem:
    struct Node* head = NULL;
    readFromFile(fileName, &head);
    
    //printLinkedListElements(&head);
    printLinkedList(head);
    
    fprintf(stderr, "%f secs\n", time_delta);
    
    /*struct DataAnalysis* dataReport = calculateStats(head, time_delta);
    
    printf("Total time: %f, Average time: %f, Min time: %f, Max time: %f\n", dataReport->totalTime, dataReport->avrgTime, dataReport->minTime, dataReport->maxTime);*/

    exit(EXIT_SUCCESS);
}



