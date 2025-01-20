#ifndef STRUKTURY_H
#define STRUKTURY_H

#include <sys/sem.h>

#define MAX_PRODUKTOW 12
#define MAX_KLIENTOW 15
#define MAX_KASJEROW 3
#define MAX_PRODUKTOW_W_PODAJNIKU 15
#define SHM_KEY 1234
#define SEM_KEY 5678
#define KOSZ_KEY 1111
#define CZAS_PRACY 15
#define msq_klient 4
#define msq_kierownik 5
#define msq_piekarz 6
#define msq_main 7
#define close_store_message "ZAMKNIJ"
#define acknowledgment_to_kierownik "ACK"
#define ready_to_close "READY_TO_CLOSE"

// Kolory
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define RESET "\033[0m"

typedef struct {
    long mtype;
    char mtext[100];
} message_buf;

typedef struct {
    char nazwa[50];
    float cena;
    int ilosc;
    int id;
} Produkt;

typedef struct {
    Produkt produkt;
} Podajnik;

typedef struct {
    int wyprodukowane[MAX_PRODUKTOW];
} StatystykiPiekarza;

typedef struct {
    int klient_id;
    Produkt lista_zakupow[MAX_PRODUKTOW];
    int ilosc_zakupow;
} Klient;

typedef struct {
    int ilosc_sprzedanych[MAX_PRODUKTOW];
    float suma;
    int kolejka_klientow[MAX_KLIENTOW];
    int head;
    int tail;
    int ilosc_klientow;
} Kasjer;

typedef struct {
    Podajnik podajniki[MAX_PRODUKTOW];
    Klient klienci[MAX_KLIENTOW];
    Kasjer kasjerzy[MAX_KASJEROW];
    int ilosc_klientow;
    int inwentaryzacja;
    StatystykiPiekarza statystyki_piekarza;
    int sklep_zamkniety;
} Sklep;

typedef struct {
    Produkt produkty[MAX_PRODUKTOW];
    int ilosc_produktow;
} Kosz;

#endif