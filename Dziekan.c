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

#define MAX_STUD 160        // do pamieci wspódzielonej maksymalna ilosc studentow 
int shmID, semID, msgID;  //ID semafora, kolejki kom. pamieci dzielonej

typedef struct {            // struktura reprezentująca każdego studenta
    int id;   
    pid_t pid_studenta;     // identyfikator kazdego studenta
    int Kierunek;
    int powtarza_egzamin;   // flaga, jeśli student powtarza egzamin (1 - tak,0 - nie)
    int zaliczona_praktyka; // flaga, czy czesc praktyczna zostala zaliczona
    float ocena_praktyka;
    float ocena_teoria;
}Student;

typedef struct {
    int students_count;          // to jest licznik który jest inkrementowany
    int ilosc_studentow;         // a to jest ilość wylosowanych studentów(tak jakby warunek stopu)
    int wybrany_kierunek;
    int index;
    Student students[MAX_STUD];  // Tablica struktur Student
} SHARED_MEMORY;


SHARED_MEMORY *shm_ptr; //wskaźnik do mapowania wspólnej pamięci

void semafor_wait(int semid, int sem_num){ //funkcja do semafor P
    struct sembuf sem;
    sem.sem_num = sem_num;
    sem.sem_op=-1; // dekrementacja 
    sem.sem_flg=0;
    int zmiensem=semop(semid,&sem,1);
    if(zmiensem == -1){
        perror("Blad semaforwait w dziekanie\n");
        exit(EXIT_FAILURE);
    }
}

void semafor_signal(int semid, int sem_num){ // funkcja do semafor V
    struct sembuf sem;
    sem.sem_num = sem_num;
    sem.sem_op=1; // inkrementacja 
    sem.sem_flg=0;
    int zmiensem=semop(semid,&sem,1);
    if(zmiensem == -1){
        perror("Blad semaforsignal w dziekanie\n");
        exit(EXIT_FAILURE);
    }
}

int main(){
  srand(time(NULL));
  key_t kluczm, kluczs, kluczk;

  if((kluczm=ftok(".",'A')) == -1){
    perror("Blad przy tworzeniu klucza do pamieci dzielonej w Dziekanie\n");
    exit(EXIT_FAILURE);
  }

  shmID=shmget(kluczm, sizeof(SHARED_MEMORY), IPC_CREAT|0666);
  if (shmID==-1){
    perror("Blad przy tworzeniu pamieci dzielonej w Dziekanie\n");
    exit(EXIT_FAILURE);
  }

  shm_ptr =(SHARED_MEMORY*)shmat(shmID, NULL, 0);
  if (shm_ptr == (void *)-1){
    perror("Blad przy dolaczaniu pamieci dzielonej w Dziekanie\n");
    exit(EXIT_FAILURE);
  }

  if((kluczs=ftok(".",'B')) == -1){
    perror("Blad przy tworzeniu klucza do semaforów w Dziekanie\n");
    exit(EXIT_FAILURE);
  }

  semID=semget(kluczs,3,IPC_CREAT|0666); 
  if(semID==-1){
    perror("Blad tworzenia semaforow w Dziekanie\n");
    exit(EXIT_FAILURE);
  }
  
  //dziekan wychodzi przed budynek i czeka, aż nie zbiorą się wszyscy studenci, żeby potem ogłosić komunikat 
  //który kierunek przystępuje do egzaminu
  while(shm_ptr->students_count != shm_ptr->ilosc_studentow){
    sleep(5);
  }

  //jak wszyscy studenci się zebrali, to dziekan ogłasza który kierunek ma egzamin
  int w_kierunek = rand() % 5 +1;
  shm_ptr->wybrany_kierunek = w_kierunek;
  

  //dziekan wpuszcza tych, którzy mają egzamin, a innych wysyła do domu
  printf("Dziekan wysyła sygnał do studentów.\n");
  for (int i = 0; i < shm_ptr->ilosc_studentow; i++) {
    semafor_signal(semID,1);
  }

  printf("Dziekan zakończył wysyłanie sygnałów. Wszyscy studenci poinformowani.\n");

  return 0;
}