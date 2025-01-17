#define _GNU_SOURCE
#include <sys/ipc.h>
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
#include <sys/types.h>
#include <time.h>
#include <pthread.h>


void* KomisjaA(){
    printf("Testuje watki\n");
    sleep(3);
    printf("Koncze watek\n");
}

void* KomisjaB(){
    printf("Testuje watki\n");
    sleep(3);
    printf("Koncze watek\n");
}


int main(){

    pid_t mainprog = getpid();

    //switch(fork())
    //{
    //    case -1:
    //       perror("Blad przy wykonaniu forka w Komisji\n");
    //        exit(EXIT_FAILURE);
   //     case 0: // komisja B
//
//
    //    default: // komisja A jako rodzic bo zaczyna caly proces
//
//
   // }







    pthread_t t1, t2;
    if(pthread_create(&t1,NULL, &KomisjaA, NULL) != 0){
        return 1;
    }
    if(pthread_create(&t2,NULL, &KomisjaA, NULL) != 0){
        return 2;
    }

    printf("Tutaj witam z wątku głównego\n");



    
    if(pthread_join(t1, NULL) != 0){
        return 3;
    }
    if(pthread_join(t2, NULL) != 0){
        return 4;
    }


    return 0;
}