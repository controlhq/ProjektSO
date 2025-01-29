#ifndef FUNKCJE_LOSUJACE_H
#define FUNKCJE_LOSUJACE_H

float losuj_ocene_pozytywna();
int losuj_ilosc_studentow();
float wylosuj_ocene();
float oblicz_srednia_i_zaokragl(float a, float b, float c);
float oblicz_srednia_i_zaokragl_DZIEKAN(float a, float b);
const char* Wylosuj_Pytanie_PKB(unsigned int seed);
const char* Wylosuj_pytanie_CZ2KB(unsigned int seed);
const char* Wylosuj_pytanie_CZ3KB(unsigned int seed);
const char* Wylosuj_pytanie_PKA(unsigned int seed);
const char* Wylosuj_pytanie_CZ2KA(unsigned int seed);
const char* Wylosuj_pytanie_CZ3KA(unsigned int seed);


#endif 