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

typedef struct message1
{
    long type;
    struct msg_1{
        int pid;
    }msg_1;
} message1;

typedef struct message3
{
    long type;
    struct msg{
        int pageorframe;
        int pid;
    }msg;
} message3;

typedef struct SM1
{
    int semid;      // semaphore id
    int pid;         // process id
    int pages_req;          // number of required pages
    int frames_alloc;          // number of frames allocated
    int pagetable[1000][3]; // page table
    int page_faults;
    int access_illegal;
} SM1;

int main(int argc, char *argv[])
{
    struct sembuf pop = {0, -1, 0};
    struct sembuf vop = {0, 1, 0};

    if (argc != 6)
    {
        printf("Usage: %s <Reference String> <Virtual Address Space size> <Physical Address Space size> <# Processes> <Shared Memory SM1>\n", argv[0]);
        exit(1);
    }

    int msgid1 = atoi(argv[2]);
    int msgid3 = atoi(argv[3]);
    int k = strlen(argv[4]);
    int shmid1 = atoi(argv[5]);
    char *refstr = (char*)malloc((strlen(argv[1])+1)*sizeof(char));
    strcpy(refstr, argv[1]);

    printf("Process has started\n");
    key_t key = ftok(".", 4);
    int semid = semget(key, 1, IPC_CREAT | 0666);

    int sem_array[k];

    for(int i = 0;i<k;i++){
        key_t key = ftok(".", 100+i);
        sem_array[i] = semget(key, 1, IPC_CREAT | 0666);
    }

    SM1 *sm1 = (SM1 *)shmat(shmid1, NULL, 0);

    int pid = getpid();
    int curr = 0;

    while(sm1[curr].pid != pid){
        curr++;
    }

    message1 msg1;
    msg1.type = 1;
    msg1.msg_1.pid = pid;

    key = ftok(".", 11);
    int semid5 = semget(key, 1, IPC_CREAT | 0666);

    key = ftok(".", 12);
    int semid6 = semget(key, 1, IPC_CREAT | 0666);

    // send pid to ready queue
    printf("pid: %d\n", pid);   
    msgsnd(msgid1, (void *)&msg1, sizeof(msg1.msg_1), 0);

    // wait till scheduler signals to start
    P(sem_array[curr]);

    // send the reference string to the scheduler, one character at a time
    int i = 0;
    while (refstr[i] != '\0')
    {
        message3 msg3;
        msg3.msg.pid = pid;
        int j = 0;
        // extract the page number from the reference string going character by character
        while (refstr[i] != '.' && refstr[i] != '\0')
        {
            j = (refstr[i] - '0') + j * 10  ;
            i++;
        }
        i++;
        msg3.msg.pageorframe = j;
        msg3.type = 2;
        msgsnd(msgid3, (void *)&msg3, sizeof(msg3.msg), 0);
        // V(semid6);

        // P(semid5);
        // wait for the mmu to allocate the frame type is pid of the process
        msgrcv(msgid3, (void *)&msg3, sizeof(msg3.msg),1, 0);

        // check the validity of the frame number
        if (msg3.msg.pageorframe == -2)
        {
            printf("Process %d: ", pid);
            printf("Illegal Page Number\nTerminating\n");
            exit(1);
        }
        else if (msg3.msg.pageorframe == -1)
        {
            printf("Process %d: ", pid);
            printf("Page Fault\nWaiting for page to be loaded\n");
            // wait for the page to be loaded
            // scheduler will signal when the page is loaded
            P(sem_array[curr]);
            continue;
        }
        else
        {
            printf("Process %d: ", pid);
            printf("Frame %d allocated\n", msg3.msg.pageorframe);
        }
    }

    // send the termination signal to the mmu
    printf("Process %d: ", pid);
    printf("Received all frames, now terminating\n");
    message3 msg3;
    msg3.type = 2;
    msg3.msg.pid = pid;
    msg3.msg.pageorframe = -9;

    msgsnd(msgid3, (void *)&msg3, sizeof(msg3.msg), 0);
    // V(semid6);

    shmdt(sm1);

    return 0;
}
