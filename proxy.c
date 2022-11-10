#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#define BUFSIZE 8192
#define SMALLBUF 1024
#define connections 1024
pthread_mutex_t runningMutex;
pthread_mutex_t cacheMutex;
pthread_mutex_t hostresolveMutex;
pthread_mutex_t parseMutex;
int timer;
int MAINsockfd; /* socket */
bool keepRunning=true;
//https://stackoverflow.com/questions/3536153/c-dynamically-growing-array
typedef struct {
  pthread_t *array;
  size_t used;
  size_t size;
} Vector;

void initArray(Vector *a, size_t initialSize) {
  a->array = malloc(initialSize * sizeof(pthread_t));
  a->used = 0;
  a->size = initialSize;
}

void insertArray(Vector *a, pthread_t element) {
  if (a->used == a->size) {
    a->size *= 2;
    a->array = realloc(a->array, a->size * sizeof(pthread_t));
  }
  a->array[a->used++] = element;
}

void freeArray(Vector *a) {
  free(a->array);
  a->array = NULL;
  a->used = a->size = 0;
}
void djb_hash(const char* cp,char* hashResult) //https://codereview.stackexchange.com/questions/85556/simple-string-hashing-algorithm-implementation
{
    size_t hash = 5381;
    while (*cp)
        hash = 33 * hash ^ (unsigned char) *cp++;
    snprintf(hashResult, SMALLBUF, "%ld", hash);
}
void error(char *msg) {
  perror(msg);
  exit(1);
}
void tolowerconnection(char* connectionString)
{

    if(connectionString == NULL)
    {
        return; //no connection header
    }
    else
    {
        int count = 0;
        for (int i = 0; connectionString[i]; i++)
        {
            if (connectionString[i] != ' ')
            {
                connectionString[count++] = connectionString[i]; //remove white space
            }
        }
        connectionString[count] = '\0'; //set count amount of values as terminating charater
        for(unsigned long k=0;k<strlen(connectionString);k++)
        {
            connectionString[k]=tolower(connectionString[k]); //change connection string to all lowercase
        }
    }
}
void BadRequest(int sockfd,char* version,char*connection)
{
    int n;
    char errorMsg[BUFSIZE];
    strcpy(errorMsg,version);
    
    strcat(errorMsg," 400 Bad Request\r\nContent-Type:text/plain\r\nContent-Length:0\r\n\r\n"); //400 error
    if(strcmp(version,"HTTP/1.1")==0)
    {//http/1.1
        if(connection==NULL || strcmp(connection,"connection:keep-alive")==0) //keep socket alive
        {
            strcat(errorMsg,"Connection:keep-alive\r\n\r\n");
        }
        else if(strcmp(connection,"connection:close")==0) //close socket
        {
            strcat(errorMsg,"Connection:close\r\n\r\n");
        }
        else
        {
            printf("\n\n\nI BROKE\n\n\n");
            strcat(errorMsg,"\r\n");
        }
    }
    else
    {//http/1.0
        if(connection==NULL || strcmp(connection,"connection:close")==0) //close socket
        {
            strcat(errorMsg,"Connection:close\r\n\r\n");
        }
        else if(strcmp(connection,"connection:keep-alive")==0) //keep socket open
        {
            strcat(errorMsg,"Connection:keep-alive\r\n\r\n");
        }
        else
        {
            printf("\n\n\nI BROKE\n\n\n");
            strcat(errorMsg,"\r\n");
        }
    }
    n = send(sockfd,errorMsg,strlen(errorMsg),0);
    if (n < 0) 
        error("ERROR in sendto");
}
void Forbidden(int sockfd,char* version,char *connection)
{
    int n;
    char errorMsg[BUFSIZE];
    strcpy(errorMsg,version);
    strcat(errorMsg," 403 Forbidden\r\nContent-Type:text/plain\r\nContent-Length:0\r\n\r\n"); //403 error
    if(strcmp(version,"HTTP/1.1")==0)
    {//http/1.1
        if(connection==NULL || strcmp(connection,"connection:keep-alive")==0) //keep socket alive
        {
            strcat(errorMsg,"Connection:keep-alive\r\n\r\n");
        }
        else if(strcmp(connection,"connection:close")==0) //close socket
        {
            strcat(errorMsg,"Connection:close\r\n\r\n");
        }
        else
        {
            BadRequest(sockfd,version,connection);
            return;
        }
    }
    else
    {//http/1.0
        if(connection==NULL || strcmp(connection,"connection:close")==0) //close socket
        {
            strcat(errorMsg,"Connection:close\r\n\r\n");
        }
        else if(strcmp(connection,"connection:keep-alive")==0) //keep socket open
        {
            strcat(errorMsg,"Connection:keep-alive\r\n\r\n");
        }
        else
        {
            BadRequest(sockfd,version,connection);
            return;
        }
    }
    n = send(sockfd,errorMsg,strlen(errorMsg),0);
    if (n < 0) 
        error("ERROR in sendto");
}
void NotFound(int sockfd,char* version,char *connection)
{
    int n;
    char errorMsg[BUFSIZE];
    strcpy(errorMsg,version);
    strcat(errorMsg," 404 Not Found\r\nContent-Type:text/plain\r\nContent-Length:0\r\n"); //404 error
    if(strcmp(version,"HTTP/1.1")==0)
    {//http/1.1
        if(connection==NULL || strcmp(connection,"connection:keep-alive")==0) //keep socket alive
        {
            strcat(errorMsg,"Connection:keep-alive\r\n\r\n");
        }
        else if(strcmp(connection,"connection:close")==0) //close socket
        {
            strcat(errorMsg,"Connection:close\r\n\r\n");
        }
        else
        {
            BadRequest(sockfd,version,connection);
            return;
        }
    }
    else
    {//http/1.0
        if(connection==NULL || strcmp(connection,"connection:close")==0) //close socket
        {
            strcat(errorMsg,"Connection:close\r\n\r\n");
        }
        else if(strcmp(connection,"connection:keep-alive")==0) //keep socket open
        {
            strcat(errorMsg,"Connection:keep-alive\r\n\r\n");
        }
        else
        {
            BadRequest(sockfd,version,connection);
            return;
        }
    }
    n = send(sockfd,errorMsg,strlen(errorMsg),0);
    if (n < 0) 
        error("ERROR in sendto");
}
void sigHandler()
{
    printf("\nWaiting for threads.\n");
    pthread_mutex_lock(&runningMutex);
    keepRunning = false;
    pthread_mutex_unlock(&runningMutex);
    close(MAINsockfd);
}
void trimHost(char* hostname,char*result)
{
    strcpy(result,hostname+6); //removes Host: 
    char *ptr;
    ptr = strchr(result, ':');
    if (ptr != NULL) {
        *ptr = '\0';
    }
}
bool isCached(char* hash)
{
    struct stat file_stat;
    DIR* dir = opendir("./temp");
    char buf[BUFSIZE];
    bzero(buf, BUFSIZE);
    strcpy(buf, "temp/");
    strcat(buf, hash);
    if(dir != NULL){
        closedir(dir);
        time_t curTime = time(NULL);
        if((stat(buf, &file_stat) != 0) || difftime(curTime, file_stat.st_mtime) > timer)
            return false;
        return true;
    }
    else
    {
        mkdir("temp", 0777);
        return false;
    }
}
void sendCached(int sockfd,char* file)
{
    int n;
    long filesize =0;
    FILE *fp = fopen(file,"rb");
    if(fp == NULL)
    {
        error("ERROR failed to open cached file");
        return;
    }
    else
    {
        fseek(fp, 0L, SEEK_END);
        filesize = ftell(fp);
        fseek(fp, 0L, SEEK_SET);
        char* fileContent = malloc(sizeof(char) * filesize);
        fread(fileContent,1,filesize,fp);
    
        n = send(sockfd,fileContent, filesize,0); //send full response
        if (n < 0) 
            {free(fileContent);error("ERROR in sendto");}
        free(fileContent);
        fclose(fp);
    }
}
bool isBlock(char* host,char*ip)
{
    FILE* fp;
    char *line=NULL;
    size_t len = 0;
    ssize_t read;
    if(access("blocklist", F_OK) == -1)
        return false;
    fp = fopen("blocklist", "rb");
    if(fp==NULL)
    {
        printf("ERROR failed to open blocklist\n");
        return false;
    }
    while ((read = getline(&line, &len, fp)) != -1) 
    {
        if(strstr(line, host) || strstr(line, ip))
            {fclose(fp);free(line);return 1;}
    }
    fclose(fp);
    if (line)
        free(line);
    return 0;
}
void* connection(void * connectionInfo)
{
    int sockfd = *(int *)connectionInfo; //gets socked fd
    int n;
    char buf[BUFSIZE];
    struct sockaddr_in serveraddr; /* server's addr */
    int serverlen;
    struct hostent* server;
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&timeout,sizeof(timeout)) < 0)  //set timeout
        error("ERROR setting receive socket timeout");

    while((n = recv(sockfd,buf,BUFSIZE,0)) > 0) //ensure socket is open and timeout hasnt been reached
    {
        bool keepAlive = true;
        char temp[BUFSIZE];
        pthread_mutex_lock(&parseMutex);
        bzero(temp, BUFSIZE);
        strcpy(temp,buf);
        temp[strlen(temp)-1] = '\0';
        char * request = strtok(temp," ");  //get user arguments
        char * file = strtok(NULL," ");
        char * version = strtok(NULL,"\r\n");
        char* connection = strstr(buf,"Connection");
        char* host = strstr(buf,"Host");
        strtok(host,"\r\n");
        strtok(connection,"\r\n");
        pthread_mutex_unlock(&parseMutex);
        char temp2[strlen(file)+1];
        strcpy(temp2,file);
        char * portno = strrchr(temp2,':');
        if(portno!=NULL)
        {
            portno++;
            for(int i=0;i<((int)strlen(portno));i++)
            {
                if(!isdigit(portno[i]))
                {
                    portno[i] = '\0';
                }
            }
            if(strlen(portno)<3)
            {
                strcpy(portno,"80");
            }
        }
        printf("\nRequest type: %s\nFilename: %s\nVersion: %s\n%s\n%s\n", request, file, version,host,connection);
        tolowerconnection(connection); //convert connection header to lower
        if(version==NULL || strlen(version) != 8)
        {
            bzero(buf, BUFSIZE);
            char* errorMsg="HTTP/1.1 400 Bad Request\r\nContent-Type:text/plain\r\nContent-Length:0\r\nConnection:keep-alive\r\n\r\n"; //one of the required header values were null
            n = send(sockfd,errorMsg,strlen(errorMsg),0);
            if (n < 0) 
                error("ERROR in sendto");
        }
        else if(strcmp(version,"HTTP/1.1") && strcmp(version,"HTTP/1.0"))
        {
            bzero(buf, BUFSIZE);
            char* errorMsg="HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Type:text/plain\r\nContent-Length:0\r\nConnection:keep-alive\r\n\r\n"; //unsupported http version
            n = send(sockfd,errorMsg,strlen(errorMsg),0);
            if (n < 0) 
                error("ERROR in sendto");
        }
        else if(connection!=NULL && (strcmp(connection,"connection:close") != 0 && strcmp(connection,"connection:keep-alive") !=0))
        {
            BadRequest(sockfd,version,connection);
            if(strcmp(version,"HTTP/1.1")==0)
            {}
            else
            {//http/1.0
                break;
            }
        }
        else if(request == NULL || file == NULL || (host ==NULL && strcmp(version,"HTTP/1.1") ==0))
        {
            BadRequest(sockfd,version,connection);
            if(strcmp(version,"HTTP/1.1")==0)
            {//http/1.1
                if(connection==NULL || strcmp(connection,"connection:keep-alive")==0) //keep socket alive
                {}
                else
                {
                    break;
                }
            }
            else
            {//http/1.0
                if(connection==NULL || strcmp(connection,"connection:close")==0) //close socket
                {
                    break;
                }
                else if(strcmp(connection,"connection:keep-alive")==0) //keep socket open
                {}
                else
                {
                    break;
                }
            }
        }
        else if(strcmp(request,"GET"))
        {
            bzero(buf, BUFSIZE);
            char errorMsg[BUFSIZE];
            strcpy(errorMsg,version);
            strcat(errorMsg," 405 Method Not Allowed\r\nContent-Type:text/plain\r\nContent-Length:0\r\n\r\n"); //unsupported method
            n = send(sockfd,errorMsg,strlen(errorMsg),0);
            if (n < 0) 
                error("ERROR in sendto");
            if(strcmp(version,"HTTP/1.1")==0)
            {//http/1.1
                if(connection==NULL || strcmp(connection,"connection:keep-alive")==0) //keep socket alive
                {}
                else
                {
                    break;
                }
            }
            else
            {//http/1.0
                if(connection==NULL || strcmp(connection,"connection:close")==0) //close socket
                {
                    break;
                }
                else if(strcmp(connection,"connection:keep-alive")==0) //keep socket open
                {}
                else
                {
                    break;
                }
            }
        }
        else if(strstr(file,"..") != NULL)
        {
            bzero(buf, BUFSIZE);
            Forbidden(sockfd,version,connection); //tried moving up tree
            if(strcmp(version,"HTTP/1.1")==0)
            {//http/1.1
                if(connection==NULL || strcmp(connection,"connection:keep-alive")==0) //keep socket alive
                {}
                else
                {
                    break;
                }
            }
            else
            {//http/1.0
                if(connection==NULL || strcmp(connection,"connection:close")==0) //close socket
                {
                    break;
                }
                else if(strcmp(connection,"connection:keep-alive")==0) //keep socket open
                {}
                else
                {
                    break;
                }
            }
        }
        else
        {
            char newConnect[SMALLBUF];
            char version1[9];
            strcpy(version1,version);
            if(strcmp(version1,"HTTP/1.1")==0)
            {//http/1.1
                if(connection==NULL || strcmp(connection,"connection:keep-alive")==0) //keep socket alive
                {
                    strcpy(newConnect,"Connection: keep-alive");
                }
                else
                {
                    strcpy(newConnect,"Connection: close");
                    keepAlive = false;
                }
            }
            else
            {//http/1.0
                if(connection==NULL || strcmp(connection,"connection:close")==0) //close socket
                {
                    strcpy(newConnect,"Connection: close");
                    keepAlive = false;
                }
                else if(strcmp(connection,"connection:keep-alive")==0) //keep socket open
                {
                    strcpy(newConnect,"Connection: keep-alive");
                }
                else
                {
                    strcpy(newConnect,"Connection: close");
                    keepAlive = false;
                }
            }
            char newHost[SMALLBUF];
            trimHost(host,newHost);
            char hash[strlen(newHost) + strlen(file) + strlen(newConnect) + strlen(version1) + 4];
            strcpy(hash,newHost);
            strcat(hash,"/");
            strcat(hash,file);
            strcat(hash,"/");
            strcat(hash,version1);
            strcat(hash,"/");
            strcat(hash,newConnect);
            char hashResult[SMALLBUF];
            djb_hash(hash,hashResult);
            char cacheName[strlen("temp/") + strlen(hashResult)+1];
            strcpy(cacheName,"temp/");
            strcat(cacheName,hashResult);
            if(isCached(hashResult))
            {
                sendCached(sockfd,cacheName);
            }
            else
            {
                int serverSock = socket(AF_INET, SOCK_STREAM, 0);
                if (serverSock < 0) 
                    error("ERROR opening socket to server");
                struct timeval timeout5;
                timeout5.tv_sec = 5;
                timeout5.tv_usec = 0;
                if (setsockopt(serverSock, SOL_SOCKET, SO_RCVTIMEO,&timeout5,sizeof(timeout5)) < 0)  //set timeout
                    error("ERROR setting receive socket timeout");
                pthread_mutex_lock(&hostresolveMutex);
                server = gethostbyname(newHost);
                pthread_mutex_unlock(&hostresolveMutex);
                if(server == NULL)
                {
                    NotFound(sockfd,version1,connection);
                }
                else
                {
                    bzero((char *) &serveraddr, sizeof(serveraddr));
                    serveraddr.sin_family = AF_INET;
                    pthread_mutex_lock(&hostresolveMutex);
                    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
                    pthread_mutex_unlock(&hostresolveMutex);
                    if(portno==NULL)
                    {
                        serveraddr.sin_port = htons(80);
                    }
                    else
                    {
                        serveraddr.sin_port = htons(atoi(portno));
                    }
                    char ip[20];
                    if (inet_ntop(AF_INET, (char *)&serveraddr.sin_addr.s_addr, ip, (socklen_t)20) == NULL)
                    {
                        NotFound(sockfd,version1,connection);
                    }
                    else
                    {
                        if(isBlock(newHost, ip))
                            Forbidden(sockfd,version1,connection);
                        else
                        {
                            int n1;
                            serverlen = sizeof(serveraddr);
                            if(connect(serverSock,(struct sockaddr*) &serveraddr,(socklen_t)serverlen) < 0)
                            {   
                                fprintf(stderr,"\nERROR connecting to server\nHostname: %s\nPort: %s\nFile: %s\n",newHost,portno,file);
                                NotFound(sockfd,version1,connection);
                            }
                            else
                            {
                                bzero(buf, BUFSIZE);
                                char* temp = strstr(file,"//");
                                if(temp!=NULL)
                                {
                                    temp +=2;
                                    file = strchr(temp,'/');
                                }
                                else
                                {
                                    file = strchr(file,'/');
                                }
                                sprintf(buf, "GET %s %s\r\nHost: %s\r\n%s\r\n\r\n", file, version1, newHost,newConnect);
                                n1 = send(serverSock, buf, strlen(buf),0);
                                if (n1 < 0)
                                    error("ERROR in sendto");
                                bzero(buf, BUFSIZE);
                                FILE* fp;
                                pthread_mutex_lock(&cacheMutex);
                                fp = fopen(cacheName, "wb");
                                while((n1 = recv(serverSock, buf, BUFSIZE,0))> 0)
                                {
                                    if (n1 < 0)
                                        error("ERROR in recvfrom\n");
                                    send(sockfd, buf, n1,0);
                                    fwrite(buf, 1, n1, fp);
                                    bzero(buf, BUFSIZE);
                                }
                                fclose(fp);
                                pthread_mutex_unlock(&cacheMutex);
                            }
                        }
                    }
                }
            }
        }
        bzero(buf, BUFSIZE); // reset buffer JUUUUUSSSSST incase
        pthread_mutex_lock(&runningMutex);
        if(!keepRunning|| !keepAlive) {pthread_mutex_unlock(&runningMutex);break;}
        pthread_mutex_unlock(&runningMutex);
    }
    bzero(buf, BUFSIZE);
    close(sockfd); //close socket
    free(connectionInfo); //free resources
    return NULL;
}

