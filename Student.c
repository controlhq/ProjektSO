#define _GNU_SOURCE
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "Funkcje_IPC.h" 
#include "Funkcje_losujace.h"

typedef struct{
    long mtype;
    float ocena;
}Wiadomosc_zocena;


typedef struct{ 
	long mtype;
    float ocena_koncowa;
    int pidStudenta;
    int czy_zdane;
    int index_studenta;
    int id_studenta;
    int zdaje_czy_pytania; // jak zdaje to 1, jak pytania to 0
}Kom_bufor;

typedef struct {
    long mtype; 
    int pid_studenta;   
    int IDczlonkakomisji; 
    char pytanie[MAX_Q_LENG];  
    int odpowiedz;  
} Pytanie;

int inicjalizacja_studenta(int student_id, int kierunek, int powtarzaEgzamin) {
    //Przygotowuje nowego studenta
    unsigned int seedOSB = time(NULL) ^ getpid();
    float ocena_poz = losuj_ocene_pozytywna(&seedOSB);
    Student student;
    student.id = student_id;
    student.pid_studenta = getpid();
    student.Kierunek = kierunek;
    student.powtarza_egzamin = powtarzaEgzamin;
    if(powtarzaEgzamin == 1){
        student.ocena_praktykasr = ocena_poz;
        student.ocena_praktyka2 = ocena_poz;
        student.ocena_praktyka3 = ocena_poz;
        student.ocena_praktykaP = ocena_poz;
        student.ocena_teoria2 =0.0;
        student.ocena_teoria3=0.0;
        student.ocena_teoriaP=0.0;
        student.ocena_teoriasr=0.0;
    }else{
        student.ocena_praktykasr=0.0;
        student.ocena_praktyka2=0.0;
        student.ocena_praktyka3=0.0;
        student.ocena_praktykaP=0.0;
        student.ocena_teoria2=0.0;
        student.ocena_teoria3=0.0;
        student.ocena_teoriaP=0.0;
        student.ocena_teoriasr=0.0;
    }
    

    int index1 = shm_ptr->index_stud;
    if(index1 < MAX_STUD){
        shm_ptr->students[index1] = student;
        shm_ptr->index_stud++;
    }else{
        printf("Przekroczono maksymalna liczbe studentow w pamieci dzielonej\n");
    }

    //printf("Student %d o pid:%d zakonczyl inicjalizaje\n",student_id, getpid());
    
    return index1;
}

void handle_sigusr1(int sig) {
    (void)sig; 
    semafor_wait(semID,7);
    Zapiszlog("Student o pid: %d ewakuowal się",getpid());
    semafor_signal(semID,7);
    exit(EXIT_SUCCESS); // Natychmiastowe zakończenie procesu
}

