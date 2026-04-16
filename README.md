# ProjektSO
Projekt Systemy Operacyjne - Egzamin

# Exam Simulator - Inter-Process Communication (IPC)

Projekt zrealizowany w ramach przedmiotu Systemy Operacyjne, symulujący złożony proces egzaminowania studentów. Głównym celem projektu jest praktyczne zastosowanie mechanizmów wieloprocesowości, wielowątkowości oraz komunikacji międzyprocesowej (IPC) w systemach klasy UNIX (POSIX).

## Technologie i Mechanizmy

Projekt został napisany w języku **C** i intensywnie wykorzystuje systemowe API Linuksa:
* **Wieloprocesowość:** `fork()`, `execl()`, `wait()`, `exit()`
* **Wielowątkowość:** POSIX Threads (`pthread_create`, `pthread_join`)
* **Synchronizacja:** Semafory (System V)
* **Komunikacja międzyprocesowa (IPC):**
  * Pamięć współdzielona (Shared Memory)
  * Kolejki komunikatów (Message Queues)
  * Łącza nazwane (FIFO / Named Pipes)
  * Sygnały (np. `SIGUSR1` do obsługi zdarzeń asynchronicznych)

## Architektura Systemu

System symuluje środowisko wydziału za pomocą odrębnych procesów i wątków:

1. **Dziekan (Główny koordynator):** * Zarządza przebiegiem egzaminu i odbiera wyniki przez kolejki FIFO.
   * Działa w nim poboczny wątek asynchroniczny, który symuluje losowe zdarzenia (ewakuacja wydziału za pomocą sygnału `SIGUSR1`).
2. **Komisje Egzaminacyjne (A i B):** * Dwa osobne procesy, z których każdy uruchamia po 3 wątki (członkowie komisji).
   * Do komunikacji ze studentami wykorzystują kolejki komunikatów.
3. **Studenci:** * Indywidualne procesy ubiegające się o dostęp do zasobów (komisji).
   * Kolejkowanie i limit miejsc w sali (max. 3 osoby) są rygorystycznie kontrolowane za pomocą semaforów.
4. **Generator Studentów:** * Proces odpowiedzialny za tworzenie instancji studentów i przekazywanie danych inicjalizacyjnych do Dziekana za pomocą pamięci współdzielonej.

## ⚙️ Logika i Działanie

Proces zdawania egzaminu wymaga pełnej synchronizacji. Studenci ustawiają się w kolejce do komisji (blokowanie na semaforze). Po wejściu komunikują się z wątkami komisji (kolejki komunikatów), pobierają pytania, "przygotowują się" (symulacja czasu operacji), a na koniec odsyłają odpowiedzi. Wyniki przesyłane są potokami do procesu Dziekana, który generuje logi i raport końcowy. Całość działania systemu jest szczegółowo monitorowana we własnym systemie logowania zdarzeń (PID, status, czas).
