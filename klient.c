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
int klient_index;
int msqid_klient;

// Funkcja czyszcząca
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
        sklep->kosz.produkty[produkt_id].ilosc += sklep->klienci[klient_index].lista_zakupow[i].ilosc;
        strcpy(sklep->kosz.produkty[produkt_id].nazwa, sklep->klienci[klient_index].lista_zakupow[i].nazwa);
        sem_post(sem_id, produkt_id);
    }

    // klient opuszcza sklep po odłożeniu produktów do kosza
    sem_wait(sem_id, SEM_KLIENCI);
    sklep->ilosc_klientow--;
    sem_post(sem_id, SEM_KLIENCI);

    cleanup_handler();
}

// zakupy klienta
void zakupy(Sklep *sklep, int sem_id, int klient_id, int msqid) {
    signal(SIGUSR1, evacuation_handler);

    message_buf kierownik_rbuf;

    // Czekanie na wejście do sklepu, jeśli sklep jest pełny to czeka przed wejściem 1s i probuje ponownie
    while (1) {
        sem_wait(sem_id, SEM_KLIENCI);
        if (sklep->ilosc_klientow < MAX_KLIENTOW) {
            klient_index = sklep->ilosc_klientow;
            sklep->ilosc_klientow++;
            sem_post(sem_id, SEM_KLIENCI);
            break;
        } else {
            sem_post(sem_id, SEM_KLIENCI);
            printf("Klient %d: Sklep jest pełny, czekam przed wejściem...\n", klient_id);
            sleep(1);
        }
    }

    // Losowanie listy zakupów klienta
    Produkt lista_zakupow[MAX_PRODUKTOW] = {0};
    int liczba_produktow = 0;

    // Czyszczenie struktury klienta
    sem_wait(sem_id, SEM_KLIENCI);
    memset(&sklep->klienci[klient_index], 0, sizeof(Klient));
    sem_post(sem_id, SEM_KLIENCI);

    losuj_liste_zakupow(sklep, lista_zakupow, &liczba_produktow);

    // Wypisanie komunikatu o wejściu klienta
    printf("Klient %d: wchodzę do sklepu z listą: ", klient_id);
    for (int i = 0; i < liczba_produktow; i++) {
        printf("%s %d szt. ", lista_zakupow[i].nazwa, lista_zakupow[i].ilosc);
    }
    printf("\n");

    // Pobieranie produktów z podajników, ile jest dostępne
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
    sem_wait(sem_id, SEM_KLIENCI);
    sklep->klienci[klient_index].klient_id = klient_id;
    memcpy(sklep->klienci[klient_index].lista_zakupow, lista_zakupow, sizeof(lista_zakupow));
    sklep->klienci[klient_index].ilosc_zakupow = liczba_produktow;
    sem_post(sem_id, SEM_KLIENCI);

    // Sprawdzenie, czy sklep jest zamknięty, jeśli tak to zwrócenie produktów do podajników i wyjście
    if (sklep->sklep_zamkniety) {
            printf("Klient %d: Sklep zamknięty, zwracam produkty do podajników i wychodzę\n", klient_id);
            for (int i = 0; i < liczba_produktow; i++) {
                int produkt_id = lista_zakupow[i].id;
                sem_wait(sem_id, produkt_id);
                sklep->podajniki[produkt_id].produkt.ilosc += lista_zakupow[i].ilosc;
                sem_post(sem_id, produkt_id);
            }
            sem_wait(sem_id, SEM_KLIENCI);
            sklep->klienci[klient_index].klient_id = 0;
            sklep->ilosc_klientow--;
            sem_post(sem_id, SEM_KLIENCI);
            cleanup_handler();
        }

    // Znalezienie kasy z najmniejszą kolejką
    int kasa_id;
    while ((kasa_id = znajdz_kase_z_najmniejsza_kolejka(sklep, sem_id)) == -1) {
        printf("Klient %d: Wszystkie kasy są zamknięte, czekam...\n", klient_id);
        sleep(1);
    }

    // Klient ustawia się w kolejce do tej kasy
    sem_wait(sem_id, 13 + kasa_id);
    sklep->kasjerzy[kasa_id].kolejka_klientow[sklep->kasjerzy[kasa_id].tail] = klient_index;
    sklep->kasjerzy[kasa_id].tail = (sklep->kasjerzy[kasa_id].tail + 1) % MAX_KLIENTOW;
    sklep->kasjerzy[kasa_id].ilosc_klientow++;
    sem_post(sem_id, 13 + kasa_id);
    

    printf("Klient %d: Ustawiam się w kolejce do kasy %d\n", klient_id, kasa_id + 1);

    // Czekanie na komunikat od kasjera
    key_t key = ftok("/tmp", kasa_id + 1);
    int msqid_kasy;
    initialize_message_queue(&msqid_kasy, key);
    
    message_buf rbuf;
    if (msgrcv(msqid_kasy, &rbuf, sizeof(rbuf.mtext), getpid(), 0) != -1) {
        printf("Klient %d: Byłem obsłużony, mogę opuścić sklep\n", klient_id);
    } else {
            exit(1);
    }
    
    // Klient opuszcza sklep po zakupach
    sem_wait(sem_id, SEM_KLIENCI);
    sklep->ilosc_klientow--;
    sem_post(sem_id, SEM_KLIENCI);
    printf("Klient %d: Opuszczam sklep\n", klient_id);
}

int main() {
    setup_signal_handlers(cleanup_handler, evacuation_handler);

    initialize_shm_sklep(&shm_id, &sklep, SKLEP_KEY);

    sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    key_t key = ftok("/tmp", msq_klient);
    initialize_message_queue(&msqid_klient, key);


    // Tworzenie procesów klientów (od 1 do 3 sekund między wejściami)
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
            if (pid == 0) { 
                srand(time(NULL) ^ (getpid() << 16));
                zakupy(sklep, sem_id, getpid(), msqid_klient);
                exit(0);
            } else if (pid > 0) { 
                sleep(rand() % 3 + 1);
            } else {
                perror("fork");
                exit(1);
            }
        }
    }

    // Czekanie na zakończenie wszystkich procesów potomnych
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0);
    
    // Wysłanie potwierdzenia do kierownika o zakonczeniu zakupów przez klientów
    send_acknowledgment_to_kierownik();

    cleanup_handler();
    return 0;
}