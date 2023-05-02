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
} ARGS;

void* handle_request(void* arg){
    
    ARGS* info = (ARGS*)arg;
    printf("client connected to IP = %s PORT = %s\n", info->IP, info->Port);
    
    char buffer[BUFSIZE], request[BUFSIZE], response_code[RESPONSE_SIZE];
    int bytes_received, total_bytes_received, sockfd;
    
    //Get url
    char ip_addr[IP_MAX_LENGTH];
    strcpy(ip_addr, info->IP);
    int port = atoi(info->Port);
    char url[URL_MAX_LENGTH];
    
    sprintf(url, "http://%s:%d", ip_addr, port);/**/
    
    //Send HTTP request
    snprintf(request, BUFSIZE, "GET / HTTP/1.1\r\nHost: %s\r\n\r\n", url);
    if (send(info->socket, request, strlen(request), 0) < 0) {
        pexit("Failed to send HTTP request");
    }
    
    //Get HTTP answer
    total_bytes_received = 0;
    bytes_received = 0;
    do {
        //detect buffer overflow
        if (total_bytes_received == BUFSIZE) {
            pexit("Buffer overflow detected");
        }
        bytes_received = recv(info->socket, buffer + total_bytes_received, BUFSIZE - total_bytes_received, 0);
        if (bytes_received < 0){
            printf("Bytes received: %d\n", bytes_received);
            pexit("Failed to receive HTTP response");
        }
        if(bytes_received == 0){
            break;
        }
        total_bytes_received += bytes_received;
    } while (bytes_received > 0);
    
    // Extract the HTTP response code
    char* http_start = strstr(buffer, "HTTP/1.1");
    if (http_start == NULL) {
        pexit("Invalid HTTP response");
    }
    char* code_start = http_start + 9; //Skip "HTTP/1.1 "
    char* code_end = strchr(code_start, ' '); // Response code ends with a space
    
    if (code_end == NULL) {
        pexit("Invalid HTTP response");
    }
    size_t code_len = code_end - code_start;
    memcpy(response_code, code_start, code_len);
    response_code[code_len] = '\0';
    
    printf("HTTP response code: %s\n", response_code);
}


int main(int argc, char *argv[]){
    int i,sockfd, batch_size, n_batches, n_port, j;
    long n_requests;
    char buffer[BUFSIZE];
    static struct sockaddr_in serv_addr;
    struct timeval tv1, tv2, tv;
    float time_delta;
    
    
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
        sprintf(buffer,"GET /index.html HTTP/1.0 \r\n\r\n");
        /* Note: spaces are delimiters and VERY important */
    }
    else{
        printf("client trying to connect to IP = %s PORT = %s retrieving FILE= %s\n",argv[1],argv[2], argv[3]);
        sprintf(buffer,"GET /%s HTTP/1.0 \r\n\r\n", argv[3]);
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
    
    
    TIMER_START();
    
    if((sockfd = socket(AF_INET, SOCK_STREAM,0)) <0)
        pexit("socket() failed");
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));
    
    /* Connect tot he socket offered by the web server */
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <0)
        pexit("connect() failed");
    
    
    
    //One thread to handle each request
    pthread_t thread_ids[n_requests];
    for (int i = 0; i < batch_size; i++) {
        for (int j = 0; j < n_batches && (i * n_batches + j) < n_requests; j++) {
            //Send the args info to the thread
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
            if(pthread_create(&thread_ids[j], NULL, handle_request, (void*)args) != 0){
                pexit("Thread creation error");
            }
        }
    }
    
    //Main thread will wait every thread to exit
    for(int i = 0; i < n_batches; i++){
        if(pthread_join(thread_ids[i], NULL) != 0){
            pexit("Join error");
        }
    }
    
    /* Now the sockfd can be used to communicate to the server the GET request */
    printf("Send bytes=%d %s\n",(int) strlen(buffer), buffer);
    write(sockfd, buffer, strlen(buffer));
    
    /* This displays the raw HTML file (if index.html) as received by the browser */
    /*while( (i=read(sockfd,buffer,BUFSIZE)) > 0)
     write(1,buffer,i);*/
    
    TIMER_STOP();
    
    fprintf(stderr, "%f secs\n", time_delta);
    return 0;
}
