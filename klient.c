#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/sem.h>
#include <string.h>
#include <sys/msg.h>
#include <signal.h>
#include "struktury.h"
#include "funkcje.h"

int shm_id;
int sem_id;
Sklep *sklep;
Kosz *kosz;
int kosz_id;
int klient_index;

void cleanup_handler(int signum) {
    shmdt(sklep);
    shmdt(kosz);
    exit(0);
}

void evacuation_handler(int signum) {
    printf("Klient %d: Otrzymałem sygnał ewakuacji, odkładam produkty i wychodzę.\n", getpid());

    // Odkładanie produktów do kosza
    for (int i = 0; i < sklep->klienci[klient_index].ilosc_zakupow; i++) {
        int produkt_id = sklep->klienci[klient_index].lista_zakupow[i].id;
        sem_wait(sem_id, produkt_id);
        kosz->produkty[produkt_id].ilosc += sklep->klienci[klient_index].lista_zakupow[i].ilosc;
        strcpy(kosz->produkty[produkt_id].nazwa, sklep->klienci[klient_index].lista_zakupow[i].nazwa); // Copy product name
        sem_post(sem_id, produkt_id);
    }

    // Zmniejszenie liczby klientów w sklepie
    sem_wait(sem_id, 12);
    sklep->ilosc_klientow--;
    sem_post(sem_id, 12);

    cleanup_handler(signum);
}

void losuj_liste_zakupow(Sklep *sklep, Produkt lista_zakupow[], int *liczba_produktow) {
    *liczba_produktow = rand() % 3 + 2; // Min. 2, max. 4 produkty
    for (int i = 0; i < *liczba_produktow; i++) {
        int produkt_id = rand() % MAX_PRODUKTOW;
        Produkt produkt = sklep->podajniki[produkt_id].produkt;
        lista_zakupow[i] = produkt;  // Kopiowanie całej struktury wraz z ID
        lista_zakupow[i].ilosc = rand() % 5 + 1;
    }
}

int znajdz_kase_z_najmniejsza_kolejka(Sklep *sklep, int sem_id) {
    int min_klienci = MAX_KLIENTOW + 1;
    int wybrana_kasa = -1;
    for (int i = 0; i < MAX_KASJEROW; i++) {
        sem_wait(sem_id, 13 + i);
        if (sklep->kasjerzy[i].ilosc_klientow != -1) { // Sprawdzenie, czy kasa jest otwarta
            int liczba_klientow = (sklep->kasjerzy[i].tail + MAX_KLIENTOW - sklep->kasjerzy[i].head) % MAX_KLIENTOW;
            if (liczba_klientow < min_klienci) {
                min_klienci = liczba_klientow;
                wybrana_kasa = i;
            }
        }
        sem_post(sem_id, 13 + i);
    }
    return wybrana_kasa;
}


