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

pthread_t KAczlonek2, KAczlonek3;
pthread_mutex_t mutexA;
pthread_mutex_t mutexA2;

int Liczba_studentow_do_zegzaminowania;
int Liczba_przepisujacych;
int Liczba_komunikatow_KOMA;

pid_t mainprogKomisja;

int terminate_threads = 0;
int flaga_komisjaA=0;


typedef struct{
    long mtype;
}Krotkie_powiadomienie;

Krotkie_powiadomienie powiadom;

typedef struct{ //struktura komunikatu
	long mtype;
    float ocena_koncowa;
    int pidStudenta;
    int czy_zdane;
    int id_studenta;
    int zdaje_czy_pytania; // jak zdaje to 1, jak pytania to 0
}Kom_bufor;

Kom_bufor msg;

typedef struct{
    long mtype; 
    int pid_studenta;   
    int IDczlonkakomisji; 
    int pytanie;   
    int odpowiedz;  
}Pytanie;

typedef struct{
    long mtype;
    float ocena;
}Wiadomosc_zocena;

void* KA_czlonek3(void* arg) {
    (void)arg;
    while (!terminate_threads) { 
        // czeka na informacje od Przewodniczacego na temat nowego studenta
        msgrcv(msgID, &powiadom, 0, CZ3KA, 0);

        //sprawdź, czy są jeszcze jacyś studenci,którzy chcą podejść do egzaminu
        if(flaga_komisjaA ==1){
            break;
        }

        if (msg.zdaje_czy_pytania == 0) {

            if (msg.czy_zdane != 1) { // jak student chce pytania
                // zadaje pytanie studentowi
                Pytanie PCZKA3;

                // tutaj symulacja losowania i przygotowywania pytania
                int PC3 = rand() % 6 + 5;
                // sleep(PC3);

                PCZKA3.IDczlonkakomisji = CZ3KA;
                PCZKA3.pytanie = PC3;
                PCZKA3.mtype = msg.pidStudenta;
                msgsnd(msgID, &PCZKA3, sizeof(Pytanie) - sizeof(long), 0);

                //tutaj potwierdzenie do przewodniczacego ze wyslal pytania
                Krotkie_powiadomienie powcz3;
                powcz3.mtype = 691;
                msgsnd(msgID, &powcz3,0,0);



            }else{//tu student przepisuje ocene

                Krotkie_powiadomienie powcz5;
                powcz5.mtype = 701;
                msgsnd(msgID, &powcz5,0,0);

                
            }

        } else { // tutaj jak chce zdawac
            // teraz czeka na odpowiedź studenta
            Pytanie PCZKA33;
            msgrcv(msgID, &PCZKA33, sizeof(Pytanie) - sizeof(long), CZ3KA, 0);

            // ocenia odpowiedź
            float ocenaCZ3KA = wylosuj_ocene();

            // zapisuje ocene w pamieci dzielonej
            pthread_mutex_lock(&mutexA);
            shm_ptr->students[msg.id_studenta].ocena_praktyka3 = ocenaCZ3KA;
            pthread_mutex_unlock(&mutexA);
            

            // wyslij wiadomosc do przewodniczacego, ze wpisales ocene
            Wiadomosc_zocena CZ3Kwiad;
            CZ3Kwiad.mtype = 555;
            CZ3Kwiad.ocena = ocenaCZ3KA;

            msgsnd(msgID, &CZ3Kwiad, sizeof(Wiadomosc_zocena) - sizeof(long), 0);
        }
        
    }

    printf("[KOMISJA] Czlonek 3 Komisji A, zakonczyl swoja prace i wrocil do domu\n");
    return NULL;
}


