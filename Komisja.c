#define _GNU_SOURCE
#include <sys/ipc.h>
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
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

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
#define MAX_STUD 160

pthread_t KAczlonek2, KAczlonek3;
pthread_mutex_t mutexA;
pthread_mutex_t mutexA2;
pthread_mutex_t mutexA3;
pthread_cond_t A1,A2;

//pthread_mutex_t mutexB;
int Liczba_studentow_do_zegzaminowania;
int Liczba_przepisujacych;
int Liczba_komunikatow_KOMA;

pid_t mainprogKomisja;


int msgID;
int shmID;
int semID;
int terminate_threads = 0;
int flaga_komisjaA=0;
int CONDVA1=0;
int CONDVA2=0;

typedef struct{
    long mtype;
}Krotkie_powiadomienie;

Krotkie_powiadomienie powiadom;

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

float wylosuj_ocene() {
    float oceny[] = {2.0, 3.0, 3.5, 4.0, 4.5, 5.0};
    int szanse[] = {5, 19, 19, 19, 19, 19}; // 5% na 2.0, reszta po 19%
    int suma_szans = 100;

    // Losowanie liczby od 1 do 100
    int los = rand() % suma_szans + 1;

    // Dopasowanie wylosowanej liczby do przedziału ocen
    int prog = 0;
    for (int i = 0; i < 6; i++) {
        prog += szanse[i];
        if (los <= prog) {
            return oceny[i];
        }
    }

    return 0.0; //awaryjnie
}
void semafor_wait(int semid, int sem_num){ //funkcja do semafor P
    struct sembuf sem;
    sem.sem_num = sem_num;
    sem.sem_op=-1; // dekrementacja 
    sem.sem_flg=0;
    int zmiensem=semop(semid,&sem,1);
    if(zmiensem == -1){
        perror("Blad semaforwait");
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
        perror("Blad semaforsignal");
        exit(EXIT_FAILURE);
    }
}

void* KA_czlonek3(void* arg) {
    while (!terminate_threads) {

        //w celu synchronizacji
        pthread_mutex_lock(&mutexA2);
        while(CONDVA1 != 1){
            printf("Czlonek3 czeka na odblokowanie CONDV1 przed sygnalem\n");
            pthread_cond_wait(&A1,&mutexA2);
        }
        pthread_mutex_unlock(&mutexA2);
        

        //sprawdź, czy są jeszcze jacyś studenci,którzy chcą podejść do egzaminu
        if(flaga_komisjaA ==1){
            break;
        }

            
        // czeka na informacje od Przewodniczacego na temat nowego studenta
        msgrcv(msgID, &powiadom, 0, CZ3KA, 0);
        printf("Czlonek3 przedostal sie przez 1 wiadomosc i obsluguje studenta o pid: %d\n",msg.pidStudenta);

        if (msg.zdaje_czy_pytania == 0) {
            if (msg.czy_zdane != 1) {
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


                pthread_mutex_lock(&mutexA3);
                while(CONDVA2 != 1){
                    printf("Czlonek3 czeka na odblokowanie CONDV2 po zadaniu pytan,pid stud: %d\n",msg.pidStudenta);
                    pthread_cond_wait(&A2,&mutexA3);
                }
                pthread_mutex_unlock(&mutexA3);


            }else{//tu student przepisuje ocene

                pthread_mutex_lock(&mutexA3);
                while(CONDVA2 != 1){
                    printf("Czlonek3 czeka na odblokowanie CONDV2 po przepisaniu oceny, pid stud:%d, \n",msg.pidStudenta);
                    pthread_cond_wait(&A2,&mutexA3);
                }
                pthread_mutex_unlock(&mutexA3);
                
            }

        } else {
            // teraz czeka na odpowiedź studenta
            Pytanie PCZKA33;
            msgrcv(msgID, &PCZKA33, sizeof(Pytanie) - sizeof(long), CZ3KA, 0);

            // ocenia odpowiedź
            float ocenaCZ3KA = wylosuj_ocene();

            // zapisuje ocene w pamieci dzielonej
            pthread_mutex_lock(&mutexA);
            shm_ptr->students[msg.id_studenta].ocena_praktyka3 = ocenaCZ3KA;
            pthread_mutex_unlock(&mutexA);
            printf("Ja, Czlonek3 wystawilem studentowi o pid: %d, ocene: %0.1f\n",msg.pidStudenta,ocenaCZ3KA);

            // wyslij wiadomosc do przewodniczacego, ze wpisales ocene
            Wiadomosc_zocena CZ3Kwiad;
            CZ3Kwiad.mtype = 555;
            CZ3Kwiad.ocena = ocenaCZ3KA;

            msgsnd(msgID, &CZ3Kwiad, sizeof(Wiadomosc_zocena) - sizeof(long), 0);

            pthread_mutex_lock(&mutexA3);
            while(CONDVA2 != 1){
                printf("Czlonek3 czeka na odblokowanie CONDV2 po ocenieniu, pid stud: %d\n", msg.pidStudenta);
                pthread_cond_wait(&A2,&mutexA3);
            }
            pthread_mutex_unlock(&mutexA3);

        }
        
    }

    printf("[KOMISJA] Czlonek 3 Komisji A, zakonczyl swoja prace i wrocil do domu\n;");

    return NULL;
}


