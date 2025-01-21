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
int msqid_klient;
int kierownik_msqid;

// Funkcja czyszcząca, która odłącza pamięć współdzieloną
void cleanup_handler()
{
    exit(0);
}

// Obsługa sygnału ewakuacji
void evacuation_handler(int signum)
{
    printf("Klient %d: Ewakuacja!, odkładam produkty do kosza i wychodzę.\n", getpid());

    // Odkładanie produktów do kosza
    for (int i = 0; i < sklep->klienci[klient_index].ilosc_zakupow; i++)
    {
        int produkt_id = sklep->klienci[klient_index].lista_zakupow[i].id;
        sem_wait(sem_id, produkt_id);
        kosz->produkty[produkt_id].ilosc += sklep->klienci[klient_index].lista_zakupow[i].ilosc;
        strcpy(kosz->produkty[produkt_id].nazwa, sklep->klienci[klient_index].lista_zakupow[i].nazwa);
        sem_post(sem_id, produkt_id);
    }

    // Zmniejszenie liczby klientów w sklepie
    sem_wait(sem_id, 12);
    sklep->ilosc_klientow--;
    sem_post(sem_id, 12);

    cleanup_handler();
}

// Zakupy klienta
void zakupy(Sklep *sklep, int sem_id, int klient_id, int msqid) {
    signal(SIGUSR1, evacuation_handler);

    message_buf kierownik_rbuf;

    // Czekanie na wejście do sklepu
    while (1) {
        sem_wait(sem_id, 12);
        if (sklep->ilosc_klientow < MAX_KLIENTOW) {
            sklep->ilosc_klientow++;
            sem_post(sem_id, 12);
            break;
        } else {
            sem_post(sem_id, 12);
            printf("Klient %d: Sklep jest pełny, czekam przed wejściem...\n", klient_id);
            sleep(1);
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
            int produkt_id = lista_zakupow[i].id; 
            sem_wait(sem_id, produkt_id); 
            
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

    // // Sprawdzenie, czy sklep jest zamknięty
    if (sklep->sklep_zamkniety) {
            printf("Klient %d: Sklep zamknięty, zwracam produkty do podajników i wychodzę\n", klient_id);
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

    // Znalezienie kasy z najmniejszą kolejką
    int kasa_id;
    while ((kasa_id = znajdz_kase_z_najmniejsza_kolejka(sklep, sem_id)) == -1) {
        printf("Klient %d: Wszystkie kasy są zamknięte, czekam...\n", klient_id);
        sleep(1);
    }

    // Dodanie klienta do kolejki w wybranej kasie
    sem_wait(sem_id, 13 + kasa_id);
    sklep->kasjerzy[kasa_id].kolejka_klientow[sklep->kasjerzy[kasa_id].tail] = klient_index;
    sklep->kasjerzy[kasa_id].tail = (sklep->kasjerzy[kasa_id].tail + 1) % MAX_KLIENTOW;
    sklep->kasjerzy[kasa_id].ilosc_klientow++; // Zwiększenie liczby klientów w kasie
    sem_post(sem_id, 13 + kasa_id);

    printf("Klient %d: Ustawiam się w kolejce do kasy %d\n", klient_id, kasa_id + 1);

    // Otwieranie kolejki komunikatów
    key_t key = ftok("/tmp", kasa_id + 1);
    int msqid_kasy = msgget(key, 0666 | IPC_CREAT);
    if (msqid_kasy == -1) {
        perror("msgget");
        exit(1);
    }
    
    // Czekanie na komunikat od kasjera
    message_buf rbuf;
    if (msgrcv(msqid_kasy, &rbuf, sizeof(rbuf.mtext), getpid(), 0) != -1) {
        printf("Klient %d: Byłem obsłużony, mogę opuścić sklep\n", klient_id);
    } else {
            perror("msgrcv klient");
            exit(1);
    }
    
    // Klient opuszcza sklep
    sem_wait(sem_id, 12);
    sklep->ilosc_klientow--;
    sem_post(sem_id, 12);
    printf("Klient %d: Opuszczam sklep\n", klient_id);
}

int main() {
    setup_signal_handlers(cleanup_handler, evacuation_handler);

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

    key_t key = ftok("/tmp", msq_klient);
    msqid_klient = msgget(key, 0666 | IPC_CREAT);
    if (msqid_klient == -1) {
        perror("msgget");
        exit(1);
    }

    key_t kierownik_key = ftok("/tmp", msq_kierownik);
    kierownik_msqid = msgget(kierownik_key, 0666 | IPC_CREAT);
    if (kierownik_msqid == -1) {
        perror("msgget");
        exit(1);
    }

    while (1) {
        message_buf rbuf;
        if (msgrcv(msqid_klient, &rbuf, sizeof(rbuf.mtext), 0, IPC_NOWAIT) != -1) {
            if (strcmp(rbuf.mtext, close_store_message) == 0) {
                printf("Klienci widzą, że sklep jest zamknięty, już nikt więcej do sklepu nie wejdzie\n");
                break;
            }
        }
        if (!sklep->sklep_zamkniety) {
            pid_t pid = fork();
            if (pid == 0) { // Proces potomny
                srand(time(NULL) ^ (getpid() << 16));
                zakupy(sklep, sem_id, getpid(), msqid_klient);
                exit(0);
            } else if (pid > 0) { // Proces macierzysty
                sleep(rand()%3+1);
            } else {
                perror("fork");
                exit(1);
            }
        }
    }

    // Czekanie na zakończenie wszystkich procesów potomnych
    while (wait(NULL) > 0);
    
    // Wysłanie potwierdzenia do kierownika
    message_buf sbuf;
    sbuf.mtype = 1;
    strcpy(sbuf.mtext, acknowledgment_to_kierownik);
    if (msgsnd(kierownik_msqid, &sbuf, sizeof(sbuf.mtext), 0) == -1) {
        perror("msgsnd klienci do kierownika");
        exit(1);
    }

    cleanup_handler();
    return 0;
}