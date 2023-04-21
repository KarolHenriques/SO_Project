//
//  tws.c
//
//
//  Adapted by Pedro Sobral on 11/02/13.
//  Credits to Nigel Griffiths
//
//  Adapted by Karol Henriques on 17/04/23.

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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h> // for semaphore functions
#include <sys/wait.h>  // for waitpid() function
#include <stdbool.h>


#define BUFSIZE         8096
#define ERROR           42
#define LOG             44
#define FORBIDDEN       403
#define NOTFOUND        404
#define VERSION         1
#define MAX_POOL_SIZE   10
#define SHM_KEY         1234
#define SHM_SIZE        10
#define INFO            2

/* Shared memory to pass the request to a new child if the one who's handling the request ends abruptly*/
int shmid;
void *shmaddr;

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
    int j, file_fd, buflen;
    long i, ret, len;
    char * fstr;
    static char buffer[BUFSIZE+1]; /* static so zero filled */
    
    ret =read(fd,buffer,BUFSIZE);     /* read Web request in one go */
    
    if(ret == 0 || ret == -1) {    /* read failure stop now */
        logger(FORBIDDEN,"failed to read browser request","",fd);
        close(fd);
        return 1;
    }
    if(ret > 0 && ret < BUFSIZE)    /* return code is valid chars */
        buffer[ret]=0;        /* terminate the buffer */
    else buffer[0]=0;
    for(i=0;i<ret;i++)    /* remove CF and LF characters */
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
    for(j=0;j<i-1;j++)     /* check for illegal parent directory use .. */
        if(buffer[j] == '.' && buffer[j+1] == '.') {
            logger(FORBIDDEN,"Parent directory (..) path names not supported",buffer,fd);
            close(fd);
            return 1;
        }
    if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) /* convert no filename to index file */
        (void)strcpy(buffer,"GET /index.html");
    
    /* work out the file type and check we support it */
    buflen=strlen(buffer);
    fstr = (char *)0;
    for(i=0;extensions[i].ext != 0;i++) {
        len = strlen(extensions[i].ext);
        if(!strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
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
        logger(NOTFOUND, "failed to open file", &buffer[5], fd);
        close(fd);
        return 1;
    }
    logger(LOG,"SEND",&buffer[5],hit);
    len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
    (void)lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
    (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: tws/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); /* Header + a blank line */
    logger(LOG,"Header",buffer,hit);
    (void)write(fd,buffer,strlen(buffer));
    
    /* send file in 8KB block - last block may be smaller */
    while ((ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
        (void)write(fd,buffer,ret);
    }
    sleep(1);    /* allow socket to drain before signalling the socket is closed */
    close(fd);
    return 0;
}

/* just checks command line arguments, setup a listening socket and block on accept waiting for clients */

int main(int argc, char **argv, char** envp){
    int i, port, pid, listenfd, socketfd, hit;
    socklen_t length;
    static struct sockaddr_in cli_addr; /* static = initialised to zeros */
    static struct sockaddr_in serv_addr; /* static = initialised to zeros */
    
    //Shared memory variables
    /*shmid = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0666);
     if (shmid == -1) {
     perror("shmget");
     exit(1);
     }
     
     // Attach shared memory segment to process
     shmaddr = shmat(shmid, NULL, 0);
     if (shmaddr == (void*)-1) {
     perror("Failed to attach shared memory segment");
     exit(1);
     }
     
     // Set up signal handler for SIGCHLD
     if (signal(SIGCHLD, handle_sigchld) == SIG_ERR) {
     perror("Failed to set up signal handler for SIGCHLD");
     exit(1);
     }*/
    
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
    /********************************If we are not using a pool**I*****************************************************************/
    /*if(listen(listenfd,64) <0)
     logger(ERROR,"system call","listen",0);*/
    
    /*for(hit=1; ;hit++) {
     length = sizeof(cli_addr);
     //Block waiting for clients
     socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length);
     if (socketfd<0)
     logger(ERROR,"system call","accept",0);
     else
     web(socketfd,hit);
     }
     (void)close(listenfd);*/
    
    /********************************Child to handle each request*********************************/
    /*while(1){
     socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length);
     if(socketfd < 0){
     logger(ERROR,"system call","accept",0);
     continue;
     }
     // fork new child process to handle client request
     pid = fork();
     if (pid < 0) {
     logger(ERROR,"fork failed","fork",0);
     close(socketfd);
     continue;
     }
     if(pid == 0){//child process to handle the client's request
     web(socketfd,hit);
     exit(EXIT_SUCCESS);
     }
     if(pid > 0){ //PARENT PROCESS ADDED WITHOUT TEST
     wait(NULL); // wait for child process to finish before creating another one
     }
     close(socketfd);
     }*/
    /********************************Pool of process***************************************/
    int pidArray[MAX_POOL_SIZE];
    int num_children = 0;
    pid_t pid_Pool;
    int shm_id, *shm_ptr;
    
    // Create shared memory for process availability flags
    shm_id = shmget(SHM_KEY, SHM_SIZE * sizeof(int), IPC_CREAT | 0666);
    
    if (shm_id < 0) {
        logger(ERROR, "shmget failed", "shmget", 0);
        exit(1);
    }
    shm_ptr = (int*)shmat(shm_id, NULL, 0);
    if (shm_ptr == (int*)-1) {
        logger(ERROR, "shmat failed", "shmat", 0);
        exit(1);
    }
    
    sem_t *sem;
    sem = sem_open("semaphore", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        logger(ERROR, "sem_open failed", "sem_open", 0);
        exit(1);
    }
    
    for (int i = 0; i < MAX_POOL_SIZE; i++) {
        shm_ptr[i] = 1; // Initialize all flags to 1 (available)
    }
    // Create child processes
    for (int i = 0; i < MAX_POOL_SIZE; i++) {
        pid_Pool = fork();
        if (pid_Pool < 0) {
            logger(ERROR,"fork failed","fork",0);
        }
        else if(pid_Pool == 0){
            // child process
            //break;
            bool request_available = false;
            while(1){
                //Wait for a request to be assigned by the parent process
                printf("I'm here!");
                sem_wait(sem);
                
                if (getppid() == 1) { //check if the parent exited
                    break;
                }
                
                // Read the socket file descriptor from shared memory
                int* socketfd_ptr = (int*)(shm_ptr + SHM_SIZE);
                int socketfd = socketfd_ptr[i];
                
                if (socketfd > 0) {
                    printf("My index value is: %d\n", i);
                    
                    //Handle the request
                    web(socketfd, hit);
                    
                    printf("Done, dad\n");
                    
                    shm_ptr[i] = 1; //Set availability flag to 1 (available)
                    
                    close(socketfd);
                    //Notify the parent process that we're done handling the request
                    sem_post(sem);
                    request_available = false;
                }
                else if (!request_available) {
                    printf("No requests available, sleeping...\n");
                    request_available = true;
                    sleep(1); // Sleep for 1 second before checking again
                }
            }
        }
        else{
            pidArray[i] = pid_Pool;
            num_children++;
        }
    }
    
    //Parent process
    while(1){
        //parent process accepts the request and assign to a child
        if(listen(listenfd, 64) < 0){
            logger(ERROR,"system call","listen",0);
            continue;
        }
        /*int*/ socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length);
        if (socketfd < 0) {
            close(socketfd);
            logger(ERROR,"system call","accept",0);
            continue;
        }
        // Find next available child process
        int index = -1;
        for (int i = 0; i < SHM_SIZE; i++) {
            if (shm_ptr[i] == 1) {
                index = i;
                shm_ptr[i] = 0; // Set availability flag to 0 (busy)
                printf("Selecting child %d to handle the request \n", pidArray[index]);
                close(socketfd);
                
                //send the socket to the shared memory
                int* socketfd_ptr = (int*)(shm_ptr + SHM_SIZE);
                socketfd_ptr[i] = socketfd;
                
                sem_post(sem);
                break;
            }
        }
        if (index == -1) {
            logger(ERROR, "no available child processes", "main", 0);
            continue;
        }
        
        /*else {
         sleep(1); // Wait for 1 second before checking for requests again
         }*/
    }
    
    // Detach shared memory
    if (shmdt(shm_ptr) == -1) {
        logger(ERROR, "shmdt failed", "shmdt", 0);
        exit(1);
    }
    
    // Remove shared memory
    if (shmctl(shm_id, IPC_RMID, 0) == -1) {
        logger(ERROR, "shmctl(IPC_RMID) failed", "shmctl", 0);
        exit(1);
    }
    
    // Close listening socket
    close(listenfd);
    
    // Kill child processes
    for (int i = 0; i < num_children; i++) {
        kill(pidArray[i], SIGTERM);
    }
    
    // Wait for all child processes to exit
    while (num_children > 0) {
        pid_t pid = wait(NULL);
        for (int i = 0; i < num_children; i++) {
            if (pidArray[i] == pid) {
                logger(INFO, "Child process exited", "", pid);
                // Remove child process from array and shift remaining processes left
                for (int j = i; j < num_children - 1; j++) {
                    pidArray[j] = pidArray[j + 1];
                }
                num_children--;
                break;
            }
        }
    }
}

void sigchld_handler(int signum) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) { // loop until no child is waiting to be reaped
        if (WIFEXITED(status)) { // child process terminated normally
            int i;
            for (i = 0; i < MAX_POOL_SIZE; i++) { // check if the child was in the pool
                if (((int*)shmaddr)[i] == pid) {
                    ((int*)shmaddr)[i] = fork(); // replace the terminated child with a new child process
                    //Check fork
                    break;
                    }
                }
            }
        
        else{// child process did not terminate normally
            ;
        }
    }
}
    /* When a child process receives a SIGCHLD, it means that the child process has terminated, and the parent process is notified about it. The parent process can then choose to do something in response to the child process termination, such as creating a new child process to handle pending requests.
     
     However, it's important to note that once a child process is terminated, all the resources associated with that process, including any pending requests or tasks, are lost. So if the child process was handling a request when it received a SIGCHLD and terminated, the parent process would not be able to create a new child process to continue handling that same request.*/





