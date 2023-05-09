//
//  tws.c
//
//
//  Adapted by Pedro Sobral on 11/02/13.
//  Credits to Nigel Griffiths
//
//  Adapted by Karol Henriques on 05-07-23

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <dispatch/dispatch.h>


#define BUFSIZE         8096
#define ERROR           42
#define LOG             44
#define FORBIDDEN       403
#define NOTFOUND        404
#define VERSION         1
#define WORKERTH        6
#define PRODUCTS        5
#define MAX_REQUESTS    10

/********Thread to handle each request  global variables **********/
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/***************************************************************************/

/****************Producer-consumer global variables ***************/
pthread_t worker_threads_ids[WORKERTH];
pthread_t buffer_printer_id;
int buf[PRODUCTS];
int producerTH = 0, consumerTH = 0;
pthread_mutex_t mutexP;
pthread_mutex_t mutexC;
int h = 1;
//Semaphore
dispatch_semaphore_t can_produce;
dispatch_semaphore_t can_consume;
/***************************************************************************/

/********************************FSM global variables *****************/
pthread_t FSML_threads_ids[WORKERTH];
pthread_mutex_t mutexFSM = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condFSM = PTHREAD_COND_INITIALIZER;
typedef enum{
    READY,
    RECEIVING,
    PROCESSING,
    SENDING,
    END
}state_t;

/*  sometimes it can be helpful to use a struct to represent the states, especially if you have complex states that require additional information to be stored. */
typedef struct {
    int descriptors[MAX_REQUESTS];
    int in;   // next free slot to insert a new request descriptor
    int out;  // next descriptor to process
    int count;  // number of requests in buffer
} request_buffer;
/***************************************************************************/
struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },
	{"jpg", "image/jpg" },
	{"jpeg","image/jpeg"},
	{"png", "image/png" },
	{"ico", "image/ico" },
	{"zip", "image/zip" },
	{"gz",  "image/gz"  },
	{"tar", "image/tar" },
	{"htm", "text/html" },
	{"html","text/html" },
	{0,0} };

/* Deals with error messages and logs everything to disk */

typedef struct threadInfo{
    int socketfd_;
    int hit_;
}INFO;

void* toConsume();
void* toPrint();
void* pool(void* args);

int pexit(char * msg){
    perror(msg);
    exit(1);
}