void* KA_czlonek2(void* arg){
    while (!terminate_threads) {
        
        //w celu synchronizacji
        pthread_mutex_lock(&mutexA2);
        while(CONDVA1 != 1){
            printf("Czlonek2 czeka na odblokowanie CONDV1 przed sygnalem\n");
            pthread_cond_wait(&A1,&mutexA2);
        }
        pthread_mutex_unlock(&mutexA2);

        //sprawdź, czy są jeszcze jacyś studenci,którzy chcą podejść do egzaminu
        if(flaga_komisjaA ==1){
            break;
        }


        //czeka na informacje od Przewodniczacego na temat nowego studenta
        msgrcv(msgID,&powiadom,0,CZ2KA,0);
        printf("Czlonek2 przedostal sie przez 1 wiadomosc i obsluguje studenta o pid: %d\n",msg.pidStudenta);
            
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

                pthread_mutex_lock(&mutexA3);
                while(CONDVA2 != 1){
                    printf("Czlonek2 czeka na odblokowanie CONDV2 po zadaniu pytan, pid stud: %d\n",msg.pidStudenta);
                    pthread_cond_wait(&A2,&mutexA3);
                }
                pthread_mutex_unlock(&mutexA3);

            }else{

                pthread_mutex_lock(&mutexA3);
                while(CONDVA2 != 1){
                    printf("Czlonek2 czeka na odblokowanie CONDV2 po przepisaniu, pid stud: %d\n", msg.pidStudenta);
                    pthread_cond_wait(&A2,&mutexA3);
                }
                pthread_mutex_unlock(&mutexA3);
                    
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
            printf("Ja, Czlonek2 wystawilem studentowi o pid: %d, ocene: %0.1f\n",msg.pidStudenta,ocenaCZ2KA);

            //wyslij wiadomosc do przewodniczacego, ze wpisales ocene
            Wiadomosc_zocena CZ2Kwiad;
            CZ2Kwiad.mtype = 556;
            CZ2Kwiad.ocena = ocenaCZ2KA;

            msgsnd(msgID, &CZ2Kwiad,sizeof(Wiadomosc_zocena)-sizeof(long),0);
            pthread_mutex_lock(&mutexA3);
            while(CONDVA2 != 1){
                printf("Czlonek2 czeka na odblokowanie CONDV2 po ocenieniu, pid stud: %d\n", msg.pidStudenta);
                pthread_cond_wait(&A2,&mutexA3);
            }
            pthread_mutex_unlock(&mutexA3);
        }
            
        

    }
    printf("[KOMISJA] Czlonek 2 Komisji A, zakonczyl swoja prace i wrocil do domu\n;");
    return NULL;
}

float oblicz_srednia_i_zaokragl(float a, float b, float c) {
    // Sprawdzanie, czy którakolwiek liczba jest mniejsza niż 2.1 (bo precyzja)
    if (a < 2.1 || b < 2.1 || c < 2.1) {
        return 2.0;
    }

    // srednia
    float srednia = (a + b + c) / 3.0;

    // zaokrąglma w góre
    if (srednia <= 3.0) {
        return 3.0;
    } else if (srednia <= 3.5) {
        return 3.5;
    } else if (srednia <= 4.0) {
        return 4.0;
    } else if (srednia <= 4.5) {
        return 4.5;
    } else {
        return 5.0;
    }
}