int main(int argc, char **argv)
{
    signal(SIGINT, sigHandler);
    int portno; /* port to listen on */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int clientlen = sizeof(clientaddr); /* byte size of client's address */
    int optval; /* flag value for setsockopt */
    
    if (argc != 3) {
        fprintf(stderr,"usage: %s <port> <timeout>\n", argv[0]);
        exit(1);
    }  
    timer = atoi(argv[2]);
    portno = atoi(argv[1]);
    MAINsockfd = socket(AF_INET, SOCK_STREAM, 0); //create TCP socket
    if (MAINsockfd < 0) 
        error("ERROR opening socket");
    /* setsockopt: Handy debugging trick that lets 
    * us rerun the server immediately after we kill it; 
    * otherwise we have to wait about 20 secs. 
    * Eliminates "ERROR on binding: Address already in use" error. 
    */
    optval = 1;
    if(setsockopt(MAINsockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int))<0)
        error("ERROR on setsockopt");
    /*
    * build the server's Internet address
    */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);
    /* 
    * bind: associate the parent socket with a port 
    */
    if (bind(MAINsockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)  //bind socket
        error("ERROR on binding");
    if(listen(MAINsockfd,connections) < 0) //create listening queue for socket
        error("ERROR on listening");
    pthread_mutex_init(&runningMutex,NULL);
    pthread_mutex_init(&cacheMutex,NULL);
    pthread_mutex_init(&parseMutex,NULL);
    pthread_mutex_init(&hostresolveMutex,NULL);
    Vector threadVec;
    initArray(&threadVec,10);
    int connectedfd;
    while((connectedfd = accept(MAINsockfd,(struct sockaddr *) &clientaddr, (socklen_t *) &clientlen))>0)
    {
        int *clientSocket = malloc(sizeof(int));
        *clientSocket = connectedfd;
        pthread_t tid;
        if(pthread_create(&tid, NULL, connection,(void *)clientSocket) < 0)
            error("Error on pthread create");
        insertArray(&threadVec,tid);
    }
    for(unsigned long i=0;i<threadVec.used;i++)
    {
        pthread_join(threadVec.array[i],NULL);
    }
    freeArray(&threadVec);
    pthread_mutex_destroy(&runningMutex);
    pthread_mutex_destroy(&cacheMutex);
    pthread_mutex_destroy(&hostresolveMutex);
     pthread_mutex_destroy(&parseMutex);
    DIR* dir = opendir("./temp");
    if(dir!=NULL)
    {
        char removeDir[SMALLBUF];
        strcpy(removeDir,"rm -rf ");
        strcat(removeDir,"temp");
        system(removeDir);
    }
    printf("Goodbye!\n");
    exit(0);
}