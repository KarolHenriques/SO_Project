//
//  client.c
//
//
//  Adapted by Pedro Sobral on 11/02/13.
//  Credits to Nigel Griffiths
//
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 8096
#define TIMER_START() gettimeofday(&tv1, NULL)
#define TIMER_STOP() \
gettimeofday(&tv2, NULL);    \
timersub(&tv2, &tv1, &tv);   \
time_delta = (float)tv.tv_sec + tv.tv_usec / 1000000.0


pexit(char * msg)
{
	perror(msg);
	exit(1);
}

main(int argc, char *argv[])
{
    int i,sockfd;
    char buffer[BUFSIZE];
    static struct sockaddr_in serv_addr;
    struct timeval tv1, tv2, tv;
    float time_delta;

    
    if (argc!=3 && argc !=4) {
        printf("Usage: ./client <SERVER IP ADDRESS> <LISTENING PORT>\n");
        printf("Example: ./client 127.0.0.1 8141\n");
        exit(1);
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
    
    TIMER_START();	
    
	if((sockfd = socket(AF_INET, SOCK_STREAM,0)) <0) 
		pexit("socket() failed");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));

	/* Connect tot he socket offered by the web server */
	if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <0) 
		pexit("connect() failed");
    

	/* Now the sockfd can be used to communicate to the server the GET request */
	printf("Send bytes=%d %s\n",(int) strlen(buffer), buffer);
	write(sockfd, buffer, strlen(buffer));

	/* This displays the raw HTML file (if index.html) as received by the browser */
	while( (i=read(sockfd,buffer,BUFSIZE)) > 0)
		write(1,buffer,i);
    
    TIMER_STOP();
    
    fprintf(stderr, "%f secs\n", time_delta);
}
