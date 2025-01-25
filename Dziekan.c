#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>                         
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "Funkcje_IPC.h"

typedef struct{
    long mtype;
}Krotkie_powiadomienie;

void sigint_handler(int sig){
  printf("[DZIEKAN] Program Dziekan otrzymal sygnal SIGINT: %d \n", sig);
  exit(0);
}

int main(){
  srand(time(NULL));
  Krotkie_powiadomienie pw;


  //obsluga sygnalu sigint
  struct sigaction act;
  act.sa_handler = sigint_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags=0;
  sigaction(SIGINT,&act,0);

  utworz_klucze();

  shmID=shmget(kluczm, sizeof(SHARED_MEMORY),0666);
  if (shmID==-1){
    perror("Blad przy dolaczaniu pamieci dzielonej w Dziekanie\n");
    koniec();
    exit(EXIT_FAILURE);
  }

  shm_ptr =(SHARED_MEMORY*)shmat(shmID, NULL, 0);
  if (shm_ptr == (void *)-1){
    perror("Blad przy dolaczaniu pamieci dzielonej w Dziekanie\n");
    koniec();
    exit(EXIT_FAILURE);
  }

  semID=semget(kluczs,5,0666); 
  if(semID==-1){
    perror("Blad tworzenia semaforow w Dziekanie\n");
    koniec();
    exit(EXIT_FAILURE);
  }
  
  msgID=msgget(kluczk,0666);
    if(msgID==-1)
	{
    perror("Blad tworzenia kolejki komunikatow w Dziekanie\n");
    koniec();
    exit(EXIT_FAILURE);
  }

  //poczekaj na wiadomość o gotowości od procesu tworzeniestudentow
  msgrcv(msgID,&pw,sizeof(Krotkie_powiadomienie)-sizeof(long),40,0);
  
  //dziekan wychodzi przed budynek i czeka, aż nie zbiorą się wszyscy studenci, żeby potem ogłosić komunikat 
  //który kierunek przystępuje do egzaminu
  while(shm_ptr->students_count != shm_ptr->ilosc_studentow){
    usleep(10000);
  }

  //jak wszyscy studenci się zebrali, to dziekan ogłasza który kierunek ma egzamin
  int w_kierunek = 2;
  shm_ptr->wybrany_kierunek = w_kierunek;
  printf("[DZIEKAN] Liczba studentow do egzaminowania: %d\n",shm_ptr->ilosc_studentow_na_wybranym_kierunku);
  semafor_signal(semID,4);

  //dziekan wpuszcza tych, którzy mają egzamin, a innych wysyła do domu
  printf("Dziekan wysyła sygnał do studentów.\n");
  for (int i = 0; i < shm_ptr->ilosc_studentow; i++) {
    semafor_signal(semID,1);
  }

  printf("Dziekan wyslal sygnal wszystkim studentom\n");

  //dziekan czeka na komunikat z ocenami od komisji A
  char KomisjaA[4096];
  int a;
  float b;
  int fd = open(FIFO_PATH, O_RDONLY);  // BLOKUJĄCE ODCZYTYWANIE
  if (fd == -1) {
    perror("blad open W DZIEKANIE");
    exit(6);
  }

  FILE *outputFile = fopen("Wyniki.txt", "w");  
  if (outputFile == NULL) {
    perror("Blad otwarcia pliku Wyniki.txt");
    close(fd);
    exit(EXIT_FAILURE);
  }
  
  while (1) {
    ssize_t bytesRead = read(fd, KomisjaA, sizeof(KomisjaA) - 1);
    if (bytesRead > 0) {
        KomisjaA[bytesRead] = '\0';  // dodaje terminator stringa   
        //parsuje wszystkie linie w buforze
        char *line = strtok(KomisjaA, "\n");
        while (line != NULL) {
            if (sscanf(line, "%d %f", &a, &b) != 2) {
              perror("Dziekan: Blad parsowania danych\n");
            }else{
              fprintf(outputFile, "%d %.1f\n", a, b);
            }
            line = strtok(NULL, "\n");  // przechodze do kolejnej linii
        }
    } else if (bytesRead == 0) {
      break;
    } else {
      break;
    }
  }

  fclose(outputFile);
  close(fd);

  while (1) {
      semafor_wait(semID,0);
      int finish = shm_ptr->Komisjazakonczenie;
      semafor_signal(semID,0);

      if (finish == 1) {
        break;
      }
      usleep(100000);
  }

  printf("[DZIEKAN] Program Dziekan zakonczyl dzialanie\n");
  koniec();
  return 0;
}