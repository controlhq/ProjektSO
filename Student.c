#define _GNU_SOURCE
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
#include <sys/ipc.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

#define T 600
#define K 5                 // ilość kierunków na wydziale X
#define MAX_STUDENTOW 800   // maksymalna ilość studentów 2 roku (160*5)
#define MAX_STUD 160        // do pamieci wspódzielonej maksymalna ilosc studentow

//indentyfikatory komunikatów dla kolejnych czlonków komisji A
#define PKA 449
#define CZ2KA 450
#define CZ3KA 451
//indentyfikatory komunikatów dla kolejnych czlonków komisji B
#define PKB 452
#define CZ2KB 453
#define CZ3KB 454

pid_t mainprogstud; // zmienna do przechowywania pidu glownego programu w celu obsluzenia sygnału

int shmID, semID, msgID;  //ID semafora, kolejki kom. pamieci dzielonej

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

typedef struct{
    long mtype;
    float ocena;
}Wiadomosc_zocena;

SHARED_MEMORY *shm_ptr; //wskaźnik do mapowania wspólnej pamięci

typedef struct{ //struktura komunikatu
	long mtype;
	float ocena;
    int pidStudenta;
    int czy_zdane;
    int id_studenta;
}Kom_bufor;

typedef struct{
    long mtype;  
    int pid_studenta;  
    int IDczlonkakomisji;    
    int i;
    int odpowiedz;
}Pytanie;

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

float losuj_ocene_pozytywna() {
    int rand_value = rand() % 5;  
    float oceny[] = {3.0, 3.5, 4.0, 4.5, 5.0};  
    float ost = oceny[rand_value];
    return ost; 
}

