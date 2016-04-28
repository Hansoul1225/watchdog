#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>

#define MAXLINE 1000
#define SERV_PORT 8000

typedef struct PACKED         //定义一个cpu occupy的结构体
{
    char name[20];      //定义一个char类型的数组名name有20个元素
    unsigned int user; //定义一个无符号的int类型的user
    unsigned int nice; //定义一个无符号的int类型的nice
    unsigned int system;//定义一个无符号的int类型的system
    unsigned int idle; //定义一个无符号的int类型的idle
}CPU_OCCUPY;

typedef struct PACKED1        //定义一个mem occupy的结构体
{
    char name[20];      //定义一个char类型的数组名name有20个元素
    unsigned int total;
    char name2[20];
    unsigned int free;
}MEM_OCCUPY;

int get_memoccupy (MEM_OCCUPY *mem) //对无类型get函数含有一个形参结构体类弄的指针O
{
    FILE *fd;
    int n, memu;
    char buff[256];
    MEM_OCCUPY *m;
    m=mem;

    fd = fopen ("/proc/meminfo", "r");

    fgets (buff, sizeof(buff), fd);
    sscanf (buff, "%s %u", m->name, &m->total);
    printf("%s %u\n", m->name, m->total);

    fgets (buff, sizeof(buff), fd); //从fd文件中读取长度为buff的字符串再存到起始地址为buff这个空间里 
    sscanf (buff, "%s %u", m->name2, &m->free);
    printf("%s %u\n", m->name2, m->free);
    memu = (m->total - m->free) * 100 / m->total;
    printf("MEM = %d\n",memu);
    fclose(fd);     //关闭文件fd
    return memu;
}
int cal_cpuoccupy (CPU_OCCUPY *o, CPU_OCCUPY *n)
{
    unsigned int od, nd;
    unsigned int id, sd;
    int cpu_use = 0;   

    od = (unsigned int) (o->user + o->nice + o->system +o->idle);//第一次(用户+优先级+系统+空闲)的时间再赋给od
    nd = (unsigned int) (n->user + n->nice + n->system +n->idle);//第二次(用户+优先级+系统+空闲)的时间再赋给od

    id = (unsigned int) (n->user - o->user);    //用户第一次和第二次的时间之差再赋给id
    sd = (unsigned int) (n->system - o->system);//系统第一次和第二次的时间之差再赋给sd
    if((nd-od) != 0)
        cpu_use = (int)((sd+id)*100)/(nd-od); //((用户+系统)乖100)除(第一次和第二次的时间差)再赋给g_cpu_used
    else cpu_use = 0;
    //printf("cpu: %u\n",cpu_use);
    return cpu_use;
}

void get_cpuoccupy (CPU_OCCUPY *cpust) //对无类型get函数含有一个形参结构体类弄的指针O
{
    FILE *fd;
    int n;
    char buff[256];
    CPU_OCCUPY *cpu_occupy;
    cpu_occupy=cpust;

    fd = fopen ("/proc/stat", "r");
    fgets (buff, sizeof(buff), fd);

    sscanf (buff, "%s %u %u %u %u", cpu_occupy->name, &cpu_occupy->user, &cpu_occupy->nice,&cpu_occupy->system, &cpu_occupy->idle);

    fclose(fd);
}


int main(int argc, char *argv[])
{
    struct sockaddr_in servaddr;
    char buf[MAXLINE] = {'0'};
    char request[MAXLINE] = {'0'};
    int sockfd, i;
    CPU_OCCUPY cpu_stat1;
    CPU_OCCUPY cpu_stat2;
    MEM_OCCUPY mem_stat;
    int cpu, mem, flag, err;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    printf("connecting %s\n", argv[1]);
    servaddr.sin_port = htons(SERV_PORT);
    connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    //flag = fcntl(sockfd, F_GETFL);
    //flag |= O_NONBLOCK;
    //fcntl(sockfd, F_SETFL, flag);
    while (1) {
        //获取内存	
        mem = get_memoccupy ((MEM_OCCUPY *)&mem_stat);
        //第一次获取cpu使用情况
        get_cpuoccupy((CPU_OCCUPY *)&cpu_stat1);
        sleep(10);

        //第二次获取cpu使用情况
        get_cpuoccupy((CPU_OCCUPY *)&cpu_stat2);

        //计算cpu使用率
        cpu = cal_cpuoccupy ((CPU_OCCUPY *)&cpu_stat1, (CPU_OCCUPY *)&cpu_stat2);
        printf("CPU = %d\n", cpu);
        sprintf(buf,"CPU = %d%% MEM = %d%%  ",cpu,mem);	
        write(sockfd, buf, sizeof(buf));
        sleep(20);
    }
    Close(sockfd);
    return 0;
}
