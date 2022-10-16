#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/stat.h>
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
  // a->used is the number of used entries, because a->array[a->used++] updates a->used only *after* the array has been accessed.
  // Therefore a->used can go up to a->size 
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
    keepRunning = false;
    close(MAINsockfd);
}

void* connection(void * connectionInfo)
{
    int sockfd = *(int *)connectionInfo; //gets socked fd
    //pthread_detach(pthread_self()); //
    //thread routine starts here
    int n;
    char buf[BUFSIZE];
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&timeout,sizeof(timeout)) < 0)  //set timeout
        error("ERROR setting receive socket timeout");
    while((n = recv(sockfd,buf,BUFSIZE,0)) > 0) //ensure socket is open and timeout hasnt been reached
    {
        bool keepAlive = true;
        //sleep(5);
        //printf("\nServer received the following request:\n%s",buf);
        char temp[BUFSIZE];
        bzero(temp, BUFSIZE);
        strcpy(temp,buf);
        temp[strlen(temp)-1] = '\0';
        char * request = strtok(temp," ");  //get user arguments
        char * file = strtok(NULL," ");
        char * version = strtok(NULL,"\r\n");
        char* connection = strstr(buf,"Connection");
        strtok(connection,"\r\n");
        
        //printf("\n\n\n\n\n%s\n\n\n\n",connection);
        printf("\nRequest type: %s\nFilename: %s\nVersion: %s\n%s\n", request, file, version,connection);
        tolowerconnection(connection); //convert connection header to lower

        if(version==NULL)
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
        else if(request == NULL || file == NULL)
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
            char version1[9];
            strcpy(version1,version);
            if(strcmp(version1,"HTTP/1.1")==0)
            {//http/1.1
                if(connection==NULL || strcmp(connection,"connection:keep-alive")==0) //keep socket alive
                {}
                else
                {
                    keepAlive = false;
                }
            }
            else
            {//http/1.0
                if(connection==NULL || strcmp(connection,"connection:close")==0) //close socket
                {
                    keepAlive = false;
                }
                else if(strcmp(connection,"connection:keep-alive")==0) //keep socket open
                {}
                else
                {
                    keepAlive = false;
                }
            }
            long filesize =0;
            if(!strcmp(file,"/"))
            {
                FILE * fp = NULL;
                if(access("www/index.htm", F_OK) == 0 || access("www/index.html", F_OK) == 0) //one of the file exists
                {
                    fp = access("www/index.htm", F_OK) == 0 ? fopen("www/index.htm","rb") : fopen("www/index.html","rb"); //attempt to open
                    if(fp ==NULL)
                    {
                        Forbidden(sockfd,version1,connection); //no read access
                    }
                    else
                    {
                        char * contentType = "text/html";
                        fseek(fp, 0L, SEEK_END);
                        filesize = ftell(fp);
                        fseek(fp, 0L, SEEK_SET);
                        char * fileContent = malloc(sizeof(char) * filesize);
                        fread(fileContent,1,filesize,fp);
                        char header[BUFSIZE];
                        if(keepAlive)
                        {
                            snprintf(header,sizeof(header), "%s 200 OK\r\nContent-Type:%s\nContent-Length:%ld\r\nConnection:keep-alive\r\n\r\n",version1,contentType,filesize); //send keepalive
                        }
                        else
                        {
                            snprintf(header,sizeof(header), "%s 200 OK\r\nContent-Type:%s\nContent-Length:%ld\r\nConnection:close\r\n\r\n",version1,contentType,filesize); //send close
                        }
                        char * fullResponse = malloc(sizeof(char) * (strlen(header) + filesize));
                        strcpy(fullResponse, header);
                        memcpy(fullResponse+strlen(header), fileContent, filesize);
                        n = send(sockfd,fullResponse,strlen(header) + filesize,0);//all good so send full responce
                        if (n < 0) 
                            {free(fullResponse);free(fileContent);error("ERROR in sendto");}
                        free(fullResponse);
                        free(fileContent);
                        fclose(fp);
                    }
                }
                else
                {
                    NotFound(sockfd,version,connection); //neither file found
                }
            }
            else
            {
                char fullPath[BUFSIZE];
                strcpy(fullPath,"www");
                strcat(fullPath,file);
                if(access(fullPath,F_OK)==0)
                { //file found
                    if(access(fullPath,R_OK)==0)
                    { //has read permissions
                        FILE * fp = fopen(fullPath,"rb");  
                        char* extentionDot = strrchr(file, '.');
                        if(extentionDot==NULL)
                        {
                            BadRequest(sockfd,version1,connection); //attempted file has no extention
                        }
                        else
                        {
                            char contentType[SMALLBUF];         
                            char extention[SMALLBUF];
                            strncpy(extention, extentionDot+1, 4);//sets contentType
                            if(strcmp(extention,"html")==0 || strcmp(extention,"htm")==0)strcpy(contentType,"text/html");
                            else if (strcmp(extention,"txt")==0)strcpy(contentType,"text/plain");
                            else if (strcmp(extention,"png")==0)strcpy(contentType,"image/png");
                            else if (strcmp(extention,"gif")==0)strcpy(contentType,"image/gif");
                            else if (strcmp(extention,"jpg")==0)strcpy(contentType,"image/jpg");
                            else if (strcmp(extention,"css")==0)strcpy(contentType,"text/css");
                            else if (strcmp(extention,"ico")==0)strcpy(contentType,"image/x-icon");
                            else if (strcmp(extention,"js")==0)strcpy(contentType,"application/javascript");
                            else
                            {
                                BadRequest(sockfd,version1,connection); //unsupported contentType
                                fclose(fp);//close file
                            }
                            if(fp!=NULL)
                            {//valid file shiet
                                fseek(fp, 0L, SEEK_END);
                                filesize = ftell(fp);
                                fseek(fp, 0L, SEEK_SET);
                                char* fileContent = malloc(sizeof(char) * filesize);
                                fread(fileContent,1,filesize,fp);
                                char header[BUFSIZE];
                                if(keepAlive)
                                {
                                    snprintf(header,sizeof(header), "%s 200 OK\r\nContent-Type:%s\nContent-Length:%ld\r\nConnection:keep-alive\r\n\r\n",version1,contentType,filesize); //send keepalive
                                }
                                else
                                {
                                    snprintf(header,sizeof(header), "%s 200 OK\r\nContent-Type:%s\nContent-Length:%ld\r\nConnection:close\r\n\r\n",version1,contentType,filesize); //send close
                                }
                                char* fullResponse = malloc(sizeof(char) * (strlen(header) + filesize));
                                strcpy(fullResponse, header);
                                memcpy(fullResponse+strlen(header), fileContent, filesize);
                                n = send(sockfd,fullResponse,strlen(header) + filesize,0); //send full response
                                if (n < 0) 
                                    {free(fullResponse);free(fileContent);error("ERROR in sendto");}
                                free(fullResponse);
                                free(fileContent);
                                fclose(fp);
                            }
                        }
                    }
                    else
                    {
                        Forbidden(sockfd,version1,connection); //no read permission
                    }
                }
                else
                {
                    NotFound(sockfd,version1,connection); //file not found
                }
            }
        }
        bzero(buf, BUFSIZE); // reset buffer JUUUUUSSSSST incase
        if(!keepRunning|| !keepAlive) break;
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
    int clientlen; /* byte size of client's address */
    int optval; /* flag value for setsockopt */
    
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }  
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
    setsockopt(MAINsockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
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
    clientlen = sizeof(clientaddr);
    int connectedfd;
    Vector threadVec;
    initArray(&threadVec,10);
    while((connectedfd = accept(MAINsockfd,(struct sockaddr *) &clientaddr, (socklen_t *) &clientlen))>0)
    {
        int *clientSocket = malloc(sizeof(int));
        *clientSocket = connectedfd;
        pthread_t tid;
        if(pthread_create(&tid, NULL, connection,(void *)clientSocket) < 0)
            error("Error on pthread create");
        insertArray(&threadVec,tid);
    }
    //sleep(10);
    for(unsigned long i=0;i<threadVec.used;i++)
    {
        pthread_join(threadVec.array[i],NULL);
    }
    freeArray(&threadVec);
    printf("Goodbye!\n");
    exit(0);
}