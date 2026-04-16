CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -pthread

all: Tworzenie_studentow Student Dziekan Komisja

Tworzenie_studentow: Tworzenie_studentow.c Funkcje_IPC.c Funkcje_losujace.c
	$(CC) $(CFLAGS) -o $@ $^

Student: Student.c Funkcje_IPC.c Funkcje_losujace.c
	$(CC) $(CFLAGS) -o $@ $^

Dziekan: Dziekan.c Funkcje_IPC.c Funkcje_losujace.c
	$(CC) $(CFLAGS) -o $@ $^

Komisja: Komisja.c Funkcje_IPC.c Funkcje_losujace.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f Tworzenie_studentow Student Dziekan Komisja
run: all
	@echo "Uruchamiam Komisja i Dziekan w tle, a na koniec Tworzenie_studentow"
	./Tworzenie_studentow &
	sleep 1
	./Dziekan &
	sleep 1
	./Komisja
