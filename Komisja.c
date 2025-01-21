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
#include <pthread.h>

//indentyfikatory komunikatów dla kolejnych czlonków komisji A
#define PKA 449
#define CZ2KA 450
#define CZ3KA 451
//indentyfikatory komunikatów dla kolejnych czlonków komisji B
#define PKB 452
#define CZ2KB 453
#define CZ3KB 454
#define MAX_STUD 160

pthread_t KAczlonek2, KAczlonek3;
pthread_mutex_t mutexA;
//pthread_mutex_t mutexB;
pthread_cond_t cond_komunikatzpA;
pthread_cond_t cond_komunikatzwA;
pthread_cond_t cond_komunikat_przejsciaA;
pthread_cond_t cond_komunikat_przejsciaA2;
int Liczba_studentow_do_zegzaminowania;

pid_t mainprogKomisja;

int komunikatzpA=0;
int msgID;
int shmID;
int semID;
int komunikatzwA=0;
int komunikat_przejsciaA=0;
int komunikat_przejsciaA2=0;
int terminate_threads = 0;

typedef struct{
    long mtype;
    pid_t PIDS;
}Krotkie_powiadomienie;

typedef struct {            // struktura reprezentująca każdego studenta
    int id;   
    pid_t pid_studenta;     // identyfikator kazdego studenta
    int Kierunek;
    int powtarza_egzamin;   // flaga, jeśli student powtarza egzamin (1 - tak,0 - nie)
    int zaliczona_praktyka; // flaga, czy czesc praktyczna zostala zaliczona
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
    int index;
    int ilosc_studentow_na_wybranym_kierunku;
    Student students[MAX_STUD];  // Tablica struktur Student
} SHARED_MEMORY;

SHARED_MEMORY *shm_ptr;



typedef struct{ //struktura komunikatu
	long mtype;
	float ocena;
    int pidStudenta;
    int czy_zdane;
    int id_studenta;
}Kom_bufor;


Kom_bufor msg;
Kom_bufor localmsg2;
Kom_bufor localmsg;
Kom_bufor Student_przewodniczacyKA;
Kom_bufor Student_Czlonek2A;
Kom_bufor Student_Czlonek3A;



typedef struct{
    long mtype; 
    int pid_studenta;   
    int IDczlonkakomisji;    
    int i;
    int odpowiedz;  
}Pytanie;

typedef struct{
    long mtype;
    float ocena;
}Wiadomosc_zocena;

Wiadomosc_zocena wiadKA;

void Sendmsg(int msgid, long mtype, int student_pid, float grade, int zdane) {
    Kom_bufor msg;
    msg.mtype = mtype;
    msg.pidStudenta = student_pid;
    msg.ocena = grade;
    msg.czy_zdane = zdane;

    // Wysyłanie wiadomości
    if (msgsnd(msgid, &msg, sizeof(Kom_bufor) - sizeof(long), IPC_NOWAIT) == -1) {
        //tutaj jak kolejka będzie przepelniona to errno zwróci EAGAIN -> zrobić do tego obsluge potem
        perror("Blad msgsnd w Komisji\n");
        exit(EXIT_FAILURE);
    }

}

void wyslij_pytanie(int msgid, long mtype, int nrpytania, int IDCZLONKA){
    Pytanie pyt;
    pyt.mtype = mtype;
    pyt.i = nrpytania;
    pyt.IDczlonkakomisji = IDCZLONKA;

    if (msgsnd(msgid, &pyt, sizeof(Pytanie) - sizeof(long), IPC_NOWAIT) == -1) {
        //tutaj jak kolejka będzie przepelniona to errno zwróci EAGAIN -> zrobić do tego obsluge potem
        perror("Blad msgsnd w KOmisji\n");
        exit(EXIT_FAILURE);
    }

}
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