/*void* KB_czlonek3(){
    


    pthread_exit(NULL);
}

void* KB_czlonek2(){
    


    pthread_exit(NULL);
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
        pthread_mutex_destroy(&mutexA3);
        pthread_cond_destroy(&A1);
        pthread_cond_destroy(&A2);

        printf("[KOMISJA] Program komisja A otrzymal sygnal SIGINT, zamknal watki i sie zamknal\n");
        exit(EXIT_SUCCESS);
    }else{
        printf("[KOMISJA] Program komisja B otrzymal sygnal SIGINT i sie zamknal\n");
        exit(EXIT_SUCCESS);
    }
}

int main(){
    srand(time(NULL));
    key_t kluczk;
    key_t kluczm;
    key_t kluczs;
    mainprogKomisja= getpid();

    //obsluga sygnalu sigint
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags=0;
    sigaction(SIGINT,&act,0);


    if((kluczs=ftok(".",'B')) == -1){
        perror("Blad przy tworzeniu klucza do semaforów\n");
        exit(EXIT_FAILURE);
    }

    semID=semget(kluczs,5,IPC_CREAT|0666); 
    if(semID==-1){
        perror("Blad tworzenia semaforow \n");
        exit(EXIT_FAILURE);
    }

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

    if((kluczk = ftok(".", 'C')) == -1)
    {
        perror("Blad ftok dla kolejki komunikatow w Komisji\n");
        exit(EXIT_FAILURE);
    }

    msgID=msgget(kluczk,IPC_CREAT|0666);
    if(msgID==-1)
	{
        perror("Blad tworzenia kolejki komunikatow w studencie \n");
        exit(EXIT_FAILURE);
    }
    
    if(mainprogKomisja == getpid()){
        semafor_wait(semID,4);
        Liczba_przepisujacych=shm_ptr->ilosc_osob_przepisujacych;
        Liczba_studentow_do_zegzaminowania= shm_ptr->ilosc_studentow_na_wybranym_kierunku;
        printf("[KOMISJA] Liczba przepisujących: %d\n", Liczba_przepisujacych);
        printf("[KOMISJA] Liczba studentów do egzaminu: %d\n", Liczba_studentow_do_zegzaminowania);
        Liczba_komunikatow_KOMA=(Liczba_studentow_do_zegzaminowania)*2;
        printf("[KOMISJA] Iteracje: %d\n",Liczba_komunikatow_KOMA);
    }
    switch(fork())
    {
        case -1:
            perror("Blad przy wykonaniu forka w Komisji\n");
            exit(EXIT_FAILURE);
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







        default: // komisja A jako rodzic bo zaczyna caly proces

        //tworze czlonkow komisji -> przewodniczacy to wątek główny


        pthread_mutex_init(&mutexA,NULL);
        pthread_mutex_init(&mutexA2,NULL);
        pthread_mutex_init(&mutexA3,NULL);
        pthread_cond_init(&A1,NULL);
        pthread_cond_init(&A2,NULL);

        if(pthread_create(&KAczlonek2,NULL, &KA_czlonek2, NULL) != 0){
            perror("Blad przy inicjowaniu watku w Komisji A\n");
            exit(EXIT_FAILURE);
        }

        if(pthread_create(&KAczlonek3,NULL, &KA_czlonek3, NULL) != 0){
            perror("Blad przy inicjowaniu watku w Komisji A\n");
            exit(EXIT_FAILURE);
        }
        //a tutaj wykonuje instrukcje przewodniczący

       

        //egzaminowanie kazdego z studentow
        int PAE=0;
        while (!terminate_threads) {
            
            pthread_mutex_lock(&mutexA2);
            if (CONDVA1 != 1) {
                printf("Przewodniczacy zmienia condva1 na 1 i condva2 na 0\n");
                CONDVA1 = 1;
                CONDVA2 = 0;
            }
            if (PAE == Liczba_komunikatow_KOMA) {
                flaga_komisjaA = 1;
            }
            pthread_mutex_unlock(&mutexA2);
            pthread_cond_broadcast(&A1);

            if(flaga_komisjaA == 1){
                break; //wychodzi z pętli bo wszyscy studenci juz zostali ocenieni
            }

            msgrcv(msgID, &msg, sizeof(Kom_bufor)-sizeof(long),PKAALL,0);
            printf("Przewodniczacy odebral wiadomosc i obsluguje studenta o pid %d\n",msg.pidStudenta);
            PAE++;
                
                
            if(msg.zdaje_czy_pytania == 0){ // 0->chce pytania albo przepisać, 1-> chce zdawać


                if(msg.czy_zdane == 1){
                    //Wysyla komunikat, ze uznaje jego przepisanie i moze isc do komisji B

                    msg.mtype=msg.pidStudenta;
                    msgsnd(msgID,&msg,sizeof(Kom_bufor)-sizeof(long),0);

                    //informuje innych czlonkow komisji o nowym studencie
                    powiadom.mtype = CZ2KA;
                    msgsnd(msgID,&powiadom,0,0);
                    powiadom.mtype = CZ3KA;
                    msgsnd(msgID,&powiadom,0,0);
                    //jeśli student przepisuje ocene, to wysyla tylko jeden komunikat, więc musze zwiekszyc PAE o 1
                    //bo zakladam, ze kazdy student, który zdaje wysyla po 2 komunikaty
                    PAE++;

                    pthread_mutex_lock(&mutexA3);
                    if(CONDVA2 != 1){
                        printf("Przewodniczacy zmienia condva2 na 1 i condva1 na 0 pid stud: %d\n", msg.pidStudenta);
                        CONDVA2=1;
                        CONDVA1=0;
                    }
                    pthread_mutex_unlock(&mutexA3);
                    pthread_cond_broadcast(&A2);


                }else{

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

                    pthread_mutex_lock(&mutexA3);
                    if(CONDVA2 != 1){
                        printf("Przewodniczacy zmienia condva2 na 1 i condva1 na 0, pid stud:%d\n",msg.pidStudenta);
                        CONDVA2=1;
                        CONDVA1=0;
                    }
                    pthread_mutex_unlock(&mutexA3);
                    pthread_cond_broadcast(&A2);
                }

            }else{

                //tutaj jak chce zdawać
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

                //zapisuje ocene koncowa z egzaminu dla danego studenta
                pthread_mutex_lock(&mutexA);
                shm_ptr->students[msg.id_studenta].ocena_praktykasr=srednia_ocena;
                pthread_mutex_unlock(&mutexA);

                printf("Ja, przewodniczacy wystawilem studentowi o pid: %d, ocene: %0.1f\n",msg.pidStudenta,ocenaPKA);
                printf("Student o pid: %d otrzymal ocene z czesci praktycznej: %0.1f\n",msg.pidStudenta,srednia_ocena);

                //wyslij wiadomosc z ocena do studenta

                Wiadomosc_zocena Zwrotna_do_studenta;
                Zwrotna_do_studenta.mtype = msg.pidStudenta;
                Zwrotna_do_studenta.ocena=srednia_ocena;

                msgsnd(msgID,&Zwrotna_do_studenta,sizeof(Wiadomosc_zocena)-sizeof(long),0);

                pthread_mutex_lock(&mutexA3);
                if(CONDVA2 != 1){
                    printf("Przewodniczacy zmienia condva2 na 1 i condva1 na 0, pid stud:%d \n",msg.pidStudenta);
                    CONDVA2=1;
                    CONDVA1=0;
                }
                pthread_mutex_unlock(&mutexA3);
                pthread_cond_broadcast(&A2);
            }
                
            
        }


        //tutaj przewodniczacy kończy swoje obowiązki i czeka, aż osoby z komisji pójdą do domu
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
        pthread_mutex_destroy(&mutexA3);
        pthread_cond_destroy(&A1);
        pthread_cond_destroy(&A2);
        
        printf("[KOMISJA] Program komisja czeka na zamkniecie sie procesu potomnego\n;");
        // Przewodniczący komisji A czeka, aż komisja B opuści budynek zanim go zamknie
        if (wait(NULL) == -1) {
            perror("Blad przy oczekiwaniu na proces potomny");
            exit(EXIT_FAILURE);
        }
        printf("[KOMISJA] Zakonczyl sie program Komisja\n;");
    
    }
    
    return 0;
}