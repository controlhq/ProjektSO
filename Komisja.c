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

//indentyfikatory komunikatów dla kolejnych czlonków komisji A
#define PKA 449
#define CZ2KA 450
#define CZ3KA 451
//indentyfikatory komunikatów dla kolejnych czlonków komisji B
#define PKB 452
#define CZ2KB 453
#define CZ3KB 454


pthread_mutex_t mutexA;
//pthread_mutex_t mutexB;
pthread_cond_t cond_komunikatzpA;
pthread_cond_t cond_komunikatzwA;

int komunikatzpA=0;
int msgID;
int komunikatzwA=0;


typedef struct{ //struktura komunikatu
	long mtype;
	float ocena;
    int pidStudenta;
    int czy_zdane;
}Kom_bufor;

Kom_bufor msg;
Kom_bufor Student_przewodniczacyKA;
Kom_bufor Student_Czlonek2A;
Kom_bufor Student_Czlonek2B;


typedef struct{
    long mtype; 
    int pid_studenta;   
    int IDczlonkakomisji;    
    int i;
    int odpowiedz;  
}Pytanie;

void Sendmsg(int msgid, long mtype, int student_pid, float grade, int zdane) {
    Kom_bufor msg;
    msg.mtype = mtype;
    msg.pidStudenta = student_pid;
    msg.ocena = grade;
    msg.czy_zdane = zdane;

    // Wysyłanie wiadomości
    if (msgsnd(msgid, &msg, sizeof(Kom_bufor) - sizeof(long), IPC_NOWAIT) == -1) {
        //tutaj jak kolejka będzie przepelniona to errno zwróci EAGAIN -> zrobić do tego obsluge potem
        perror("Blad msgsnd w Komisji\n");
        exit(EXIT_FAILURE);
    }

}

void wyslij_pytanie(int msgid, long mtype, int nrpytania, int IDCZLONKA){
    Pytanie pyt;
    pyt.mtype = mtype;
    pyt.i = nrpytania;
    pyt.IDczlonkakomisji = IDCZLONKA;

    if (msgsnd(msgid, &pyt, sizeof(Pytanie) - sizeof(long), IPC_NOWAIT) == -1) {
        //tutaj jak kolejka będzie przepelniona to errno zwróci EAGAIN -> zrobić do tego obsluge potem
        perror("Blad msgsnd w KOmisji\n");
        exit(EXIT_FAILURE);
    }

}


void* KA_czlonek3(){
    //stworzenie lokalnej wiadomości
    Kom_bufor localmsg;

    pthread_mutex_lock(&mutexA);
    while (!komunikatzpA) {
        pthread_cond_wait(&cond_komunikatzpA, &mutexA);
    }
    localmsg = msg;
    pthread_mutex_unlock(&mutexA);

    

    if(localmsg.czy_zdane != 1){
        //zaczyna egzamin, zadaje pytanie itd...

    }
    else {
        //tutaj drugi condition variable, który bedzie czekał,aż przewodniczący wyśle wiadomość
        //czeka aż przewodniczacy wyśle komunikat o tym że student moze isc do komisji B i zmieni wartość komunikatzwA z powrotem na 0
        pthread_mutex_lock(&mutexA);
        while(!komunikatzwA){
            pthread_cond_wait(&cond_komunikatzwA, &mutexA);
        }
        pthread_mutex_unlock(&mutexA);
       
    }


    pthread_exit(NULL);
}

void* KA_czlonek2(){
    Kom_bufor localmsg;

    pthread_mutex_lock(&mutexA);
    while (!komunikatzpA) {
        pthread_cond_wait(&cond_komunikatzpA, &mutexA);
    }
    localmsg = msg;
    pthread_mutex_unlock(&mutexA);

    if(localmsg.czy_zdane != 1){
        //zaczyna egzamin, zadaje pytanie itd...
    }
    else {
        //tutaj drugi condition variable, który bedzie czekał,aż przewodniczący wyśle wiadomość
        //czeka aż przewodniczacy wyśle komunikat i zmieni wartość komunikatzwA z powrotem na 0
        pthread_mutex_lock(&mutexA);
        while(!komunikatzwA){
            pthread_cond_wait(&cond_komunikatzwA, &mutexA);
        }
        pthread_mutex_unlock(&mutexA);
       
    }


    pthread_exit(NULL);
}

/*void* KB_czlonek3(){
    


    pthread_exit(NULL);
}

void* KB_czlonek2(){
    


    pthread_exit(NULL);
}
*/

