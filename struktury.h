#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#define P 15          // liczba produktow
#define N 9          // max liczba klientow
#define MAX_ZAKUPY 5  // max liczba roznych produktow na liscie zakupow
#define K (N/3)       // liczba klientow na kase
#define CZAS_PRACY 28800 // 8 godzin w sekundach

// Struktura produktu
struct Produkt {
    int id;
    float cena;
    char nazwa[50];
};

// Struktura podajnika
struct Podajnik {
    int pojemnosc;      // Ki - max pojemnosc
    int liczba_prod;    // aktualna liczba produktow
    int *produkty;      // kolejka FIFO produktow
    int head;           // id do pobrania
    int tail;           // id do dodania
};

// Struktura wspólnej pamięci
struct SharedMemory {
    struct Podajnik podajniki[P];
    int liczba_klientow;
    int czynne_kasy[3];
    long long utarg[3]; // utarg na kasę
    int sprzedane[P]; // liczba sprzedanych sztuk każdego produktu
    int ewakuacja;
    int inwentaryzacja;
    time_t czas_otwarcia;
};

// Struktura komunikatu
struct msg_buf {
    long mtype;
    struct {
        int produkty[MAX_ZAKUPY];
        int ilosci[MAX_ZAKUPY];
        int liczba_prod;
    } zakupy;
};

#define SEM_SHOP 0      // dostep do sklepu
#define SEM_CONV 1      // dostep do podajnikow
#define SEM_MEM 2       // dostep do pamieci wspoldzielonej
#define SEM_KASY 3      // dostep do kas
#define SEM_NEW_CLIENT 4 // nowi klienci
#define TOTAL_SEMS 5    // ilosc semaforow

extern int shmid, semid, msgid;
extern struct SharedMemory *shm;