#define GNU_SOURCE
#include<signal.h>
#include<errno.h>
#include<sys/wait.h>
#include<stdio.h>
#include<stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
//rETURN THE 500 CODE
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
void get_content_type(char * fullpath,char * filetype,char ** contenttype,FILE **f,int * media)
{
                 if(!strcmp(filetype,".html")) 
                {
                    *contenttype = "text/html";
                    *f=fopen(fullpath,"r");
                }
                else if(!strcmp(filetype,".css")) 
                {
                    *contenttype ="text/css";
                    *f=fopen(fullpath,"r");
                }
                else if(!strcmp(filetype,".js") )
                {
                    *contenttype ="text/javascript";
                    *f=fopen(fullpath,"r");
                }
                else if(!strcmp(filetype,".jpeg") || !strcmp(filetype,".jpg") ) 
                {
                    *contenttype ="image/jpeg";
                    *f=fopen(fullpath,"rb");
                    *media=1;
                }
                else if(!strcmp(filetype,".png") ) 
                {
                    *contenttype ="image/png";
                    *f=fopen(fullpath,"rb");
                    *media=1;
                }
                else if(!strcmp(filetype,".gif") ) 
                {
                    *contenttype ="image/gif";
                    *f=fopen(fullpath,"rb");
                    *media=1;
                }
                else if(!strcmp(filetype,".ico") ) 
                {
                    *contenttype ="image/ico";
                    *f=fopen(fullpath,"rb");
                    *media=1;
                }
                else if(!strcmp(filetype,".mp3"))
                {
                    *contenttype="audio/mpeg";
                    *f=fopen(fullpath,"rb");
                    *media=1;
                }
                else if(!strcmp(filetype,".mp4"))
                {
                    *contenttype="video/mp4";
                    *f=fopen(fullpath,"rb");
                    *media=1;
                }
                else
                {
                    *contenttype="application/octet-stream";
                    *f=fopen(fullpath,"rb");
                    *media=1;
                }
}
void sigchild_handler(int s)
{
    (void) s;
    int saved_errno=errno;
    while(waitpid(-1,NULL,WNOHANG)>0);
    errno=saved_errno;
}