void inicjalizacja_studenta(int student_id, int kierunek, int powtarzaEgzamin) {
    //Przygotowuje nowego studenta
    float ocena_poz = losuj_ocene_pozytywna();
    Student student;
    student.id = student_id;
    student.pid_studenta = getpid();
    student.Kierunek = kierunek;
    student.powtarza_egzamin = powtarzaEgzamin;
    if(powtarzaEgzamin == 1){
        student.zaliczona_praktyka = 1;
        student.ocena_praktykasr = ocena_poz;
        student.ocena_praktyka2 = ocena_poz;
        student.ocena_praktyka3 = ocena_poz;
        student.ocena_praktykaP = ocena_poz;
        student.ocena_teoria2 =0.0;
        student.ocena_teoria3=0.0;
        student.ocena_teoriaP=0.0;
        student.ocena_teoriasr=0.0;
    }else{
        student.zaliczona_praktyka = 0;
        student.ocena_praktykasr= 0.0;
        student.ocena_praktyka2 =0.0;
        student.ocena_praktyka3=0.0;
        student.ocena_praktykaP=0.0;
        student.ocena_teoria2 =0.0;
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

    printf("Student %d o pid:%d zakonczyl inicjalizaje\n",student_id, getpid());
}


int losuj_ilosc_studentow() {
    return (rand() % (160 - 80 + 1)) + 80; // losuje liczbe studentów w zakresie 80<=Ni<=160
}

void Sendmsg(int msgid, long mtype, int student_pid, float grade, int zdane, int id) {
    Kom_bufor msg;
    msg.mtype = mtype;
    msg.pidStudenta = student_pid;
    msg.ocena = grade;
    msg.czy_zdane = zdane;
    msg.id_studenta = id;

    // Wysyłanie wiadomości
    if (msgsnd(msgid, &msg, sizeof(Kom_bufor) - sizeof(long), IPC_NOWAIT) == -1) {
        //tutaj jak kolejka będzie przepelniona to errno zwróci EAGAIN -> zrobić do tego obsluge potem
        perror("Blad msgsnd w studencie");
        exit(EXIT_FAILURE);
    }

}

void koniec() //Funkcja zwalniająca zasoby
{
    // Zwalnianie kolejki komunikatów
    if (msgctl(msgID, IPC_RMID, NULL) == -1) {
        perror("Blad przy zwalnianiu kolejki komunikatow");
    } else {
        printf("Kolejka komunikatow zostala pomyslnie zwolniona\n");
    }

    // Zwalnianie pamięci dzielonej
    if (shmctl(shmID, IPC_RMID, NULL) == -1) {
        perror("Blad przy zwalnianiu pamieci dzielonej");
    } else {
        printf("Pamiec dzielona zostala pomyslnie zwolniona\n");
    }

    // Zwalnianie semaforów
    if (semctl(semID, 0, IPC_RMID) == -1) {
        perror("Blad przy zwalnianiu semaforow");
    } else {
        printf("Semafory zostaly pomyslnie zwolnione\n");
    }

    printf("Zasoby zostaly zwolnione\n");
}

void odbierz_komunikat_przepisanie(){
    Kom_bufor msgKA;

    // Odbieranie komunikatu z kolejki o mtype = getpid()
    if (msgrcv(msgID, &msgKA, sizeof(Kom_bufor) - sizeof(long), getpid(), 0) == -1) {
        perror("Blad msgrcv w komisji\n");
        exit(EXIT_FAILURE);
    }
    
}

void Egzamin_komisjaA(){
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
                usleep(100000); // Opóźnienie dla odciążenia procesora (100ms)
                continue;
            } else {
                // Inny błąd
                perror("Blad msgrcv w studencie");
                exit(EXIT_FAILURE);
            }
        } else {
            // Zidentyfikowanie wiadomości od którego członka pytanie
            if (msg.IDczlonkakomisji == PKA) {
                pyt1 = msg;
            } else if (msg.IDczlonkakomisji == CZ2KA) {
                pyt2 = msg;
            } else  {
                pyt3 = msg;
            } 
            pytaniaodebrane++;
        }
        
        
    }
    

    //po otrzymaniu pytań, student przygotowuje się przez T = 10 minut do odpowiedzi
    //sleep(T);

    //Sprawdza, czy jest wolne miejsce (czy nikt nie siedzi) przed komisja
    semafor_wait(semID, 3);

    //najpierw odpowiada przewodniczacemu
    int odp_przewodniczacy = rand()%6+5;
    pyt1.odpowiedz=odp_przewodniczacy;
    pyt1.mtype = PKA;
    pyt1.pid_studenta=getpid();
    if (msgsnd(msgID, &pyt1, sizeof(Pytanie) - sizeof(long), IPC_NOWAIT) == -1) {
        //tutaj jak kolejka będzie przepelniona to errno zwróci EAGAIN -> zrobić do tego obsluge potem
        perror("Blad msgsnd w studencie");
        exit(EXIT_FAILURE);
    }

    //teraz odpowiadam czlonkowi 2
    int odp_czl2 = rand()%6+5;
    pyt2.odpowiedz=odp_czl2;
    pyt2.mtype = CZ2KA;
    pyt2.pid_studenta=getpid();
    if (msgsnd(msgID, &pyt2, sizeof(Pytanie) - sizeof(long), IPC_NOWAIT) == -1) {
        //tutaj jak kolejka będzie przepelniona to errno zwróci EAGAIN -> zrobić do tego obsluge potem
        perror("Blad msgsnd w studencie");
        exit(EXIT_FAILURE);
    }

    //teraz odpowiadam czlonkowi 3
    int odp_czl3 = rand()%6+5;
    pyt3.odpowiedz=odp_czl3;
    pyt3.mtype = CZ3KA;
    pyt3.pid_studenta=getpid();
    if (msgsnd(msgID, &pyt3, sizeof(Pytanie) - sizeof(long), IPC_NOWAIT) == -1) {
        //tutaj jak kolejka będzie przepelniona to errno zwróci EAGAIN -> zrobić do tego obsluge potem
        perror("Blad msgsnd w studencie");
        exit(EXIT_FAILURE);
    }

    //czeka na wiadomość od przewodniczącego o ocenie 
    Wiadomosc_zocena wiadA;
    if (msgrcv(msgID, &wiadA, sizeof(Wiadomosc_zocena) - sizeof(long), getpid(), 0) == -1) {
        perror("Blad msgrcv w studencie");
        exit(EXIT_FAILURE);
    }


    //tutaj po udzieleniu odpowiedzi i otrzymaniu oceny wychodzi z pokoju i zwalnia kolejke do komisji
    semafor_signal(semID,3);

    //wychodzi z pokoju i zwalnia w nim miejsce
    semafor_signal(semID,2);

    //sprawdza ocene i jak zdał, to idzie dalej, jak nie zdał to exituje
    if(wiadA.ocena < 2.5){
        //student nie zdał i wraca do domu
        printf("Student o pid: %d nie zdal praktyki i poszedl do domu\n",getpid());
        exit(EXIT_SUCCESS);
    }
    printf("Student o pid: %d zdal praktyke na ocene %0.2f\n",getpid(), wiadA.ocena);


}
void sigint_handler(int sig){
    if(mainprogstud == getpid()){
        while(wait(NULL)>0);
        koniec();
        printf("MAIN - funkcja koniec sygnal %d: Koniec.\n",sig);
        exit(0);
    }else{
        exit(EXIT_SUCCESS);
    }
    
}





