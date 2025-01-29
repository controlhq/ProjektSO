#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include "Funkcje_IPC.h"
#include "Funkcje_losujace.h"
#include <stdlib.h>

pid_t mainprogstud; // zmienna do przechowywania pidu glownego programu w celu obsluzenia sygnału

typedef struct{
    long mtype;
    float ocena;
}Wiadomosc_zocena;

typedef struct{
    long mtype;
}Krotkie_powiadomienie;

typedef struct{ //struktura komunikatu
	long mtype;
    float ocena_koncowa;
    int pidStudenta;
    int czy_zdane;
    int index_studenta;
    int id_studenta;
    int zdaje_czy_pytania; // jak zdaje to 1, jak pytania to 0
}Kom_bufor;

typedef struct{
    long mtype; 
    int pid_studenta;   
    int IDczlonkakomisji; 
    int pytanie;   
    int odpowiedz;  
}Pytanie;

void sigint_handler(int sig){
    if(mainprogstud == getpid()){
        printf("Odebrano sygnal intsig w tworzenie studentow: %d\n",sig);
        while(wait(NULL)>0);
        return;
    }
}


int main()
{
    
    mainprogstud = getpid();
    unsigned int seedT = time(NULL) ^ mainprogstud;

    //obsluga sygnalu sigint
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags=0;
    sigaction(SIGINT,&act,0);
    
    utworz_klucze();

    shmID = shmget(kluczm, sizeof(SHARED_MEMORY), IPC_CREAT | IPC_EXCL | 0666);
    if (shmID==-1){
        perror("Blad przy tworzeniu pamieci dzielonej w Tworzenie_studentow\n");
        exit(EXIT_FAILURE);
    }

    shm_ptr =(SHARED_MEMORY*)shmat(shmID, NULL, 0);
    if (shm_ptr == (void *)-1){
        perror("Blad przy dolaczaniu pamieci dzielonej w Tworzenie_studentow\n");
        koniec();
        exit(EXIT_FAILURE);
    }

    msgID=msgget(kluczk,IPC_CREAT | IPC_EXCL |0666);
    if(msgID==-1)
	{
        perror("Blad tworzenia kolejki komunikatow w Tworzenie_studentow\n");
        koniec();
        exit(EXIT_FAILURE);
    }

    semID=semget(kluczs, 9, IPC_CREAT| IPC_EXCL | 0666); 
    if(semID==-1){
        perror("Blad tworzenia semaforow w Tworzenie_studentow\n");
        exit(EXIT_FAILURE);
    }

    //to jest mutex do operacji na sekcji krytycznej 
    init_semaphore(0,1);
    //to jest semafor, który czeka na informacje o kierunku zdającym egzamin od dziekana
    init_semaphore(1,0);
    //tutaj kolejka do komisji A (maks 3 osoby)
    init_semaphore(2,3);
    //to jest kolejka do odpowiedzi w pokoju komisji A
    init_semaphore(3,1);
    init_semaphore(4,0);
    //kolejka do komisji B (3os)
    init_semaphore(5,3);
    //kolejka do odpowiedzi lub pytan w komisji B
    init_semaphore(6,1);
    //SEMAFOR DO ZAPISU LOGÓW
    init_semaphore(7,1);
    init_semaphore(8,1);

    int lsKierunek_1 = losuj_ilosc_studentow(&seedT); //losuje ilości studentów na poszczególnych kierunkach
    int lsKierunek_2 = losuj_ilosc_studentow(&seedT);
    int lsKierunek_3 = losuj_ilosc_studentow(&seedT);
    int lsKierunek_4 = losuj_ilosc_studentow(&seedT);
    int lsKierunek_5 = losuj_ilosc_studentow(&seedT);
    int liczba_studentow = lsKierunek_1 + lsKierunek_2 + lsKierunek_3 + lsKierunek_4 + lsKierunek_5;

    semafor_wait(semID,0);
    shm_ptr->flagakomA=0;
    shm_ptr->Liczba_studentow_dokomB=0;
    shm_ptr->students_count=0; //inicjalizacja counta na 0
    shm_ptr->index_stud =0;
    shm_ptr->ilosc_osob_przepisujacych=0;
    shm_ptr->ilosc_studentow=liczba_studentow;
    shm_ptr->ilosc_studentow_na_wybranym_kierunku=lsKierunek_2; // bo zakładam sztywno, że kierunek 2 (tak bylo napisane w opisie tematu)
    semafor_signal(semID,0);

    //wyslij wiadomosc dziekanowi, ze moze startować
    Krotkie_powiadomienie LS;
    LS.mtype=40;
    if((msgsnd(msgID,&LS,sizeof(Krotkie_powiadomienie)-sizeof(long),0)) == -1){
        semafor_wait(semID,7);
        handle_msgsnd_error_with_logging(Zapiszlog);
        semafor_signal(semID,7);
        exit(EXIT_FAILURE);
    }

    if (liczba_studentow > MAX_STUDENTOW)
    {
        printf("Przekroczona maksymalna liczba studentow\n");
        koniec();
        exit(EXIT_FAILURE);
    }

    int student_id = 1;
    for (int kierunek = 1; kierunek <= K; kierunek++) {
        int liczba_na_kierunku = (kierunek == 1) ? lsKierunek_1 :
                                 (kierunek == 2) ? lsKierunek_2 :
                                 (kierunek == 3) ? lsKierunek_3 :
                                 (kierunek == 4) ? lsKierunek_4 : lsKierunek_5;

        for (int i = 0; i < liczba_na_kierunku; i++) {

            //int opoznienie = rand() % 6 +5; // studenci pojawiaja sie w losowych momentach czasu(5-10sek)
            //sleep(opoznienie);
            
            pid_t pid = fork();
            if (pid < 0) {
                perror("Blad forka\n");
                koniec();
                exit(EXIT_FAILURE);
            } else if (pid == 0) {

                char semID_str[16], msgID_str[16], shmID_str[16], studentID_str[16], kierunek_str[16];

                sprintf(semID_str, "%d", semID);
                sprintf(msgID_str, "%d", msgID);
                sprintf(shmID_str, "%d", shmID);
                sprintf(studentID_str, "%d", student_id);
                sprintf(kierunek_str, "%d", kierunek);

                execl("./Student", "Student", semID_str, msgID_str, shmID_str, studentID_str, kierunek_str, (char *)NULL);
                perror("execl() error\n");
                exit(EXIT_FAILURE);

            } else {
                // Proces macierzysty
                student_id++;
            }
        }
    }
    
    
    while (wait(NULL) > 0); // czekam na zakończenie wszystkich procesów potomnych
    
    return 0;
}
