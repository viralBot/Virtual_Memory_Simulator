#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

struct sembuf pop = {0, -1, 0};
struct sembuf vop = {0, 1, 0};
typedef struct message1
{
    long type;
    struct msg_1{
        int pid;
    }msg_1;
} message1;

typedef struct message2
{
    long type;
    struct msg{
        int pid;
        int msg_type;
    } msg;
} message2;

typedef struct SM1
{
    int semid;              // semaphore id
    int pid;                // process id
    int pages_req;          // number of required pages
    int frames_alloc;       // number of frames allocated
    int pagetable[1000][3]; // page table
    int page_faults;
    int access_illegal;
} SM1;

int main(int argc, char *argv[])
{

    if (argc != 5)
    {
        printf("Usage: %s <Message Queue 1 ID> <Message Queue 2 ID> <# Processes> <Shared Memory SM1>\n", argv[0]);
        exit(1);
    }

    int msgid1 = atoi(argv[1]);
    int msgid2 = atoi(argv[2]);
    int k = atoi(argv[3]);
    int shmid1 = atoi(argv[4]);

    key_t key;

    // create a new semaphore with initial value 1
    // key = ftok(".", 23);
    // int semid7 = semget(key, 1, IPC_CREAT | 0666);
    // semctl(semid7, 0, SETVAL, 0);
  

    key = ftok(".", 7);
    int semid4 = semget(key, 1, IPC_CREAT | 0666);

    SM1 *sm1 = (SM1 *)shmat(shmid1, NULL, 0);

    int sem_array[k];

    for (int i = 0; i < k; i++)
    {
        key_t key = ftok(".", 100 + i);
        sem_array[i] = semget(key, 1, IPC_CREAT | 0666);
    }

    message1 msg1;
    message2 msg2;
    // msgrcv(msgid1, (void *)&msg1, sizeof(message1), 0, 0);

    // printf("\t\tScheduling process %d\n", msg1.pid);

    while (k > 0)
    {
        // wait for processes to come
        // P(semid7);
        msgrcv(msgid1, (void *)&msg1, sizeof(msg1.msg_1), 1, 0);

        printf("\t\tScheduling process %d\n", msg1.msg_1.pid);

        int curr = 0;
        while (sm1[curr].pid != msg1.msg_1.pid)
        {
            curr++;
        }

        // signal process to start
        V(sem_array[curr]);

        // wait for mmu
        msgrcv(msgid2, (void *)&msg2, sizeof(msg2.msg), 1, 0);

        // check the type of message
      
         if (msg2.msg.msg_type == 1)
        {
            printf("\t\tProcess %d added to end of queue\n", msg2.msg.pid);
            msg1.msg_1.pid = msg2.msg.pid;
            msg1.type = 1;
            msgsnd(msgid1, (void *)&msg1, sizeof(msg1.msg_1), 0);
            // V(semid7);
           
        }
         else if (msg2.msg.msg_type == 2)
        {
            printf("\t\tProcess %d terminated\n", msg2.msg.pid);
            k--;
        }

    }

    printf("\t\tScheduler terminated\n");

    // signal master on semid4
    V(semid4);

    // shmdt(sm1);

    return 0;
}