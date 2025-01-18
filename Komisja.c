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


void* KA_czlonek3(){
    printf("Jestem w komisji A, czlonek 3 i rozpoczynam prace\n");
    sleep(3);
    printf("Jestem w komisji A, czlonek 3 i koncze prace\n");
}

void* KA_czlonek2(){
    printf("Jestem w komisji A, czlonek 2 i rozpoczynam prace\n");
    sleep(3);
    printf("Jestem w komisji A, czlonek 2 i koncze prace\n");
}

void* KB_czlonek3(){
    printf("Jestem w komisji B, czlonek 3 i rozpoczynam prace\n");
    sleep(3);
    printf("Jestem w komisji B, czlonek 3 i koncze prace\n");
}

void* KB_czlonek2(){
    printf("Jestem w komisji B, czlonek 2 i rozpoczynam prace\n");
    sleep(3);
    printf("Jestem w komisji B, czlonek 2 i koncze prace\n");
}

int main(){


    switch(fork())
    {
        case -1:
            perror("Blad przy wykonaniu forka w Komisji\n");
            exit(EXIT_FAILURE);
        case 0: // komisja B

        //tworze czlonkow komisji -> przewodniczacy to wątek główny
        pthread_t KBczlonek2, KBczlonek3;

        if(pthread_create(&KBczlonek2,NULL, &KB_czlonek2, NULL) != 0){
        return 1;
        }

        if(pthread_create(&KBczlonek3,NULL, &KB_czlonek3, NULL) != 0){
            return 2;
        }
        //a tutaj wykonuje instrukcje przewodniczący

        printf("Jestem przewodniczacym komisji B i rozpoczynam egzamin\n");
        sleep(3);
        
        printf("Jestem przewodniczacym komisji B i koncze egzamin\n");


        //tutaj przewodniczacy kończy swoje obowiązki i czeka, aż osoby z komisji pójdą do domu
        if(pthread_join(KBczlonek2, NULL) != 0){
        return 3;
        }
        if(pthread_join(KBczlonek3, NULL) != 0){
            return 4;
        }
        exit(EXIT_SUCCESS);

        default: // komisja A jako rodzic bo zaczyna caly proces

        //tworze czlonkow komisji -> przewodniczacy to wątek główny
        pthread_t KAczlonek2, KAczlonek3;

        if(pthread_create(&KAczlonek2,NULL, &KA_czlonek2, NULL) != 0){
        return 1;
        }

        if(pthread_create(&KAczlonek3,NULL, &KA_czlonek3, NULL) != 0){
            return 2;
        }
        //a tutaj wykonuje instrukcje przewodniczący
        printf("Jestem przewodniczacym komisji A i rozpoczynam egzamin\n");
        sleep(3);

        printf("Jestem przewodniczacym komisji A i koncze egzamin\n");
        //tutaj przewodniczacy kończy swoje obowiązki i czeka, aż osoby z komisji pójdą do domu
        if(pthread_join(KAczlonek2, NULL) != 0){
        return 3;
        }
        if(pthread_join(KAczlonek3, NULL) != 0){
            return 4;
        }
        
    
    }
    printf("BAOBAB\n");
    // Przewodniczący komisji A czeka, aż komisja B opuści budynek zanim go zamknie
    wait(NULL);
    return 0;
}