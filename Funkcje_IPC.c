#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

#define SHM_SIZE sizeof(SHARED_MEMORY)
#define SEM_COUNT 9
#define MAX_STUD 160

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
    int flagakomA;
    pid_t PidkomisjiA;
    pid_t PidkomisjiB;
    pid_t PidTworzenie_studentow;
    int Liczba_studentow_dokomB;
    int Komisjazakonczenie;
    int students_count;
    int ilosc_studentow;
    int wybrany_kierunek;
    int ilosc_osob_przepisujacych;
    int index_stud;
    int ilosc_studentow_na_wybranym_kierunku;
    Student students[MAX_STUD];
} SHARED_MEMORY;

typedef struct {
    int index;
    int pid;           
    float ocenaPrzewA;   
    float ocenaCzA2;     
    float ocenaCzA3;     
    float sredniaA;   
    float ocenaPrzewB;  
    float ocenaCzB2;     
    float ocenaCzB3;     
    float sredniaB;
} WynikEgzaminu;


key_t kluczm, kluczs, kluczk;
int shmID, semID, msgID;
SHARED_MEMORY *shm_ptr;

void utworz_klucze() {
    if ((kluczm = ftok(".", 'A')) == -1) {
        perror("Blad przy tworzeniu klucza do pamieci dzielonej");
        exit(EXIT_FAILURE);
    }
    if ((kluczs = ftok(".", 'B')) == -1) {
        perror("Blad przy tworzeniu klucza do semaforow");
        exit(EXIT_FAILURE);
    }
    if ((kluczk = ftok(".", 'C')) == -1) {
        perror("Blad przy tworzeniu klucza do kolejki komunikatow");
        exit(EXIT_FAILURE);
    }
}

void semafor_wait(int semid, int sem_num) {
    struct sembuf sem = {sem_num, -1, 0};
    if (semop(semid, &sem, 1) == -1) {
        if(errno == EINTR){
           semop(semid, &sem, 1);
        }else{
            perror("Blad semafor_wait");
        }
        exit(EXIT_FAILURE);
    }
}

void semafor_signal(int semid, int sem_num) {
    struct sembuf sem = {sem_num, 1, 0};
    if (semop(semid, &sem, 1) == -1) {
        perror("Blad semafor_signal");
        exit(EXIT_FAILURE);
    }
}

void init_semaphore(int sem_num, int value) {
    if (semctl(semID, sem_num, SETVAL, value) == -1) {
        perror("Blad inicjalizacji semafora");
        exit(EXIT_FAILURE);
    }
}

void koniec() {
    if (msgctl(msgID, IPC_RMID, NULL) == -1) {
        perror("Blad przy zwalnianiu kolejki komunikatow");
    } else {
        printf("Kolejka komunikatow zostala pomyslnie zwolniona\n");
    }
    if (shmctl(shmID, IPC_RMID, NULL) == -1) {
        perror("Blad przy zwalnianiu pamieci dzielonej");
    } else {
        printf("Pamiec dzielona zostala pomyslnie zwolniona\n");
    }
    if (semctl(semID, 0, IPC_RMID) == -1) {
        perror("Blad przy zwalnianiu semaforow");
    } else {
        printf("Semafory zostaly pomyslnie zwolnione\n");
    }
}

void Zapiszlog(const char* format, ...) {
    FILE *log_file = fopen("procesy.log", "a");
    if (log_file != NULL) {
        //wypisuje pid procesu n poczatku
        fprintf(log_file, "PID:%d ", getpid());

        // obsluguje zmienna ilosc argumentow
        va_list args;
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);

        fprintf(log_file, "\n"); // dodaje nowa linie
        fclose(log_file);
    } else {
        perror("Blad w otwarciu pliku do logow\n");
    }
}

void handle_msgrcv_error_with_logging(void (*log_function)(const char*, ...)) {
    const char* error_message;

    switch (errno) {
        case E2BIG:
            error_message = "Komunikat jest za duzy, aby zmiescil się w buforze (E2BIG)";
            break;
        case EACCES:
            error_message = "Brak dostepu do kolejki komunikatow (EACCES)";
            break;
        case EFAULT:
            error_message = "Nieprawidlowy wskaznik na bufor wiadomosci (EFAULT)";
            break;
        case EIDRM:
            error_message = "Kolejka komunikatow zostala usunieta (EIDRM)";
            break;
        case EINTR:
            error_message = "Operacja przerwana przez sygnal (EINTR)";
            break;
        case EINVAL:
            error_message = "Nieprawidlowe parametry (EINVAL)";
            break;
        case ENOMSG:
            error_message = "Brak komunikatow pasujacych do kryteriow wyszukiwania (ENOMSG)";
            break;
        default:
            error_message = "Nieznany blad podczas odbierania wiadomosci (msgrcv)";
            break;
    }

    perror(error_message);

    if (log_function) {
        log_function("Blad w msgrcv: %s (errno: %d)", error_message, errno);
    }

}

void handle_msgsnd_error_with_logging(void (*log_function)(const char*, ...)) {
    const char* error_message;

    switch (errno) {
        case EACCES:
            error_message = "Brak dostepu do kolejki komunikatow (EACCES)";
            break;
        case EAGAIN:
            error_message = "Brak miejsca w kolejce komunikatow (EAGAIN)";
            break;
        case EFAULT:
            error_message = "Nieprawidlowy wskaznik na komunikat (EFAULT)";
            break;
        case EIDRM:
            error_message = "Kolejka komunikatow zostala usunieta (EIDRM)";
            break;
        case EINTR:
            error_message = "Operacja przerwana przez sygnal (EINTR)";
            break;
        case EINVAL:
            error_message = "Nieprawidlowe parametry (EINVAL)";
            break;
        case ENOMEM:
            error_message = "Brak pamieci dla wysylania wiadomosci (ENOMEM)";
            break;
        default:
            error_message = "Nieznany blad podczas wysylania wiadomosci (msgsnd)";
            break;
    }

    perror(error_message);

    if (log_function) {
        log_function("Blad w msgrcv: %s (errno: %d)", error_message, errno);
    }

}
