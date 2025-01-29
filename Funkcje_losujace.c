#include <stdlib.h>

float losuj_ocene_pozytywna(unsigned int* seed) {
    int rand_value = rand_r(seed) % 5;  
    float oceny[] = {3.0, 3.5, 4.0, 4.5, 5.0};  
    return oceny[rand_value]; 
}

int losuj_ilosc_studentow(unsigned int* seed) {
    return (rand_r(seed) % (160 - 80 + 1)) + 80; // losuje liczbę studentów w zakresie 80<=Ni<=160
}

float wylosuj_ocene(unsigned int* seed) {
    float oceny[] = {2.0, 3.0, 3.5, 4.0, 4.5, 5.0};
    int szanse[] = {5, 19, 19, 19, 19, 19}; // 5% na 2.0, reszta po 19%
    int suma_szans = 100;

    // losuje liczbe od 1 do 100
    int los = rand_r(seed) % suma_szans + 1;

    // dopasowanie liczby do przedzialu ocen
    int prog = 0;
    for (int i = 0; i < 6; i++) {
        prog += szanse[i];
        if (los <= prog) {
            return oceny[i];
        }
    }

    return 0.0; // awaryjnie
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

float oblicz_srednia_i_zaokragl_DZIEKAN(float a, float b) {
    // Sprawdzanie, czy którakolwiek liczba jest mniejsza niż 2.1 (bo precyzja)
    if (a < 2.1 || b < 2.1) {
        return 2.0;
    }

    // Obliczanie średniej
    float srednia = (a + b) / 2.0;

    // Zaokrąglanie w górę lub w dół w zależności od wartości średniej
    if (srednia < 2.75) {
        return 2.0; // to nigdy nie powinno się osiągnąć
    } else if (srednia < 3.25) {
        return 3.0;
    } else if (srednia < 3.75) {
        return 3.5;
    } else if (srednia < 4.25) {
        return 4.0;
    } else if (srednia < 4.75) {
        return 4.5;
    } else {
        return 5.0;
    }
}