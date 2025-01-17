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
#include <sys/types.h>
#include <time.h>

#define K 5                 // ilość kierunków na wydziale X
#define MAX_STUDENTOW 800   // maksymalna ilość studentów 2 roku (160*5)
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

int losuj_ilosc_studentow() {
    return (rand() % (160 - 80 + 1)) + 80; // losuje liczbe studentów w zakresie 80<=Ni<=160
}

void inicjalizacja_studenta(int student_id, int kierunek, int powtarzaEgzamin) {
    //Przygotowuje nowego studenta
    Student student;
    student.id = student_id;
    student.pid_studenta = getpid();
    student.Kierunek = kierunek;
    student.powtarza_egzamin = powtarzaEgzamin;
    if(powtarzaEgzamin == 1){
        student.zaliczona_praktyka = 1;
        student.ocena_praktyka = losuj_ocene_pozytywna();
    }else{
        student.zaliczona_praktyka = 0;
        student.ocena_praktyka= 0.0;
    }

    student.ocena_teoria =0.0;
    
    int index1 = shm_ptr->index;
    if(index1 < MAX_STUD){
        shm_ptr->students[index1] = student;
        shm_ptr->index++;
    }else{
        printf("Przekroczono maksymalna liczbe studentow w pamieci dzielonej\n");
    }

    printf("Student o pid:%d zakonczyl inicjalizaje\n",getpid());
}


void koniec() //Funkcja zwalniająca zasoby
{
    // Zwalnianie kolejki komunikatów
    //if (msgctl(msgID, IPC_RMID, NULL) == -1) {
    //    perror("Blad przy zwalnianiu kolejki komunikatow");
    //} else {
    //    printf("Kolejka komunikatow zostala pomyslnie zwolniona\n");
   // }

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
        printf("Semafory zostały pomyslnie zwolnione\n");
    }

    printf("Zasoby zostały zwolnione\n");
}

int main()
{
    srand(time(NULL));
    

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

    semID=semget(kluczs,2,IPC_CREAT|0666); 
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
    

    int lsKierunek_1 = losuj_ilosc_studentow(); //losuje ilości studentów na poszczególnych kierunkach
    int lsKierunek_2 = losuj_ilosc_studentow();
    int lsKierunek_3 = losuj_ilosc_studentow();
    int lsKierunek_4 = losuj_ilosc_studentow();
    int lsKierunek_5 = losuj_ilosc_studentow();
    int liczba_studentow = lsKierunek_1 + lsKierunek_2 + lsKierunek_3 + lsKierunek_4 + lsKierunek_5;
    shm_ptr->ilosc_studentow=liczba_studentow;

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
                perror("Blad forka");
                return 1;
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
                    printf("Student o pid: %d, wrocil do domu, poniewaz egzamin nie dotyczy jego kierunku\n", getpid());
                    exit(EXIT_SUCCESS);
                }

                semafor_wait(semID,0);
                //tutaj inicjalizacja we wspolnej pamieci danych studentów którzy zdają egzamin 
                inicjalizacja_studenta(student_id, kierunek, powtarzaEgzamin);
                semafor_signal(semID,0);




                exit(0);

            } else {
                // Proces macierzysty
                student_id++;
            }
        }
    }
    
    
    while (wait(NULL) > 0); // czekam na zakończenie wszystkich procesów potomnych
    
    Student first_student = shm_ptr->students[0];
    printf("Dane studenta na indeksie 0:\n");
    printf("ID: %d\n", first_student.id);
    printf("PID: %d\n", first_student.pid_studenta);
    printf("Kierunek: %d\n", first_student.Kierunek);
    printf("Powtarza egzamin: %d\n", first_student.powtarza_egzamin);
    printf("Zaliczona praktyka: %d\n", first_student.zaliczona_praktyka);
    printf("Ocena praktyka: %.1f\n", first_student.ocena_praktyka);
    printf("Ocena teoria: %.1f\n", first_student.ocena_teoria);
    
    koniec();
    return 0;
}