void* KA_czlonek2(void* arg){
    (void)arg;
    while (!terminate_threads) {
    
        //czeka na informacje od Przewodniczacego na temat nowego studenta
        msgrcv(msgID,&powiadom,0,CZ2KA,0);

        //sprawdź, czy są jeszcze jacyś studenci,którzy chcą podejść do egzaminu
        if(flaga_komisjaA ==1){
            break;
        }
            
        if(msg.zdaje_czy_pytania== 0){
            if(msg.czy_zdane != 1){ //-jak nie zdane to w tym przypadku chce pytania
                //zadaje pytanie
                //zadaje pytanie studentowi
                Pytanie PCZKA2;

                //tutaj symulacja losowania i przygotowywania pytania
                int PC2 = rand()%6 +5;
                //sleep(PC2);
                PCZKA2.IDczlonkakomisji=CZ2KA;
                PCZKA2.pytanie = PC2;
                PCZKA2.mtype = msg.pidStudenta;
                msgsnd(msgID, &PCZKA2,sizeof(Pytanie)-sizeof(long),0);

                //tutaj potwierdzenie do przewodniczacego ze wyslal pytania
                Krotkie_powiadomienie powcz2;
                powcz2.mtype = 690;

                msgsnd(msgID, &powcz2,0,0);

                
            }else{//chce przepisać ocene

                Krotkie_powiadomienie powcz5;
                powcz5.mtype = 700;
                msgsnd(msgID, &powcz5,0,0);
           
            }

        }else{
            //teraz czeka na odpowiedź studenta do swojego pytania
            Pytanie PCZKA22;
            msgrcv(msgID,&PCZKA22,sizeof(Pytanie)-sizeof(long),CZ2KA,0);
                
            //ocenia odpowiedź
            float ocenaCZ2KA = wylosuj_ocene();
                    
            //zapisuje ocene w pamieci dzielonej
            pthread_mutex_lock(&mutexA);
            shm_ptr->students[msg.id_studenta].ocena_praktyka2=ocenaCZ2KA;
            pthread_mutex_unlock(&mutexA);
            

            //wyslij wiadomosc do przewodniczacego, ze wpisales ocene
            Wiadomosc_zocena CZ2Kwiad;
            CZ2Kwiad.mtype = 556;
            CZ2Kwiad.ocena = ocenaCZ2KA;

            msgsnd(msgID, &CZ2Kwiad,sizeof(Wiadomosc_zocena)-sizeof(long),0);
                      
        }
            
    }
    printf("[KOMISJA] Czlonek 2 Komisji A, zakonczyl swoja prace i wrocil do domu\n");
    return NULL;
}

