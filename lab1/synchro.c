#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <stdlib.h>

//2个生产者，每个生产者重复6次
#define pro_max 2
#define pro_rep 6
 
//3个消费者，每个消费者重复4次
#define con_max 3
#define con_rep 4
 
//缓冲区为3
#define buffer_len 11
#define buffer_cnt 3
 
#define MYBUF_LEN (sizeof(struct mybuffer))
 
#define SHM_MODE 0600//可读可写
#define SEM_ALL_KEY 1234
#define SHM_ALL_KEY 1235
#define SEM_EMPTY 0
#define SEM_FULL 1
#define SEM_MUTEX 2

//缓冲区的结构
struct mybuffer
{
    char str[buffer_cnt][buffer_len];
    int head;
    int tail;
};

// 随机得到一个字符串
char *ca()
{
    static char le[buffer_len];
    memset(le, 0, sizeof(le));
    int n = rand() %10 + 1;
    for (int i = 0; i < n; i++)
        le[i] = (char)(rand() % 26 + 'A');
    return le;
}


// 随机得10以内的整数
int myrandom(int mod)
{
    int sand;
    sand = rand() % mod;
    return sand;
}
// P 操作
void P(int sem_id, int sem_num)
{
    struct sembuf mut;
    mut.sem_num = sem_num;
    // 信号量索引
    mut.sem_flg = 0;
    // 访问标志
    mut.sem_op = -1;
    // 信号量操作值
    semop(sem_id, &mut, 1);
    //一次需进行的操作的数组sembuf中的元素数为1
}

// V 操作
void V(int sem_id, int sem_num)
{
    struct sembuf mut;
    mut.sem_num = sem_num;
    // 信号量索引
    mut.sem_flg = 0;
    // 访问标志
    mut.sem_op = 1;
    // 信号量操作值
    semop(sem_id, &mut, 1);
    //一次需进行的操作的数组sembuf中的元素数为1
}


int main(int argc, char *argv[])
{
    struct timespec time1 = {0, 0};
    struct timespec time2 = {0, 0};
    int shm_id, sem_id;
    int pro_num = 0, con_num = 0;
    // 生产者数量、消费者数量

    struct mybuffer *shmptr;
    // 缓冲区指针
    pid_t pid_p, pid_c;
    // 生产者、消费者pid

    sem_id = semget(SEM_ALL_KEY, 3, IPC_CREAT | SHM_MODE);
    // 创建一个信号量的集合，信号量数为3
    if (sem_id < 0)
    {
        printf("Failed to Create!\n");
        exit(1);
    }
    // 创建信号量失败

    semctl(sem_id, SEM_EMPTY, SETVAL, buffer_cnt);
    semctl(sem_id, SEM_FULL, SETVAL, 0);
    semctl(sem_id, SEM_MUTEX, SETVAL, 1);
    // 申请共享内存区，返回共享内存区标识

    shm_id = shmget(SHM_ALL_KEY, MYBUF_LEN, SHM_MODE | IPC_CREAT);
    if (shm_id < 0)
    {
        printf("Apply for Shared Memory Failed!\n");
        exit(1);
    }
    // 申请共享内存失败

    shmptr = shmat(shm_id, 0, 0);
    if ((void *)-1 == shmptr)
    {
        printf("Attach Failed!\n");
        exit(1);
    }
    memset(shmptr, 0, sizeof(struct mybuffer));
    //将共享段与进程之间解除连接
    shmdt(shmptr);

    // 2个生产者进程
    for (pro_num = 0; pro_num < pro_max; pro_num++)
    {
        pid_p = fork();
        if (pid_p < 0)
        {
            printf("Fork Failed!\n");
            exit(1);
        }
        // 创建子进程失败

        if (pid_p == 0) // 子进程，创建生产者
        {
            srand((unsigned int)getpid());
            shmptr = shmat(shm_id, 0, 0);
            if ((void *)-1 == shmptr)
            {
                printf("Attach Failed!\n");
                exit(1);
            }
            for (int cnt = 0; cnt < pro_rep; cnt++)
            {              
                clock_gettime(CLOCK_MONOTONIC_RAW, &time1);
                P(sem_id, SEM_EMPTY);
                P(sem_id, SEM_MUTEX);

                usleep(myrandom(1e6));
                // 随机等待一段时间
                strcpy(shmptr->str[shmptr->tail], ca());

                printf("Pro %d push %-10s ", pro_num, shmptr->str[shmptr->tail]);

                shmptr->tail = (shmptr->tail + 1) % buffer_cnt;
                for (int cnt = 0; cnt < buffer_cnt; cnt++)
                        printf("|%-10s", shmptr->str[cnt]);
                printf("|");

                fflush(stdout);
                V(sem_id, SEM_FULL);
                V(sem_id, SEM_MUTEX);
                
                clock_gettime(CLOCK_MONOTONIC_RAW, &time2);
                double ti = (time2.tv_sec - time1.tv_sec)*1000 + (time2.tv_nsec - time1.tv_nsec)/1000000;
                printf(" running time:%lf ms\n", ti);
            }
            // 将共享段与进程之间解除连接
            shmdt(shmptr);
            exit(0);
        }
    }

    // 3个消费者进程
    for (con_num = 0; con_num < con_max; con_num++)
    {
        pid_c = fork();
        if (pid_c < 0)
        {
            printf("Fork Failed!\n");
            exit(1);
        }
        // 创建子进程失败

        if (pid_c == 0) // 子进程，创建消费者
        {
            srand((unsigned int)getpid());
            shmptr = shmat(shm_id, 0, 0);
            if ((void *)-1 == shmptr)
            {
                printf("Attach Failed!\n");
                exit(1);
            }
            for (int cnt = 0; cnt < con_rep; cnt++)
            {
                clock_gettime(CLOCK_MONOTONIC_RAW, &time1);
                P(sem_id, SEM_FULL);
                P(sem_id, SEM_MUTEX);

                usleep(myrandom(1e6));
                // 随机等待一段时间
                
                printf("Con %d pop  %-10s ", pro_num, shmptr->str[shmptr->head]);
                memset(shmptr->str[shmptr->head], 0, sizeof(shmptr->str[shmptr->head]));
                shmptr->head = (shmptr->head + 1) % buffer_cnt;

                for (int cnt = 0; cnt < buffer_cnt; cnt++)
                    printf("|%-10s", shmptr->str[cnt]);
                printf("|");

                fflush(stdout);
                V(sem_id, SEM_EMPTY);
                V(sem_id, SEM_MUTEX);
                
                clock_gettime(CLOCK_MONOTONIC_RAW, &time2);
                double ti = (time2.tv_sec - time1.tv_sec)*1000 + (time2.tv_nsec - time1.tv_nsec)/1000000;
                printf(" running time:%lf ms\n", ti);
            }
            // 将共享段与进程之间解除连接
            shmdt(shmptr);
            exit(0);
        }
    }
    for (int i = 0; i < 5; i++)
    {
        wait(NULL);
    }

    //对共享内存区执行控制操作
    shmctl(shm_id, IPC_RMID, NULL); //当cmd为IPC_RMID时，删除该共享段
    semctl(sem_id, 0, IPC_RMID);
    printf("Exit.\n");
    fflush(stdout);
    exit(0);
}