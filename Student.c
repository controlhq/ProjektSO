#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>
#include "Funkcje_IPC.h" 
#include "Funkcje_losujace.h"

typedef struct{
    long mtype;
    float ocena;
}Wiadomosc_zocena;


typedef struct{ //struktura komunikatu
	long mtype;
    float ocena_koncowa;
    int pidStudenta;
    int czy_zdane;
    int id_studenta;
    int zdaje_czy_pytania; // jak zdaje to 1, jak pytania to 0
}Kom_bufor;

typedef struct{
    long mtype; 
    int pid_studenta;   
    int IDczlonkakomisji; 
    int pytanie;   
    int odpowiedz;  
}Pytanie;

void inicjalizacja_studenta(int student_id, int kierunek, int powtarzaEgzamin) {
    //Przygotowuje nowego studenta
    float ocena_poz = losuj_ocene_pozytywna();
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
    
    int index1 = shm_ptr->index;
    if(index1 < MAX_STUD){
        shm_ptr->students[index1] = student;
        shm_ptr->index++;
    }else{
        printf("Przekroczono maksymalna liczbe studentow w pamieci dzielonej\n");
    }

    //printf("Student %d o pid:%d zakonczyl inicjalizaje\n",student_id, getpid());
}

int main(int argc, char *argv[]) {

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

    //printf("Student %d o pid: %d pojawił się przed budynkiem i czeka w kolejce na wejscie\n", student_id, getpid());
    
    semafor_wait(semID,0);
    shm_ptr->students_count++;
    semafor_signal(semID,0);


    //tutaj semafor, który czeka na sygnał od dziekana
    semafor_wait(semID,1);
    
    //tutaj wysyłanie przez dziekana studentów do domu, którzy nie piszą egzaminu
    semafor_wait(semID,0);
    if(kierunek != shm_ptr->wybrany_kierunek){
        semafor_signal(semID,0);
        //printf("Student %d o pid: %d, wrocil do domu, poniewaz egzamin nie dotyczy jego kierunku\n",student_id ,getpid());
        exit(EXIT_SUCCESS);
    }

    printf("Student o pid %d czeka na rozpoczecie egzaminu\n",getpid());
    semafor_signal(semID,0);

    semafor_wait(semID,0);
    //tutaj inicjalizacja we wspolnej pamieci danych studentów którzy zdają egzamin 
    inicjalizacja_studenta(student_id, kierunek, powtarzaEgzamin);
    //printf("Student o pid: %d zainicijalizowal sie\n",getpid());
    semafor_signal(semID,0);



    // TUTAJ JEST PRZEPROWADZANY EGZAMIN KOMISJI A
    //student ustawia się do kolejki do komisji A
    semafor_wait(semID, 2);


    //semafor przed wyslaniem komunikatów do komisji
    semafor_wait(semID,3);

    if(shm_ptr->students[student_id].powtarza_egzamin == 1){
        //przepisuje ocene
        Kom_bufor przepisuje;
        przepisuje.zdaje_czy_pytania=0;
        przepisuje.czy_zdane=1;
        przepisuje.pidStudenta=getpid();
        przepisuje.id_studenta=student_id;
        przepisuje.ocena_koncowa=shm_ptr->students[student_id].ocena_praktykasr;

        //wysylam wiadomosci do przewodniczacego komisji, ze chce przepisac ocene
        przepisuje.mtype = PKAALL;
        msgsnd(msgID,&przepisuje,sizeof(Kom_bufor)-sizeof(long),0);

        //czekam na wiadomosci od przewodniczacego komisji, ze mam przepisana ocene
        msgrcv(msgID, &przepisuje, sizeof(Kom_bufor) - sizeof(long), getpid(), 0);
                
        //zwalnia semafor do komunikatow
        semafor_signal(semID,3);

        //wychodzi z pokoju
        semafor_signal(semID,2);

    }else{

        Kom_bufor zdaje;
        Kom_bufor potwierdz;
        zdaje.zdaje_czy_pytania=0;
        zdaje.czy_zdane=0;
        zdaje.id_studenta=student_id;
        zdaje.pidStudenta=getpid();
        zdaje.mtype = PKAALL;

        //wysylam wiadomosci do przewodniczacego, ze chce pytania

        msgsnd(msgID,&zdaje,sizeof(Kom_bufor)-sizeof(long),0);


        //czekam na wiadomosc z potwierdzeniem
        msgrcv(msgID, &potwierdz, sizeof(Kom_bufor) - sizeof(long), getpid(), 0);
        


        //czekam na wszystkie pytania od komisji
        int pytaniaodebrane=0;
        Pytanie pyt1;
        Pytanie pyt2;
        Pytanie pyt3;

        //czeka na odbiór wszystkich 3 wiadomości

        while (pytaniaodebrane < 3) {
            Pytanie msg;
            // Próba odebrania wiadomości
            if (msgrcv(msgID, &msg, sizeof(Pytanie) - sizeof(long), getpid(), IPC_NOWAIT) == -1) {
                if (errno == ENOMSG) {
                    // Brak wiadomości - kontynuuj pętlę
                    usleep(100000); 
                    continue;
                } else {
                    // Inny blad
                    perror("Blad msgrcv w studencie przy czekaniu na wszystkie pytania\n");
                    exit(EXIT_FAILURE);
                }
            } else {
                // Zidentyfikowanie wiadomości od którego członka pytanie
                if (msg.IDczlonkakomisji == PKA) {
                    pyt1 = msg; //czyli to jest pytanie od przewodniczacego
                } else if (msg.IDczlonkakomisji == CZ2KA) {
                    pyt2 = msg; // to jest od członka 2
                } else  {
                    pyt3 = msg; // a to od czlonka 3
                } 
                pytaniaodebrane++;
            }


        }
        
        //zwalnia semafor, zeby inni studenci mogli otrzymac pytania od komisji
        semafor_signal(semID,3);

        //sleep(T); //tutaj student przygotowuje sie przez okreslony czas do odpowiedzi

        //sprawdza, czy zaden student nie siedzi teraz przed komisja, jak nie to zdaje
        semafor_wait(semID,3);

        //wysyla przewodniczacemu komunikat, ze chce zdawac
        zdaje.zdaje_czy_pytania=1;
        msgsnd(msgID,&zdaje,sizeof(Kom_bufor)-sizeof(long),0);
        
        //wysyla komisji odpowiedzi

        //najpierw przewodniczacemu
        pyt1.odpowiedz = 1; //przykladowa odpowiedz
        pyt1.mtype = PKA;
        msgsnd(msgID,&pyt1,sizeof(Pytanie)-sizeof(long),0);

        //teraz członkowi 2
        pyt2.odpowiedz = 2; //przykladowa odpowiedz
        pyt2.mtype = CZ2KA;
        msgsnd(msgID,&pyt2,sizeof(Pytanie)-sizeof(long),0);

        //teraz członkowi 3
        pyt3.odpowiedz = 3; //przykladowa odpowiedz
        pyt3.mtype = CZ3KA;
        msgsnd(msgID,&pyt3,sizeof(Pytanie)-sizeof(long),0);

        //czekaj na ocene koncowa od przewodniczacego komisji

        Wiadomosc_zocena Zwrotna;

        msgrcv(msgID,&Zwrotna,sizeof(Wiadomosc_zocena)-sizeof(long),getpid(),0);

        //zwalnia miejsce innemu studentowi, zeby mogl zdawac
        semafor_signal(semID,3);

        //wychodzi z pokoju komisji A
        semafor_signal(semID,2);
        //printf("Ja, student o pid: %d otrzymalem ocene: %0.1f\n",getpid(),Zwrotna.ocena);

        if(Zwrotna.ocena < 2.5){
            //printf("Student o pid: %d, wrocil do domu, bo otrzymal ocene %0.1f z praktyki\n",getpid(),Zwrotna.ocena);
            exit(EXIT_SUCCESS);
        }
        else{
            //printf("Student o pid %d zdal praktyke na ocene %0.1f\n",getpid(),Zwrotna.ocena);
        }
    }

    // TUTAJ EGZAMIN CZESC TEORETYCZNA









    
    shmdt(shm_ptr);

    return 0;
}