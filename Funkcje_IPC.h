#ifndef IPC_FUNCTIONS_H
#define IPC_FUNCTIONS_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <unistd.h>

#define SHM_SIZE sizeof(SHARED_MEMORY)
#define FIFO_PATH "my_fifo"
#define SEM_COUNT 5
#define MAX_STUD 160
#define T 600
#define K 5                 // ilość kierunków na wydziale X
#define MAX_STUDENTOW 800   // maksymalna ilość studentów 2 roku (160*5)
#define MAX_STUD 160        // do pamieci wspódzielonej maksymalna ilosc studentow
#define MAX_SIZE_OCENY 200

//indentyfikatory komunikatów dla kolejnych czlonków komisji A
#define PKA 449
#define CZ2KA 450
#define CZ3KA 451
#define PKAALL 1550
#define CZ2KAALL 1551
#define CZ3KAALL 1552
//indentyfikatory komunikatów dla kolejnych czlonków komisji B
#define PKB 452
#define CZ2KB 453
#define CZ3KB 454

// student
typedef struct {
    int id;
    pid_t pid_studenta;
    int Kierunek;
    int powtarza_egzamin;
    float ocena_praktykaP;
    float ocena_praktyka2;
    float ocena_praktyka3;
    float ocena_praktykasr;
    float ocena_teoriaP;
    float ocena_teoria2;
    float ocena_teoria3;
    float ocena_teoriasr;
} Student;

// pamiec dzielona
typedef struct {
     int Komisjazakonczenie;
    int students_count;
    int ilosc_studentow;
    int wybrany_kierunek;
    int ilosc_osob_przepisujacych;
    int index;
    int ilosc_studentow_na_wybranym_kierunku;
    Student students[MAX_STUD];
} SHARED_MEMORY;

// zmienne globalne
extern key_t kluczm, kluczs, kluczk;
extern int shmID, semID, msgID;
extern SHARED_MEMORY *shm_ptr;

// funkcje do kluczyy, semaforow i funkcja czyszczaca
void utworz_klucze();
void semafor_wait(int semid, int sem_num);
void semafor_signal(int semid, int sem_num);
void init_semaphore(int sem_num, int value);
void koniec();

#endif // IPC_FUNCTIONS_H