int main()
{
    struct sigaction sa;
    int status;
    struct addrinfo hints,*servinfo;
    struct sockaddr_storage their_addr;
    socklen_t size_of_their_addr;
    memset(&hints,0,sizeof(hints));
    hints.ai_family= AF_UNSPEC;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    // First we get the info to create the socket
    status = getaddrinfo(NULL,"4390",&hints,&servinfo);
    if(status!=0)
    {
        perror("GETADDREINFO FAILED");
        return -1;
    }
    // Then we create the socket
    int s,accepted;
    s= socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol);
    //Bind the socket to an adress
    if(bind(s,servinfo->ai_addr,servinfo->ai_addrlen)!=0)
    {
        perror("Binding failed");
        return -1;
    }
    printf("bound and is gonna listen\n");
    // Start listening on said socket
    if(listen(s,10)!=0)
    {
        perror("LISTEN FAILED");
        return -1;
    }
    sa.sa_handler = sigchild_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    char host[INET6_ADDRSTRLEN];
    char buffer[64000];
    while(1)
    {
        printf("Server is listening!\n");
        size_of_their_addr = sizeof their_addr;
        accepted=accept(s,(struct sockaddr *)&their_addr,&size_of_their_addr);
        //Accept blocks the process in the case that the buffer is empty until a request is received otehrwise if it returns -1 it has failed
        if(accepted==-1) continue;
        inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),host,sizeof host);
        if(host==NULL) printf("INET NTOP FAILED");
        else printf("Connection received on %s\n",host);
        ssize_t bytesread = recv(accepted,buffer,64000,0);
        if(bytesread>=0) buffer[bytesread]='\0';
        else{
            printf("error reading the request\n");
            continue;
        }
        // We create a child to handle the response to the request 
        if(!fork())
        {
            //We get the path of the the file and check if it exists
            char function[20],path[256],version[30] ;
            sscanf(buffer,"%s %s %s",function,path,version);
            char fullpath[256] = "files";
            strcat(fullpath,path);
            int exists = access(fullpath,F_OK);
            char * finding_str = strrchr(path,'.');
            char * filetype;
            if (finding_str!=NULL)
            {
                int pointindex = finding_str-path;
                filetype = &path[pointindex];
            }
            else filetype="";
            
            if(exists==0)
            {
                int media=0;
                FILE *f;
                char* contenttype;
                char header[512];
                get_content_type(fullpath,filetype,&contenttype,&f,&media);
                if(f!=NULL)
                {     
                    if(!media)
                    {
                        //if it isn't a media file we open textually and get the size make the header add the textual content and voila we send it all in one request
                        fseek(f,0,SEEK_END);
                        int filesize = ftell(f);
                        char file[filesize+1];
                        sprintf(header,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %i\r\n\r\n",contenttype,filesize);
                        fseek(f,0,SEEK_SET);
                        int read = fread(file,sizeof(char),filesize,f);
                        file[filesize]='\0';
                        int responsesize = strlen(header)+filesize+1;
                        char response[responsesize];
                        strcpy(response,header);
                        strcat(response,file);
                        send(accepted,response,strlen(response),0);
                        fclose(f);
                    }
                    else
                    {
                        // However if it is indeed a media file we send the header first and then read the file in chunks, for the time being it supports mp3 mp4 and images
                        fseek(f,0,SEEK_END);
                        int filesize = ftell(f);
                        sprintf(header,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %i\r\n\r\n",contenttype,filesize);
                        fseek(f,0,SEEK_SET);
                        send(accepted,header,strlen(header),0);
                        char filebuffer[4096];
                        int read;
                        while((read = fread(filebuffer,1,sizeof(filebuffer),f))>0)
                        {
                            send(accepted,filebuffer,read,0);
                        }
                        fclose(f);
                    }
                }
                else
                {
                    FILE *g = fopen("files/500.html","r");
                    int filesize;
                    fseek(g,0,SEEK_END);
                    filesize=ftell(g);
                    fseek(g,0,SEEK_SET);
                    char file[filesize+1];
                    char *header="HTTP/1.1  500 Internal Server Error \r\n"
                                                "Content-Type: text/html; charset\r\n\r\n";
                    fread(file,sizeof(char),filesize,g);
                    file[filesize]='\0';
                    int responsize=filesize+strlen(header);
                    char response[responsize+1];
                    strcpy(response,header);
                    strcat(response,file);
                    send(accepted,response,strlen(response),0);
                    fclose(g);
                }
            }
            else
            {
                // However if the file does not exist it sends a 404 not found page if it is an html file otherwise it sends only the header
                if(!strcmp(filetype,".html"))
                {
                    printf("the filetype is %s\n",filetype);
                    char header[512] = "HTTP/1.1 404 Not Found\r\n"
                                "Content-Type: text/html; charset=UTF-8\r\n\r\n";
                    FILE *f = fopen("files/nfound.html","r");
                    fseek(f,0,SEEK_END);
                    int filesize = ftell(f);
                    char file[filesize+1];
                    fseek(f,0,SEEK_SET);
                    int read = fread(file,sizeof(char),filesize,f);
                    file[filesize]='\0';
                    fclose(f);
                    int responsesize = strlen(header)+filesize+1;
                    char response[responsesize];
                    strcpy(response,header);
                    strcat(response,file);
                    send(accepted,response,strlen(response),0);
                }
                else
                {
                    printf("the filetype is %s\n",filetype);
                    char * header = "HTTP/1.1 404 Not Found\r\n"
                                "Content-Length: 0\r\n\r\n";
                    send(accepted,header,strlen(header),0);
                
                }
            }
            shutdown(accepted,SHUT_RDWR);
            close(accepted);
            exit(0);
        }
    }
    return 0;
}