void* KA_czlonek3(){
    //stworzenie lokalnej wiadomości
    while (!terminate_threads) {
        Pytanie p2;

        for (int KACZLON3 = 0; KACZLON3 < Liczba_studentow_do_zegzaminowania; KACZLON3++) {

            //czeka aż przewodniczący odbierze wiadomość, czy student zdaje czy przepisuje ocene
            pthread_mutex_lock(&mutexA);
            while (!komunikatzpA) {
                pthread_cond_wait(&cond_komunikatzpA, &mutexA);
            }
            pthread_mutex_unlock(&mutexA);

            

            if(localmsg.czy_zdane != 1){
                //odbiera wiadomość o long CZK3 kom_bufor
                if (msgrcv(msgID, &Student_Czlonek3A, sizeof(Kom_bufor) - sizeof(long), CZ3KA, 0) == -1) {
                        perror("Blad msgrcv w komisji\n");
                        exit(EXIT_FAILURE);
                }
                //przygotowuje pytanie:
                int PPopoz1 = rand()%6 +5; 
                //zadaje pytanie studentowi
                wyslij_pytanie(msgID,Student_Czlonek3A.pidStudenta,PPopoz1,CZ3KA);
                //czeka na pytanie od studenta, ocenia je 
                if (msgrcv(msgID, &p2, sizeof(Pytanie) - sizeof(long),CZ3KA , 0) == -1) {
                    perror("Blad msgrcv w Komisji");
                    exit(EXIT_FAILURE); 
                }
                float ocenaCZ3KA = wylosuj_ocene();
                shm_ptr->students[localmsg.id_studenta].ocena_praktyka3 = ocenaCZ3KA;
                printf("Ocena od czlonka 3 dla studenta: %d wynosi: %0.1f\n",Student_Czlonek3A.pidStudenta,ocenaCZ3KA);
                //wysyla komunikat do przewodniczacego, ze ocenil juz studenta
                Krotkie_powiadomienie c3KA;
                c3KA.mtype=888;
                c3KA.PIDS=localmsg.pidStudenta;
                msgsnd(msgID,&c3KA,sizeof(Krotkie_powiadomienie)-sizeof(long),0);




                //czeka na wystawienie średniej przez przewodniczącego i przechodzi do nastepnego studenta
                pthread_mutex_lock(&mutexA);
                while (!komunikat_przejsciaA) {
                pthread_cond_wait(&cond_komunikat_przejsciaA, &mutexA);
                }
                pthread_mutex_unlock(&mutexA);





            }
            else {
                //tutaj drugi condition variable, który bedzie czekał,aż przewodniczący wyśle wiadomość
                //czeka aż przewodniczacy wyśle komunikat o tym że student moze isc do komisji B i zmieni wartość komunikatzwA z powrotem na 0
                pthread_mutex_lock(&mutexA);
                while(!komunikatzwA){
                    pthread_cond_wait(&cond_komunikatzwA, &mutexA);
                }
                pthread_mutex_unlock(&mutexA);
            
            }
            //tutaj czekam na reset flag do synchronizacji, który wykonuje przewodniczacy
            pthread_mutex_lock(&mutexA);
            while (!komunikat_przejsciaA2) {
                pthread_cond_wait(&cond_komunikat_przejsciaA2, &mutexA);
            }
            pthread_mutex_unlock(&mutexA);
        }
    }


    pthread_exit(NULL);
}

