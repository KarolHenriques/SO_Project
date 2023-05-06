//
//  client.c
//
//
//  Adapted by Pedro Sobral on 11/02/13.
//  Credits to Nigel Griffiths
//
// Adapted by Karol Henriques on 05-02-23

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

struct timeval tv1, tv2, tv;
float time_delta;

#define MAX_PORT_NUMBER 65535
#define REQUEST_MAX 10000
#define MAX_LINE_SIZE 1024
#define URL_MAX_LENGTH 40
#define IP_MAX_LENGTH 16
#define BUFSIZE 8096
#define RESPONSE_SIZE 4
#define TIMER_START() gettimeofday(&tv1, NULL)
#define TIMER_STOP() \
gettimeofday(&tv2, NULL);    \
timersub(&tv2, &tv1, &tv);   \
time_delta = (float)tv.tv_sec + tv.tv_usec / 1000000.0

int pexit(char * msg){
    perror(msg);
    exit(1);
}

typedef struct{
    char* IP;
    char* Port;
    int i;
    int j;
    int socket;
   int file_fd;
} ARGS;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


void* handle_request(void* arg){
    
    ARGS* info = (ARGS*)arg;
    
    printf("client connected to IP = %s PORT = %s Handled by thread = %d\n", info->IP, info->Port, info->i);
    
    //Check if port and IP are null terminators:
    
    char request[BUFSIZE], buffer[BUFSIZE], response_code[RESPONSE_SIZE];
    int bytes_received, total_bytes_received;
    
    //printf("Send bytes=%d %s\n",(int) strlen(buffer), buffer);
    //write(info->socket, buffer, strlen(buffer));
    
    //Get url
    char ip_addr[IP_MAX_LENGTH];
    strcpy(ip_addr, info->IP);
    int port = atoi(info->Port);
    char url[URL_MAX_LENGTH];
    
    sprintf(url, "http://%s:%d", ip_addr, port);
    
    total_bytes_received = 0;
    bytes_received = 0;
    
    //Send HTTP request
    
    snprintf(request, BUFSIZE, "GET / HTTP/1.1\r\nHost: %s\r\n\r\n", url);
    
    if (send(info->socket, request, strlen(request), 0) < 0) {
        pexit("Failed to send HTTP request");
    }
    
    do {
        //detect buffer overflow
        
        if (total_bytes_received == BUFSIZE) {
            pexit("Buffer overflow detected");
        }
        
        bytes_received = recv(info->socket, buffer + total_bytes_received, BUFSIZE - total_bytes_received, 0);
        
        
        if (bytes_received < 0){
            pexit("Failed to receive HTTP response");
        }
        if(bytes_received == 0){
            break;
        }
        
        total_bytes_received += bytes_received;
        
    } while (bytes_received > 0);
    
    /* f you declare a variable inside a function, it is a local variable that is only visible within that function. When a thread is created, it is given its own stack, which includes space for all the local variables it needs. This means that each thread has its own copy of the local variables declared inside a function, and changes made to those variables are not visible to other threads.*/
    
    //printf("Buffer:\n%s\n", buffer);
    
    // Extract the HTTP response code
    char* http_start = strstr(buffer, "HTTP/1.1");
    if (http_start == NULL) {
        pexit("Invalid HTTP response. Start");
    } else if (http_start + 9 >= buffer + total_bytes_received) {
        pexit("Invalid HTTP response. Start string exceeds buffer length");
    }
    
    char* code_start = http_start + 9; //Skip "HTTP/1.1 "
    char* code_end = strchr(code_start, ' '); // Response code ends with a space
    
    if (code_end == NULL) {
        pexit("Invalid HTTP response. End");
    }
    size_t code_len = code_end - code_start;
    memcpy(response_code, code_start, code_len);
    response_code[code_len] = '\0';
    printf("HTTP response code: %s\n", response_code);
    
    TIMER_STOP();
    //Open the file once, in the main thread, and send the descriptor to each thread
    
    //Use mutex to lock the file while we are writing to it
    char toFile[MAX_LINE_SIZE];
    sprintf(toFile, "%lu;%d;%d;%s;%f\n", pthread_self(), info->j, info->i, response_code, time_delta/**/);
    printf("%s\n", toFile);
    
    pthread_mutex_lock(&mutex);
     if(write(info->file_fd, toFile, strlen(toFile)) < 0){ //add writeN function
     pexit("writing to shared file error (parent)");
     }
    pthread_mutex_unlock(&mutex);
    
    close(info->socket);
    free(info);
    pthread_exit(NULL);
}


int main(int argc, char *argv[], char** envp){
    int i,sockfd, batch_size, n_batches, n_port, j;
    long n_requests;
    char buffer[BUFSIZE];
    static struct sockaddr_in serv_addr;
    
    //File
    char* fileName = "sharedTextFile_threads.txt";
    
    if (argc!=4 && argc !=5) {
        printf("Usage: ./client <SERVER IP ADDRESS> <LISTENING PORT>\n");
        printf("Example: ./client 127.0.0.1 8141\n");
        exit(1);
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
    
    if (argc==3){
        printf("client trying to connect to IP = %s PORT = %s\n",argv[1],argv[2]);
        sprintf(buffer,"GET /index.html HTTP/1.1 \r\n\r\n");
        /* Note: spaces are delimiters and VERY important */
    }
    else{
        printf("client trying to connect to IP = %s PORT = %s retrieving FILE= %s\n",argv[1],argv[2], argv[3]);
        sprintf(buffer,"GET /%s HTTP/1.1 \r\n\r\n", argv[3]);
        /* Note: spaces are delimiters and VERY important */
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

    if((sockfd = socket(AF_INET, SOCK_STREAM,0)) <0)
        pexit("socket() failed");
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));
    
    /* Connect to the socket offered by the web server */
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <0)
        pexit("connect() failed");
    
    /* Now the sockfd can be used to communicate to the server the GET request */
    /*printf("Send bytes=%d %s\n",(int) strlen(buffer), buffer);
     write(sockfd, buffer, strlen(buffer));*/
    
    /* This displays the raw HTML file (if index.html) as received by the browser */
    
    /*while((i = read(sockfd,buffer,BUFSIZE)) > 0)
     write(1,buffer,i); */
    
    //Open shared file:
    int fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        pexit("Unable to open file");
    }
    
    //One thread to handle each reques
    pthread_t thread_ids[batch_size];
    TIMER_START();
    for (int i = 0; i < batch_size; i++) {
        for (int j = 0; j < n_batches && (i * n_batches + j) < n_requests; j++) {
            //Send the args info to the thread
            //ending time here to send the star time for each thread
            ARGS *args = malloc(sizeof(ARGS));
            //Check malloc
            if(args == NULL){
                pexit("Malloc error");
            }
            args->IP = argv[1];
            args->Port = argv[2];
            args->i = i;
            args->j = j;
            args->socket = sockfd;
            args->file_fd = fd;
            if(pthread_create(&thread_ids[j], NULL, handle_request, (void*)args) != 0){
                pexit("Thread creation error");
            }
            TIMER_START();
        }
    }
    
    //Main thread will wait every thread to exit
    for(int i = 0; i < batch_size; i++){
        if(pthread_join(thread_ids[i], NULL) != 0){
            pexit("Join error");
        }
    }/**/
    
    
    TIMER_STOP();
    printf("Main thread here\n");
    fprintf(stderr, "%f secs\n", time_delta);
    close(sockfd);
    return 0;
}
