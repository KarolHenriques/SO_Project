//
//  SO_Project_1version.c
//
//  Adapted by Pedro Sobral on 02-22-13.
//  Credits to Nigel Griffiths
//  Adapted by Karol Henriques on 04-22-23.
//
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
#define REQUEST_MAX 10000
#define RESPONSE_SIZE 4
#define MAX_PORT_NUMBER 65535

//Global variables

int pipefd[2];
int sockfd;
long* childrenPids;
char* fileName = "sharedTextFile.txt";
struct timeval tv1, tv2, tv;
float time_delta;

int pexit(char* msg){
    perror(msg);
    exit(EXIT_FAILURE);
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

ssize_t readn(int fd, void *buf, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *ptr = buf;
    
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR){  /* Interrupted by signal */
                nread = 0;      /* and call read() again */
            }
            else{
                return -1;      /* Error */
            }
        }else if (nread == 0){/* EOF */
            break;
        }
        nleft -= nread;
        ptr += nread;
    }
    
    return n - nleft;           /* Return >= 0 */
}

ssize_t writen(int fd, const void *buf, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    const char *ptr = buf;
    
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (errno == EINTR){  /* Interrupted by signal */
                nwritten = 0;   /* and call write() again */
            }
            else{
                return -1;      /* Error */
            }
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;                   /* Return >= 0 */
}

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
        printf("pid: %d, bsn: %d, rsn: %d, rc: %d, t: %f\n", current->record.pid, current->record.bsn, current->record.rsn, current->record.rc, current->record.t);
        current = current->next;
    }
}