void* KA_czlonek2(){
    while (!terminate_threads) {
        
        Pytanie p3;

        for (int KACZLON2 = 0; KACZLON2 < Liczba_studentow_do_zegzaminowania; KACZLON2++) {

            pthread_mutex_lock(&mutexA);
            while (!komunikatzpA) {
                pthread_cond_wait(&cond_komunikatzpA, &mutexA);
            }
            pthread_mutex_unlock(&mutexA);

            if(localmsg2.czy_zdane != 1){
                //zaczyna egzamin
                if (msgrcv(msgID, &Student_Czlonek2A, sizeof(Kom_bufor) - sizeof(long), CZ2KA, 0) == -1) {
                        perror("Blad msgrcv w komisji\n");
                        exit(EXIT_FAILURE);
                }
                //przygotowuje pytanie:
                int PPopoz2 = rand()%6 +5; 
                //zadaje pytanie studentowi
                wyslij_pytanie(msgID,Student_Czlonek2A.pidStudenta,PPopoz2,CZ2KA);
                //czeka na pytanie od studenta, ocenia je 
                if (msgrcv(msgID, &p3, sizeof(Pytanie) - sizeof(long),CZ2KA , 0) == -1) {
                    perror("Blad msgrcv w Komisji");
                    exit(EXIT_FAILURE); 
                }
                float ocenaCZ2KA = wylosuj_ocene();
                
                shm_ptr->students[localmsg2.id_studenta].ocena_praktyka2 = ocenaCZ2KA;
                printf("Ocena od czlonka 2 dla studenta: %d wynosi: %0.1f\n",Student_Czlonek2A.pidStudenta,ocenaCZ2KA);

                //wysyla prowadzacemu komunikat, ze juz ocenil danego studenta
                Krotkie_powiadomienie c2KA;
                c2KA.mtype=889;
                c2KA.PIDS=localmsg2.pidStudenta;
                msgsnd(msgID,&c2KA,sizeof(Krotkie_powiadomienie)-sizeof(long),0);

                //czeka na wystawienie średniej przez przewodniczącego i przechodzi do nastepnego studenta
                pthread_mutex_lock(&mutexA);
                while (!komunikat_przejsciaA) {
                pthread_cond_wait(&cond_komunikat_przejsciaA, &mutexA);
                }
                pthread_mutex_unlock(&mutexA);




            }
            else {
                //tutaj drugi condition variable, który bedzie czekał,aż przewodniczący wyśle wiadomość
                //czeka aż przewodniczacy wyśle komunikat i zmieni wartość komunikatzwA z powrotem na 0
                pthread_mutex_lock(&mutexA);
                while(!komunikatzwA){
                    pthread_cond_wait(&cond_komunikatzwA, &mutexA);
                }
                pthread_mutex_unlock(&mutexA);
            
            }
            //tutaj czekam na reset flag do synchronizacji, który wykonuje przewodniczacy
            pthread_mutex_lock(&mutexA);
            while (!komunikat_przejsciaA2) {
                pthread_cond_wait(&cond_komunikat_przejsciaA2, &mutexA);
            }
            pthread_mutex_unlock(&mutexA);
        }

    }
    pthread_exit(NULL);
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
        wait(NULL);
        if(pthread_join(KAczlonek2, NULL) != 0){
            perror("Blad przy joinie czlonka 2 komisji A\n");
            exit(EXIT_FAILURE);
        }
        if(pthread_join(KAczlonek3, NULL) != 0){
            perror("Blad przy joinie czlonka 3 komisji A\n");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_destroy(&mutexA);
        pthread_cond_destroy(&cond_komunikatzpA);
        pthread_cond_destroy(&cond_komunikatzwA);
        pthread_cond_destroy(&cond_komunikat_przejsciaA);
        pthread_cond_destroy(&cond_komunikat_przejsciaA2);
        exit(EXIT_SUCCESS);

    }else{
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
    Liczba_studentow_do_zegzaminowania= shm_ptr->ilosc_studentow_na_wybranym_kierunku;


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

        exit(EXIT_SUCCESS);







        default: // komisja A jako rodzic bo zaczyna caly proces

        //tworze czlonkow komisji -> przewodniczacy to wątek główny





         // [[[[[[[[[[[tutaj byly zdefiniowane]]]]]]]]]]]




        pthread_mutex_init(&mutexA,NULL);
        pthread_cond_init(&cond_komunikatzpA,NULL);
        pthread_cond_init(&cond_komunikatzwA,NULL);
        pthread_cond_init(&cond_komunikat_przejsciaA,NULL);
        pthread_cond_init(&cond_komunikat_przejsciaA2,NULL);

        Pytanie pKA;


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
        for (int PAE = 0; PAE < Liczba_studentow_do_zegzaminowania; PAE++) {
         


            pthread_mutex_lock(&mutexA);
            //odbiera wiadomość od studenta czy przepisuje ocene, czy zdaje normalnie
            if (msgrcv(msgID, &msg, sizeof(Kom_bufor) - sizeof(long), 5, 0) == -1) {
                perror("Blad msgrcv w komisji\n");
                exit(EXIT_FAILURE);
            }
            localmsg2=msg;
            localmsg=msg;
            printf("Odebralem wiadomosc od studenta\n");
            // flaga do synchronizacji watkow
            komunikat_przejsciaA2=0;
            int student_pid = msg.pidStudenta;
            printf("Wiadomosc ta byla od studenta z pid: %d\n",student_pid);
            komunikatzpA =1;
            pthread_mutex_unlock(&mutexA);
            pthread_cond_broadcast(&cond_komunikatzpA); // sygnalizuje wszystkim wątkom żeby sprawdzić warunek


            if(msg.czy_zdane == 1){
                //wysylam mu komunikat, ze może iść do komisji B
                pthread_mutex_lock(&mutexA);
                Sendmsg(msgID,student_pid,student_pid,msg.ocena,1);
                komunikatzwA =1;
                pthread_mutex_unlock(&mutexA);
                pthread_cond_broadcast(&cond_komunikatzwA);

            }else{
                //rozpoczynam egzamin
                //odbierz wiadomość od studenta o gotowości przystapienia do egzaminu
                if (msgrcv(msgID, &msg, sizeof(Kom_bufor) - sizeof(long), PKA, 0) == -1) {
                    perror("Blad msgrcv w komisji\n");
                    exit(EXIT_FAILURE);
                }

                //Przygotowuje pytanie
                int PPopoz = rand()%6 +5; // Symulacja przygotowywania i(losowania) pytania
                //sleep(PPopoz);


                //wysyla pytanie do studenta
                wyslij_pytanie(msgID,msg.pidStudenta,PPopoz, PKA);

                //czyta odpowiedź, ocenia ją i czeka, aż inni członkowie ocenią, potem sprawdza, czy nie ma
                //oceny 2 i liczy średnią a nastepnie wysyla wiadomosc z oceną do studenta
                if (msgrcv(msgID, &pKA, sizeof(Pytanie) - sizeof(long),CZ2KA , 0) == -1) {
                    perror("Blad msgrcv w Komisji");
                    exit(EXIT_FAILURE); 
                }
                float ocenaPKA = wylosuj_ocene();
                shm_ptr->students[msg.id_studenta].ocena_praktykaP = ocenaPKA;

                printf("Ocena od Przewodniczacego dla studenta: %d wynosi: %0.1f\n",msg.pidStudenta,ocenaPKA);


                
                //czekam dopóki czlonkowie komisji ocenią studenta (defaultowo oceny sa zainicjowane na 0.0 stad taki warunek)

                
                //while(shm_ptr->students[msg.id_studenta].ocena_praktyka2 < 1.0 && shm_ptr->students[msg.id_studenta].ocena_praktyka3 < 1.0){
                 //   usleep(100000); 
                //}
                
                Krotkie_powiadomienie Pkomisa;

                msgrcv(msgID,&Pkomisa,sizeof(Krotkie_powiadomienie)-sizeof(long),889,0);
                msgrcv(msgID,&Pkomisa,sizeof(Krotkie_powiadomienie)-sizeof(long),888,0);

                //obliczam ocene koncowa dla studenta
                float ocena_koncowaKA = oblicz_srednia_i_zaokragl(shm_ptr->students[msg.id_studenta].ocena_praktykaP,shm_ptr->students[msg.id_studenta].ocena_praktyka2,shm_ptr->students[msg.id_studenta].ocena_praktyka3);
                printf("Student z pid: %d dostal ocene %0.2f\n",student_pid,ocena_koncowaKA);
                //zapisuje studentowi tą ocene
                shm_ptr->students[msg.id_studenta].ocena_praktykasr = ocena_koncowaKA;

                //wysylam wiadomośc studentowi z oceną
                wiadKA.mtype =msg.pidStudenta;
                wiadKA.ocena = ocena_koncowaKA;
                if (msgsnd(msgID, &wiadKA, sizeof(Wiadomosc_zocena) - sizeof(long), IPC_NOWAIT) == -1) {
                //tutaj jak kolejka będzie przepelniona to errno zwróci EAGAIN -> zrobić do tego obsluge potem
                perror("Blad msgsnd w Komisji\n");
                exit(EXIT_FAILURE);
                }

                pthread_mutex_lock(&mutexA);
                komunikat_przejsciaA=1;
                pthread_mutex_unlock(&mutexA);
                pthread_cond_broadcast(&cond_komunikat_przejsciaA);

                //wysyla ocene koncową z części praktycznej do dziekana






            }
            //tutaj robie reset flag do synchronizacji
            pthread_mutex_lock(&mutexA);
            komunikatzwA=0;
            komunikatzpA=0;
            komunikat_przejsciaA =0;
            komunikat_przejsciaA2=1;
            pthread_mutex_unlock(&mutexA);
            pthread_cond_broadcast(&cond_komunikat_przejsciaA2);
            
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
        pthread_cond_destroy(&cond_komunikatzpA);
        pthread_cond_destroy(&cond_komunikatzwA);
        pthread_cond_destroy(&cond_komunikat_przejsciaA);
        pthread_cond_destroy(&cond_komunikat_przejsciaA2);
        
        // Przewodniczący komisji A czeka, aż komisja B opuści budynek zanim go zamknie
        if (wait(NULL) == -1) {
            perror("Blad przy oczekiwaniu na proces potomny");
            exit(EXIT_FAILURE);
        }
    
    }
    
    return 0;
}