int main(){
    srand(time(NULL));
    key_t kluczk;

    if((kluczk = ftok(".", 'C')) == -1)
    {
        perror("Blad ftok dla kolejki komunikatow w Komisji\n");
        exit(EXIT_FAILURE);
    }

    msgID=msgget(kluczk,IPC_CREAT|0666);
    if(msgID==-1)
	{
        perror("Blad tworzenia kolejki komunikatow w studencie \n");
        exit(EXIT_FAILURE);
    }


    switch(fork())
    {
        case -1:
            perror("Blad przy wykonaniu forka w Komisji\n");
            exit(EXIT_FAILURE);
        case 0: // komisja B
        /*
        //tworze czlonkow komisji -> przewodniczacy to wątek główny
        pthread_t KBczlonek2, KBczlonek3;
        pthread_mutex_init(&mutexB,NULL);

        if(pthread_create(&KBczlonek2,NULL, &KB_czlonek2, NULL) != 0){
            perror("Blad przy inicjowaniu watku w Komisji B\n");
            exit(EXIT_FAILURE);
        }

        if(pthread_create(&KBczlonek3,NULL, &KB_czlonek3, NULL) != 0){
            perror("Blad przy inicjowaniu watku w Komisji B\n");
            exit(EXIT_FAILURE);
        }
        //a tutaj wykonuje instrukcje przewodniczący

        printf("Jestem przewodniczacym komisji B i rozpoczynam egzamin\n");
        
        sleep(3);
        
        printf("Jestem przewodniczacym komisji B i koncze egzamin\n");


        //tutaj przewodniczacy kończy swoje obowiązki i czeka, aż osoby z komisji pójdą do domu
        if(pthread_join(KBczlonek2, NULL) != 0){
            perror("Blad przy joinie czlonka 2 komisji B\n");
            exit(EXIT_FAILURE);
        }
        if(pthread_join(KBczlonek3, NULL) != 0){
            perror("Blad przy joinie czlonka 3 komisji B\n");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_destroy(&mutexB);

        //Komisja B kończy działanie
        */

        exit(EXIT_SUCCESS);







        default: // komisja A jako rodzic bo zaczyna caly proces

        //tworze czlonkow komisji -> przewodniczacy to wątek główny
        pthread_t KAczlonek2, KAczlonek3;
        pthread_mutex_init(&mutexA,NULL);
        pthread_cond_init(&cond_komunikatzpA,NULL);
        pthread_cond_init(&cond_komunikatzwA,NULL);


        if(pthread_create(&KAczlonek2,NULL, &KA_czlonek2, NULL) != 0){
            perror("Blad przy inicjowaniu watku w Komisji A\n");
            exit(EXIT_FAILURE);
        }

        if(pthread_create(&KAczlonek3,NULL, &KA_czlonek3, NULL) != 0){
            perror("Blad przy inicjowaniu watku w Komisji A\n");
            exit(EXIT_FAILURE);
        }
        //a tutaj wykonuje instrukcje przewodniczący





        pthread_mutex_lock(&mutexA);
        //odbiera wiadomość od studenta
        if (msgrcv(msgID, &msg, sizeof(Kom_bufor) - sizeof(long), 5, 0) == -1) {
            perror("Blad msgrcv w komisji\n");
            exit(EXIT_FAILURE);
        }
        //przewodniczący przepisuje ocene
        int student_pid = msg.pidStudenta;
        komunikatzpA =1;
        pthread_mutex_unlock(&mutexA);
        pthread_cond_broadcast(&cond_komunikatzpA); // sygnalizuje wszystkim wątkom żeby sprawdzić warunek


        if(msg.czy_zdane == 1){
            //wysylam mu komunikat, ze może iść do komisji B
            pthread_mutex_lock(&mutexA);
            Sendmsg(msgID,student_pid,student_pid,msg.ocena,1);
            komunikatzwA =1;
            pthread_mutex_unlock(&mutexA);
            pthread_cond_broadcast(&cond_komunikatzwA);

        }else{
            //rozpoczynam egzamin
            //odbierz wiadomość od studenta o gotowości przystapienia do egzaminu
            if (msgrcv(msgID, &Student_przewodniczacyKA, sizeof(Kom_bufor) - sizeof(long), 5, 0) == -1) {
        
                perror("Blad msgrcv w komisji\n");
                exit(EXIT_FAILURE);
            }

            //Przygotowuje pytanie
            int PPopoz = rand()%6 +5; // Symulacja przygotowywania i(losowania) pytania
            //sleep(PPopoz);


            //wysyla pytanie do studenta
            wyslij_pytanie(msgID,Student_przewodniczacyKA.pidStudenta,PPopoz, PKA);

            //czyta odpowiedź, ocenia ją i czeka, aż inni członkowie ocenią, potem sprawdza, czy nie ma
            //oceny 2 i liczy średnią a nastepnie wysyla wiadomosc z oceną do studenta




        }


        //tutaj przewodniczacy kończy swoje obowiązki i czeka, aż osoby z komisji pójdą do domu
        if(pthread_join(KAczlonek2, NULL) != 0){
            perror("Blad przy joinie czlonka 2 komisji A\n");
            exit(EXIT_FAILURE);
        }
        if(pthread_join(KAczlonek3, NULL) != 0){
            perror("Blad przy joinie czlonka 3 komisji A\n");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_destroy(&mutexA);
        pthread_cond_destroy(&cond_komunikatzpA);
        pthread_cond_destroy(&cond_komunikatzwA);
        
        // Przewodniczący komisji A czeka, aż komisja B opuści budynek zanim go zamknie
        if (wait(NULL) == -1) {
            perror("Blad przy oczekiwaniu na proces potomny");
            exit(EXIT_FAILURE);
        }
    
    }
    
    return 0;
}