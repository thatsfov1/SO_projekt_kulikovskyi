#ifndef STRUKTURY_H
#define STRUKTURY_H

#include <sys/sem.h>

// Ograniczenia w sklepie
#define MAX_PRODUKTOW 12
#define MAX_KLIENTOW 15
#define MAX_KASJEROW 3
#define MAX_PRODUKTOW_W_PODAJNIKU 15
#define CZAS_PRACY 15

// Klucze do procedur
#define SKLEP_KEY 1234
#define SEM_KEY 5678

#define KOSZ_KEY 1111
#define msq_kasa1 1
#define msq_kasa2 2
#define msq_kasa3 3
#define msq_klient 4
#define msq_kierownik 5
#define msq_piekarz 6

// liczba kolejek komunikatów dla wyczyszczenia
#define LICZBA_KOLEJEK 6

// zdefiniowane komunikaty dla kolejki komunikatów
#define close_store_message "ZAMKNIJ"
#define acknowledgment_to_kierownik "ACK"
#define klient_rozliczony "OK"
#define ready_to_close "READY_TO_CLOSE"

// indeksy semaforów 
#define SEM_MUTEX_CUSTOMERS 0 
#define SEM_MUTEX_STORE 1  
#define SEM_STORE_CLOSE 2   
#define SEM_QUEUE_MUTEX 3 
#define SEM_DISPENSER 4     
#define SEM_CASHIER_MUTEX 16   
#define SEM_BASKET_MUTEX 19   
#define SEM_STATS_MUTEX 20   
#define SEM_MUTEX_CUSTOMERS_NUMBER 21   


// Kolory
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define RESET "\033[0m"

// Struktura komunikatu
typedef struct
{
    long mtype;
    char mtext[100];
} message_buf;

// Struktura produktu
typedef struct
{
    char nazwa[50];
    float cena;
    int ilosc;
    int id;
} Produkt;

// Struktura podajnika
typedef struct
{
    Produkt produkt;
} Podajnik;

// Statystyki piekarza dla wywołania inwentaryzacji
typedef struct
{
    int wyprodukowane[MAX_PRODUKTOW];
} StatystykiPiekarza;

// Struktura klienta
typedef struct
{
    int klient_id;
    Produkt lista_zakupow[MAX_PRODUKTOW];
    int ilosc_zakupow;
} Klient;

// Struktura kasjera
typedef struct
{
    int ilosc_sprzedanych[MAX_PRODUKTOW];
    float suma;
    int kolejka_klientow[MAX_KLIENTOW];
    int head;
    int tail;
    int ilosc_klientow;
    int otwarta;
} Kasjer;

// Struktura kosza dla ewakuacji
typedef struct
{
    Produkt produkty[MAX_PRODUKTOW];
    int ilosc_produktow;
} Kosz;


// Struktura sklepu
typedef struct
{
    Podajnik podajniki[MAX_PRODUKTOW];
    Klient klienci[MAX_KLIENTOW];
    Kasjer kasjerzy[MAX_KASJEROW];
    int ilosc_klientow;
    int inwentaryzacja;
    Kosz kosz;
    StatystykiPiekarza statystyki_piekarza;
    int sklep_zamkniety;
} Sklep;

#endif