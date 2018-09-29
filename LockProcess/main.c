#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/sem.h>
#include<sys/shm.h>
#include<stdlib.h>
#include<errno.h>
#include<sys/wait.h>
#include<string.h>
//全局变量
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
    SemID = semget(SemKey,3,IPC_CREAT);
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
    semctl(SemID,0,SETVAL,sem_union);
    semctl(SemID,1,SETVAL,sem_union);
    semctl(SemID,2,SETVAL,sem_union);
    perror("semctl");
}
void ShmInit(){
    ShmID = shmget(ShmKey,sizeof(struct BufferArea),IPC_CREAT);
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
    ShmAddr->num[0] = 0;
    ShmAddr->num[1] = 0;
    printf("ShmAddr:%d\n",ShmAddr);
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

int main(int args, char *argv[]){
    SemInit();
    ShmInit();
    unsigned char serialFlag;
    unsigned char visionFlag;
    int comport;
    pid_t pid;
    pid = fork();
    if(pid == 0){
        execl("./Serial/Serial","Serial","1");
        perror( "execl" );
        printf("test%d\n",getpid());
        return 0;
    }
    pid = fork();
    if(pid == 0){
        execl("./Vision/Vision","Vision");
        perror( "execl" );
        printf("test%d\n",getpid());
        return 0;
    }
    while(1){
    	Semaphore_P(1);
    	//printf("maintest\n");
    	visionFlag = ShmAddr->num[1];
    	Semaphore_V(1);
    	if(visionFlag == 1){
    		printf("Send text\n");
    		pid = fork();
    		if(pid == 0){
    			execl("./Sockect/Sockect","Sockect","15528015967","4263");
    			perror( "execl" );
    			printf("test%d\n",getpid());
    			return 0;
    		}
    	}
    }
}
