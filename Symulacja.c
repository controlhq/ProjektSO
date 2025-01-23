#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <stdlib.h>                         
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

int semID;
int shmID;
int msgID;
#define MAX_STUD 160

pid_t mainprogSYM;

typedef struct {            // struktura reprezentująca każdego studenta
    int id;   
    pid_t pid_studenta;     // identyfikator kazdego studenta
    int Kierunek;
    int powtarza_egzamin;   // flaga, jeśli student powtarza egzamin (1 - tak,0 - nie)
    float ocena_praktykaP;
    float ocena_praktyka2;
    float ocena_praktyka3;
    float ocena_praktykasr;
    float ocena_teoriaP;
    float ocena_teoria2;
    float ocena_teoria3;
    float ocena_teoriasr;
}Student;

typedef struct {
    int students_count;          // to jest licznik który jest inkrementowany
    int ilosc_studentow;         // a to jest ilość wylosowanych studentów(tak jakby warunek stopu)
    int wybrany_kierunek;
    int ilosc_osob_przepisujacych; // ile osob przepisuje ocene(ma zdany egz. prakt)
    int index;
    int ilosc_studentow_na_wybranym_kierunku;
    Student students[MAX_STUD];  // Tablica struktur Student
} SHARED_MEMORY;

SHARED_MEMORY *shm_ptr;

void koniec() //Funkcja zwalniająca zasoby
{
    // Zwalnianie kolejki komunikatów
    if (msgctl(msgID, IPC_RMID, NULL) == -1) {
        perror("Blad przy zwalnianiu kolejki komunikatow");
    } else {
        printf("Kolejka komunikatow zostala pomyslnie zwolniona\n");
    }

    // Zwalnianie pamięci dzielonej
    if (shmctl(shmID, IPC_RMID, NULL) == -1) {
        perror("Blad przy zwalnianiu pamieci dzielonej");
    } else {
        printf("Pamiec dzielona zostala pomyslnie zwolniona\n");
    }

    // Zwalnianie semaforów
    if (semctl(semID, 0, IPC_RMID) == -1) {
        perror("Blad przy zwalnianiu semaforow");
    } else {
        printf("Semafory zostaly pomyslnie zwolnione\n");
    }

    printf("Zasoby zostaly zwolnione\n");
}

void sigint_handler(int sig){
    if(mainprogSYM == getpid()){
        kill(0, SIGINT);

        while (1) {
        if (wait(NULL) == -1) {
                if (errno == ECHILD) {
                    // Brak więcej procesów potomnych
                    break;
                } else if (errno == EINTR) {
                    // `wait` przerwany przez sygnał - kontynuuj
                    continue;
                } else {
                    // Inny błąd - wypisz komunikat i wyjdź
                    perror("wait error");
                    exit(EXIT_FAILURE);
                }
            }
        }
        koniec();
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

    key_t kluczm, kluczs, kluczk;
    if((kluczm=ftok(".",'A')) == -1){
      perror("Blad przy tworzeniu klucza do pamieci dzielonej w Studencie\n");
      exit(EXIT_FAILURE);
    }

    shmID=shmget(kluczm, sizeof(SHARED_MEMORY), IPC_CREAT|0666);
    if (shmID==-1){
        perror("Blad przy tworzeniu pamieci dzielonej w Studencie\n");
        exit(EXIT_FAILURE);
    }

    shm_ptr =(SHARED_MEMORY*)shmat(shmID, NULL, 0);
    if (shm_ptr == (void *)-1){
        perror("Blad przy dolaczaniu pamieci dzielonej w Studencie\n");
        exit(EXIT_FAILURE);
    }

    if((kluczs=ftok(".",'B')) == -1){
        perror("Blad przy tworzeniu klucza do semaforów\n");
        exit(EXIT_FAILURE);
    }

    semID=semget(kluczs,5,IPC_CREAT|0666); 
    if(semID==-1){
        perror("Blad tworzenia semaforow \n");
        exit(EXIT_FAILURE);
    }

    if((kluczk = ftok(".", 'C')) == -1)
    {
        perror("Blad ftok dla kolejki komunikatow w studencie\n");
        exit(EXIT_FAILURE);
    }

    msgID=msgget(kluczk,IPC_CREAT|0666);
    if(msgID==-1)
	{
        perror("Blad tworzenia kolejki komunikatow w studencie \n");
        exit(EXIT_FAILURE);
    }

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