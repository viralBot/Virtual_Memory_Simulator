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
#include <limits.h>

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

typedef struct message2
{
    long type;
    struct msg_2
    {
        int pid;
        int msg_type;
    } msg_2;
} message2;

typedef struct message3
{
    long type;
    struct msg_3
    {
        int pageorframe;
        int pid;
    } msg_3;
} message3;

FILE *fp;

void print(char *str)
{
    // usleep(100000);
    fp = fopen("result.txt", "a");
    fprintf(fp, "%s", str);
    fflush(fp);
    fclose(fp);
    // sleep(1);
    printf("%s", str);
}

int main(int argc, char *argv[])
{
    int tim = 0;
    // FILE *fptr;
    fp = fopen("result.txt", "w");
    fclose(fp);

    struct sembuf pop = {0, -1, 0};
    struct sembuf vop = {0, 1, 0};

    if (argc != 5)
    {
        printf("Usage: %s <Message Queue 2 ID> <Message Queue 3 ID> <Shared Memory 1 ID> <Shared Memory 2 ID>\n", argv[0]);
        exit(1);
    }

    int msgid2 = atoi(argv[1]);
    int msgid3 = atoi(argv[2]);
    int shmid1 = atoi(argv[3]);
    int shmid2 = atoi(argv[4]);

    message2 msg2;
    message3 msg3;

    SM1 *sm1 = (SM1 *)shmat(shmid1, NULL, 0);
    int *sm2 = (int *)shmat(shmid2, NULL, 0);

    key_t key;
    key = ftok(".", 11);
    int semid5 = semget(key, 1, IPC_CREAT | 0666);

    key = ftok(".", 12);
    int semid6 = semget(key, 1, IPC_CREAT | 0666);
    char msg[100];

    while (1)
    {
        // wait for process to come
        // P(semid6);
        msgrcv(msgid3, (void *)&msg3, sizeof(msg3.msg_3), 2, 0);
        tim++;

        int i = 0;
        while (sm1[i].pid != msg3.msg_3.pid)
        {
            i++;
        }
        memset(msg, 0, sizeof(msg));
        sprintf(msg, "Global Ordering - (tim %d, Process %d, Page %d)\n", tim, msg3.msg_3.pid, msg3.msg_3.pageorframe);
        print(msg);

        // printf("Global Ordering - (tim %d, Process %d, Page %d)\n", tim, msg3.msg_3.pid, msg3.msg_3.pageorframe);
        // printf("page table: %d %d %d\n", sm1[i].pagetable[0][0], sm1[i].pagetable[0][1], sm1[i].pagetable[0][2]);

        // check if the requested page is in the page table of the process with that pid

        int page = msg3.msg_3.pageorframe;
        if (sm1[i].pagetable[page][0] != -1 && sm1[i].pagetable[page][1] == 1)
        {
            // page there in memory and valid, return frame number
            sm1[i].pagetable[page][2] = tim;
            msg3.msg_3.pageorframe = sm1[i].pagetable[page][0];
            msg3.type = 1;
            msgsnd(msgid3, (void *)&msg3, sizeof(msg3.msg_3), 0);
            // V(semid5);
        }
        else if (page == -9)
        {
            // process is done
            // free the frames
            memset(msg, 0, sizeof(msg));
            sprintf(msg, "\t\tProcess %d completed\n\t\tNumber of page faults: %d, Number of illegal accesses: %d\n", msg3.msg_3.pid, sm1[i].page_faults, sm1[i].access_illegal);
            print(msg);
            for (int j = 0; j < sm1[i].pages_req; j++)
            {
                if (sm1[i].pagetable[j][0] != -1)
                {
                    sm2[sm1[i].pagetable[j][0]] = 1;
                    sm1[i].pagetable[j][0] = -1;
                    sm1[i].pagetable[j][1] = 0;
                    sm1[i].pagetable[j][2] = INT_MAX;
                }
            }
            msg2.type = 1;
            msg2.msg_2.pid = msg3.msg_3.pid;
            msg2.msg_2.msg_type = 2;
            printf("Process %d has finished 1\n", msg2.msg_2.pid);
            msgsnd(msgid2, (void *)&msg2, sizeof(msg2.msg_2), 0);
        }

        else if (page >= sm1[i].pages_req)
        {
            // illegal page number
            // ask process to kill themselves
            msg3.msg_3.pageorframe = -2;
            msg3.type = 1;
            msgsnd(msgid3, (void *)&msg3, sizeof(msg3.msg_3), 0);
            // V(semid5);
            memset(msg, 0, sizeof(msg));
            sprintf(msg, "Invalid Page Reference - (Process %d, Page %d)\n", i + 1, page);
            print(msg);
            sm1[i].access_illegal++;
            memset(msg, 0, sizeof(msg));
            sprintf(msg, "\t\tProcess %d terminated on illegal access\n\t\tNumber of page faults: %d, Number of illegal accesses: %d\n", msg3.msg_3.pid, sm1[i].page_faults, sm1[i].access_illegal);
            print(msg);

            printf("Invalid Page Reference - (Process %d, Page %d)\n", i + 1, page);

            // free the frames
            for (int j = 0; j < sm1[i].pages_req; j++)
            {
                if (sm1[i].pagetable[j][0] != -1)
                {
                    sm2[sm1[i].pagetable[j][0]] = 1;
                    sm1[i].pagetable[j][0] = -1;
                    sm1[i].pagetable[j][1] = 0;
                    sm1[i].pagetable[j][2] = 1e9;
                }
            }

            msg2.type = 1;
            msg2.msg_2.pid = msg3.msg_3.pid;
            msg2.msg_2.msg_type = 2;
            
            printf("Process %d has been terminated 2\n", msg2.msg_2.pid);
            msgsnd(msgid2, (void *)&msg2, sizeof(msg2.msg_2), 0);
        }
        else
        {
            // page fault
            // ask process to wait
            msg3.msg_3.pageorframe = -1;
            msg3.type = 1;
            msgsnd(msgid3, (void *)&msg3, sizeof(msg3.msg_3), 0);
            // V(semid5);
            sm1[i].page_faults++;
            memset(msg, 0, sizeof(msg));
            sprintf(msg, "Page fault sequence - (Process %d, Page %d)\n", i + 1, page);
            print(msg);

            printf("Page fault sequence - (Process %d, Page %d)\n", i + 1, page);

            // Page Fault Handler (PFH)
            // check if there is a free frame in sm2
            int j = 0 ,flag=0;
            while (sm2[j] != -1)
            {
                if (sm2[j] == 1)
                {
                    sm2[j] = 0;
                    flag = 1;
                    break;
                }
                j++;
            }
            if (flag)
            {
                // free frame found
                sm1[i].pagetable[page][0] = j;
                sm1[i].pagetable[page][1] = 1;
                sm1[i].pagetable[page][2] = tim;

                msg2.type = 1;
                msg2.msg_2.pid = msg3.msg_3.pid;
                msg2.msg_2.msg_type = 1;
                printf("Process %d has been terminated 4\n", msg2.msg_2.pid);
                msgsnd(msgid2, (void *)&msg2, sizeof(msg2.msg_2), 0);
            }
            else
            {
                // no free frame
                // find the page with the least time of access
                int min = INT_MAX;
                int minpage = -1;
                for (int k = 0; k < sm1[i].pages_req; k++)
                {
                    if (sm1[i].pagetable[k][2] < min)
                    {
                        min = sm1[i].pagetable[k][2];
                        minpage = k;
                    }
                }

                sm1[i].pagetable[minpage][1] = 0;
                sm1[i].pagetable[page][0] = sm1[i].pagetable[minpage][0];
                sm1[i].pagetable[page][1] = 1;
                sm1[i].pagetable[page][2] = tim;
                sm1[i].pagetable[minpage][2] = INT_MAX;

                msg2.type = 1;
                msg2.msg_2.pid = msg3.msg_3.pid;
                msg2.msg_2.msg_type = 1;
                printf("Process %d has been terminated 3\n", msg2.msg_2.pid);
                msgsnd(msgid2, (void *)&msg2, sizeof(msg2.msg_2), 0);
            }
        }
    }
    shmdt(sm1);
    shmdt(sm2);
}