int main(int argc, char *argv[]) {

    struct sigaction sa_usr1;
    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handle_sigusr1; 
    sigemptyset(&sa_usr1.sa_mask);        
    sa_usr1.sa_flags = 0;                  

    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        perror("[STUDENT] Blad ustawienia sigaction dla SIGUSR1");
        exit(EXIT_FAILURE);
    }

    if (argc < 6) {
        fprintf(stderr, "Użycie: %s semID msgID shmID student_id kierunek\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int semID = atoi(argv[1]);
    int msgID = atoi(argv[2]);
    int shmID = atoi(argv[3]);
    int student_id = atoi(argv[4]);
    int kierunek = atoi(argv[5]);

    shm_ptr =(SHARED_MEMORY*)shmat(shmID, NULL, 0);
    if (shm_ptr == (void *)-1){
        perror("Blad przy dolaczaniu pamieci dzielonej w Studencie\n");
        exit(EXIT_FAILURE);
    }

    int powtarzaEgzamin = rand() % 100 < 5 ? 1 : 0; // 5% studentów powtarza egzamin z zaliczoną praktyką
    
    //tutaj zapisuje ilosc osob, które przepisuje ocene (zalożenie ze kierunek==2)
    if(kierunek == 2 && powtarzaEgzamin ==1){
        semafor_wait(semID,0);
        shm_ptr->ilosc_osob_przepisujacych++;
        semafor_signal(semID,0);
    }

    semafor_wait(semID,7);
    Zapiszlog("Student pojawił się przed budynkiem i czeka w kolejce na wejscie");
    //printf("Student %d o pid: %d pojawił się przed budynkiem i czeka w kolejce na wejscie\n", student_id, getpid());
    semafor_signal(semID,7);


    semafor_wait(semID,0);
    shm_ptr->students_count++;
    semafor_signal(semID,0);


    //tutaj semafor, który czeka na sygnał od dziekana
    semafor_wait(semID,1);
    
    //tutaj wysyłanie przez dziekana studentów do domu, którzy nie piszą egzaminu
    semafor_wait(semID,0);
    if(kierunek != shm_ptr->wybrany_kierunek){
        semafor_signal(semID,0);
        semafor_wait(semID,7);
        Zapiszlog("Student o pid %d wrocil do domu, poniewaz egzamin nie dotyczy jego kierunku",getpid());
        semafor_signal(semID,7);
        //printf("Student %d o pid: %d, wrocil do domu, poniewaz egzamin nie dotyczy jego kierunku\n",student_id ,getpid());
        return 0;
    }

    printf("Student o pid %d czeka na rozpoczecie egzaminu\n",getpid());
    semafor_signal(semID,0);

    semafor_wait(semID,0);
    //tutaj inicjalizacja we wspolnej pamieci danych studentów którzy zdają egzamin 
    int curr_indx = inicjalizacja_studenta(student_id, kierunek, powtarzaEgzamin);
    semafor_wait(semID,7);
    Zapiszlog("Student zakonczyl inicjalizacje");
    semafor_signal(semID,7);
    //printf("Student o pid: %d zainicijalizowal sie\n",getpid());
    semafor_signal(semID,0);





    // TUTAJ JEST PRZEPROWADZANY EGZAMIN KOMISJI A
    //student ustawia się do kolejki do komisji A
    semafor_wait(semID, 2);


    //semafor przed wyslaniem komunikatów do komisji
    semafor_wait(semID,3);

    if(shm_ptr->students[curr_indx].powtarza_egzamin == 1){
        //przepisuje ocene
        Kom_bufor przepisuje;
        przepisuje.zdaje_czy_pytania=0;
        przepisuje.czy_zdane=1;
        przepisuje.index_studenta=curr_indx;
        przepisuje.pidStudenta=getpid();
        przepisuje.id_studenta=student_id;
        przepisuje.ocena_koncowa=shm_ptr->students[curr_indx].ocena_praktykasr;

        //wysylam wiadomosci do przewodniczacego komisji, ze chce przepisac ocene
        przepisuje.mtype = PKAALL;
        if((msgsnd(msgID,&przepisuje,sizeof(Kom_bufor)-sizeof(long),0)) == -1){
            semafor_wait(semID,7);
            handle_msgsnd_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }

        //czekam na wiadomosci od przewodniczacego komisji, ze mam przepisana ocene
        if((msgrcv(msgID, &przepisuje, sizeof(Kom_bufor) - sizeof(long), getpid(), 0)) == -1){
            semafor_wait(semID,7);
            handle_msgrcv_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }

        semafor_wait(semID,7);
        Zapiszlog("Student o pid: %d przepisal ocene",getpid());
        semafor_signal(semID,7);
                
        //zwalnia semafor do komunikatow
        semafor_signal(semID,3);

        //wychodzi z pokoju
        semafor_signal(semID,2);

    }else{
        //tutaj chce pytania
        
        Kom_bufor zdaje;
        zdaje.index_studenta=curr_indx;
        zdaje.zdaje_czy_pytania=0;
        zdaje.czy_zdane=0;
        zdaje.id_studenta=student_id;
        zdaje.pidStudenta=getpid();
        zdaje.mtype = PKAALL;

        //wysylam wiadomosci do przewodniczacego, ze chce pytania

        if((msgsnd(msgID,&zdaje,sizeof(Kom_bufor)-sizeof(long),0))== -1){
            semafor_wait(semID,7);
            handle_msgsnd_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }

        //czekam na wszystkie pytania od komisji
        int pytaniaodebrane=0;
        Pytanie pyt1;
        Pytanie pyt2;
        Pytanie pyt3;

        //czeka na odbiór wszystkich 3 wiadomości

        while (pytaniaodebrane < 3) {
            Pytanie msg;
            // Próba odebrania wiadomości
            if (msgrcv(msgID, &msg, sizeof(Pytanie) - sizeof(long), getpid(), 0) == -1) {
                    semafor_wait(semID,7);
                    handle_msgrcv_error_with_logging(Zapiszlog);
                    semafor_signal(semID,7);
                    exit(EXIT_FAILURE);
                }else{
                // Zidentyfikowanie wiadomości od którego członka pytanie
                if (msg.IDczlonkakomisji == PKA) {
                    pyt1 = msg; //czyli to jest pytanie od przewodniczacego
                    semafor_wait(semID,7);
                    Zapiszlog("Student otrzymal od Przewodniczacego komisji A Pytanie o tresci: %s",
                    msg.pytanie);
                    semafor_signal(semID,7);
                } else if (msg.IDczlonkakomisji == CZ2KA) {
                    pyt2 = msg; // to jest od członka 2
                    semafor_wait(semID,7);
                    Zapiszlog("Student otrzymal od Czlonka 2 Komisji A Pytanie o tresci: %s",
                    msg.pytanie);
                    semafor_signal(semID,7);
                } else if (msg.IDczlonkakomisji == CZ3KA) {
                    pyt3 = msg; // a to od czlonka 3
                    semafor_wait(semID,7);
                    Zapiszlog("Student otrzymal od Czlonka 3 Komisji A Pytanie o tresci: %s",
                    msg.pytanie);
                    semafor_signal(semID,7);
                } else{
                    perror("nieznana wiadomosc odebrana w Stud->od kom.A\n");
                }
                pytaniaodebrane++;
            }

        }
        
        semafor_wait(semID,7);
        Zapiszlog("Student o pid: %d, dostal wszystkie pytania od komisji A",getpid());
        semafor_signal(semID,7);
        
        //zwalnia semafor, zeby inni studenci mogli otrzymac pytania od komisji
        semafor_signal(semID,3);

        //sleep(T); //tutaj student przygotowuje sie przez okreslony czas do odpowiedzi

        //sprawdza, czy zaden student nie siedzi teraz przed komisja, jak nie to zdaje
        semafor_wait(semID,3);

        //wysyla przewodniczacemu komunikat, ze chce zdawac
        zdaje.zdaje_czy_pytania=1;

        if((msgsnd(msgID,&zdaje,sizeof(Kom_bufor)-sizeof(long),0)) == -1){
            semafor_wait(semID,7);
            handle_msgsnd_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }
        
        //wysyla komisji odpowiedzi

        //najpierw przewodniczacemu
        pyt1.odpowiedz = 1; //przykladowa odpowiedz
        pyt1.mtype = PKA;
        if((msgsnd(msgID,&pyt1,sizeof(Pytanie)-sizeof(long),0)) == -1){
            semafor_wait(semID,7);
            handle_msgsnd_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }

        //teraz członkowi 2
        pyt2.odpowiedz = 2; //przykladowa odpowiedz
        pyt2.mtype = CZ2KA;
        if((msgsnd(msgID,&pyt2,sizeof(Pytanie)-sizeof(long),0)) == -1){
            semafor_wait(semID,7);
            handle_msgsnd_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }

        //teraz członkowi 3
        pyt3.odpowiedz = 3; //przykladowa odpowiedz
        pyt3.mtype = CZ3KA;
        if((msgsnd(msgID,&pyt3,sizeof(Pytanie)-sizeof(long),0)) == -1){
            semafor_wait(semID,7);
            handle_msgsnd_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }

        //czekaj na ocene koncowa od przewodniczacego komisji

        Wiadomosc_zocena Zwrotna;

        if((msgrcv(msgID,&Zwrotna,sizeof(Wiadomosc_zocena)-sizeof(long),getpid(),0)) == -1){
            semafor_wait(semID,7);
            handle_msgrcv_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }

        //zwalnia miejsce innemu studentowi, zeby mogl zdawac
        semafor_signal(semID,3);

        //wychodzi z pokoju komisji A
        semafor_signal(semID,2);
        
        semafor_wait(semID,7);
        Zapiszlog("Student o pid: %d, otrzymał ocene: %0.1f",getpid(),Zwrotna.ocena);
        semafor_signal(semID,7);
        //printf("Ja, student o pid: %d otrzymalem ocene: %0.1f\n",getpid(),Zwrotna.ocena);

        if(Zwrotna.ocena < 2.5){
            semafor_wait(semID,7);
            Zapiszlog("Student o pid: %d wrocil do domu, bo nie zdal egzaminu Kommisji A",getpid());
            semafor_signal(semID,7);
            //printf("Student o pid: %d, wrocil do domu, bo otrzymal ocene %0.1f z praktyki\n",getpid(),Zwrotna.ocena);
            return 0;
        }
        else{
            semafor_wait(semID,7);
            Zapiszlog("Student o pid: %d zdal egzamin praktyczny na ocene: %0.1f i ustawia się do komisji B",getpid(),Zwrotna.ocena);
            semafor_signal(semID,7);
            //printf("Student o pid %d zdal praktyke na ocene %0.1f\n",getpid(),Zwrotna.ocena);
        }
    }


    // TUTAJ EGZAMIN CZESC TEORETYCZNA

    //student ustawia się do kolejki do komisji b
    semafor_wait(semID, 5);


    //semafor przed wyslaniem komunikatów do komisji
    semafor_wait(semID,6);

    Kom_bufor zdaje2;
    zdaje2.index_studenta=curr_indx;
    zdaje2.zdaje_czy_pytania=0; // to znaczy, ze chce pytania
    zdaje2.id_studenta=student_id;
    zdaje2.pidStudenta=getpid();
    zdaje2.mtype = PKBALL;

    //wysylam wiadomosci do przewodniczacego, ze chce pytania

    if((msgsnd(msgID,&zdaje2,sizeof(Kom_bufor)-sizeof(long),0))== -1){
        semafor_wait(semID,7);
        handle_msgsnd_error_with_logging(Zapiszlog);
        semafor_signal(semID,7);
        exit(EXIT_FAILURE);
    }
    
    //czekam na wszystkie pytania od komisji
    int pytaniaodebrane=0;
    Pytanie pyt4;
    Pytanie pyt5;
    Pytanie pyt6;

    //czeka na odbiór wszystkich 3 wiadomości

    while (pytaniaodebrane < 3) {
        Pytanie msg2;
        // Próba odebrania wiadomości
        if ((msgrcv(msgID, &msg2, sizeof(Pytanie) - sizeof(long), getpid(), 0)) == -1) {
            semafor_wait(semID,7);
            handle_msgrcv_error_with_logging(Zapiszlog);
            semafor_signal(semID,7);
            exit(EXIT_FAILURE);
        }else{
            // Zidentyfikowanie wiadomości od którego członka pytanie
            if (msg2.IDczlonkakomisji == PKB) {
                pyt4 = msg2; //czyli to jest pytanie od przewodniczacego
                semafor_wait(semID,7);
                Zapiszlog("Student otrzymal od Przewodniczacego Komisji B Pytanie o tresci: %s",
                msg2.pytanie);
                semafor_signal(semID,7);
            } else if (msg2.IDczlonkakomisji == CZ2KB) {
                pyt5 = msg2; // to jest od członka 2
                semafor_wait(semID,7);
                Zapiszlog("Student otrzymal od Czlonka 2 Komisji B Pytanie o tresci: %s",
                msg2.pytanie);
                semafor_signal(semID,7);
            } else  {
                pyt6 = msg2; // a to od czlonka 3
                semafor_wait(semID,7);
                Zapiszlog("Student otrzymal od Czlonka 3 Komisji B Pytanie o tresci: %s",
                msg2.pytanie);
                semafor_signal(semID,7);
            } 
            pytaniaodebrane++;
        }

    }
    
    semafor_wait(semID,7); 
    Zapiszlog("Student o pid: %d otrzymal wszystkie pytania od komisji B i sie przygotowuje do odpowiedzi",getpid());
    semafor_signal(semID,7);

    //zwalnia semafor, zeby inni studenci mogli otrzymac pytania od komisji
    semafor_signal(semID,6);

    //sleep(T); //tutaj student przygotowuje sie przez okreslony czas do odpowiedzi

    //sprawdza, czy zaden student nie siedzi teraz przed komisja, jak nie to zdaje
    semafor_wait(semID,6);

    //wysyla przewodniczacemu komunikat, ze chce zdawac
    zdaje2.zdaje_czy_pytania=1;

    if((msgsnd(msgID,&zdaje2,sizeof(Kom_bufor)-sizeof(long),0)) == -1){
        semafor_wait(semID,7);
        handle_msgsnd_error_with_logging(Zapiszlog);
        semafor_signal(semID,7);
        exit(EXIT_FAILURE);
    }
    
    //wysyla komisji odpowiedzi

    //najpierw przewodniczacemu
    pyt4.odpowiedz = 1; //przykladowa odpowiedz
    pyt4.mtype = PKB;
    if((msgsnd(msgID,&pyt4,sizeof(Pytanie)-sizeof(long),0))== -1){
        semafor_wait(semID,7);
        handle_msgsnd_error_with_logging(Zapiszlog);
        semafor_signal(semID,7);
        exit(EXIT_FAILURE);
    }

    //teraz członkowi 2
    pyt5.odpowiedz = 2; //przykladowa odpowiedz
    pyt5.mtype = CZ2KB;
    if((msgsnd(msgID,&pyt5,sizeof(Pytanie)-sizeof(long),0))== -1){
        semafor_wait(semID,7);
        handle_msgsnd_error_with_logging(Zapiszlog);
        semafor_signal(semID,7);
        exit(EXIT_FAILURE);
    }

    //teraz członkowi 3
    pyt6.odpowiedz = 3; //przykladowa odpowiedz
    pyt6.mtype = CZ3KB;
    if((msgsnd(msgID,&pyt6,sizeof(Pytanie)-sizeof(long),0))== -1){
        semafor_wait(semID,7);
        handle_msgsnd_error_with_logging(Zapiszlog);
        semafor_signal(semID,7);
        exit(EXIT_FAILURE);
    }

    //czekaj na ocene koncowa od przewodniczacego komisji

    Wiadomosc_zocena Zwrotna2;

    if((msgrcv(msgID,&Zwrotna2,sizeof(Wiadomosc_zocena)-sizeof(long),getpid(),0))== -1){
        semafor_wait(semID,7);
        handle_msgrcv_error_with_logging(Zapiszlog);
        semafor_signal(semID,7);
        exit(EXIT_FAILURE);
    }

    semafor_wait(semID,7);
    Zapiszlog("Student o pid: %d skonczyl egzamin komisji B i otrzymal ocene %0.1f", getpid(),Zwrotna2.ocena);
    semafor_signal(semID,7);

    //zwalnia miejsce innemu studentowi, zeby mogl zdawac
    semafor_signal(semID,6);

    //wychodzi z pokoju komisji B
    semafor_signal(semID,5);
    
    shmdt(shm_ptr);
    return 0;
}