int main()
{
    srand(time(NULL));
    mainprogstud = getpid();

    //obsluga sygnalu sigint
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags=0;
    sigaction(SIGINT,&act,0);
    

    key_t kluczm, kluczs, kluczk;
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
    shm_ptr->students_count=0; //inicjalizacja counta na 0
    shm_ptr->index =0;

    if((kluczs=ftok(".",'B')) == -1){
        perror("Blad przy tworzeniu klucza do semaforów\n");
        exit(EXIT_FAILURE);
    }

    semID=semget(kluczs,5,IPC_CREAT|0666); 
    if(semID==-1){
        perror("Blad tworzenia semaforow \n");
        exit(EXIT_FAILURE);
    }

    //to jest mutex do operacji na sekcji krytycznej 
    if((semctl(semID,0,SETVAL,1))== -1)
    {
        perror("Blad przy inicjalizacji semafora 0\n");
        exit(EXIT_FAILURE);
    }
    //to jest semafor, który czeka na informacje o kierunku zdającym egzamin od dziekana
    if((semctl(semID,1,SETVAL,0))== -1)
    {
        perror("Blad przy inicjalizacji semafora 1\n");
        exit(EXIT_FAILURE);
    }
    //tutaj kolejka do komisji A (maks 3 osoby)
    if((semctl(semID,2,SETVAL,3))== -1)
    {
        perror("Blad przy inicjalizacji semafora 2\n");
        exit(EXIT_FAILURE);
    }
    //to jest kolejka do odpowiedzi w pokoju komisji A
    if((semctl(semID,3,SETVAL,1))== -1)
    {
        perror("Blad przy inicjalizacji semafora 3\n");
        exit(EXIT_FAILURE);
    }

    if((kluczk = ftok(".", 'C')) == -1)
    {
        perror("Blad ftok dla kolejki komunikatow w studencie\n");
        exit(EXIT_FAILURE);
    }

    msgID=msgget(kluczk,IPC_CREAT|0666);
    if(msgID==-1)
	{
        perror("Blad tworzenia kolejki komunikatow w studencie \n");
        exit(EXIT_FAILURE);
    }


    int lsKierunek_1 = losuj_ilosc_studentow(); //losuje ilości studentów na poszczególnych kierunkach
    int lsKierunek_2 = losuj_ilosc_studentow();
    int lsKierunek_3 = losuj_ilosc_studentow();
    int lsKierunek_4 = losuj_ilosc_studentow();
    int lsKierunek_5 = losuj_ilosc_studentow();
    int liczba_studentow = lsKierunek_1 + lsKierunek_2 + lsKierunek_3 + lsKierunek_4 + lsKierunek_5;
    shm_ptr->ilosc_studentow=liczba_studentow;
    shm_ptr->ilosc_studentow_na_wybranym_kierunku=lsKierunek_2; // bo zakładam sztywno, że kierunek 2 (tak bylo napisane w opisie tematu)

    if (liczba_studentow > MAX_STUDENTOW)
    {
        printf("Przekroczona maksymalna liczba studentow\n");
        koniec();
        exit(EXIT_FAILURE);
    }

    int student_id = 1;
    for (int kierunek = 1; kierunek <= K; kierunek++) {
        int liczba_na_kierunku = (kierunek == 1) ? lsKierunek_1 :
                                 (kierunek == 2) ? lsKierunek_2 :
                                 (kierunek == 3) ? lsKierunek_3 :
                                 (kierunek == 4) ? lsKierunek_4 : lsKierunek_5;

        for (int i = 0; i < liczba_na_kierunku; i++) {

            //int opoznienie = rand() % 6 +5; // studenci pojawiaja sie w losowych momentach czasu(5-10sek)
            //sleep(opoznienie);


            int powtarzaEgzamin = rand() % 100 < 5 ? 1 : 0; // 5% studentów powtarza egzamin z zaliczoną praktyką

            pid_t pid = fork();
            if (pid < 0) {
                perror("Blad forka\n");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                // Proces potomny (student)
                printf("Student %d o pid: %d pojawił się przed budynkiem i czeka w kolejce na wejscie\n", student_id, getpid());
                
                semafor_wait(semID,0);
                shm_ptr->students_count++;
                semafor_signal(semID,0);


                //tutaj semafor, który czeka na sygnał od dziekana
                semafor_wait(semID,1);
                

                //tutaj wysyłanie przez dziekana studentów do domu, którzy nie piszą egzaminu
                if(kierunek != shm_ptr->wybrany_kierunek){
                    printf("Student %d o pid: %d, wrocil do domu, poniewaz egzamin nie dotyczy jego kierunku\n",student_id ,getpid());
                    exit(EXIT_SUCCESS);
                }

                semafor_wait(semID,0);
                //tutaj inicjalizacja we wspolnej pamieci danych studentów którzy zdają egzamin 
                inicjalizacja_studenta(student_id, kierunek, powtarzaEgzamin);
                semafor_signal(semID,0);

                //student ustawia się do kolejki do komisji A
                semafor_wait(semID, 2);
                semafor_wait(semID,0);
                if(shm_ptr->students[student_id].powtarza_egzamin == 1){
                    //wyslij komunikat przewodniczacego komisji A, ze ma juz zdaną praktyke
                    Sendmsg(msgID, 5, getpid(), shm_ptr->students[student_id].ocena_praktykasr,shm_ptr->students[student_id].powtarza_egzamin,student_id);

                    //czeka na komunikat o tym, że zdał i może  iść do komisji B
                    odbierz_komunikat_przepisanie();
                   

                }
                else{
                    
                    //wysyla komunikat, że on nie przepisuje oceny, tylko chce zdawać normalnie
                    Sendmsg(msgID, 5, getpid(), shm_ptr->students[student_id].ocena_praktykasr,shm_ptr->students[student_id].powtarza_egzamin,student_id);
                    // wyślij komunikat do każdego członka komisji, chcesz zdawac egzamin
                    Sendmsg(msgID, PKA,getpid(),shm_ptr->students[student_id].ocena_praktykasr,shm_ptr->students[student_id].powtarza_egzamin,student_id);
                    Sendmsg(msgID, CZ2KA,getpid(),shm_ptr->students[student_id].ocena_praktykasr,shm_ptr->students[student_id].powtarza_egzamin,student_id);
                    Sendmsg(msgID, CZ3KA,getpid(),shm_ptr->students[student_id].ocena_praktykasr,shm_ptr->students[student_id].powtarza_egzamin,student_id);
                    
                    //funkcja która symuluje dostanie pytań, czekanie 10 minut(okreslony czas i wyslanie odpowiedzi)
                    Egzamin_komisjaA();
                    
                }
                semafor_signal(semID,0);


                exit(0);

            } else {
                // Proces macierzysty
                student_id++;
            }
        }
    }
    
    
    while (wait(NULL) > 0); // czekam na zakończenie wszystkich procesów potomnych
    
    koniec();
    return 0;
}
