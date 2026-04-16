#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>                         
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "Funkcje_IPC.h"
#include "Funkcje_losujace.h"
#include <errno.h>
#include <pthread.h>

volatile sig_atomic_t ewakuacja = 0; 
volatile sig_atomic_t naturalnezamkniecie = 0; 

pthread_t ewakuacja_thread;

typedef struct{
    long mtype;
}Krotkie_powiadomienie;

WynikEgzaminu tabWynik[200];
int licznikWynik=0;

int znajdzLubDodajStudenta(int pid_studenta) {
  for(int i = 0; i < licznikWynik; i++) {
    if(tabWynik[i].pid == pid_studenta) {
        return i;
    }
  }
  if(licznikWynik < MAX_STUD) {
    tabWynik[licznikWynik].pid = pid_studenta;
    tabWynik[licznikWynik].ocenaPrzewA = 0.0;
    tabWynik[licznikWynik].ocenaCzA2 = 0.0;
    tabWynik[licznikWynik].ocenaCzA3 = 0.0;
    tabWynik[licznikWynik].sredniaA = 0.0;
    tabWynik[licznikWynik].ocenaPrzewB = 0.0;
    tabWynik[licznikWynik].ocenaCzB2 = 0.0;
    tabWynik[licznikWynik].ocenaCzB3 = 0.0;
    tabWynik[licznikWynik].sredniaB = 0.0;
    return licznikWynik++;
  }
  return -1; // tablica pelna
}

void* watek_ewakuacja(void* arg) {
  (void)arg;  // zeby kompilator sie nie czepail
  srand(time(NULL) ^ pthread_self());

  while (!ewakuacja && !naturalnezamkniecie) { 
    if(naturalnezamkniecie == 1){
      break;
    }

    int losowa_liczba = rand() % 1000 + 1; // 0,1% szans na ewakuacje
    if (losowa_liczba == 1) {
      printf("[DZIEKAN] EWAKUACJA! Wysyłam sygnał SIGUSR1 do komisji i studentów!\n");
      ewakuacja = 1;  // Ustawienie flagi globalnej

      kill(shm_ptr->PidkomisjiA, SIGUSR1);
      kill(shm_ptr->PidkomisjiB, SIGUSR1);
      kill(shm_ptr->PidTworzenie_studentow, SIGUSR1);
      for (int i = 0; i < shm_ptr->ilosc_studentow_na_wybranym_kierunku; i++) {
        pid_t student_pid = shm_ptr->students[i].pid_studenta;
        
        if (student_pid > 0) {  // sprawdzam czy pidjeest poprawny
          kill(student_pid, SIGUSR1);
        } 
      }
      break;
    }
    usleep(100000); // sleep na 1s zeby nie losowalo za szybko
  }
  
  printf("[DZIEKAN] Wątek ewakuacji zakończył działanie.\n");
  return NULL;
}