void pipeToFile(int pipefd[], char* fileName) {
    close(pipefd[1]);
    //int fd = open(fileName, O_RDONLY); this permition is to read the data if was the child that wrote that
    
    int fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        pexit("Unable to open file");
    }
    
    char buf[BUFSIZE];
    ssize_t count = readn(pipefd[0], buf, sizeof(buf));
    
    if (count == -1) {
        pexit("read from pipe");
    }
    
    printf("Parent read message from pipe:\n%.*s\n", (int)count, buf);
    
    
    if(writen(fd, buf, strlen(buf)) < 0){
        pexit("writing to shared file error (parent)");
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
    
    while ((n = readn(fd, &c, 1)) > 0) {
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
    
    //printf("Count (number of lines read from file): %d\n", count);
    close(fd);
}

void terminateChildren(){
    int status;
    for(long i = 0; i < sizeof(childrenPids); i++){
        if(childrenPids[i] != 0){
            kill(SIGTERM, childrenPids[i]);
            waitpid(childrenPids[i], &status, 0);
            if (WIFSIGNALED(status)) {
                /*printf("Child %ld terminated by signal %d\n", childrenPids[i], WTERMSIG(status));*/
                continue;
            } else {
                /*printf("Child %ld terminated with status %d\n", childrenPids[i], WEXITSTATUS(status));*/
                kill(SIGKILL, childrenPids[i]);
            }
        }
    }
    free(childrenPids);
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

void DataAnalysisReport (struct Node* head, float time_delta){
    struct DataAnalysis* dataReport = calculateStats(head, time_delta);
    printf("Total time: %f, Average time: %f, Min time: %f, Max time: %f\n", dataReport->totalTime, dataReport->avrgTime, dataReport->minTime, dataReport->maxTime);/**/
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
        
        //terminate all existenting children
        terminateChildren();
        close(sockfd);
    }
        //read available data
        pipeToFile(pipefd, fileName);
        
        //Save it to a struct to add
        struct Node* head = NULL;
        readFromFile(fileName, &head);
        
        //now the process ends here
        TIMER_STOP();
        
        DataAnalysisReport (head, time_delta);
        exit(EXIT_SUCCESS);
   
}

int main(int argc, char *argv[], char** envp){
    
    int i, /*sockfd,*/ batch_size, n_batches, n_port, j, bytes_received, total_bytes_received;
    long n_requests;
    char buffer[BUFSIZE], request[BUFSIZE], response_code[RESPONSE_SIZE];
    static struct sockaddr_in serv_addr;
    
    //child variables
    pid_t pid;
    
    signal(SIGPIPE, handle_signal);
    signal(SIGINT, handle_signal);
    
    if (argc != 4 && argc != 5) {
        printf("Usage: ./client <SERVER IP ADDRESS> <LISTENING PORT> <N REQUESTS> <BATCH SIZE>\n");
        printf("Example: ./client 127.0.0.1 8141 10 2\n");
        pexit("Wrong request input");
    }
    
    //Check port number valid
    char *port;
    n_port = strtol(argv[2], &port, 10); //10 stands for the decimal system
    
    if (*port != '\0') {
        pexit("Invalid port");
    }
    
    if(n_port > MAX_PORT_NUMBER || n_port < 0){
        pexit("Invalid port");
    }
    
    //Get the number of requests
    char *requests;
    n_requests = strtol(argv[3], &requests, 10); //10 stands for the decimal system
    
    if (*requests != '\0') {
        pexit("Invalid number of requests");
    }
    
    if(n_requests > REQUEST_MAX){
        pexit("Max number of requests exceeded");
    }
    
    char *batch;
    n_batches = strtol(argv[4], &batch, 10); //10 stands for the decimal system
    
    if (*batch != '\0') {
        pexit("Invalid number of batches");
    }
    
    if(n_requests % n_batches != 0){
        batch_size = (n_requests + n_batches - 1) / n_batches;  // Round up division
    }
    
    batch_size = n_requests / n_batches;
    if(n_batches > n_requests){
        pexit("Batches should be < than requests");
    }
    
    //Alloc memory to the childrenPids
    childrenPids = (long*)malloc(n_batches * sizeof(long));
    
    /********************************************If children are writing to the shared file**********************************************************************************/
    
    //open File - we open the file here onluy if is the child to write to it
    /*int fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
     
     if(fd == -1){
     pexit("file opening/creation error");
     }*/
    /****************************************************************************************************************************************************************************/
    
    //The pipe should be opened by the parent process; otherwise, the children wouldn't have this channel in this descriptor
    if (pipe(pipefd) == -1) {
        pexit("pipe creation");
        exit(EXIT_FAILURE);
    }
    
    //Start to count the total time of the code
    TIMER_START();
    for (i = 0; i < batch_size; i++) {
        for (j = 0; j < n_batches && (i * n_batches + j) < n_requests; j++) {
            childrenPids[j] = fork();
            if (childrenPids[j] == -1) {
                pexit("fork");
            } else if(childrenPids[j] == 0){
                //close the pipe channel that won't be used (child only writes to the pipe)
                close(pipefd[STDIN_FILENO]);
                
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
                    //detect buffer overflow
                    if (total_bytes_received == BUFSIZE) {
                        pexit("Buffer overflow detected");
                        exit(EXIT_FAILURE);
                    }
                    /* The recv function is a system call in C used for receiving data from a socket.
                     It returns the number of bytes received on succes, or -1 on failure.
                     recv and read are both system calls in C that are used for reading data from a file descriptor.
                     However, recv is specifically designed for use with network sockets,
                     while read is a more general-purpose function that can be used with any file descriptor, including sockets.*/
                    bytes_received = recv(sockfd, buffer + total_bytes_received, BUFSIZE - total_bytes_received, 0);
                    /************************bytes_received = recv(sockfd, where to store, possible size, flag****************************************/
                    if (bytes_received < 0){
                        pexit("Failed to receive HTTP response");
                        exit(EXIT_FAILURE);
                    }
                    if(bytes_received == 0){
                        break;
                    }
                    
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
                sprintf(toFile, "%d;%d;%d;%s;%f\n", pid_, j, i, response_code, time_delta);
                
                /********************************************If children are writing to the shared file**********************************************************************************/
                //child writing to the file
                /*if(write(fd, toFile, strlen(toFile)) < 0){
                 pexit("writing to shared file error (child %d)", getpid());
                 }*/
                /****************************************************************************************************************************************************************************/
                
                if(write(STDOUT_FILENO, toFile, strlen(toFile)) < 0){
                    pexit("writing to STDOUT_FILENO error");
                }
                close(sockfd);
                
                if(write(pipefd[STDOUT_FILENO], toFile, strlen(toFile)) < 0){
                    pexit("pipe writing");
                }
                close(pipefd[STDOUT_FILENO]);
                exit(EXIT_SUCCESS);
            }
            /*else{
                //parent code - should wait each child from each batch finishes
                
            }*/
        }
        printf("Parent code waiting \n");
        for(int i = 0; i < batch_size; i++){
            waitpid(childrenPids[i], NULL, 0);
        }memset(childrenPids, 0, n_batches * sizeof(long));
        
        /********************************************If children are writing to the shared file**********************************************************************************/
        //close(fd);
        /****************************************************************************************************************************************************************************/
        
    }
    //time for all processes
    TIMER_STOP();
    
    pipeToFile(pipefd, fileName);
    close(sockfd);
    
    //Linked list:
    struct Node* head = NULL;
    readFromFile(fileName, &head);
    
    printLinkedList(head);
    
    fprintf(stderr, "%f s\n", time_delta);
    
    DataAnalysisReport(head, time_delta);
    
    free(childrenPids);
    exit(EXIT_SUCCESS);
}



