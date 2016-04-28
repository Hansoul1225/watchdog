#include <iostream>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <error.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <map>

#define MAXLINE 1000
#define SERV_PORT 8000
#define MAXBACKLOG 20
#define MAXCLIENT 30
#define MAX_QUEUE_LENGTH 32

using namespace std;
struct info{
    char *IP;
    time_t tiptok;
    int timeout;
};
static int client = 0;
map<int, info*> clientInfo;
FILE* file;
void tick(time_t ntime)
{
    map<int, info*>::iterator iter;
    for(iter = clientInfo.begin(); iter != clientInfo.end();)
    {
        int fd = iter->first;
        info* cInfo = iter->second;
        char* IP = cInfo->IP;
        time_t tiptok = cInfo->tiptok;
        if((ntime - tiptok) > 60)
        {
            cout<<"Client "<<IP<<" timeout...\n";
            cInfo->timeout++;
            if(cInfo->timeout >= 3)
            {
                cout<<"Client "<<IP<<" has crashed\n"<<endl;
                char err[120];
                memset(err, 0, sizeof(err));
                time_t now;
                time(&now);
                sprintf(err,"%s at %s lost connect\n",IP,ctime(&now));
                fwrite(err, strlen(err), 1, file);

                delete cInfo;
                clientInfo.erase(iter++);

                close(fd);

            }
            else
                iter++;
        }
    }
}

void setnonblocking(int sock)  
{  
    int opts;  
    opts=fcntl(sock,F_GETFL);  
    if(opts<0)  
    {  
        cout<<"fcntl(sock,GETFL)";  
        exit(1);  
    }  
    opts = opts|O_NONBLOCK;  
    if(fcntl(sock,F_SETFL,opts)<0)  
    {  
        cout<<"fcntl(sock,SETFL,opts)";  
        exit(1);  
    }  
}  

int main(void)
{
    struct sockaddr_in servaddr, cliaddr;
    socklen_t cliaddr_len;
    int listenfd, connfd;
    char buf[MAXLINE];
    char ipaddress[INET_ADDRSTRLEN];
    int i, efd;

    if((file = fopen("error.txt", "a")) == NULL)
    {
        cout<<"cannot open error.txt\n";
    }

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    setnonblocking(listenfd);
    memset(&servaddr, 0,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    listen(listenfd, MAX_QUEUE_LENGTH);
    cout<<"Watching..."<<endl;

    struct epoll_event event;
    struct epoll_event resevent[MAXCLIENT];
    int res, len;

    efd = epoll_create(MAXCLIENT);
    if(efd < 0){
        cout<<"epoll_create error!";
        exit(1);
    }

    event.events = EPOLLIN | EPOLLET;
    event.data.fd = listenfd;
    res = epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &event);

    for(;;){
        cout<<"total "<<client<<" online"<<endl;
        res = epoll_wait(efd, resevent, MAXCLIENT, 5000);
        if(res < 0)
            cout<<"epoll wait failed";
        for(int i = 0;i < res;i++)
        {
            int fd = resevent[i].data.fd;
            if(fd == listenfd)
            {
                connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &cliaddr_len);
                setnonblocking(connfd);
                if(connfd > 0)
                {
                    event.events = EPOLLIN | EPOLLHUP;//监听读事件 以及挂断close以及ctrl+c
                    event.data.fd = connfd;
                    if(epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &event)<0)
                    {
                        cout<<"epoll_ctr error, cannot add client"<<endl;
                    }
                    cout<<"received from "<<inet_ntop(AF_INET, &cliaddr.sin_addr, ipaddress, sizeof(ipaddress))<<" at POR "<<ntohs(cliaddr.sin_port)<<endl;
                    time_t st = time(NULL);
                    info *cinfo = new info;
                    cinfo->IP = ipaddress;
                    cinfo->timeout = 0;
                    cinfo->tiptok = st;
                    clientInfo[connfd] = cinfo;
                    client++;
                    cout<<cinfo->IP;
                }
            }
            else if(resevent[i].events & EPOLLIN)
            {
                if ((len = read(fd,buf,sizeof(buf)))>0)
                {
                    write(STDOUT_FILENO, buf, len);
                    time_t st = time(NULL);
                    clientInfo[fd]->tiptok = st;
                    cout<<"IP = "<<clientInfo[fd]->IP<<endl;
                }
                else
                {
                    cout<<clientInfo[fd]->IP<<"   has closed"<<endl;

                    FILE* file;//write this in error.txt to mark
                    char err[120];
                    memset(err, 0, sizeof(err));
                    time_t now;
                    time(&now);
                    sprintf(err,"%s at %s lost connect\n",clientInfo[fd]->IP,ctime(&now));
                    fwrite(err, strlen(err), 1, file);

                    delete clientInfo[fd];
                    clientInfo.erase(fd);
                    client--;
                    close(fd);
                }                   
            }
            else
                cout<<"IP = "<<clientInfo[fd]->IP<<"has crashed"<<endl;
        }    
        time_t ntime = time(NULL);
        tick(ntime);
    }

    close(efd);
    return 0;
}