void sigusr1_handler(int sig) {
  (void)sig;
  printf("[DZIEKAN] Otrzymano sygnal SIGUSR1. Koncze egzamin i publikuje oceny!\n");
  ewakuacja = 1;

  //oczekuje na oceny od Komisji A:
  ssize_t bytesReadA;
  char bufferA[4096];
  
  semafor_wait(semID,7);
  Zapiszlog("Dziekan czeka na otworzenie fifo przez Komisje A podczas ewakuacji");
  semafor_signal(semID,7);
  semafor_wait(semID,8);
  int fd = open(FIFO_PATH, O_RDONLY);  // BLOKUJĄCE ODCZYTYWANIE
  if (fd == -1) {
    perror("blad open W DZIEKANIE podczas ewakuacji");
    exit(6);
  }
  
  while ((bytesReadA = read(fd, bufferA, sizeof(bufferA) - 1)) > 0) {
    // dodaje terminatoraby strtok dzialal ok
    bufferA[bytesReadA] = '\0';

    // dziele na linie
    char *line = strtok(bufferA, "\n");
    while (line != NULL) {
        int pid_studenta;
        float ocenaPrzewA, ocenaCz2A, ocenaCz3A, Asr;

        int parsed = sscanf(line, "%d %f %f %f %f", &pid_studenta,&ocenaPrzewA,&ocenaCz2A,&ocenaCz3A, &Asr);

        if (parsed == 5) {
          int index = znajdzLubDodajStudenta(pid_studenta);
          if(index!= -1){
            // zapisuje do tablicy
              tabWynik[index].pid= pid_studenta;
              tabWynik[index].ocenaPrzewA = ocenaPrzewA;
              tabWynik[index].ocenaCzA2 = ocenaCz2A;
              tabWynik[index].ocenaCzA3 = ocenaCz3A;
              tabWynik[index].sredniaA = Asr;
            } else {
              fprintf(stderr, "[Dziekan]: za dużo danych w FIFO, brak miejsca w tabWynik!\n");
            }
        } else {
            fprintf(stderr, "[Dziekan] Blad parsowania linii: '%s'\n", line);
        }

      line = strtok(NULL, "\n");
    }
  }
  
  close(fd);
  semafor_wait(semID,7);
  Zapiszlog("[DZIEKAN] Dziekan zapisał dane od komisji A w tablicy tabWynik podczas ewakuacji");
  semafor_signal(semID,7);
  printf("[Dziekan] Odczytano %d wyników z Komisji A podczas ewakuacji\n", licznikWynik);


  semafor_wait(semID,7);
  Zapiszlog("[DZIEKAN] Dziekan czeka na otworzenie fifo przez Komisje B podczas ewakuacji");
  semafor_signal(semID,7);

  //teraz czekam na oceny od komisji B
  int fd2 = open(FIFO_PATH2, O_RDONLY);  
  if (fd2 == -1) {
    perror("blad open W DZIEKANIE podczas ewakuacji");
    semafor_wait(semID,7);
    Zapiszlog("[DZIEKAN] Blad otworzenia fifo od komisji B");
    semafor_signal(semID,7);
    exit(EXIT_FAILURE);
  }

  char bufferB[4096];
  ssize_t bytesReadB;

  while ((bytesReadB = read(fd2, bufferB, sizeof(bufferB) - 1)) > 0) {
      // dodaje terminatoraby strtok dzialal ok
      bufferB[bytesReadB] = '\0';

      // dziele na linie
      char *line = strtok(bufferB, "\n");
      while (line != NULL) {
          int pid_studenta;
          float ocenaPrzewB, ocenaCz2B, ocenaCz3B, Bsr;

          int parsed = sscanf(line, "%d %f %f %f %f", &pid_studenta,&ocenaPrzewB,&ocenaCz2B,&ocenaCz3B, &Bsr);

          if (parsed == 5) {
            int index = znajdzLubDodajStudenta(pid_studenta);
              // zapisuje do tablicy
              if (index != -1) {
                  tabWynik[index].pid= pid_studenta;
                  tabWynik[index].ocenaPrzewB = ocenaPrzewB;
                  tabWynik[index].ocenaCzB2 = ocenaCz2B;
                  tabWynik[index].ocenaCzB3 = ocenaCz3B;
                  tabWynik[index].sredniaB = Bsr;
              } else {
                fprintf(stderr, "[Dziekan]: za dużo danych w FIFO, brak miejsca w tabWynik!\n");
              }
          } else {
            fprintf(stderr, "[Dziekan] Blad parsowania linii: '%s'\n", line);
          }

          line = strtok(NULL, "\n");
      }
  }

  close(fd2);
  printf("[Dziekan] Odczytano %d wyników z Komisji B\n", licznikWynik);
  semafor_wait(semID,7);
  Zapiszlog("[DZIEKAN] Dziekan odczytał wynik od komisji B i wczytał je do tablicy tabWynik");
  semafor_signal(semID,7);


  FILE *outputFile = fopen("Wyniki.txt", "w");  
  if (outputFile == NULL) {
    perror("[DZIEKAN] Blad otwarcia pliku Wyniki.txt");
    semafor_wait(semID,7);
    Zapiszlog("[DZIEKAN] Blad otwarcia pliku Wyniki.txt");
    semafor_signal(semID,7);
    exit(EXIT_FAILURE);
  }

  fprintf(outputFile, "Wyniki egzaminu\n");
  fprintf(outputFile, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
  fprintf(outputFile, "| %-5s | %-20s | %-15s | %-15s | %-12s | %-20s | %-15s | %-15s | %-12s | %-13s |\n",
          "PID", "Ocena Przewodnicz.A", "Ocena Cz2A", "Ocena Cz3A", "Śr. Praktyka",
          "Ocena Przewodnicz.B", "Ocena Cz2B", "Ocena Cz3B", "Śr. Teoria", "Ocena Końcowa");
  fprintf(outputFile, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

  for (int i = 0; i < licznikWynik; i++) {
    // obliczam oceny końcowe tylko dla studentów, którzy przeszli obie komisje
    float ocenaKoncowa = 0.0;
    if(tabWynik[i].sredniaA > 0.0 && tabWynik[i].sredniaB > 0.0){
      ocenaKoncowa = oblicz_srednia_i_zaokragl_DZIEKAN(tabWynik[i].sredniaA,tabWynik[i].sredniaB);
    } else {
      // Student nie przeszedł do Komisji B
      ocenaKoncowa = tabWynik[i].sredniaA;
    } 
    fprintf(outputFile, "| %-5d | %-20.1f | %-15.1f | %-15.1f | %-12.1f | %-20.1f | %-15.1f | %-15.1f | %-12.1f | %-13.1f |\n",
      tabWynik[i].pid,
      tabWynik[i].ocenaPrzewA,
      tabWynik[i].ocenaCzA2,
      tabWynik[i].ocenaCzA3,
      tabWynik[i].sredniaA,
      tabWynik[i].ocenaPrzewB,
      tabWynik[i].ocenaCzB2,
      tabWynik[i].ocenaCzB3,
      tabWynik[i].sredniaB,
      ocenaKoncowa);
  }

  fprintf(outputFile, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
  fclose(outputFile);
  semafor_wait(semID,7);
  Zapiszlog("[DZIEKAN] Dziekan zapisał wyniki do pliku");
  semafor_signal(semID,7);




  if (pthread_join(ewakuacja_thread, NULL) != 0) {
    perror("Blad przy dolaczaniu watku ewakuacja w dziekanie\n");
  }
  koniec();
  exit(EXIT_SUCCESS);
}

int main(){
  
  srand(time(NULL));
  Krotkie_powiadomienie pw;

  //obsluga sygnalu SIGUSR1
  struct sigaction act;
  act.sa_handler = sigusr1_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags=0;
  sigaction(SIGUSR1,&act,0);

  if (mkfifo(FIFO_PATH, 0666) == -1 && errno != EEXIST) {
    perror("Blad tworzenia my_fifo");
    exit(EXIT_FAILURE);
  }
  if (mkfifo(FIFO_PATH2, 0666) == -1 && errno != EEXIST) {
    perror("Blad tworzenia my_fifo2");
    exit(EXIT_FAILURE);
  }

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

  semID=semget(kluczs,9,0666); 
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
  if((msgrcv(msgID,&pw,sizeof(Krotkie_powiadomienie)-sizeof(long),40,0)) == -1){
    semafor_wait(semID,7);
    handle_msgrcv_error_with_logging(Zapiszlog);
    semafor_signal(semID,7);
    exit(EXIT_FAILURE);
  }
  
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
  semafor_signal(semID,4);

  //dziekan wpuszcza tych, którzy mają egzamin, a innych wysyła do domu
  printf("Dziekan wysyła sygnał do studentów.\n");
  for (int i = 0; i < shm_ptr->ilosc_studentow; i++) {
    semafor_signal(semID,1);
  }

  printf("[DZIEKAN] Dziekan wyslal sygnal wszystkim studentom\n");
  //teraz tworze wątek odpowiedzialny za ewakuacje
  if (pthread_create(&ewakuacja_thread, NULL, &watek_ewakuacja, NULL) != 0) {
    perror("Blad przy inicjowaniu watku ewakuacja w Dziekanie\n");
    Zapiszlog("[DZIEKAN] Wystapil blad przy inicjowaniu watka ewakuacja w Dziekanie");
    exit(EXIT_FAILURE);
  }

  //dziekan czeka na fifo z ocenami od komisji A
  ssize_t bytesReadA;
  char bufferA[4096];
  
  semafor_wait(semID,7);
  Zapiszlog("Dziekan czeka na otworzenie fifo przez Komisje A");
  semafor_signal(semID,7);
  int fd = open(FIFO_PATH, O_RDONLY);  // BLOKUJĄCE ODCZYTYWANIE
  if (fd == -1) {
    perror("blad open W DZIEKANIE");
    Zapiszlog("[DZIEKAN] Wystapil Blad otwierania fifo od komisji A");
    exit(EXIT_FAILURE);
  }
  
  while ((bytesReadA = read(fd, bufferA, sizeof(bufferA) - 1)) > 0) {
    // dodaje terminatoraby strtok dzialal ok
    bufferA[bytesReadA] = '\0';

    // dziele na linie
    char *line = strtok(bufferA, "\n");
    while (line != NULL) {
        int pid_studenta;
        float ocenaPrzewA, ocenaCz2A, ocenaCz3A, Asr;

        int parsed = sscanf(line, "%d %f %f %f %f", &pid_studenta,&ocenaPrzewA,&ocenaCz2A,&ocenaCz3A, &Asr);

        if (parsed == 5) {
          int index = znajdzLubDodajStudenta(pid_studenta);
          if(index!= -1){
            // zapisuje do tablicy
              tabWynik[index].pid= pid_studenta;
              tabWynik[index].ocenaPrzewA = ocenaPrzewA;
              tabWynik[index].ocenaCzA2 = ocenaCz2A;
              tabWynik[index].ocenaCzA3 = ocenaCz3A;
              tabWynik[index].sredniaA = Asr;
            } else {
              fprintf(stderr, "[Dziekan]: za dużo danych w FIFO, brak miejsca w tabWynik!\n");
            }
        } else {
            fprintf(stderr, "[Dziekan] Blad parsowania linii: '%s'\n", line);
        }

      line = strtok(NULL, "\n");
    }
  }
  
  close(fd);
  semafor_wait(semID,7);
  Zapiszlog("Dziekan zapisał dane od komisji A w tablicy tabWynik");
  semafor_signal(semID,7);
  printf("[Dziekan] Odczytano %d wyników z Komisji A\n", licznikWynik);


  semafor_wait(semID,7);
  Zapiszlog("Dziekan czeka na otworzenie fifo przez Komisje B");
  semafor_signal(semID,7);

  //teraz czekam na oceny od komisji B
  int fd2 = open(FIFO_PATH2, O_RDONLY);  
  if (fd2 == -1) {
    perror("blad open W DZIEKANIE");
    exit(7);
  }

  char bufferB[4096];
  ssize_t bytesReadB;

  while ((bytesReadB = read(fd2, bufferB, sizeof(bufferB) - 1)) > 0) {
      // dodaje terminatoraby strtok dzialal ok
      bufferB[bytesReadB] = '\0';

      // dziele na linie
      char *line = strtok(bufferB, "\n");
      while (line != NULL) {
          int pid_studenta;
          float ocenaPrzewB, ocenaCz2B, ocenaCz3B, Bsr;

          int parsed = sscanf(line, "%d %f %f %f %f", &pid_studenta,&ocenaPrzewB,&ocenaCz2B,&ocenaCz3B, &Bsr);

          if (parsed == 5) {
            int index = znajdzLubDodajStudenta(pid_studenta);
              // zapisuje do tablicy
              if (index != -1) {
                  tabWynik[index].pid= pid_studenta;
                  tabWynik[index].ocenaPrzewB = ocenaPrzewB;
                  tabWynik[index].ocenaCzB2 = ocenaCz2B;
                  tabWynik[index].ocenaCzB3 = ocenaCz3B;
                  tabWynik[index].sredniaB = Bsr;
              } else {
                fprintf(stderr, "[Dziekan]: za dużo danych w FIFO, brak miejsca w tabWynik!\n");
              }
          } else {
              fprintf(stderr, "[Dziekan] Blad parsowania linii: '%s'\n", line);
          }

          line = strtok(NULL, "\n");
      }
  }

  close(fd2);
  printf("[Dziekan] Odczytano %d wyników z Komisji B\n", licznikWynik);
  semafor_wait(semID,7);
  Zapiszlog("Dziekan odczytał wynik od komisji B i wczytał je do tablicy tabWynik");
  semafor_signal(semID,7);


  FILE *outputFile = fopen("Wyniki.txt", "w");  
  if (outputFile == NULL) {
    perror("Blad otwarcia pliku Wyniki.txt");
    exit(3);
  }

  fprintf(outputFile, "Wyniki egzaminu\n");
  fprintf(outputFile, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
  fprintf(outputFile, "| %-5s | %-20s | %-15s | %-15s | %-12s | %-20s | %-15s | %-15s | %-12s | %-13s |\n",
          "PID", "Ocena Przewodnicz.A", "Ocena Cz2A", "Ocena Cz3A", "Śr. Praktyka",
          "Ocena Przewodnicz.B", "Ocena Cz2B", "Ocena Cz3B", "Śr. Teoria", "Ocena Końcowa");
  fprintf(outputFile, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

  for (int i = 0; i < licznikWynik; i++) {
    // obliczam oceny końcowe tylko dla studentów, którzy przeszli obie komisje
    float ocenaKoncowa = 0.0;
    if(tabWynik[i].sredniaA > 0.0 && tabWynik[i].sredniaB > 0.0){
      ocenaKoncowa = oblicz_srednia_i_zaokragl_DZIEKAN(tabWynik[i].sredniaA,tabWynik[i].sredniaB);
    } else {
      // Student nie przeszedł do Komisji B
      ocenaKoncowa = tabWynik[i].sredniaA;
    } 
    fprintf(outputFile, "| %-5d | %-20.1f | %-15.1f | %-15.1f | %-12.1f | %-20.1f | %-15.1f | %-15.1f | %-12.1f | %-13.1f |\n",
      tabWynik[i].pid,
      tabWynik[i].ocenaPrzewA,
      tabWynik[i].ocenaCzA2,
      tabWynik[i].ocenaCzA3,
      tabWynik[i].sredniaA,
      tabWynik[i].ocenaPrzewB,
      tabWynik[i].ocenaCzB2,
      tabWynik[i].ocenaCzB3,
      tabWynik[i].sredniaB,
      ocenaKoncowa);
  }

  fprintf(outputFile, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
  fclose(outputFile);
  semafor_wait(semID,7);
  Zapiszlog("Dziekan zapisał wyniki do pliku");
  semafor_signal(semID,7);
  naturalnezamkniecie = 1;

  //to dla synchronizacji, zeby komisja zakonczyla dzialanie przed dziekanem
  semafor_wait(semID,8);
  

  if (pthread_join(ewakuacja_thread, NULL) != 0) {
    perror("Blad przy dolaczaniu watku ewakuacja w dziekanie\n");
  }

  printf("[DZIEKAN] Program Dziekan zakonczyl dzialanie\n");
  koniec();
  return 0;
}