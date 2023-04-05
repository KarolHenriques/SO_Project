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
#include <curl/curl.h>

#define SOCK_PATH "/tmp/socket"
#define MAX_LINE_SIZE 1024
#define BUFSIZE 4096
#define TIMER_START() gettimeofday(&tv1, NULL)
#define TIMER_STOP() \
gettimeofday(&tv2, NULL); \
timersub(&tv2, &tv1, &tv); \
time_delta = (float)tv.tv_sec + tv.tv_usec / 1000000.0

int pexit(char * msg){
    perror(msg);
    exit(1);
}

void handle_error(const char* message) {
    perror(message);
    exit(EXIT_FAILURE);
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
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char *argv[], char** envp){
    
    signal(SIGPIPE, handle_signal);
    signal(SIGINT, handle_signal);
    
    int i, sockfd, batch_size, n_batches, j, bytes_received, total_bytes_received;
    long n_requests;
    char buffer[BUFSIZE], request[BUFSIZE], response_code[4];
    static struct sockaddr_in serv_addr;
    struct timeval tv1, tv2, tv;
    float time_delta;
    
    //child variables
    pid_t pid;
    
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
    
    batch_size = atoi(argv[4]);
    n_batches = (n_requests + batch_size - 1) / batch_size;  // Round up division
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        pexit("socket() failed");
    }
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));
    
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        pexit("connect() failed");
    
    printf("client connected to IP = %s PORT = %s\n", argv[1], argv[2]);
    
    //Get url
    char ip_addr[16];
    strcpy(ip_addr, argv[1]);
    int port = atoi(argv[2]);
    char url[40];
    
    sprintf(url, "http://%s:%d", ip_addr, port);
    
    TIMER_START();
    
    //open File
    int fd = open("sharedTextFile.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
    if(fd < 0){
        pexit("file opening/creation error");
    }
    
    for (i = 0; i < n_batches; i++) {
        for (j = 0; j < batch_size && (i * batch_size + j) < n_requests/*n_batches*/; j++) {
            pid = fork();
            if (pid == -1) {
                handle_error("fork");
            } else if(pid == 0){
                //snprintf(request, BUFSIZE, "GET / HTTP/1.1\r\nHost: %s\r\n\r\n", argv[1]);
                snprintf(request, BUFSIZE, "GET / HTTP/1.1\r\nHost: %s\r\n\r\n", url);
                if (send(sockfd, request, strlen(request), 0) < 0) {
                    perror("Failed to send HTTP request");
                    return 1;
                }
                total_bytes_received = 0;
                do {
                    bytes_received = recv(sockfd, buffer + total_bytes_received, BUFSIZE - total_bytes_received, 0);
                    if (bytes_received < 0) {
                        perror("Failed to receive HTTP response");
                        return 1;
                    }
                    total_bytes_received += bytes_received;
                } while (bytes_received > 0);
                
                // Extract the HTTP response code
                /*memcpy(response_code, buffer + 9, 3); // HTTP response code is at position 9
                response_code[3] = '\0';
                printf("HTTP response code: %s\n", response_code);*/
                char* http_start = strstr(buffer, "HTTP/1.1");
                if (http_start == NULL) {
                    printf("Invalid HTTP response\n");
                    return 1;
                }
                char* code_start = http_start + 9; // Skip "HTTP/1.1 "
                char* code_end = strchr(code_start, ' '); // Response code ends with a space
                
                if (code_end == NULL) {
                    printf("Invalid HTTP response\n");
                    return 1;
                }
                
                size_t code_len = code_end - code_start;
                memcpy(response_code, code_start, code_len);
                response_code[code_len] = '\0';
                
                printf("HTTP response code: %s\n", response_code);
                
                char toFile[30];
                int pid_ = getpid();
                sprintf(toFile, "%d;%s\n", pid_, response_code);
                if(write(fd, toFile, strlen(toFile)) < 0){
                    pexit("writing to shared file error");
                }
                if(write(STDOUT_FILENO, toFile, strlen(toFile)) < 0){
                    pexit("writing to STDOUT_FILENO error");
                }
                close(fd);
                exit(EXIT_SUCCESS);
            }
            else{
                //parent code
                continue;
            }
        }
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
    }
    TIMER_STOP();
    fprintf(stderr, "%f secs\n", time_delta);
}

