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
#include <errno.h>
#include <pthread.h>   
#include "Funkcje_IPC.h"
#include "Funkcje_losujace.h"

void runKomisjaA();
void runKomisjaB();
void sigusr1_handler_komisjaA(int sig, siginfo_t *siginfo, void *context);
void sigusr1_handler_komisjaB(int sig, siginfo_t *siginfo, void *context);

volatile sig_atomic_t ewakuacja_komisjaA=0; // bezpieczne flagi do ewakuacji 
volatile sig_atomic_t ewakuacja_komisjaB=0;

pthread_t KAczlonek2, KAczlonek3;
pthread_t KBczlonek2, KBczlonek3;
pthread_mutex_t mutexA;
pthread_mutex_t mutexA2;
pthread_mutex_t mutexB;

int Liczba_studentow_do_zegzaminowania;
int Liczba_przepisujacych;
int Liczba_komunikatow_KOMA;

pid_t mainprogKomisjaA;  // PID głównego procesu (dla obsługi sygnałów)
pid_t mainprogKomisjaB;

int terminate_threads = 0;
int flaga_komisjaA=0;
int flaga_komisjaB=0;
int PAE=0;
int ileDoObsluzenia =0;

typedef struct {
    long mtype;
} Krotkie_powiadomienie;

typedef struct {
	long mtype;
    float ocena_koncowa;
    int pidStudenta;
    int czy_zdane;
    int index_studenta;
    int id_studenta;
    int zdaje_czy_pytania; // jak zdaje to 1, jak pytania to 0
} Kom_bufor;

typedef struct {
    long mtype; 
    int pid_studenta;   
    int IDczlonkakomisji; 
    char pytanie[MAX_Q_LENG];  
    int odpowiedz;  
} Pytanie;

typedef struct {
    long mtype;
    float ocena;
} Wiadomosc_zocena;

Krotkie_powiadomienie powiadom;
Krotkie_powiadomienie powiadomB;
Kom_bufor msg;
Kom_bufor msgKB;
Kom_bufor msgKBCZ2;
Kom_bufor msgKBCZ3;


