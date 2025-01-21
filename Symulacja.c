#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <stdlib.h>                         
#include <stdio.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <time.h>

pid_t mainprogSYM;
void sigint_handler(int sig){
    if(mainprogSYM == getpid()){
        kill(0, SIGINT);
        while(wait(NULL)>0);
        exit(0);
    }else{
        exit(EXIT_SUCCESS);
    }
    
}

int main(){
   
    pid_t pid1, pid2, pid3;
    mainprogSYM=getpid();
    //obsluga sygnalu sigint
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags=0;
    sigaction(SIGINT,&act,0);

    // Tworzymy pierwszy proces dla programu ./Student
    pid1 = fork();
    if (pid1 == -1) {
        perror("Fork failed");
        return 1;
    }
    if (pid1 == 0) {
        // Proces dziecka wykonuje ./Student
        execl("./Student", "Student", (char *)NULL);
        perror("Exec failed for Student");
        return 1;
    }

    // Tworzymy drugi proces dla programu ./Dziekan
    pid2 = fork();
    if (pid2 == -1) {
        perror("Fork failed");
        return 1;
    }
    if (pid2 == 0) {
        // Proces dziecka wykonuje ./Dziekan
        sleep(5);
        execl("./Dziekan", "Dziekan", (char *)NULL);
        perror("Exec failed for Dziekan");
        return 1;
    }

    // Tworzymy trzeci proces dla programu ./Komisja
    pid3 = fork();
    if (pid3 == -1) {
        perror("Fork failed");
        return 1;
    }
    if (pid3 == 0) {
        sleep(6);
        // Proces dziecka wykonuje ./Komisja
        execl("./Komisja", "Komisja", (char *)NULL);
        perror("Exec failed for Komisja");
        return 1;
    }

    // Proces macierzysty czeka na zakończenie wszystkich procesów
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    waitpid(pid3, NULL, 0);

    return 0;
}