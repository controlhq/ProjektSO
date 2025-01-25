#include <stdlib.h>

float losuj_ocene_pozytywna() {
    int rand_value = rand() % 5;  
    float oceny[] = {3.0, 3.5, 4.0, 4.5, 5.0};  
    float ost = oceny[rand_value];
    return ost; 
}

int losuj_ilosc_studentow() {
    return (rand() % (160 - 80 + 1)) + 80; // losuje liczbe studentów w zakresie 80<=Ni<=160
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