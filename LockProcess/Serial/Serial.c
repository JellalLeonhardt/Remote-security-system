#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
//全局变量
struct Serial_st{
    unsigned char IsPeople;
}serial_st;
typedef struct BufferArea{
	unsigned char num[5];
}AREA;
union semun{
    short val;
    struct semid_ds *buf;
    unsigned short *arry;
};
int SemKey = 0xAE12;
int ShmKey = 0xAE86;
struct sembuf SopsP;
struct sembuf SopsV;
int SemID;
int ShmID;
AREA *ShmAddr = NULL;
union semun sem_union;

void SemInit(){
    SemID = semget(SemKey,3,0);
    if(SemID == -1){
        printf("创建信号量失败\n");
        exit(0);
    }
    printf("SemID:%d\n",SemID);
    SopsP.sem_num = 0;
    SopsP.sem_op = -1;
    SopsP.sem_flg = SEM_UNDO;
    SopsV.sem_num = 0;
    SopsV.sem_op = 1;
    SopsV.sem_flg = SEM_UNDO;
    sem_union.val = 1;
    perror("semctl");
}
void ShmInit(){
    ShmID = shmget(ShmKey,sizeof(struct BufferArea),0);
    if(ShmID == -1){
        printf("创建共享内存失败\n");
        exit(0);
    }
    printf("ShmID:%d\n",ShmID);
    ShmAddr = shmat(ShmID,NULL,0);
    if(ShmAddr == -1){
        printf("共享内存连接失败\n");
        semctl(SemID,0,IPC_RMID,sem_union); //删除信号量
        exit(0);
    }
}
void Semaphore_P(int num){
    SopsP.sem_num = num;
    if(semop(SemID,&SopsP,1) == -1){
        printf("P操作失败.\n");
        semctl(SemID,0,IPC_RMID,sem_union);
        shmdt(ShmAddr);
        shmctl(ShmID,IPC_RMID,0);
        exit(0);
    }
}
void Semaphore_V(int num){
    SopsV.sem_num = num;
    if(semop(SemID,&SopsV,1) == -1){
        printf("V操作失败.\n");
        semctl(SemID,0,IPC_RMID,sem_union);
        shmdt(ShmAddr);
        shmctl(ShmID,IPC_RMID,0);
        exit(0);
    }
}

int open_port(int comport){
    int fd;
    char *dev[] = {"/dev/ttyUSB0","/dev/ttyUSB1","/dev/ttyUSB2"};
    if (comport == 0){
        fd = open(dev[0],O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (-1 == fd){
            perror("Can't open port.\n");
            return -1;
        }
    }
    else if(comport == 1){
        fd = open(dev[1],O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (-1 == fd){
            perror("Can't open port.\n");
            return -1;
        }
    }
    else if(comport == 2){
        fd = open(dev[2],O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (-1 == fd){
            perror("Can't open port.\n");
            return -1;
        }
    }

    if(fcntl(fd, F_SETFL,0) < 0){
        printf("Fcntl failed.\n");
    }
    else{
        printf("Fcntl = %d\n",fcntl(fd, F_SETFL));
    }

    if(isatty(STDIN_FILENO) == 0){
        printf("standard input is not a terminal device.\n");
    }
    else{
        printf("isatty success.\n");
    }
    printf("fd-open = %d\n",fd);
    return fd;
}

int set_opt(int fd,int nSpeed, int nBits, char nEvent, int nStop)   
{
     struct termios newtio,oldtio;   
/*保存测试现有串口参数设置，在这里如果串口号等出错，会有相关的出错信息*/   
     if  ( tcgetattr( fd,&oldtio)  !=  0) {    
      perror("SetupSerial 1");  
      printf("tcgetattr( fd,&oldtio) -> %d\n",tcgetattr( fd,&oldtio));   
      return -1;   
     }
     bzero( &newtio, sizeof( newtio ) );   
/*步骤一，设置字符大小*/   
     newtio.c_cflag  |=  CLOCAL | CREAD;    
     newtio.c_cflag &= ~CSIZE;    
/*设置停止位*/   
     switch( nBits )   
     {   
     case 7:   
      newtio.c_cflag |= CS7;   
      break;   
     case 8:   
      newtio.c_cflag |= CS8;   
      break;   
     }   
/*设置奇偶校验位*/   
     switch( nEvent )   
     {   
     case 'o':  
     case 'O': //奇数   
      newtio.c_cflag |= PARENB;   
      newtio.c_cflag |= PARODD;   
      newtio.c_iflag |= (INPCK | ISTRIP);   
      break;   
     case 'e':  
     case 'E': //偶数   
      newtio.c_iflag |= (INPCK | ISTRIP);   
      newtio.c_cflag |= PARENB;   
      newtio.c_cflag &= ~PARODD;   
      break;  
     case 'n':  
     case 'N':  //无奇偶校验位   
      newtio.c_cflag &= ~PARENB;   
      break;  
     default:  
      break;  
     }   
     /*设置波特率*/   
    switch( nSpeed )   
     {   
     case 2400:   
      cfsetispeed(&newtio, B2400);   
      cfsetospeed(&newtio, B2400);   
      break;   
     case 4800:   
      cfsetispeed(&newtio, B4800);   
      cfsetospeed(&newtio, B4800);   
      break;   
     case 9600:   
      cfsetispeed(&newtio, B9600);   
      cfsetospeed(&newtio, B9600);   
      break;   
     case 115200:   
      cfsetispeed(&newtio, B115200);   
      cfsetospeed(&newtio, B115200);   
      break;   
     case 460800:   
      cfsetispeed(&newtio, B460800);   
      cfsetospeed(&newtio, B460800);   
      break;   
     default:   
      cfsetispeed(&newtio, B9600);   
      cfsetospeed(&newtio, B9600);   
     break;   
     }   
/*设置停止位*/   
     if( nStop == 1 )   
      newtio.c_cflag &=  ~CSTOPB;   
     else if ( nStop == 2 )   
      newtio.c_cflag |=  CSTOPB;   
/*设置等待时间和最小接收字符*/   
     newtio.c_cc[VTIME]  = 0;   
     newtio.c_cc[VMIN] = 0;   
/*处理未接收字符*/   
     tcflush(fd,TCIFLUSH);   
/*激活新配置*/   
    if((tcsetattr(fd,TCSANOW,&newtio))!=0)   
     {   
      perror("com set error");   
      return -1;   
     }   
     printf("set done!\n");   
     return 0;   
}
int main(int args,char *argv[]){
	SemInit();
	ShmInit();
    int comport;
    int read_cnt;
    printf("Serialtest\n");
    //printf("Input comport:");
    //scanf("%d",&comport);
    comport = atoi(argv[1]);
    int fd = open_port(comport);
    if(fd == -1){
        printf("Open failed.\n");
        return 1;
    }
    while(set_opt(fd,115200,8,'N',1) == -1);
    while(1){
        read_cnt = read(fd,&serial_st,1);
        if(read_cnt) {
        	printf("%d %c\n",read_cnt,serial_st.IsPeople);
        	Semaphore_P(0);
        	printf("Serialtest\n");
        	ShmAddr->num[0] = serial_st.IsPeople;
        	Semaphore_V(0);
        }
    }
    return fd;
}