void logger(int type, char *s1, char *s2, int socket_fd){
	int fd ;
	char logbuffer[BUFSIZE*2];
    
	switch (type) {
	case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",s1, s2, errno,getpid()); 
		break;
	case FORBIDDEN: 
		(void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
		(void)sprintf(logbuffer,"FORBIDDEN: %s:%s",s1, s2); 
		break;
	case NOTFOUND: 
		(void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
		(void)sprintf(logbuffer,"NOT FOUND: %s:%s",s1, s2); 
		break;
	case LOG: (void)sprintf(logbuffer," INFO: %s:%s:%d",s1, s2,socket_fd); break;
	}	
	/* No checks here, nothing can be done with a failure anyway */
	if((fd = open("tws.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		(void)write(fd,logbuffer,strlen(logbuffer)); 
		(void)write(fd,"\n",1);      
		(void)close(fd);
	}

}

/* this is the web server function imlementing a tiny portion of the HTTP 1.1 specification */

int web(int fd, int hit){
    printf("Hi, web here\n");
    /* the web server function web is being called by multiple threads, each handling a different client connection, and accessing the same shared data, i.e., the file descriptor fd and the hit count hit.
     
     Since multiple threads may try to access and modify these shared variables simultaneously, it is necessary to protect them using a mutex to avoid race conditions and ensure data consistency.*/
    pthread_mutex_lock(&mutex);
    
	int j, file_fd, buflen;
	long i, ret, len;
	char * fstr;
	static char buffer[BUFSIZE+1]; /* static so zero filled */
    
	ret = read(fd,buffer,BUFSIZE); 	/* read Web request in one go */

	if(ret == 0 || ret == -1) {	/* read failure stop now */
		logger(FORBIDDEN,"failed to read browser request","",fd);
        close(fd);
        return 1;
	}
	if(ret > 0 && ret < BUFSIZE)	/* return code is valid chars */
		buffer[ret]=0;		/* terminate the buffer */
	else buffer[0]=0;
	for(i=0;i<ret;i++)	/* remove CF and LF characters */
		if(buffer[i] == '\r' || buffer[i] == '\n')
			buffer[i]='*';
	logger(LOG,"request",buffer,hit);
	if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) {
		logger(FORBIDDEN,"Only simple GET operation supported",buffer,fd);
        close(fd);
        return 1;
	}
	for(i=4;i<BUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */
		if(buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
			buffer[i] = 0;
			break;
		}
	}
	for(j=0;j<i-1;j++) 	/* check for illegal parent directory use .. */
		if(buffer[j] == '.' && buffer[j+1] == '.') {
			logger(FORBIDDEN,"Parent directory (..) path names not supported",buffer,fd);
            close(fd);
            return 1;
		}
	if(!strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) /* convert no filename to index file */
		(void)strcpy(buffer,"GET /index.html");

	/* work out the file type and check we support it */
	buflen = strlen(buffer);
	fstr = (char *)0;
	for(i=0;extensions[i].ext != 0;i++) {
		len = strlen(extensions[i].ext);
		if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
			fstr =extensions[i].filetype;
			break;
		}
	}
	if(fstr == 0){
        logger(FORBIDDEN,"file extension type not supported",buffer,fd);
        close(fd);
        return 1;
    }

	if((file_fd = open(&buffer[5],O_RDONLY)) == -1) {  /* open the file for reading */
		logger(NOTFOUND, "failed to open file",&buffer[5],fd);
        close(fd);
        return 1;
	}
	logger(LOG,"SEND",&buffer[5],hit);
	len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
	      (void)lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
          (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: tws/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); /* Header + a blank line */
	logger(LOG,"Header",buffer,hit);
    
    //pthread_mutex_lock(&mutex);

	(void)write(fd,buffer,strlen(buffer));

	/* send file in 8KB block - last block may be smaller */
	while ((ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
		(void)write(fd,buffer,ret);
	}
    
    sleep(1);	/* allow socket to drain before signalling the socket is closed */
    close(fd);
    pthread_mutex_unlock(&mutex);
	return 0;
}

/* Function to the threads*/
void* handle_request(void* arg){
    //printf("Thread here!\n");
    INFO* info = (INFO*)arg;
    int fd = info->socketfd_;
    int hit = info->hit_;
    free(arg);
    web(fd, hit);
    return NULL;
}

/* just checks command line arguments, setup a listening socket and block on accept waiting for clients */

int main(int argc, char **argv, char** envp){
    int i, port, pid, listenfd, socketfd, hit;
    socklen_t length;
    static struct sockaddr_in cli_addr; /* static = initialised to zeros */
    static struct sockaddr_in serv_addr; /* static = initialised to zeros */
    
    if( argc < 3  || argc > 3 || !strcmp(argv[1], "-?") ) {
        (void)printf("\n\nhint: ./tws Port-Number Top-Directory\t\tversion %d\n\n"
                     "\ttws is a small and very safe mini web server\n"
                     "\ttws only serves out file/web pages with extensions named below\n"
                     "\tand only from the named directory or its sub-directories.\n"
                     "\tThere are no fancy features = safe and secure.\n\n"
                     "\tExample: ./tws 8181 ./webdir \n\n"
                     "\tOnly Supports:", VERSION);
        for(i=0;extensions[i].ext != 0;i++)
            (void)printf(" %s",extensions[i].ext);
        
        (void)printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
                     "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n\n");
        exit(0);
    }
    if( !strncmp(argv[2],"/"   ,2 ) || !strncmp(argv[2],"/etc", 5 ) ||
       !strncmp(argv[2],"/bin",5 ) || !strncmp(argv[2],"/lib", 5 ) ||
       !strncmp(argv[2],"/tmp",5 ) || !strncmp(argv[2],"/usr", 5 ) ||
       !strncmp(argv[2],"/dev",5 ) || !strncmp(argv[2],"/sbin",6) ){
        (void)printf("ERROR: Bad top directory %s, see tws -?\n",argv[2]);
        exit(3);
    }
    if(chdir(argv[2]) == -1){
        (void)printf("ERROR: Can't Change to directory %s\n",argv[2]);
        exit(4);
    }
    
    logger(LOG,"tws starting",argv[1],getpid());
    
    /* setup the network socket */
    if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
        logger(ERROR, "system call","socket",0);
    port = atoi(argv[1]);
    if(port < 0 || port >60000)
        logger(ERROR,"Invalid port number (try 1->60000)",argv[1],0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
        logger(ERROR,"system call","bind",0);
    
    /************************************************SEQUENTIAL****************************************************************/
    /*if(listen(listenfd,64) <0)
     logger(ERROR,"system call","listen",0);
     
     for(hit=1; ;hit++) {
     length = sizeof(cli_addr);
     // block waiting for clients
     socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length);
     if (socketfd<0)
     logger(ERROR,"system call","accept",0);
     else
     web(socketfd,hit);
     
     }*/
    
    /************************************ One thread to handle each request***************************************************/
    
    /* if(listen(listenfd,64) <0){
     logger(ERROR,"system call","listen",0);
     }
     hit = 1;
     //Create a thread to handle each request
     while(1){
     length = sizeof(cli_addr);
     socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length);
     if(socketfd < 0){
     logger(ERROR,"system call","accept",0);
     }
     //Thread creation. Threads here won't be attached since we don't need to wait them to finish
     // Thread creation with detached state:
     pthread_attr_t attr;
     pthread_attr_init(&attr);
     pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
     pthread_t dummy_thread;
     //        NULL cannot be used as the first argument of pthread_create. The first argument of pthread_create is a pointer to a pthread_t variable that will be used to store the thread ID. If we don't need to store the thread ID, we can pass a pointer to a dummy variable instead.
     INFO *info = malloc(sizeof(INFO));
     info->socketfd_ = socketfd;
     
     pthread_mutex_lock(&mutex);
     info->hit_ = hit++;
     pthread_mutex_unlock(&mutex);
     
     if(pthread_create(&dummy_thread, &attr, handle_request, (void*)info) != 0){
     logger(ERROR, "Thread creation error", 0, 0);
     }
     }(void)close(listenfd);
     
     }*/
    /************************************************producer-consumer*******************************************************/
    /*can_produce = dispatch_semaphore_create(PRODUCTS);
     can_consume = dispatch_semaphore_create(0);
     pthread_mutex_init(&mutexP, NULL);
     pthread_mutex_init(&mutexC, NULL);
     //Consumers threads creation
     for(int i = 0; i < WORKERTH; i++){
     if(pthread_create(&worker_threads_ids[i], NULL, toConsume, 0) != 0){
     logger(ERROR,"system call", "thread", 0);
     }
     }
     
     //buffer printer thread
     if(pthread_create(&buffer_printer_id, NULL, toPrint, 0) != 0){
     logger(ERROR,"system call", "thread", 0);
     }
     
     //producer - one!
     if(listen(listenfd,64) <0){
     logger(ERROR,"system call","listen",0);
     }
     while(1){
     //"produce" item:
     length = sizeof(cli_addr);
     socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length);
     if(socketfd < 0){
     logger(ERROR,"system call","accept",0);
     }
     //wait on the can_produce semaphore
     else{
     dispatch_semaphore_wait(can_produce, DISPATCH_TIME_FOREVER);
     //to guarantee that producer and consumer will not try to access the same place, we are going to lock the index where the socket index is going to be placed
     pthread_mutex_lock(&mutexP);
     buf[producerTH] = socketfd;
     printf("Item produced\n");
     //for circular walking in the array:
     producerTH = (producerTH + 1) % PRODUCTS;
     pthread_mutex_unlock(&mutexP);
     //signal on the can_consume sempahore
     dispatch_semaphore_signal(can_consume);
     }
     }
     
     //Wait all threads in the pool to end:
     for (int i = 0; i < WORKERTH; i++) {
     pthread_join(worker_threads_ids[i], NULL);
     }
     
     //Destroy all semaphores and locks
     dispatch_release(can_produce);
     dispatch_release(can_consume);
     pthread_mutex_destroy(&mutexP);
     pthread_mutex_destroy(&mutexC);
     
     return 0;
     
     }*/
    /****************************************************FSM*******************************************************************/
    
    //Initialise buffer
    request_buffer buffer = { .in = 0, .out = 0, .count = 0 };
    
    //Worker threads creation
    for(int i = 0; i < WORKERTH; i++){
        if(pthread_create(&FSML_threads_ids[i], NULL, pool, &buffer) != 0){
            logger(ERROR,"system call", "thread", 0);
        }
    }
    
    if(listen(listenfd,64) <0){
        logger(ERROR,"system call","listen",0);
    }
    
    //The main thread will allocate the requests in a shared struct
    while(1){
        //printf("Main thread here\n");
        length = sizeof(cli_addr);
        socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length);
        if(socketfd < 0){
            logger(ERROR,"system call","accept",0);
        }
        pthread_mutex_lock(&mutexFSM);
        //If the buffer is full:
        while (buffer.count == MAX_REQUESTS) {
            pthread_cond_wait(&condFSM, &mutexFSM);
        }
        buffer.descriptors[buffer.in] = socketfd;
        buffer.in = (buffer.in + 1) % MAX_REQUESTS;
        buffer.count++;
        //printf("Number of request on buffer: %d\n", buffer.count);
        pthread_mutex_unlock(&mutexFSM);
        pthread_cond_signal(&condFSM);
    }
    
    //Cleanup
    for (int i = 0; i < WORKERTH; i++) {
        if(pthread_cancel(FSML_threads_ids[i]) != 0){
            pexit("Thread cancellation error");
        }
        if(pthread_join(FSML_threads_ids[i], NULL) != 0){
            pexit("Join error");
        }
    }
    close(socketfd);
    
    // Release resources
    if(pthread_mutex_destroy(&mutexFSM) != 0){
        pexit("Mutex destroy error");
    }
    if(pthread_cond_destroy(&condFSM) != 0){
        pexit("Cond destroy error");
    }
    
}//Final curly

void* toConsume(){
    while(1){
        //wait on can_consume semaphore
        dispatch_semaphore_wait(can_consume, DISPATCH_TIME_FOREVER);
        printf("Thread ready to consume!\n");
        //to guarantee that producer and consumer will not try to access the same place, we are going to lock the index where the socket index is going to be taken
        pthread_mutex_lock(&mutexC);
        int socketfd_ = buf[consumerTH];
        //for circular walking in the array:
        consumerTH = (consumerTH + 1) % PRODUCTS;
        pthread_mutex_unlock(&mutexC);
        //signal on the can_produce sempahore
        dispatch_semaphore_signal(can_produce);
        web(socketfd_, h++);
        close(socketfd_);
    }
}

void* toPrint(){
    while(1){
        printf("Buffer state\n");
        for(int i = 0; i < PRODUCTS; i++){
            printf("buf[%d] = %d\n", i, buf[i]);
        }
        sleep(1);
    }
}

void* pool(void* args){
    //unwrap the arguments sent by the main thread
    request_buffer* buffer = (request_buffer*)args;
    int descriptor;
    state_t current_state = READY;
    int hit = 1;
    //printf("WORKER THREAD HERE\n");
    while(1){
        // Wait for a request to arrive in the buffer
        pthread_mutex_lock(&mutexFSM);
        while (buffer->count == 0){
            pthread_cond_wait(&condFSM, &mutexFSM);
        }
        printf("A request arrived!\n");
        switch(current_state){
            case READY:
                printf("I'm ready!. ID: %lu\n", pthread_self());
                current_state = RECEIVING;
                break;
            case RECEIVING:
                descriptor = buffer->descriptors[buffer->out];
                printf("Receiving a request\n");
                current_state = PROCESSING;
                break;
            case PROCESSING:
                buffer->out = (buffer->out + 1) % MAX_REQUESTS;
                buffer->count--;
                web(descriptor, hit++);
                current_state = SENDING;
                break;
            case SENDING:
                printf("Request handled\n");
                current_state = END;
                break;
            case END:
                printf("Request ended\n");
                close(descriptor);
                current_state = READY;
               // break;
        }
        //pthread_mutex_unlock(&mutex);
    }

    return NULL;
}
