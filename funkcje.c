#include "funkcje.h"
#include "struktury.h"
#include <string.h>

void sem_wait(int sem_id, int sem_num) {
    struct sembuf sem_op;
    sem_op.sem_num = sem_num;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    if (semop(sem_id, &sem_op, 1) == -1) {
        perror("semop wait");
        exit(1);
    }
}

void sem_post(int sem_id, int sem_num) {
    struct sembuf sem_op;
    sem_op.sem_num = sem_num;
    sem_op.sem_op = 1;
    sem_op.sem_flg = 0;
    if (semop(sem_id, &sem_op, 1) == -1) {
        perror("semop post");
        exit(1);
    }
}

void init_produkty(Sklep *sklep) {
    memset(sklep, 0, sizeof(Sklep));
    char *nazwy[MAX_PRODUKTOW] = {
        "Chleb", "Bułka", "Rogalik", "Bagietka", "Pączek",
        "Ciasto", "Tort", "Muffin", "Keks", "Babka",
        "Sernik", "Makowiec"
    };
    float ceny[MAX_PRODUKTOW] = {
        3.0, 1.0, 2.5, 4.0, 2.0,
        10.0, 50.0, 3.5, 15.0, 20.0,
        25.0, 30.0
    };

    for (int i = 0; i < MAX_PRODUKTOW; i++) {
        strcpy(sklep->podajniki[i].produkt.nazwa, nazwy[i]);
        sklep->podajniki[i].produkt.cena = ceny[i];
        sklep->podajniki[i].produkt.ilosc = 0;
        sklep->podajniki[i].produkt.id = i;
    }
}

void init_kosz(Kosz *kosz, Sklep *sklep) {
    memset(kosz, 0, sizeof(Kosz));
    for (int i = 0; i < MAX_PRODUKTOW; i++) {
        kosz->produkty[i].id = i;
        strcpy(kosz->produkty[i].nazwa, sklep->podajniki[i].produkt.nazwa);
        kosz->produkty[i].ilosc = 0;
        kosz->produkty[i].cena = sklep->podajniki[i].produkt.cena;
    }
}

void drukuj_produkt(const char* nazwa, int ilosc) {
    printf("%s %d szt. ", nazwa, ilosc);
}

// Losowanie listy zakupów
void losuj_liste_zakupow(Sklep *sklep, Produkt lista_zakupow[], int *liczba_produktow)
{
    *liczba_produktow = rand() % 3 + 2; // Min. 2, max. 4 produkty
    for (int i = 0; i < *liczba_produktow; i++)
    {
        int produkt_id = rand() % MAX_PRODUKTOW;
        Produkt produkt = sklep->podajniki[produkt_id].produkt;
        lista_zakupow[i] = produkt; // Kopiowanie całej struktury wraz z ID
        lista_zakupow[i].ilosc = rand() % 5 + 1;
    }
}

// Znalezienie kasy z najmniejszą kolejką
int znajdz_kase_z_najmniejsza_kolejka(Sklep *sklep, int sem_id)
{
    int min_klienci = MAX_KLIENTOW + 1;
    int wybrana_kasa = -1;
    for (int i = 0; i < MAX_KASJEROW; i++)
    {
        sem_wait(sem_id, 13 + i);
        if (sklep->kasjerzy[i].ilosc_klientow != -1)
        { // Sprawdzenie, czy kasa jest otwarta
            int liczba_klientow = (sklep->kasjerzy[i].tail + MAX_KLIENTOW - sklep->kasjerzy[i].head) % MAX_KLIENTOW;
            if (liczba_klientow < min_klienci)
            {
                min_klienci = liczba_klientow;
                wybrana_kasa = i;
            }
        }
        sem_post(sem_id, 13 + i);
    }
    return wybrana_kasa;
}

void setup_signal_handlers(void (*cleanup_handler)(int), void (*evacuation_handler)(int)) {
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);
    signal(SIGUSR1, evacuation_handler);
}