void* KA_czlonek2(void* arg){
    (void)arg;

    unsigned int seedcz2KA = time(NULL) ^ pthread_self();

    while (!ewakuacja_komisjaA) {
    
        //czeka na informacje od Przewodniczacego na temat nowego studenta
        
        if((msgrcv(msgID,&powiadom,0,CZ2KAALL,0)) == -1){
            semafor_wait(semID,7);
            handle_msgrcv_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }

        //sprawdź, czy są jeszcze jacyś studenci,którzy chcą podejść do egzaminu
        if(flaga_komisjaA ==1){
            break;
        }
            
        if(msg.zdaje_czy_pytania== 0){
            if(msg.czy_zdane != 1){ //jak nie zdane to w tym przypadku chce pytania
                //zadaje pytanie studentowi
                Pytanie PCZKA2;

                //tutaj symulacja losowania i przygotowywania pytania
                //int PC2 = rand_r(&seedcz2KA) % 6 + 5;
                //sleep(PC2);


                PCZKA2.IDczlonkakomisji=CZ2KA;
                const char* wylos_pyt_KACZ2 = Wylosuj_pytanie_CZ2KA(seedcz2KA);
                strncpy(PCZKA2.pytanie, wylos_pyt_KACZ2, MAX_Q_LENG - 1);
                PCZKA2.pytanie[MAX_Q_LENG - 1] = '\0'; 
                PCZKA2.mtype = msg.pidStudenta;
                if((msgsnd(msgID, &PCZKA2,sizeof(Pytanie)-sizeof(long),0))== -1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }

                //tutaj potwierdzenie do przewodniczacego ze wyslal pytanie
                Krotkie_powiadomienie powcz2;
                powcz2.mtype = 690;

                if((msgsnd(msgID, &powcz2,0,0))==-1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }

                
            }else{//chce przepisać ocene

                Krotkie_powiadomienie powcz5;
                powcz5.mtype = 700;
                if((msgsnd(msgID, &powcz5,0,0))== -1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }
           
            }

        }else{
            //teraz czeka na odpowiedź studenta do swojego pytania
            Pytanie PCZKA22;
            if((msgrcv(msgID,&PCZKA22,sizeof(Pytanie)-sizeof(long),CZ2KA,0)) == -1){
                semafor_wait(semID,7);
                handle_msgrcv_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
                
            //ocenia odpowiedź
            float ocenaCZ2KA = wylosuj_ocene(&seedcz2KA);
                    
            //zapisuje ocene w pamieci dzielonej
            shm_ptr->students[msg.index_studenta].ocena_praktyka2=ocenaCZ2KA;

            semafor_wait(semID,7);
            Zapiszlog("Czlonek 2 Komisji A zapisał ocene: %0.1f studentowi o pid: %d",ocenaCZ2KA,msg.pidStudenta);
            semafor_signal(semID,7);
            

            //wyslij wiadomosc do przewodniczacego, ze wpisales ocene
            Wiadomosc_zocena CZ2Kwiad;
            CZ2Kwiad.mtype = 556;
            CZ2Kwiad.ocena = ocenaCZ2KA;

            if((msgsnd(msgID, &CZ2Kwiad,sizeof(Wiadomosc_zocena)-sizeof(long),0))== -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
                      
        }
            
    }

    if(ewakuacja_komisjaA == 1){
        printf("[KOMISJA A] Czlonek 2 komisji A ewakuowal sie pomyslnie\n");
        semafor_wait(semID,7);
        Zapiszlog("[KOMISJA A] Czlonek 2 komisji A ewakuowal sie pomyslnie");
        semafor_signal(semID,7);
    }else{
        printf("[KOMISJA A] Czlonek 2 komisji A zakonczyl swoja prace i wrocil do domu\n");
        semafor_wait(semID,7);
        Zapiszlog("[KOMISJA A] Czlonek 2 komisji A zakonczyl swoja prace i wrocil do domu");
        semafor_signal(semID,7);
    }

    return NULL;
}

void* KA_czlonek3(void* arg) {
    (void)arg;

    unsigned int seedcz3KA = time(NULL) ^ pthread_self();

    while (!ewakuacja_komisjaA) { 
        // czeka na informacje od Przewodniczacego na temat nowego studenta
        
        if((msgrcv(msgID, &powiadom, 0, CZ3KAALL, 0)) == -1){
            semafor_wait(semID,7);
            handle_msgrcv_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }

        //sprawdź, czy są jeszcze jacyś studenci,którzy chcą podejść do egzaminu
        if(flaga_komisjaA ==1){
            break;
        }

        if (msg.zdaje_czy_pytania == 0) {

            if (msg.czy_zdane != 1) { // jak student chce pytania
                // zadaje pytanie studentowi
                Pytanie PCZKA3;

                // tutaj symulacja losowania i przygotowywania pytania
                //int PC3 = rand_r(&seedcz3KA) % 6 + 5;
                // sleep(PC3);
                
                const char* wylos_pyt_KACZ3 = Wylosuj_pytanie_CZ3KA(seedcz3KA);
                strncpy(PCZKA3.pytanie, wylos_pyt_KACZ3, MAX_Q_LENG - 1);
                PCZKA3.pytanie[MAX_Q_LENG - 1] = '\0'; 
                PCZKA3.IDczlonkakomisji = CZ3KA;
                PCZKA3.mtype = msg.pidStudenta;
                if((msgsnd(msgID, &PCZKA3, sizeof(Pytanie) - sizeof(long), 0))== -1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }

                //tutaj potwierdzenie do przewodniczacego ze wyslal pytania
                Krotkie_powiadomienie powcz3;
                powcz3.mtype = 691;
                if((msgsnd(msgID, &powcz3,0,0)) == -1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }



            }else{//tu student przepisuje ocene
                //powiadomienie w celu synchronizacji
                Krotkie_powiadomienie powcz5;
                powcz5.mtype = 701;
                if((msgsnd(msgID, &powcz5,0,0))==-1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }
            }

        } else { // tutaj jak chce zdawac
            // teraz czeka na odpowiedź studenta
            Pytanie PCZKA33;
            if((msgrcv(msgID, &PCZKA33, sizeof(Pytanie) - sizeof(long), CZ3KA, 0)) == -1){
                semafor_wait(semID,7);
                handle_msgrcv_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

            // ocenia odpowiedź
            float ocenaCZ3KA = wylosuj_ocene(&seedcz3KA);

            // zapisuje ocene w pamieci dzielonej
           
            shm_ptr->students[msg.index_studenta].ocena_praktyka3 = ocenaCZ3KA;

            semafor_wait(semID,7);
            Zapiszlog("Czlonek 3 Komisji A zapisał ocene: %0.1f studentowi o pid: %d",ocenaCZ3KA,msg.pidStudenta);
            semafor_signal(semID,7);
            

            // wyslij wiadomosc do przewodniczacego, ze wpisales ocene
            Wiadomosc_zocena CZ3Kwiad;
            CZ3Kwiad.mtype = 555;
            CZ3Kwiad.ocena = ocenaCZ3KA;

            if((msgsnd(msgID, &CZ3Kwiad, sizeof(Wiadomosc_zocena) - sizeof(long), 0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
        }
        
    }

    if(ewakuacja_komisjaA == 1){
        printf("[KOMISJA A] Czlonek 3 komisji A ewakuowal sie pomyslnie\n");
        semafor_wait(semID,7);
        Zapiszlog("[KOMISJA A] Czlonek 3 komisji A ewakuowal sie pomyslnie");
        semafor_signal(semID,7);
    }else{
        printf("[KOMISJA A] Przewodniczacy komisji A zakonczyl swoja prace i wrocil do domu\n");
        semafor_wait(semID,7);
        Zapiszlog("[KOMISJA A] Przewodniczacy komisji A zakonczyl swoja prace i wrocil do domu");
        semafor_signal(semID,7);
    }

    return NULL;
}

void runKomisjaA() {

    mainprogKomisjaA = getpid();
    unsigned int seed = time(NULL) ^ mainprogKomisjaA;

    struct sigaction act_ewakuacja;
    memset(&act_ewakuacja, 0, sizeof(act_ewakuacja));
    act_ewakuacja.sa_sigaction = sigusr1_handler_komisjaA;
    act_ewakuacja.sa_flags = SA_SIGINFO;
    sigemptyset(&act_ewakuacja.sa_mask);

    if (sigaction(SIGUSR1, &act_ewakuacja, NULL) == -1) {
        perror("[KOMISJA A] Blad ustawienia sigaction dla SIGUSR1");
        exit(EXIT_FAILURE);
    }


    utworz_klucze();
    semID = semget(kluczs, 9, 0666); 
    if (semID == -1) {
        perror("Blad tworzenia semaforow \n");
        exit(EXIT_FAILURE);
    }

    shmID = shmget(kluczm, sizeof(SHARED_MEMORY), 0666);
    if (shmID == -1) {
        perror("Blad przy tworzeniu pamieci dzielonej w Komisji A\n");
        exit(EXIT_FAILURE);
    }

    shm_ptr = (SHARED_MEMORY*)shmat(shmID, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("Blad przy dolaczaniu pamieci dzielonej w Komisji A\n");
        exit(EXIT_FAILURE);
    }

    msgID = msgget(kluczk, 0666);
    if (msgID == -1) {
        perror("Blad tworzenia kolejki komunikatow w Komisji A\n");
        exit(EXIT_FAILURE);
    }

    // Oczekiwanie na liczbę studentów i przepisujących
    semafor_wait(semID, 4); // czeka na sygnal od dziekana
    shm_ptr->PidkomisjiA = mainprogKomisjaA;
    shm_ptr->Liczba_studentow_dokomB=shm_ptr->ilosc_studentow_na_wybranym_kierunku; 
    Liczba_przepisujacych = shm_ptr->ilosc_osob_przepisujacych;
    Liczba_studentow_do_zegzaminowania = shm_ptr->ilosc_studentow_na_wybranym_kierunku;
    printf("[KOMISJA A] Liczba przepisujących: %d\n", Liczba_przepisujacych);
    printf("[KOMISJA A] Liczba studentów do egzaminu: %d\n", Liczba_studentow_do_zegzaminowania);
    Liczba_komunikatow_KOMA = (Liczba_studentow_do_zegzaminowania)*2;
    shm_ptr->Komisjazakonczenie = 0;  

    // ========== Tworzenie wątków członków Komisji A ==========

    pthread_mutex_init(&mutexA, NULL);
    pthread_mutex_init(&mutexA2, NULL);

    if (pthread_create(&KAczlonek2, NULL, &KA_czlonek2, NULL) != 0) {
        perror("Blad przy inicjowaniu watku czlonka 2 Komisji A\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&KAczlonek3, NULL, &KA_czlonek3, NULL) != 0) {
        perror("Blad przy inicjowaniu watku czlonka 3 Komisji A\n");
        exit(EXIT_FAILURE);
    }

    // ========== Logika przewodniczącego Komisji A ==========

    int counter1 = 0;

    typedef struct {
    int pid;
    float ocenaPrzew;  // ocena od przewodniczącego
    float ocenaCz2;    // ocena od członka 2
    float ocenaCz3;    // ocena od członka 3
    float srednia;      // średnia z powyższych ocen
    } OcenyStud;
    OcenyStud OcenyKA[MAX_SIZE_OCENY];

    while (!ewakuacja_komisjaA) {

        // Odbieram wiadomość od studenta 
        
        if((msgrcv(msgID, &msg, sizeof(Kom_bufor)-sizeof(long), PKAALL, 0)) == -1){
            semafor_wait(semID,7);
            handle_msgrcv_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }

        printf("Przewodniczacy A odebral wiadomosc i obsluguje studenta o pid %d\n", msg.pidStudenta);
        PAE++;

        if(msg.zdaje_czy_pytania == 0){ // 0->chce pytania albo przepisać, 1-> chce zdawać


            if(msg.czy_zdane == 1){ // tutaj gdy student przepisuje ocene z zeszlego roku
                //Wysyla komunikat, ze uznaje jego przepisanie i moze isc do komisji B

                msg.mtype=msg.pidStudenta;
                printf("Student o pid: %d, ma przepisana ocene: %0.1f z zeszlego roku\n",msg.pidStudenta,msg.ocena_koncowa);
                if((msgsnd(msgID,&msg,sizeof(Kom_bufor)-sizeof(long),0))== -1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }

                //informuje innych czlonkow komisji o nowym studencie
                powiadom.mtype = CZ2KAALL;
                if((msgsnd(msgID,&powiadom,0,0))== -1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }
                powiadom.mtype = CZ3KAALL;
                if((msgsnd(msgID,&powiadom,0,0)) == -1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }
                //jeśli student przepisuje ocene, to wysyla tylko jeden komunikat, więc musze zwiekszyc PAE o 1
                //bo zakladam, ze kazdy student, który zdaje wysyla po 2 komunikaty
                PAE++;

                //zapisuje ocene w bufferze, który potem przesylam do dziekana
                OcenyKA[counter1].pid = msg.pidStudenta;
                OcenyKA[counter1].ocenaPrzew = msg.ocena_koncowa;  
                OcenyKA[counter1].ocenaCz2 = msg.ocena_koncowa;
                OcenyKA[counter1].ocenaCz3 = msg.ocena_koncowa;
                OcenyKA[counter1].srednia = msg.ocena_koncowa;
                counter1++;

                Krotkie_powiadomienie PKAw4; // od czlonka 2
                Krotkie_powiadomienie PKAw5; // od czlonka 3

                if((msgrcv(msgID,&PKAw4,0,700,0)) == -1){ // od czl.2
                    semafor_wait(semID,7);
                    handle_msgrcv_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                } 
                if((msgrcv(msgID,&PKAw5,0,701,0)) == -1){ // od czl.3
                    semafor_wait(semID,7);
                    handle_msgrcv_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                } 
                
                //to w przypadku, gdy ostatni student przepisuje ocene (i jest oceniany jako ostatni)
                if (PAE == Liczba_komunikatow_KOMA) {
                    shm_ptr->flagakomA=1; // to dla komisji B
                    flaga_komisjaA = 1; // to jest dla wątków tej samej komisji

                    int fd = open(FIFO_PATH, O_WRONLY);
                    if (fd == -1) {
                        perror("blad open FIFO\n");
                        exit(1);
                    }

                    char buffer[4096];
                    int bufferIndex = 0;

                    for (int i = 0; i < counter1; i++) {
                        // wypiszemy teraz: indeks, pid, ocenaPrzew, ocenaCz2, ocenaCz3, średnia
                        int len = snprintf(
                            buffer + bufferIndex,
                            sizeof(buffer) - bufferIndex,
                            "%d %.1f %.1f %.1f %.1f\n",
                            OcenyKA[i].pid,
                            OcenyKA[i].ocenaPrzew,
                            OcenyKA[i].ocenaCz2,
                            OcenyKA[i].ocenaCz3,
                            OcenyKA[i].srednia
                        );
                        bufferIndex += len;
                    }

                    write(fd, buffer, bufferIndex);
                    close(fd);

                    powiadom.mtype = CZ2KAALL;
                    if((msgsnd(msgID,&powiadom,0,0)) == -1){
                        semafor_wait(semID,7);
                        handle_msgsnd_error_with_logging(Zapiszlog);
                        semafor_signal(semID,7);
                        exit(EXIT_FAILURE);
                    }
                    powiadom.mtype = CZ3KAALL;
                    if((msgsnd(msgID,&powiadom,0,0)) == -1){
                        semafor_wait(semID,7);
                        handle_msgsnd_error_with_logging(Zapiszlog);
                        semafor_signal(semID,7);
                        exit(EXIT_FAILURE);
                    }
            
                    break;
                }

            }else{ // tutaj, gdy student chce pytania


                //informuje innych czlonkow komisji o nowym studencie
                powiadom.mtype = CZ2KAALL;
                if((msgsnd(msgID,&powiadom,0,0)) == -1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }
                powiadom.mtype = CZ3KAALL;
                if((msgsnd(msgID,&powiadom,0,0)) == -1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }

                //zadaje pytanie studentowi
                Pytanie PPKA;

                //tutaj symulacja losowania i przygotowywania pytania
                //int PP = rand_r(&seed) % 6 + 5;
                    
                //sleep(PP);
                const char* wylos_pyt_PKA = Wylosuj_pytanie_PKA(seed);
                strncpy(PPKA.pytanie, wylos_pyt_PKA, MAX_Q_LENG - 1);
                PPKA.pytanie[MAX_Q_LENG - 1] = '\0'; 
                PPKA.IDczlonkakomisji=PKA;
                PPKA.mtype = msg.pidStudenta;
                if((msgsnd(msgID, &PPKA,sizeof(Pytanie)-sizeof(long),0)) == -1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }
                    
                //czeka na potwierdzenie, czy inni juz wyslali pytania
                printf("Przewodniczacy A wyslal pytania studentowi o pid: %d\n",msg.pidStudenta);

                Krotkie_powiadomienie PKAw2; // od czlonka 2
                Krotkie_powiadomienie PKAw3; // od czlonka 3
                    
                if((msgrcv(msgID,&PKAw2,0,690,0))== -1){ // od.czl2
                    semafor_wait(semID,7);
                    handle_msgrcv_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }
                if((msgrcv(msgID,&PKAw3,0,691,0)) == -1){
                    semafor_wait(semID,7);
                    handle_msgrcv_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }
                
            }

        }else{
            //tutaj jak chce zdawać

            //informuje innych czlonkow komisji o nowym studencie
            powiadom.mtype = CZ2KAALL;
            if((msgsnd(msgID,&powiadom,0,0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
            powiadom.mtype = CZ3KAALL;
            if((msgsnd(msgID,&powiadom,0,0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
            
            //teraz czeka na odpowiedź studenta
            Pytanie PPKA1;
            if((msgrcv(msgID,&PPKA1,sizeof(Pytanie)-sizeof(long),PKA,0)) == -1){
                semafor_wait(semID,7);
                handle_msgrcv_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

            //ocenia odpowiedź
            float ocenaPKA = wylosuj_ocene(&seed);
                    
            //zapisuje swoją ocene w pamieci dzielonej
            shm_ptr->students[msg.index_studenta].ocena_praktykaP=ocenaPKA;

            //odbierz powiadomienie o ocenach od innych czlonkow
            Wiadomosc_zocena PKAw2; // od czlonka 2
            Wiadomosc_zocena PKAw3; // od czlonka 3
                    
            if((msgrcv(msgID,&PKAw2,sizeof(Wiadomosc_zocena)-sizeof(long),556,0)) == -1){
                semafor_wait(semID,7);
                handle_msgrcv_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
            if((msgrcv(msgID,&PKAw3,sizeof(Wiadomosc_zocena)-sizeof(long),555,0)) == -1){
                semafor_wait(semID,7);
                handle_msgrcv_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }   

            float srednia_ocena = oblicz_srednia_i_zaokragl(ocenaPKA,PKAw2.ocena,PKAw3.ocena);
            if (srednia_ocena < 2.5){
                shm_ptr->Liczba_studentow_dokomB--;
            }

            //zapisuje ocene w bufferze, który potem przesylam do dziekana
            OcenyKA[counter1].pid = msg.pidStudenta;
            OcenyKA[counter1].ocenaPrzew = ocenaPKA;  
            OcenyKA[counter1].ocenaCz2 = PKAw2.ocena;
            OcenyKA[counter1].ocenaCz3 = PKAw3.ocena;
            OcenyKA[counter1].srednia = srednia_ocena;
            counter1++;

            //zapisuje ocene koncowa z egzaminu dla danego studenta
            shm_ptr->students[msg.index_studenta].ocena_praktykasr=srednia_ocena;

            printf("Student o pid: %d, otrzymal od przewodniczacego ocene: %0.1f\n",msg.pidStudenta,ocenaPKA);
            printf("Student o pid: %d, otrzymal od Czlonka2 Komisji A ocene: %0.1f\n",msg.pidStudenta,PKAw2.ocena);
            printf("Student o pid: %d, otrzymal od Czlonka3 Komisji A ocene: %0.1f\n",msg.pidStudenta,PKAw3.ocena);
            printf("Student o pid: %d otrzymal ocene z czesci praktycznej: %0.1f\n",msg.pidStudenta,srednia_ocena);
            semafor_wait(semID,7);
            Zapiszlog("Przewodniczacy Komisji A zapisał ocene: %0.1f studentowi o pid: %d",ocenaPKA,msg.pidStudenta);
            semafor_signal(semID,7);
            //wyslij wiadomosc z ocena do studenta

            Wiadomosc_zocena Zwrotna_do_studenta;
            Zwrotna_do_studenta.mtype = msg.pidStudenta;
            Zwrotna_do_studenta.ocena=srednia_ocena;

            if ((msgsnd(msgID,&Zwrotna_do_studenta,sizeof(Wiadomosc_zocena)-sizeof(long),0))== -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

            if (PAE == Liczba_komunikatow_KOMA) {

                shm_ptr->flagakomA=1;
                flaga_komisjaA=1;
                printf("KOMA Liczba:%d\n",shm_ptr->Liczba_studentow_dokomB);
                
                semafor_wait(semID,7);
                Zapiszlog("Komisja A czeka na otworzenie kolejki fifo od dziekana");
                semafor_signal(semID,7);
                int fd = open(FIFO_PATH, O_WRONLY);
                if (fd == -1) {
                    perror("blad open FIFO\n");
                    exit(1);
                }

                char buffer[4096];
                int bufferIndex = 0;

                for (int i = 0; i < counter1; i++) {
                    // wypiszemy teraz: indeks, pid, ocenaPrzew, ocenaCz2, ocenaCz3, średnia
                    int len = snprintf(
                        buffer + bufferIndex,
                        sizeof(buffer) - bufferIndex,
                        "%d %.1f %.1f %.1f %.1f\n",
                        OcenyKA[i].pid,
                        OcenyKA[i].ocenaPrzew,
                        OcenyKA[i].ocenaCz2,
                        OcenyKA[i].ocenaCz3,
                        OcenyKA[i].srednia
                    );
                    bufferIndex += len;
                }

                write(fd, buffer, bufferIndex);
                close(fd);
                semafor_wait(semID,7);
                Zapiszlog("Komisja A zapisała wyniku do fifo");
                semafor_signal(semID,7);

                powiadom.mtype = CZ2KAALL;
                if((msgsnd(msgID,&powiadom,0,0))==-1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }
                powiadom.mtype = CZ3KAALL;
                if((msgsnd(msgID,&powiadom,0,0))==-1){
                    semafor_wait(semID,7);
                    handle_msgsnd_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }
        
                break;
            }
        }
                    
    }


    // ========== Kończenie pracy komisji A ==========

    if(ewakuacja_komisjaA == 1){
        semafor_wait(semID,7);
        Zapiszlog("Komisja A rozpoczela ewakuacje i wysyla oceny do dziekana");
        semafor_signal(semID,7);
        semafor_signal(semID,8);

        int fd = open(FIFO_PATH, O_WRONLY);
        if (fd == -1) {
            perror("blad open FIFO\n");
            exit(1);
        }

        char buffer[4096];
        int bufferIndex = 0;

        for (int i = 0; i < counter1; i++) {
            // wypiszemy teraz: indeks, pid, ocenaPrzew, ocenaCz2, ocenaCz3, średnia
            int len = snprintf(
                buffer + bufferIndex,
                sizeof(buffer) - bufferIndex,
                "%d %.1f %.1f %.1f %.1f\n",
                OcenyKA[i].pid,
                OcenyKA[i].ocenaPrzew,
                OcenyKA[i].ocenaCz2,
                OcenyKA[i].ocenaCz3,
                OcenyKA[i].srednia
            );
            bufferIndex += len;
        }

        write(fd, buffer, bufferIndex);
        close(fd);
        semafor_wait(semID,7);
        Zapiszlog("Komisja A skonczyla wysylac dane do dziekana i ucieka z budynku");
        semafor_signal(semID,7);
        Krotkie_powiadomienie ew;
        ew.mtype = EWAK;
        if((msgsnd(msgID, &ew, 0,0)) == -1){
            semafor_wait(semID,7);
            handle_msgsnd_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
        }

    }

    if (pthread_join(KAczlonek2, NULL) != 0) {
        perror("Blad przy joinie czlonka 2 komisji A\n");
    }
    if (pthread_join(KAczlonek3, NULL) != 0) {
        perror("Blad przy joinie czlonka 3 komisji A\n");
    }

    pthread_mutex_destroy(&mutexA);
    pthread_mutex_destroy(&mutexA2);

    shmdt(shm_ptr);

    if(ewakuacja_komisjaA == 1){
        printf("[KOMISJA A] Przewodniczacy komisji A ewakuowal sie pomyslnie\n");
        semafor_wait(semID,7);
        Zapiszlog("[KOMISJA A] Przewodniczacy komisji A ewakuowal sie pomyslnie");
        semafor_signal(semID,7);
    }else{
        printf("[KOMISJA A] Przewodniczacy komisji A zakonczyl swoja prace i wrocil do domu\n");
        semafor_wait(semID,7);
        Zapiszlog("[KOMISJA A] Przewodniczacy komisji A zakonczyl swoja prace i wrocil do domu");
        semafor_signal(semID,7);
    }

    return;
}

void* KB_czlonek2(void* arg) {
    (void)arg;

    unsigned int seedcz2KB = time(NULL) ^ pthread_self();
    
    while(!ewakuacja_komisjaB){
        // czeka na informacje od Przewodniczacego na temat nowego studenta
        
        if((msgrcv(msgID, &powiadomB, 0, CZ2KBALL, 0)) == -1){
            semafor_wait(semID,7);
            handle_msgrcv_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }   

        if(flaga_komisjaB == 1){
            break;
        }

        if(msgKBCZ2.zdaje_czy_pytania == 0){//student chce pytania
            // zadaje pytanie studentowi
            Pytanie PCZKB2;

            // tutaj symulacja losowania i przygotowywania pytania
            //int bPC2 = rand_r(&seedcz2KB) % 6 + 5;
            // sleep(bPC2);
            const char* wylos_pyt_KBCZ2 = Wylosuj_pytanie_CZ2KB(seedcz2KB);
            strncpy(PCZKB2.pytanie, wylos_pyt_KBCZ2, MAX_Q_LENG - 1);
            PCZKB2.pytanie[MAX_Q_LENG - 1] = '\0'; 
            PCZKB2.IDczlonkakomisji = CZ2KB;
            PCZKB2.mtype = msgKBCZ2.pidStudenta;
            if ((msgsnd(msgID, &PCZKB2, sizeof(Pytanie) - sizeof(long), 0))== -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

            //tutaj potwierdzenie do przewodniczacego ze wyslal pytania
            Krotkie_powiadomienie bpowcz2;
            bpowcz2.mtype = 270;
            if((msgsnd(msgID, &bpowcz2,0,0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

        }else{
            // tutaj jak chce zdawac
            // teraz czeka na odpowiedź studenta
            Pytanie PCZKB2;
            if((msgrcv(msgID, &PCZKB2, sizeof(Pytanie) - sizeof(long), CZ2KB, 0)) == -1){
                semafor_wait(semID,7);
                handle_msgrcv_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

            // ocenia odpowiedź
            float ocenaCZ2KB = wylosuj_ocene(&seedcz2KB);

            // zapisuje ocene w pamieci dzielonej
            shm_ptr->students[msgKBCZ2.index_studenta].ocena_teoria2 = ocenaCZ2KB;
            semafor_wait(semID,7);
            Zapiszlog("Czlonek 2 Komisji B zapisał ocene: %0.1f studentowi o pid: %d",ocenaCZ2KB,msgKBCZ2.pidStudenta);
            semafor_signal(semID,7);

            // wyslij wiadomosc do przewodniczacego, ze wpisales ocene
            Wiadomosc_zocena CZ2BKwiad;
            CZ2BKwiad.mtype = 272;
            CZ2BKwiad.ocena = ocenaCZ2KB;

            if((msgsnd(msgID, &CZ2BKwiad, sizeof(Wiadomosc_zocena) - sizeof(long), 0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
        }

    }
    if(ewakuacja_komisjaB == 1){
        printf("Czlonek 2 komisji B ewakuowal sie pomyslnie\n");
        semafor_wait(semID,7);
        Zapiszlog("Czlonek 2 komisji B ewakuowal sie pomyslnie");
        semafor_signal(semID,7);
    }else{
        printf("[KOMISJA] Czlonek 2 Komisji B, zakonczyl swoja prace i wrocil do domu\n");
        semafor_wait(semID,7);
        Zapiszlog("[KOMISJA] Czlonek 2 Komisji B, zakonczyl swoja prace i wrocil do domu");
        semafor_signal(semID,7);
    }
    
    return NULL;
}

void* KB_czlonek3(void* arg) {
    (void)arg;//zeby kompilator nie wariował

    unsigned int seedcz3KB = time(NULL) ^ pthread_self();

    while(!ewakuacja_komisjaB){

        // czeka na informacje od Przewodniczacego na temat nowego studenta
        
        if((msgrcv(msgID, &powiadomB, 0, CZ3KBALL, 0)) == -1){
            semafor_wait(semID,7);
            handle_msgrcv_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }

        if(flaga_komisjaB == 1){
            break;
        }


        if(msgKBCZ3.zdaje_czy_pytania == 0){//student chce pytania
            // zadaje pytanie studentowi
            Pytanie PCZKB3;

            // tutaj symulacja losowania i przygotowywania pytania
            //int bPC3 = rand_r(&seedcz3KB) % 6 + 5;
            // sleep(bPC3);
            const char* wylos_pyt_KBCZ3 = Wylosuj_pytanie_CZ3KB(seedcz3KB);
            strncpy(PCZKB3.pytanie, wylos_pyt_KBCZ3, MAX_Q_LENG - 1);
            PCZKB3.pytanie[MAX_Q_LENG - 1] = '\0'; 
            PCZKB3.IDczlonkakomisji = CZ3KB;
            PCZKB3.mtype = msgKBCZ3.pidStudenta;
            if((msgsnd(msgID, &PCZKB3, sizeof(Pytanie) - sizeof(long), 0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

            //tutaj potwierdzenie do przewodniczacego ze wyslal pytania
            Krotkie_powiadomienie bpowcz3;
            bpowcz3.mtype = 271;
            if((msgsnd(msgID, &bpowcz3,0,0))== -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

        }else{
            // tutaj jak chce zdawac
            // teraz czeka na odpowiedź studenta
            Pytanie PCZKB3;
            if((msgrcv(msgID, &PCZKB3, sizeof(Pytanie) - sizeof(long), CZ3KB, 0)) == -1){
                semafor_wait(semID,7);
                handle_msgrcv_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

            // ocenia odpowiedź
            float ocenaCZ3KB = wylosuj_ocene(&seedcz3KB);

            // zapisuje ocene w pamieci dzielonej
            shm_ptr->students[msgKBCZ3.index_studenta].ocena_teoria3 = ocenaCZ3KB;

            semafor_wait(semID,7);
            Zapiszlog("Czlonek 3 Komisji B zapisał ocene: %0.1f studentowi o pid: %d",ocenaCZ3KB,msgKBCZ3.pidStudenta);
            semafor_signal(semID,7);
            

            // wyslij wiadomosc do przewodniczacego, ze wpisales ocene
            Wiadomosc_zocena CZ3BKwiad;
            CZ3BKwiad.mtype = 273;
            CZ3BKwiad.ocena = ocenaCZ3KB;

            if((msgsnd(msgID, &CZ3BKwiad, sizeof(Wiadomosc_zocena) - sizeof(long), 0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
        }
        
    }
    if(ewakuacja_komisjaB == 1){
        printf("[KOMISJA B] Czlonek 3 komisji B ewakuowal sie pomyslnie\n");
        semafor_wait(semID,7);
        Zapiszlog("[KOMISJA B] Czlonek 3 komisji B ewakuowal sie pomyslnie");
        semafor_signal(semID,7);
    }else{
        printf("[KOMISJA B] Czlonek 3 komisji B zakonczyl swoja prace i wrocil do domu\n");
        semafor_wait(semID,7);
        Zapiszlog("[KOMISJA B] Czlonek 3 komisji B zakonczyl swoja prace i wrocil do domu");
        semafor_signal(semID,7);
    }

    return NULL;
}

void runKomisjaB() {
   
    mainprogKomisjaB = getpid();
    unsigned int seedB = time(NULL) ^ mainprogKomisjaB;

    struct sigaction act_ewakuacja;
    memset(&act_ewakuacja, 0, sizeof(act_ewakuacja));
    act_ewakuacja.sa_sigaction = sigusr1_handler_komisjaB;
    act_ewakuacja.sa_flags = SA_SIGINFO;
    sigemptyset(&act_ewakuacja.sa_mask);

    if (sigaction(SIGUSR1, &act_ewakuacja, NULL) == -1) {
        perror("[KOMISJA B] Blad ustawienia sigaction dla SIGUSR1");
        exit(EXIT_FAILURE);
    }

    utworz_klucze();
    semID = semget(kluczs, 9, 0666); 
    if (semID == -1) {
        perror("Blad dolaczania semaforow w komisji B\n");
        exit(EXIT_FAILURE);
    }

    shmID = shmget(kluczm, sizeof(SHARED_MEMORY), 0666);
    if (shmID == -1) {
        perror("Blad przy tworzeniu pamieci dzielonej w Komisji B\n");
        exit(EXIT_FAILURE);
    }

    shm_ptr = (SHARED_MEMORY*)shmat(shmID, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("Blad przy dolaczaniu pamieci dzielonej w Komisji B\n");
        exit(EXIT_FAILURE);
    }

    msgID = msgget(kluczk, 0666);
    if (msgID == -1) {
        perror("Blad tworzenia kolejki komunikatow w Komisji B\n");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_init(&mutexB, NULL);

    if (pthread_create(&KBczlonek2, NULL, &KB_czlonek2, NULL) != 0) {
        perror("Blad przy inicjowaniu watku czlonka 2 Komisji B\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&KBczlonek3, NULL, &KB_czlonek3, NULL) != 0) {
        perror("Blad przy inicjowaniu watku czlonka 3 Komisji B\n");
        exit(EXIT_FAILURE);
    }
    shm_ptr->PidkomisjiB = mainprogKomisjaB;

    semafor_wait(semID,4);
    int counterKB = 0;

    typedef struct {
    int pid;
    float ocenaPrzew;  // ocena od przewodniczącego
    float ocenaCz2;    // ocena od członka 2
    float ocenaCz3;    // ocena od członka 3
    float srednia;      // średnia z powyższych ocen
    } OcenyStud;
    OcenyStud OcenyKB[MAX_SIZE_OCENY];


    int Liczba_stud_obsluzonych=0;
    while (!ewakuacja_komisjaB) {
        
        printf("Liczba stud obsluzonych: %d, Liczba stud do zegzaminowania: %d\n",Liczba_stud_obsluzonych,shm_ptr->Liczba_studentow_dokomB);

        // Odbieram wiadomość od studenta 
        if((msgrcv(msgID, &msgKB, sizeof(Kom_bufor)-sizeof(long), PKBALL, 0)) == -1){
            semafor_wait(semID,7);
            handle_msgrcv_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }

        msgKBCZ2=msgKB;
        msgKBCZ3=msgKB;
        printf("Przewodniczacy B odebral wiadomosc i obsluguje studenta o pid %d\n", msgKB.pidStudenta);

        if(msgKB.zdaje_czy_pytania == 0){ // STUDENT CHCE PYTANIA
            

            //informuje innych czlonkow komisji o nowym studencie
            powiadomB.mtype = CZ2KBALL;
            if((msgsnd(msgID,&powiadomB,0,0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
            powiadomB.mtype = CZ3KBALL;
            if((msgsnd(msgID,&powiadomB,0,0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

            //zadaje pytanie studentowi
            Pytanie PPKB;

            //tutaj symulacja losowania i przygotowywania pytania
            //int PPB = rand_r(&seedB) % 6 + 5;

            //sleep(PP);
            const char* wylos_pyt_PKB = Wylosuj_Pytanie_PKB(seedB);
            strncpy(PPKB.pytanie, wylos_pyt_PKB, MAX_Q_LENG - 1);
            PPKB.pytanie[MAX_Q_LENG - 1] = '\0'; 
            PPKB.IDczlonkakomisji=PKB;
            PPKB.mtype = msgKB.pidStudenta;

            if((msgsnd(msgID, &PPKB,sizeof(Pytanie)-sizeof(long),0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
                
            //czeka na potwierdzenie, czy inni juz wyslali pytania
            printf("Przewodniczacy Komisji B wyslal pytania studentowi o pid: %d\n",msgKB.pidStudenta);

            Krotkie_powiadomienie PKBP1; // od czlonka 2
            Krotkie_powiadomienie PKBP2; // od czlonka 3
                
            if((msgrcv(msgID,&PKBP1,0,270,0)) == -1){
                semafor_wait(semID,7);
                handle_msgrcv_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
            if((msgrcv(msgID,&PKBP2,0,271,0)) == -1){
                semafor_wait(semID,7);
                handle_msgrcv_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }


        }else{ //STUDENT CHCE ZDAWAĆ

            //informuje innych czlonkow komisji o nowym studencie
            powiadomB.mtype = CZ2KBALL;
            if((msgsnd(msgID,&powiadomB,0,0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
            powiadomB.mtype = CZ3KBALL;
            if((msgsnd(msgID,&powiadomB,0,0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

            //teraz czeka na odpowiedź studenta
            Pytanie PPKB1;
            if((msgrcv(msgID,&PPKB1,sizeof(Pytanie)-sizeof(long),PKB,0)) == -1){
                semafor_wait(semID,7);
                handle_msgrcv_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

            //ocenia odpowiedź
            float ocenaPKB = wylosuj_ocene(&seedB);
                    
            //zapisuje swoją ocene w pamieci dzielonej
            shm_ptr->students[msgKB.index_studenta].ocena_teoriaP=ocenaPKB;

            //odbierz powiadomienie o ocenach od innych czlonkow
            Wiadomosc_zocena PKBZW1; // od czlonka 2
            Wiadomosc_zocena PKBZW2; // od czlonka 3
                    
            if((msgrcv(msgID,&PKBZW1,sizeof(Wiadomosc_zocena)-sizeof(long),272,0)) == -1){
                semafor_wait(semID,7);
                handle_msgrcv_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
            if((msgrcv(msgID,&PKBZW2,sizeof(Wiadomosc_zocena)-sizeof(long),273,0)) == -1){
                semafor_wait(semID,7);
                handle_msgrcv_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }

            float srednia_ocena = oblicz_srednia_i_zaokragl(ocenaPKB,PKBZW1.ocena,PKBZW2.ocena);

            //zapisuje ocene koncowa z egzaminu dla danego studenta
            shm_ptr->students[msgKB.index_studenta].ocena_teoriasr=srednia_ocena;

            //zapisuje ocene do tablicy
            OcenyKB[counterKB].pid = msgKB.pidStudenta;
            OcenyKB[counterKB].ocenaPrzew = ocenaPKB;  // ocena od przewodniczącego
            OcenyKB[counterKB].ocenaCz2 = PKBZW1.ocena; // ocena od członka 2
            OcenyKB[counterKB].ocenaCz3 = PKBZW2.ocena; // ocena od członka 3
            OcenyKB[counterKB].srednia = srednia_ocena; // średnia z ocen
            counterKB++;
        

            printf("Student o pid: %d, otrzymal od przewodniczacego Komisji B ocene: %0.1f\n",msgKB.pidStudenta,ocenaPKB);
            printf("Student o pid: %d, otrzymal od Czlonka2 Komisji B ocene: %0.1f\n",msgKB.pidStudenta,PKBZW1.ocena);
            printf("Student o pid: %d, otrzymal od Czlonka3 Komisji B ocene: %0.1f\n",msgKB.pidStudenta,PKBZW2.ocena);
            printf("Student o pid: %d otrzymal ocene z czesci teoretycznej: %0.1f\n",msgKB.pidStudenta,srednia_ocena);
            
            semafor_wait(semID,7);
            Zapiszlog("Przewodniczacy Komisji B zapisał ocene: %0.1f studentowi o pid: %d",ocenaPKB,msgKB.pidStudenta);
            semafor_signal(semID,7);
            //wyslij wiadomosc z ocena do studenta

            Wiadomosc_zocena Zwrotna_do_studenta;
            Zwrotna_do_studenta.mtype = msgKB.pidStudenta;
            Zwrotna_do_studenta.ocena=srednia_ocena;

            if((msgsnd(msgID,&Zwrotna_do_studenta,sizeof(Wiadomosc_zocena)-sizeof(long),0)) == -1){
                semafor_wait(semID,7);
                handle_msgsnd_error_with_logging(Zapiszlog);
                semafor_signal(semID,7);
                exit(EXIT_FAILURE);
            }
            Liczba_stud_obsluzonych++;

            if(shm_ptr->flagakomA ==1){
            printf("Komisjab wszedlem tu\n");
            printf("z flaga:Liczba stud obsluzonych: %d, Liczba stud do zegzaminowania: %d\n",Liczba_stud_obsluzonych,shm_ptr->Liczba_studentow_dokomB);
                if(shm_ptr->Liczba_studentow_dokomB == Liczba_stud_obsluzonych){
                    flaga_komisjaB=1; // flaga dla czlonków komisji zeby wyszli z petli

                    // powiadamiam czlonkow komisji
                    powiadomB.mtype = CZ2KBALL;
                    if((msgsnd(msgID, &powiadomB, 0, 0)) == -1){
                        semafor_wait(semID,7);
                        handle_msgsnd_error_with_logging(Zapiszlog);
                        semafor_signal(semID,7);
                        exit(EXIT_FAILURE);
                    }
                    powiadomB.mtype = CZ3KBALL;
                    if((msgsnd(msgID, &powiadomB, 0, 0)) == -1){
                        semafor_wait(semID,7);
                        handle_msgsnd_error_with_logging(Zapiszlog);
                        semafor_signal(semID,7);
                        exit(EXIT_FAILURE);
                    }
                    
                    //wysylam oceny dziekanowi
                    semafor_wait(semID,7);
                    Zapiszlog("[KOMISJA B PRZEWODNICZACY] Czekam na dziekana zeby fifo otworzyl");
                    semafor_signal(semID,7);

                    int fd2 = open(FIFO_PATH2, O_WRONLY);
                    if (fd2 == -1) {
                        perror("blad open FIFO\n");
                        exit(1);
                    }

                    char buffer2[4096];
                    int bufferIndexB = 0;

                    for (int i = 0; i < counterKB; i++) {
                        // wypiszemy teraz: pid, ocenaPrzew, ocenaCz2, ocenaCz3, średnia
                        int len = snprintf(
                            buffer2 + bufferIndexB,
                            sizeof(buffer2) - bufferIndexB,
                            "%d %.1f %.1f %.1f %.1f\n",
                            OcenyKB[i].pid,
                            OcenyKB[i].ocenaPrzew,
                            OcenyKB[i].ocenaCz2,
                            OcenyKB[i].ocenaCz3,
                            OcenyKB[i].srednia
                        );
                        bufferIndexB += len;
                    }
                    printf("Komisja B zapisuje %d ocen do FIFO_PATH2\n", counterKB);
                    semafor_wait(semID,7);
                    Zapiszlog("Komisja B zapisala dane w fifo");
                    semafor_signal(semID,7);
                    write(fd2, buffer2, bufferIndexB);
                    close(fd2);
                    break;
                }
                semafor_wait(semID,7);
                Zapiszlog("[KOMISJA B PRZEWODNICZACY] Nie obslużono jeszcze wszystkich studentów, obsluguje dalej");
                semafor_signal(semID,7);
            }
        }
    }
    
    if(ewakuacja_komisjaB == 1){
        //wysylam oceny dziekanowi w czasie ewakuacji
        semafor_wait(semID,7);
        Zapiszlog("[KOMISJA B PRZEWODNICZACY] Czekam na dziekana zeby fifo otworzyl podczas ewakuacji");
        semafor_signal(semID,7);
        Krotkie_powiadomienie ewB;
        if((msgrcv(msgID, &ewB,0,EWAK,0)) == -1){
            semafor_wait(semID,7);
            handle_msgrcv_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
        }

        int fd2 = open(FIFO_PATH2, O_WRONLY);
        if (fd2 == -1) {
            perror("blad open FIFO w ewakuacji \n");
            Zapiszlog("Blad open fifo w Komisji B");
            exit(1);
        }

        char buffer2[4096];
        int bufferIndexB = 0;

        for (int i = 0; i < counterKB; i++) {
            // wypiszemy teraz: pid, ocenaPrzew, ocenaCz2, ocenaCz3, średnia
            int len = snprintf(
                buffer2 + bufferIndexB,
                sizeof(buffer2) - bufferIndexB,
                "%d %.1f %.1f %.1f %.1f\n",
                OcenyKB[i].pid,
                OcenyKB[i].ocenaPrzew,
                OcenyKB[i].ocenaCz2,
                OcenyKB[i].ocenaCz3,
                OcenyKB[i].srednia
            );
            bufferIndexB += len;
        }
        printf("[Komisja B] przewodniczacy komisji B zapisuje %d ocen do FIFO_PATH2 w czasie ewakuacji\n", counterKB);
        semafor_wait(semID,7);
        Zapiszlog("[Komisja B] zapisala dane w fifo w czasie ewakuacji");
        semafor_signal(semID,7);
        write(fd2, buffer2, bufferIndexB);
        close(fd2);
    }


    if (pthread_join(KBczlonek2, NULL) != 0) {
        perror("Blad przy joinie czlonka 2 komisji B\n");
    }
    if (pthread_join(KBczlonek3, NULL) != 0) {
        perror("Blad przy joinie czlonka 3 komisji B\n");
    }

    pthread_mutex_destroy(&mutexB);
   

    semafor_signal(semID,8); // daje znać dziekanowi że może już czyścić zasoby
    shmdt(shm_ptr);
    //tutaj konczy dzialanie komisja B

    if(ewakuacja_komisjaB == 1){
        printf("[KOMISJA B] Przewodniczacy komisji B ewakuowal sie pomyslnie\n");
        semafor_wait(semID,7);
        Zapiszlog("[KOMISJA B] Przewodniczacy komisji B ewakuowal sie pomyslnie");
        semafor_signal(semID,7);
    }else{
        printf("[KOMISJA B] Przewodniczacy komisji B zakonczyl swoja prace i wrocil do domu\n");
        semafor_wait(semID,7);
        Zapiszlog("[KOMISJA B] Przewodniczacy komisji B zakonczyl swoja prace i wrocil do domu");
        semafor_signal(semID,7);
    }

    return;
}

void sigusr1_handler_komisjaA(int sig, siginfo_t *siginfo, void *context) {
    (void)sig;
    (void)siginfo;
    (void)context; // zeby sie kompilator nie czepiał
    ewakuacja_komisjaA = 1;
    Zapiszlog("[Komisja A] Otrzymano sygnał SIGUSR1");
    printf("[KOMISJA A] Otrzymano sygnał ewakuacji (SIGUSR1).\n");
}

void sigusr1_handler_komisjaB(int sig, siginfo_t *siginfo, void *context) {
    (void)sig;
    (void)siginfo;
    (void)context; //zeby sie kompilator nie czepiał
    ewakuacja_komisjaB = 1;
    Zapiszlog("[Komisja B] Otrzymano sygnał SIGUSR1");
    printf("[KOMISJA B] Otrzymano sygnał ewakuacji (SIGUSR1).\n");
}


int main(int argc, char *argv[])
{
    if (argc > 1) {
        if (strcmp(argv[1], "A") == 0) {
            runKomisjaA(); 
            return 0;
        } else if (strcmp(argv[1], "B") == 0) {
            runKomisjaB();
            return 0;
        } else {
            fprintf(stderr, "Nieznany argument: %s (dozwolone: A lub B)\n", argv[1]);
            exit(EXIT_FAILURE);
        }
    } 
    else {
        printf("[MAIN] Uruchamiam procesy Komisji A i B...\n");

        pid_t pidA = fork();
        if (pidA == -1) {
            perror("fork() dla Komisji A nie powiodl sie");
            exit(EXIT_FAILURE);
        }
        if (pidA == 0) {
            execl("./Komisja", "Komisja", "A", (char*)NULL);
            perror("Blad exec() dla Komisji A");
            exit(EXIT_FAILURE);
        }

        pid_t pidB = fork();
        if (pidB == -1) {
            perror("fork() dla Komisji B nie powiodl sie");
            exit(EXIT_FAILURE);
        }
        if (pidB == 0) {
            execl("./Komisja", "Komisja", "B", (char*)NULL);
            perror("Blad exec() dla Komisji B");
            exit(EXIT_FAILURE);
        }

        wait(NULL);
        wait(NULL);

        if(ewakuacja_komisjaA == 1 && ewakuacja_komisjaB == 1){
            printf("[MAIN - Komisja] Obie komisje Pomyslnie sie ewakuowaly\n"); 
        }else{
            printf("[MAIN - Komisja] Obie komisje zakończyły egzamin.\n");
        }
        return 0;
    }

    return 0; //awaryjnie (nigdy nie powinno tego wywolac)
}