/*void* KB_czlonek3(){
    


    return NULL;
}

void* KB_czlonek2(){
    


    return NULL;
}
*/
void sigint_handler(int sig){
    if(mainprogKomisja == getpid()){
        pthread_mutex_lock(&mutexA);
        terminate_threads = 1;
        pthread_mutex_unlock(&mutexA);

        if (wait(NULL) == -1 && errno == ECHILD) {
            // Brak procesów potomnych
        }

        if(pthread_join(KAczlonek2, NULL) != 0){
            perror("Blad przy joinie czlonka 2 komisji A\n");
            exit(EXIT_FAILURE);
        }
        if(pthread_join(KAczlonek3, NULL) != 0){
            perror("Blad przy joinie czlonka 3 komisji A\n");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_destroy(&mutexA);
        pthread_mutex_destroy(&mutexA2);
        

        printf("[KOMISJA] Program komisja A otrzymal sygnal SIGINT, zamknal watki i sie zamknal: %d\n",sig);
        exit(EXIT_SUCCESS);
    }else{
        printf("[KOMISJA] Program komisja B otrzymal sygnal SIGINT i sie zamknal: %d\n",sig);
        exit(EXIT_SUCCESS);
    }
}

int main(){
    srand(time(NULL));
    mainprogKomisja= getpid();

    //obsluga sygnalu sigint
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags=0;
    sigaction(SIGINT,&act,0);

    utworz_klucze();

    semID=semget(kluczs,5,0666); 
    if(semID==-1){
        perror("Blad tworzenia semaforow \n");
        exit(EXIT_FAILURE);
    }

    shmID=shmget(kluczm, sizeof(SHARED_MEMORY),0666);
    if (shmID==-1){
        perror("Blad przy tworzeniu pamieci dzielonej w Studencie\n");
        exit(EXIT_FAILURE);
    }

    shm_ptr =(SHARED_MEMORY*)shmat(shmID, NULL, 0);
    if (shm_ptr == (void *)-1){
        perror("Blad przy dolaczaniu pamieci dzielonej w Studencie\n");
        exit(EXIT_FAILURE);
    }

    msgID=msgget(kluczk,0666);
    if(msgID==-1)
	{
        perror("Blad tworzenia kolejki komunikatow w studencie \n");
        exit(EXIT_FAILURE);
    }
    
    if(mainprogKomisja == getpid()){
        //tutaj czekam, az dziekan zapisze mi ile studentów bedzie podchodzić do egzaminu i ile z nich przepisuje ocene
        semafor_wait(semID,4);
        semafor_wait(semID,0);
        Liczba_przepisujacych=shm_ptr->ilosc_osob_przepisujacych;
        Liczba_studentow_do_zegzaminowania= shm_ptr->ilosc_studentow_na_wybranym_kierunku;
        printf("[KOMISJA] Liczba przepisujących: %d\n", Liczba_przepisujacych);
        printf("[KOMISJA] Liczba studentów do egzaminu: %d\n", Liczba_studentow_do_zegzaminowania);
        Liczba_komunikatow_KOMA=(Liczba_studentow_do_zegzaminowania)*2;
        shm_ptr->Komisjazakonczenie=0;
        printf("[KOMISJA] Iteracje: %d\n",Liczba_komunikatow_KOMA);
        semafor_signal(semID,0);
    }

    switch(fork())
    {
        case -1:
            perror("Blad przy wykonaniu forka w Komisji\n");
            exit(EXIT_FAILURE);
            break;
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
            printf("[KOMISJA] Komisja B (TO TEORIA) zakonczyla dzialanie\n");
            exit(EXIT_SUCCESS);
            break;







        default: // komisja A jako rodzic bo zaczyna caly proces

            //tworze czlonkow komisji -> przewodniczacy to wątek główny

            pthread_mutex_init(&mutexA,NULL);
            pthread_mutex_init(&mutexA2,NULL);
            

            if(pthread_create(&KAczlonek2,NULL, &KA_czlonek2, NULL) != 0){
                perror("Blad przy inicjowaniu watku w Komisji A\n");
                exit(EXIT_FAILURE);
            }

            if(pthread_create(&KAczlonek3,NULL, &KA_czlonek3, NULL) != 0){
                perror("Blad przy inicjowaniu watku w Komisji A\n");
                exit(EXIT_FAILURE);
            }

            if (mkfifo(FIFO_PATH, 0666) == -1) {
                if (errno != EEXIST) {  
                    perror("Blad tworzenia FIFO");
                    exit(EXIT_FAILURE);
                }
            }

            struct{
                int intpar;
                float floatpar;
            }OcenyKA[MAX_SIZE_OCENY];

            int counter1=0;

            //a tutaj wykonuje instrukcje przewodniczący

            //egzaminowanie kazdego z studentow
            int PAE=0;
            while (!terminate_threads) {
                
                
                if (PAE == Liczba_komunikatow_KOMA) {
                    // Otwieram FIFO do zapisu
                    int fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
                    if (fd == -1) {
                        perror("blad open FIFO\n");
                        exit(1);
                    }

                    // tworze buffer do zapisu w odpowiednim formacie
                    char buffer[4096];
                    int bufferIndex = 0;

                    // zapisuje wszystkie dane z tablicy do bufora
                    for (int i = 0; i < counter1; i++) {
                    int len = snprintf(buffer + bufferIndex, sizeof(buffer) - bufferIndex, "%d %f\n", OcenyKA[i].intpar, OcenyKA[i].floatpar);
                    bufferIndex += len;  // pzremieszczam wskaźnik w buforze
                    }

                    // Zapisuje dane do FIFO
                    write(fd, buffer, bufferIndex);
                    close(fd);

                    flaga_komisjaA = 1;
                    powiadom.mtype = CZ2KA;
                    msgsnd(msgID,&powiadom,0,0);
                    powiadom.mtype = CZ3KA;
                    msgsnd(msgID,&powiadom,0,0);
                    //wysyla wiadomosc do czlonkow komisji, ze juz nie ma studentow do przepytania
                    break;
                }

                msgrcv(msgID, &msg, sizeof(Kom_bufor)-sizeof(long),PKAALL,0);
                printf("Przewodniczacy odebral wiadomosc i obsluguje studenta o pid %d\n",msg.pidStudenta);
                PAE++;
                      
                if(msg.zdaje_czy_pytania == 0){ // 0->chce pytania albo przepisać, 1-> chce zdawać


                    if(msg.czy_zdane == 1){ // tutaj gdy student przepisuje ocene z zeszlego roku
                        //Wysyla komunikat, ze uznaje jego przepisanie i moze isc do komisji B

                        msg.mtype=msg.pidStudenta;
                        printf("Student o pid: %d, ma przepisana ocene: %0.1f z zeszlego roku\n",msg.pidStudenta,msg.ocena_koncowa);
                        msgsnd(msgID,&msg,sizeof(Kom_bufor)-sizeof(long),0);

                        //informuje innych czlonkow komisji o nowym studencie
                        powiadom.mtype = CZ2KA;
                        msgsnd(msgID,&powiadom,0,0);
                        powiadom.mtype = CZ3KA;
                        msgsnd(msgID,&powiadom,0,0);
                        //jeśli student przepisuje ocene, to wysyla tylko jeden komunikat, więc musze zwiekszyc PAE o 1
                        //bo zakladam, ze kazdy student, który zdaje wysyla po 2 komunikaty
                        PAE++;

                        //zapisuje ocene w bufferze, który potem przesylam do dziekana
                        OcenyKA[counter1].intpar=msg.pidStudenta;
                        OcenyKA[counter1].floatpar=msg.ocena_koncowa;
                        counter1++;

                        Krotkie_powiadomienie PKAw4; // od czlonka 2
                        Krotkie_powiadomienie PKAw5; // od czlonka 3

                        msgrcv(msgID,&PKAw4,0,700,0); // od czlonka 2
                        msgrcv(msgID,&PKAw5,0,701,0); // od czlonka 3


                    }else{ // tutaj, gdy student chce pytania

                        //wysyla wiadomosc z potwierdzeniem
                        msg.mtype=msg.pidStudenta;
                        msgsnd(msgID,&msg,sizeof(Kom_bufor)-sizeof(long),0);

                        //informuje innych czlonkow komisji o nowym studencie
                        powiadom.mtype = CZ2KA;
                        msgsnd(msgID,&powiadom,0,0);
                        powiadom.mtype = CZ3KA;
                        msgsnd(msgID,&powiadom,0,0);

                        //zadaje pytanie studentowi
                        Pytanie PPKA;

                        //tutaj symulacja losowania i przygotowywania pytania
                        int PP = rand()%6 +5;
                            
                        //sleep(PP);

                        PPKA.IDczlonkakomisji=PKA;
                        PPKA.pytanie = PP;
                        PPKA.mtype = msg.pidStudenta;
                        msgsnd(msgID, &PPKA,sizeof(Pytanie)-sizeof(long),0);
                            
                        //czeka na potwierdzenie, czy inni juz wyslali pytania
                        printf("Przewodniczcy wyslal pytania studentowi o pid: %d\n",msg.pidStudenta);

                        Krotkie_powiadomienie PKAw2; // od czlonka 2
                        Krotkie_powiadomienie PKAw3; // od czlonka 3
                            
                        msgrcv(msgID,&PKAw2,0,690,0); // od czlonka 2
                        msgrcv(msgID,&PKAw3,0,691,0); // od czlonka 3

                    }

                }else{//tutaj jak chce zdawać
                    
                    
                    //teraz czeka na odpowiedź studenta
                    Pytanie PPKA1;
                    msgrcv(msgID,&PPKA1,sizeof(Pytanie)-sizeof(long),PKA,0);

                    //ocenia odpowiedź
                    float ocenaPKA = wylosuj_ocene();
                            
                    //zapisuje swoją ocene w pamieci dzielonej
                    pthread_mutex_lock(&mutexA);
                    shm_ptr->students[msg.id_studenta].ocena_praktykaP=ocenaPKA;
                    pthread_mutex_unlock(&mutexA);

                    //odbierz powiadomienie o ocenach od innych czlonkow
                    Wiadomosc_zocena PKAw2; // od czlonka 2
                    Wiadomosc_zocena PKAw3; // od czlonka 3
                            
                    msgrcv(msgID,&PKAw2,sizeof(Wiadomosc_zocena)-sizeof(long),556,0);
                    msgrcv(msgID,&PKAw3,sizeof(Wiadomosc_zocena)-sizeof(long),555,0);

                    float srednia_ocena = oblicz_srednia_i_zaokragl(ocenaPKA,PKAw2.ocena,PKAw3.ocena);

                    //zapisuje ocene w bufferze, który potem przesylam do dziekana
                    OcenyKA[counter1].intpar=msg.pidStudenta;
                    OcenyKA[counter1].floatpar=srednia_ocena;
                    counter1++;

                    //zapisuje ocene koncowa z egzaminu dla danego studenta
                    pthread_mutex_lock(&mutexA);
                    shm_ptr->students[msg.id_studenta].ocena_praktykasr=srednia_ocena;
                    pthread_mutex_unlock(&mutexA);

                    printf("Student o pid: %d, otrzymal od przewodniczacego ocene: %0.1f\n",msg.pidStudenta,ocenaPKA);
                    printf("Student o pid: %d, otrzymal od Czlonka2 Komisji A ocene: %0.1f\n",msg.pidStudenta,PKAw2.ocena);
                    printf("Student o pid: %d, otrzymal od Czlonka3 Komisji A ocene: %0.1f\n",msg.pidStudenta,PKAw3.ocena);
                    printf("Student o pid: %d otrzymal ocene z czesci praktycznej: %0.1f\n",msg.pidStudenta,srednia_ocena);

                    //wyslij wiadomosc z ocena do studenta

                    Wiadomosc_zocena Zwrotna_do_studenta;
                    Zwrotna_do_studenta.mtype = msg.pidStudenta;
                    Zwrotna_do_studenta.ocena=srednia_ocena;

                    msgsnd(msgID,&Zwrotna_do_studenta,sizeof(Wiadomosc_zocena)-sizeof(long),0);

                }
                    
            }
        
            if(pthread_join(KAczlonek2, NULL) != 0){
                perror("Blad przy joinie czlonka 2 komisji A\n");
                exit(EXIT_FAILURE);
            }else{
                printf("Watek KAczlonek2 zostal zwolniony\n");
            }
            if(pthread_join(KAczlonek3, NULL) != 0){
                perror("Blad przy joinie czlonka 3 komisji A\n");
                exit(EXIT_FAILURE);
            }else{
                printf("Watek KAczlonek2 zostal zwolniony\n");
            }
            printf("[KOMISJA] Mutexy niszczenie....\n");
            pthread_mutex_destroy(&mutexA);
            pthread_mutex_destroy(&mutexA2);
            printf("[KOMISJA] Mutexy zniszczone\n");
            break;
    
    }
        
    if(mainprogKomisja == getpid()){
        printf("[KOMISJA] Program komisja czeka na zamkniecie sie procesu potomnego\n");
        // Przewodniczący komisji A czeka, aż komisja B opuści budynek zanim go zamknie
        if (wait(NULL) == -1) {
            perror("Blad przy oczekiwaniu na proces potomny\n");
            exit(EXIT_FAILURE);
        }
        semafor_wait(semID,0);
        shm_ptr->Komisjazakonczenie = 1;
        semafor_signal(semID,0);
 
        printf("[KOMISJA] Zakonczyl sie program Komisja\n");
        
    }
    
    return 0;
}