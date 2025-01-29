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

const char* Wylosuj_Pytanie_PKB(unsigned int seed) {
    
    const char* questions[] = {
        "Ile wynosi stała Avogadra w przybliżeniu?", 
        "Ile wynosi liczba neuronów w ludzkim mózgu w miliardach?", 
        "Ile par chromosomów ma człowiek?", 
        "Ile podstawowych zasad termodynamiki wyróżniamy?", 
        "Ile lat trwa pełny cykl słoneczny?", 
        "Ile planet w Układzie Słonecznym zostało uznanych za planety według definicji IAU z 2006 roku?", 
        "Ile wynosi temperatura wrzenia wody w stopniach Celsjusza przy ciśnieniu atmosferycznym?", 
        "Ile wynosi 10! (silnia dziesięciu)?", 
        "Ile wynosi liczba protonów w jądrze atomu wodoru?", 
        "Ile wynosi maksymalna liczba elektronów na pierwszej powłoce atomowej?", 
        "Ile wynosi obwód Ziemi w kilometrach w przybliżeniu?", 
    };
    size_t numQuestions = sizeof(questions) / sizeof(questions[0]);
    srand(seed);
    size_t index = rand() % numQuestions;

    return questions[index];
}

const char* Wylosuj_pytanie_CZ2KB(unsigned int seed) {
    const char* questions[] = {
        "Ile wynosi pierwiastek kwadratowy z 144?",
        "Ile wynosi 12 podniesione do potęgi 2?",
        "Ile wynosi suma kątów wewnętrznych czworokąta?",
        "Ile wynosi 7 silnia?",
        "Ile jest liczb pierwszych poniżej 50?",
        "Ile wynosi 3 do potęgi 4?",
        "Ile stopni ma kąt prosty?",
        "Ile wynosi największy wspólny dzielnik 36 i 48?",
        "Ile wynosi logarytm dziesiętny z 1000?",
        "Ile wynosi liczba e z dokładnością do dwóch miejsc po przecinku?"
    };

    size_t numQuestions = sizeof(questions) / sizeof(questions[0]);
    srand(seed);
    size_t index = rand() % numQuestions;
    return questions[index];
}

const char* Wylosuj_pytanie_CZ3KB(unsigned int seed) {
    const char* questions[] = {
        "Ile wynosi prędkość dźwięku w powietrzu w m/s?",
        "Ile wynosi liczba Newtona dla wody?",
        "Ile wynosi stała Plancka w dżulach na sekundę (z dokładnością do 2 miejsc)?",
        "Ile wynosi liczba podstawowych sił w przyrodzie?",
        "Ile wynosi temperatura wrzenia helu w kelwinach?",
        "Ile wynosi stała grawitacji w jednostkach SI?",
        "Ile wynosi liczba falowa fali o długości 500 nm?",
        "Ile lat świetlnych wynosi odległość Słońce-Ziemia?",
        "Ile elektronów przewodnictwa ma atom srebra?",
        "Ile neutronów ma izotop węgla 14?"
    };

    size_t numQuestions = sizeof(questions) / sizeof(questions[0]);
    srand(seed);
    size_t index = rand() % numQuestions;
    return questions[index];
}

const char* Wylosuj_pytanie_PKA(unsigned int seed) {
    const char* questions[] = {
        "Ile bitów ma adres IPv4?",
        "Ile bajtów zajmuje typ 'int' w standardzie C na architekturze 32-bitowej?",
        "Ile wynosi maksymalna wartość w systemie binarnym dla 8-bitowego bajtu?",
        "Ile wynosi maksymalna liczba połączeń w sieci Wi-Fi na paśmie 2,4 GHz?",
        "Ile bajtów wynosi minimalny rozmiar ramki Ethernet?",
        "Ile sekund wynosi TTL (Time-To-Live) domyślnie w pakiecie ICMP (ping)?",
        "Ile poziomów RAID istnieje oficjalnie w standardzie?",
        "Ile wynosi maksymalna długość nazwy pliku w systemie FAT32?",
        "Ile rdzeni ma procesor Intel Core i7-12700K?",
        "Ile maksymalnie znaków może zawierać hasło w systemie Windows 10?"
    };

    size_t numQuestions = sizeof(questions) / sizeof(questions[0]);
    srand(seed);
    size_t index = rand() % numQuestions;
    return questions[index];
}

const char* Wylosuj_pytanie_CZ2KA(unsigned int seed) {
    const char* questions[] = {
        "Ile wynosi napięcie sieciowe w Europie w woltach?",
        "Ile amperów maksymalnie może przewodzić standardowy przewód 1,5 mm²?",
        "Ile wynosi częstotliwość napięcia przemiennego w Europie?",
        "Ile wynosi maksymalne napięcie USB-C PD (Power Delivery) w woltach?",
        "Ile wynosi minimalna rezystancja bezpiecznika 10A?",
        "Ile pinów ma standardowy mikrokontroler ATmega328?",
        "Ile wynosi standardowe napięcie baterii CR2032?",
        "Ile diod LED można połączyć szeregowo na napięciu 12V (zakładając spadek 2V na diodę)?",
        "Ile wynosi w przybliżeniu rezystancja człowieka (sucha skóra) w kiloomach?",
        "Ile voltów wynosi progowe napięcie diody LED czerwonej?"
    };

    size_t numQuestions = sizeof(questions) / sizeof(questions[0]);
    srand(seed);
    size_t index = rand() % numQuestions;
    return questions[index];
}

const char* Wylosuj_pytanie_CZ3KA(unsigned int seed) {
    const char* questions[] = {
        "Ile cegieł potrzeba na zbudowanie 1m^2 ściany jednowarstwowej?",
        "Ile wynosi minimalna grubość izolacji cieplnej w domach pasywnych?",
        "Ile wynosi maksymalne dopuszczalne obciążenie stropu mieszkalnego w kg/m²?",
        "Ile litrów betonu potrzeba do zalania słupa o wymiarach 30x30x300 cm?",
        "Ile wynosi średni czas wiązania betonu w godzinach?",
        "Ile wynosi minimalna grubość fundamentu dla domu jednorodzinnego?",
        "Ile procent cementu zawiera standardowa mieszanka betonu C25/30?",
        "Ile wynosi kąt nachylenia dachu stromego w stopniach?",
        "Ile wynosi minimalna odległość budynku od granicy działki?",
        "Ile minut potrzeba na wymieszanie betonu w betoniarce wolnospadowej?"
    };

    size_t numQuestions = sizeof(questions) / sizeof(questions[0]);
    srand(seed);
    size_t index = rand() % numQuestions;
    return questions[index];
}