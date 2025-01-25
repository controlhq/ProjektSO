#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <unistd.h>

#define SHM_SIZE sizeof(SHARED_MEMORY)
#define SEM_COUNT 5
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
    int Komisjazakonczenie;
    int students_count;          // to jest licznik który jest inkrementowany
    int ilosc_studentow;         // a to jest ilość wylosowanych studentów(tak jakby warunek stopu)
    int wybrany_kierunek;
    int ilosc_osob_przepisujacych; // ile osob przepisuje ocene(ma zdany egz. prakt)
    int index;
    int ilosc_studentow_na_wybranym_kierunku;
    Student students[MAX_STUD];  // Tablica struktur Student
} SHARED_MEMORY;


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
        perror("Blad semafor_wait");
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
