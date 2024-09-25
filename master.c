#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>

#define probability 0.7

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

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

int main()
{
    struct sembuf pop, vop;
    pop.sem_num = 0;vop.sem_num = 0;
    pop.sem_flg = 0;vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;

    srand(time(0));

    int k, m, f;
    printf("Number of processes: ");
    scanf("%d", &k);
    printf("Virtual Address Space size: ");
    scanf("%d", &m);
    printf("Physical Address Space size: ");
    scanf("%d", &f);

    char msgid1_str[100], msgid2_str[100], msgid3_str[100];
    char shmid1_str[100], shmid2_str[100], shmid3_str[100];
    char k_str[100];

    sprintf(k_str, "%d", k);

    // shared memory for per process page table
    key_t key = ftok(".", 1);
    int shmid1 = shmget(key, k * sizeof(SM1), IPC_CREAT | 0666);
    SM1 *sm1 = (SM1 *)shmat(shmid1, NULL, 0);
    sprintf(shmid1_str, "%d", shmid1);

    // shared memory for list of free frames
    key = ftok(".", 2);
    int shmid2 = shmget(key, (f + 1) * sizeof(int), IPC_CREAT | 0666);
    int *sm2 = (int *)shmat(shmid2, NULL, 0);
    sprintf(shmid2_str, "%d", shmid2);

    // shared memory for process to page mapping
    key = ftok(".", 3);
    int shmid3 = shmget(key, k * sizeof(int), IPC_CREAT | 0666);
    int *sm3 = (int *)shmat(shmid3, NULL, 0);
    sprintf(shmid3_str, "%d", shmid3);

    // sm1 initialization
    for (int i = 0; i < k; i++)
    {
        sm1[i].page_faults = 0;
        sm1[i].access_illegal = 0;
        key = ftok(".", i + 100); 
        sm1[i].semid = semget(key, 1, IPC_CREAT | 0666);
        semctl(sm1[i].semid, 0, SETVAL, 0);
    }

    // Free frames list initialization
    // free -> 1, occupied -> 0, end of list -> -1
    memset(sm2, 1, f * sizeof(int));
    sm2[f] = -1;

    // sm3 initialization
    // initially no frames are allocated to any process
    memset(sm3, 0, k * sizeof(int));

    // semaphore for sched to signal master
    key = ftok(".", 7);
    int sem1 = semget(key, 1, IPC_CREAT | 0666);
    semctl(sem1, 0, SETVAL, 0);

    // Message Queue 1 for Ready Queue
    key = ftok(".", 8);
    int msgid1 = msgget(key, IPC_CREAT | 0666);
    sprintf(msgid1_str, "%d", msgid1);

    // Message Queue 2 for Scheduler
    key = ftok(".", 9);
    int msgid2 = msgget(key, IPC_CREAT | 0666);
    sprintf(msgid2_str, "%d", msgid2);

    // Message Queue 3 for Memory Management Unit
    key = ftok(".", 10);
    int msgid3 = msgget(key, IPC_CREAT | 0666);
    sprintf(msgid3_str, "%d", msgid3);

    int sched_pid, mmu_pid;

    // create Scheduler process, pass msgid1_str, msgid2_str, k_str and shmid1_str
    sched_pid = fork();
    if (sched_pid == 0)
    {
        execlp("./sched", "./sched", msgid1_str, msgid2_str, k_str, shmid1_str, NULL);
    }

    // create Memory Management Unit process, pass msgid2_str, msgid3_str, shmid1_str and shmid2_str
    mmu_pid = fork();
    if (mmu_pid == 0)
    {
        execlp("xterm", "xterm", "-T", "Memory Management Unit", "-e", "./mmu", msgid2_str, msgid3_str, shmid1_str, shmid2_str, NULL);
    }

    // generate reference strings for each process
    // int **reference = (int **)malloc(k * sizeof(int *));
    // char **reference_str = (char **)malloc(k * sizeof(char *));
    int reference[k][1000];
    char reference_str[k][1000];

    // initialize the processes
    for (int i = 0; i < k; i++)
    {
        // generate random number of pages between 1 to m
        sm1[i].pages_req = rand() % m + 1;
        sm1[i].frames_alloc = 0;
        for (int j = 0; j < m; j++)
        {
            sm1[i].pagetable[j][0] = -1;      // no frame allocated
            sm1[i].pagetable[j][1] = 0;       // invalid
            sm1[i].pagetable[j][2] = 1e9; // timestamp
        }

        int dig = 0;
        int x = rand() % (8 * sm1[i].pages_req + 1) + 2 * sm1[i].pages_req;

        // reference[i] = (int *)malloc(x * sizeof(int));

        for (int j = 0; j < x; j++)
        {
            reference[i][j] = rand() % sm1[i].pages_req; // valid page number
            int temp = reference[i][j];
            do
            {
                temp /= 10;
                dig++;
            } while (temp > 0);
            dig++;
        }
        dig++;

        // with probability = probability, corrupt the reference string with illegal page number
        for (int j = 0; j < x; j++)
        {
            double random_value = (double)rand() / (double)RAND_MAX; // Random value between 0 and 1
            if (random_value < probability)
            {
                reference[i][j] = rand() % m; // Assign a random value from 0 to m-1
            }
        }

        // reference_str[i] = (char *)malloc(dig * sizeof(char));
        // memset(reference_str[i], '\0', dig);

        for (int j = 0; j < x; j++)
        {
            char temp[100];
            memset(temp, '\0', 100);
            sprintf(temp, "%d.", reference[i][j]);
            strcat(reference_str[i], temp);
        }
    }

    // create Processes
    for (int i = 0; i < k; i++)
    {
        sleep(1);
        int pid = fork();
        if (pid)
        {
            sm1[i].pid = pid;
        }
        else
        {
            execlp("./process", "./process", reference_str[i], msgid1_str, msgid3_str, k_str, shmid1_str, NULL);
        }
    }

    // wait for Scheduler to signal
    P(sem1);

    printf("Master: All processes have completed. Quiting...\n");

    // terminate Scheduler
    kill(sched_pid, SIGINT);

    // terminate Memory Management Unit
    kill(mmu_pid, SIGINT);

    // detach and remove shared memory
    shmdt(sm1);
    shmctl(shmid1, IPC_RMID, NULL);

    shmdt(sm2);
    shmctl(shmid2, IPC_RMID, NULL);

    shmdt(sm3);
    shmctl(shmid3, IPC_RMID, NULL);

    // remove semaphore
    semctl(sem1, 0, IPC_RMID, 0);

    // remove message queues
    msgctl(msgid1, IPC_RMID, NULL);
    msgctl(msgid2, IPC_RMID, NULL);
    msgctl(msgid3, IPC_RMID, NULL);

    return 0;
}