void zakupy(Sklep *sklep, int sem_id, int klient_id) {
    signal(SIGUSR1, evacuation_handler);
    key_t kierownik_key = ftok("/tmp", msq_kierownik);
    int msqid = msgget(kierownik_key, 0666 | IPC_CREAT);
    if (msqid == -1) {
        perror("msgget");
        exit(1);
    }

    message_buf kierownik_rbuf;
    if (msgrcv(msqid, &kierownik_rbuf, sizeof(kierownik_rbuf.mtext), 0, IPC_NOWAIT) != -1) {
        if (strcmp(kierownik_rbuf.mtext, close_store_message) == 0) {
            printf("Klient %d: Próbuje wejść do sklepu, ale już ogłoszono decyzję o zamknięciu.\n", klient_id);
            exit(0);
        }
    }

    while (1) {
        sem_wait(sem_id, 12);
        if (sklep->ilosc_klientow < MAX_KLIENTOW) {
            sklep->ilosc_klientow++;
            sem_post(sem_id, 12);
            break;
        } else {
            sem_post(sem_id, 12);
            printf( "Klient %d: Sklep jest pełny, czekam przed wejściem...\n", klient_id);
            sleep(1); // Czekanie 1 sekundy przed ponownym sprawdzeniem
        }
    }
    // Losowanie listy zakupów
    Produkt lista_zakupow[MAX_PRODUKTOW] = {0};
    int liczba_produktow = 0;

    // Czyszczenie struktury klienta
    sem_wait(sem_id, 12);
    klient_index = sklep->ilosc_klientow;
    memset(&sklep->klienci[klient_index], 0, sizeof(Klient));
    sem_post(sem_id, 12);

    losuj_liste_zakupow(sklep, lista_zakupow, &liczba_produktow);

    // Wypisanie komunikatu o wejściu klienta
    printf("Klient %d: wchodzę do sklepu z listą: ", klient_id);
    for (int i = 0; i < liczba_produktow; i++) {
        printf("%s %d szt. ", lista_zakupow[i].nazwa, lista_zakupow[i].ilosc);
    }
    printf("\n");

    // Zakupy
    for (int i = 0; i < liczba_produktow; i++) {
        if (lista_zakupow[i].ilosc > 0) {
            int produkt_id = lista_zakupow[i].id;  // Używamy ID produktu
            sem_wait(sem_id, produkt_id);  // Używamy ID produktu jako indeksu semafora
            
            int dostepne = sklep->podajniki[produkt_id].produkt.ilosc;
            int zadane = lista_zakupow[i].ilosc;
            int pobrane = (dostepne >= zadane) ? zadane : dostepne;
            
            sklep->podajniki[produkt_id].produkt.ilosc -= pobrane;
            lista_zakupow[i].ilosc = pobrane;
            
            printf("Klient %d: Biorę %d szt. %s\n", 
                   klient_id, pobrane, lista_zakupow[i].nazwa);
            
            sem_post(sem_id, produkt_id);
        }
    }

    // Zapisanie zaktualizowanej listy do struktury klienta
    sem_wait(sem_id, 12);
    sklep->klienci[klient_index].klient_id = klient_id;
    memcpy(sklep->klienci[klient_index].lista_zakupow, lista_zakupow, sizeof(lista_zakupow));
    sklep->klienci[klient_index].ilosc_zakupow = liczba_produktow;
    sklep->ilosc_klientow++;
    sem_post(sem_id, 12);

    int kasa_id;
    while ((kasa_id = znajdz_kase_z_najmniejsza_kolejka(sklep, sem_id)) == -1) {
        printf("Klient %d: Wszystkie kasy są zamknięte, czekam...\n", klient_id);
        usleep(1000000); // Czekanie 1 sekundy przed ponownym sprawdzeniem
    }

    // Sprawdzenie, czy sklep jest zamknięty
    if (sklep->ilosc_klientow == 0) {
        printf("Klient %d: Sklep jest zamknięty, odkładam produkty do podajnikow i wychodzę\n", klient_id);
        for (int i = 0; i < liczba_produktow; i++) {
            int produkt_id = lista_zakupow[i].id;
            sem_wait(sem_id, produkt_id);
            sklep->podajniki[produkt_id].produkt.ilosc += lista_zakupow[i].ilosc;
            sem_post(sem_id, produkt_id);
        }
        sem_wait(sem_id, 12);
        sklep->ilosc_klientow--;
        sem_post(sem_id, 12);
        return;
    }

    sem_wait(sem_id, 13 + kasa_id);
    sklep->kasjerzy[kasa_id].kolejka_klientow[sklep->kasjerzy[kasa_id].tail] = klient_index;
    sklep->kasjerzy[kasa_id].tail = (sklep->kasjerzy[kasa_id].tail + 1) % MAX_KLIENTOW;
    sklep->kasjerzy[kasa_id].ilosc_klientow++; // Zwiększenie liczby klientów w kasie
    sem_post(sem_id, 13 + kasa_id);

    printf( "Klient %d: Ustawiam się w kolejce do kasy %d\n" , klient_id, kasa_id + 1);

    // Otwieranie kolejki komunikatów
    key_t key = ftok("/tmp", kasa_id + 1);
    int msqid_kasy = msgget(key, 0666 | IPC_CREAT);
    if (msqid_kasy == -1) {
        perror("msgget");
        exit(1);
    }
    
    message_buf rbuf;
    // Czekanie na komunikat od kasjera
    if (msgrcv(msqid_kasy, &rbuf, sizeof(rbuf.mtext), getpid(), 0) == -1) {
        perror("msgrcv klient");
        exit(1);
    }
    printf( "Klient %d: Otrzymałem komunikat od kasjera, mogę opuścić sklep\n" , klient_id);
    // Klient opuszcza sklep
    sem_wait(sem_id, 12);
    sklep->ilosc_klientow--;
    sem_post(sem_id, 12);
    printf( "Klient %d: Opuszczam sklep\n" , klient_id);
}

int main(){
    signal(SIGTERM, cleanup_handler);

    key_t key = ftok("/tmp", msq_kierownik);
    int msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid == -1) {
        perror("msgget");
        exit(1);
    }

    shm_id = shmget(SHM_KEY, sizeof(Sklep), 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    sklep = (Sklep *)shmat(shm_id, NULL, 0);
    if (sklep == (Sklep *)-1) {
        perror("shmat");
        exit(1);
    }

    kosz_id = shmget(KOSZ_KEY, sizeof(Kosz), IPC_CREAT | 0666);
    if (kosz_id < 0) {
        perror("shmget kosz");
        exit(1);
    }

    kosz = (Kosz *)shmat(kosz_id, NULL, 0);
    if (kosz == (Kosz *)-1) {
        perror("shmat kosz");
        exit(1);
    }

    sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    while(1) {
        pid_t pid = fork();
        if (pid == 0) { // Proces potomny
            srand(time(NULL) ^ (getpid()<<16)); // Inicjalizacja generatora liczb losowych na podstawie PID
            zakupy(sklep, sem_id, getpid());
            shmdt(sklep);
            shmdt(kosz);
            exit(0);
        } else if (pid > 0) { // Proces macierzysty
            usleep(1000000); // 1 sekunda
        } else {
            perror("fork");
            exit(1);
        }
        message_buf rbuf;
        if (msgrcv(msqid, &rbuf, sizeof(rbuf.mtext), 0, IPC_NOWAIT) != -1) {
            if (strcmp(rbuf.mtext, close_store_message) == 0) {
                printf("Klient: Otrzymałem komunikat o zamknięciu sklepu, kończę pracę.\n");
                break;
            }
        }
    }

    

    // Czekanie na zakończenie wszystkich procesów potomnych
    while(1) {
        wait(NULL);
    }

    shmdt(sklep);
    shmdt(kosz);
    return 0;
}