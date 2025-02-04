#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/sem.h>
#include <string.h>
#include <sys/msg.h>
#include <signal.h>
#include "struktury.h"
#include "funkcje.h"
#include <pthread.h>
#include <errno.h>

int shm_id;
int sem_id;
Sklep *sklep;
int klient_index;
int msqid_klient;
int klient_w_sklepie= 0;
int semop_wait_invalid_argument = 0;
pthread_t cleanup_thread;

// Funkcja czyszcząca
void cleanup_handler(){
    exit(0);
}

// Obsługa sygnału ewakuacji
void evacuation_handler(int signum){

    if (!klient_w_sklepie) {
        cleanup_handler();
        return;
    }

    // Odkładanie produktów do kosza
    printf("Klient %d: Ewakuacja, Odkladam produkty do kosza i wychodzę.\n", getpid());
    
    sem_wait(sem_id, SEM_BASKET_MUTEX);
    

    for (int i = 0; i < sklep->klienci[klient_index].ilosc_zakupow; i++){
        int prod_id = sklep->klienci[klient_index].lista_zakupow[i].id;
        sklep->kosz.produkty[prod_id].ilosc += sklep->klienci[klient_index].lista_zakupow[i].ilosc;
    }

    sem_wait(sem_id, SEM_MUTEX_CUSTOMERS);
    sklep->klienci[klient_index].klient_id = 0;
    
    sem_post(sem_id, SEM_MUTEX_CUSTOMERS);
    

    // klient opuszcza sklep po odłożeniu produktów do kosza
    sem_wait(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
    sklep->ilosc_klientow--;
    printf("Ilosc klientow: %d\n", sklep->ilosc_klientow);
    sem_post(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
    sem_post(sem_id, SEM_BASKET_MUTEX);

    cleanup_handler();
}

// zakupy klienta
void zakupy(Sklep *sklep, int sem_id, int klient_id, int msqid) {
    signal(SIGUSR1, evacuation_handler);

    message_buf kierownik_rbuf;
    klient_index = klient_id % MAX_KLIENTOW;

    // Czekanie na wejście do sklepu, jeśli sklep jest pełny to czeka przed wejściem 1s i probuje ponownie
    int proby = 0;
    while (proby < MAX_PROBY) {

        sem_wait(sem_id, SEM_MUTEX_STORE);
        int zamkniety = sklep->sklep_zamkniety;
        int ewakuacja = sklep->ewakuacja;
        sem_post(sem_id, SEM_MUTEX_STORE);

        if (zamkniety || ewakuacja) {
            printf("Klient %d: Sklep zamknięty, odchodzę.\n", klient_id);
            exit(0);
        }

        sem_wait(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
        if (sklep->ilosc_klientow < MAX_KLIENTOW) {
            sklep->ilosc_klientow++;
            klient_w_sklepie = 1;
            sem_post(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
            break;
        }
        sem_post(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);

        proby++;
        sleep(1);
    }

    if (proby == MAX_PROBY) {
        printf("Klient %d: Zrezygnowałem z czekania na wejście.\n", klient_id);
        exit(0);
    }

    // Losowanie listy zakupów klienta
    Produkt lista_zakupow[MAX_PRODUKTOW] = {0};
    int liczba_produktow = 0;

    // Czyszczenie struktury klienta
    sem_wait(sem_id, SEM_MUTEX_CUSTOMERS);
    memset(&sklep->klienci[klient_index], 0, sizeof(Klient));
    sem_post(sem_id, SEM_MUTEX_CUSTOMERS);

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
            int prod_id = lista_zakupow[i].id;
            sem_wait(sem_id, SEM_DISPENSER + prod_id); 

            int dostepne = sklep->podajniki[prod_id].produkt.ilosc;
            int do_wziecia = (lista_zakupow[i].ilosc <= dostepne) ? 
                         lista_zakupow[i].ilosc : dostepne;
            
            sklep->podajniki[prod_id].produkt.ilosc -= do_wziecia;
            lista_zakupow[i].ilosc = do_wziecia;
        
            printf("Klient %d: Biorę %d szt. %s\n", 
                   klient_id, do_wziecia, lista_zakupow[i].nazwa);
            
            sem_post(sem_id, SEM_DISPENSER + prod_id);
        }
    }

    // Zapisanie zaktualizowanej listy do struktury klienta
    sem_wait(sem_id, SEM_MUTEX_CUSTOMERS);
    sklep->klienci[klient_index].klient_id = klient_id;
    memcpy(sklep->klienci[klient_index].lista_zakupow, lista_zakupow, sizeof(lista_zakupow));
    sklep->klienci[klient_index].ilosc_zakupow = liczba_produktow;
    sem_post(sem_id, SEM_MUTEX_CUSTOMERS);

    // Sprawdzenie, czy sklep jest zamknięty, jeśli tak to zwrócenie produktów do podajników i wyjście
    sem_wait(sem_id, SEM_MUTEX_STORE);
    if (sklep->sklep_zamkniety) {
        sem_post(sem_id, SEM_MUTEX_STORE);
            printf("Klient %d: Sklep zamknięty, zwracam produkty do podajników i wychodzę\n", klient_id);

            for (int i = 0; i < liczba_produktow; i++) {
                int prod_id = lista_zakupow[i].id;
                sem_wait(sem_id, SEM_DISPENSER + prod_id);
                sklep->podajniki[prod_id].produkt.ilosc += lista_zakupow[i].ilosc;
                sem_post(sem_id, SEM_DISPENSER + prod_id);
            }

            sem_wait(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
                sklep->ilosc_klientow--;
                klient_w_sklepie = 0;
            sem_post(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
            
            cleanup_handler();
            return;
    } else {
        sem_post(sem_id, SEM_MUTEX_STORE);
    }

    // Znalezienie kasy z najmniejszą kolejką
    int kasa_id;
    while ((kasa_id = znajdz_kase_z_najmniejsza_kolejka(sklep, sem_id)) == -1) {
        printf("Klient %d: Wszystkie kasy są zamknięte, czekam...\n", klient_id);

        sem_wait(sem_id, SEM_MUTEX_STORE);
        int zamkniety = sklep->sklep_zamkniety;
        int ewakuacja = sklep->ewakuacja;
        sem_post(sem_id, SEM_MUTEX_STORE);

        if (zamkniety || ewakuacja) {
            printf("Klient %d: Sklep zamknięty/ewakuacja w trakcie, zwracam produkty i wychodzę.\n", klient_id);
            
            // Zwróć ewentualnie produkty do podajników 
            for (int i = 0; i < liczba_produktow; i++) {
                int prod_id = lista_zakupow[i].id;
                sem_wait(sem_id, SEM_DISPENSER + prod_id);
                sklep->podajniki[prod_id].produkt.ilosc += lista_zakupow[i].ilosc;
                sem_post(sem_id, SEM_DISPENSER + prod_id);
            }

            sem_wait(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
            sklep->ilosc_klientow--;
            klient_w_sklepie = 0;
            sem_post(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);

            cleanup_handler(); 
        }
        sleep(1);
    }

    // Klient ustawia się w kolejce do tej kasy
    sem_wait(sem_id, SEM_QUEUE_MUTEX);
    sklep->kasjerzy[kasa_id].kolejka_klientow[sklep->kasjerzy[kasa_id].tail] = klient_index;
    sklep->kasjerzy[kasa_id].tail = (sklep->kasjerzy[kasa_id].tail + 1) % MAX_KLIENTOW;
    sklep->kasjerzy[kasa_id].ilosc_klientow++;
    sem_post(sem_id, SEM_QUEUE_MUTEX);

    printf("Klient %d: Ustawiam się w kolejce do kasy %d\n", klient_id, kasa_id + 1);

    // Czekanie na komunikat od kasjera
    key_t key = ftok("/tmp", kasa_id + 1);
    int msqid_kasy;
    initialize_message_queue(&msqid_kasy, key);
    
    message_buf rbuf;
    int ret;
    do {
        ret = msgrcv(msqid_kasy, &rbuf, sizeof(rbuf.mtext), klient_id, IPC_NOWAIT);
        if (ret == -1) {
            if (errno == ENOMSG) {
                // Brak wiadomości – sprawdzamy, czy sklep zamknięty
                sem_wait(sem_id, SEM_MUTEX_STORE);
                int zamkniety = sklep->sklep_zamkniety;
                int ewakuacja = sklep->ewakuacja;
                sem_post(sem_id, SEM_MUTEX_STORE);

                if (zamkniety || ewakuacja) {
                    printf("Klient %d: Sklep zamknięty (msgrcv). Wychodzę.\n", klient_id);
                    sem_wait(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
                    sklep->ilosc_klientow--;
                    sem_post(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
                    exit(0);
                }
                usleep(100000);
            } else {
                perror("msgrcv error");
                exit(1);
            }
        }
    } while (ret == -1);
    
    // Klient opuszcza sklep po zakupach
    sem_wait(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
    sklep->ilosc_klientow--;
    sem_post(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
    printf("Klient %d: Opuszczam sklep\n", klient_id);
}

void* cleanup_thread_func(void* arg) {
    while (1) {
        if (semop_wait_invalid_argument) {
         
            if (klient_w_sklepie) {
                printf("Klient %d: Wykryto błąd semafora, wymuszam wyjście ze sklepu\n", getpid());
                sem_post(sem_id, SEM_MUTEX_CUSTOMERS_NUMBER);
                sem_post(sem_id, SEM_MUTEX_CUSTOMERS);
                sem_post(sem_id, SEM_BASKET_MUTEX);
                sem_post(sem_id, SEM_MUTEX_STORE);
                sem_post(sem_id, SEM_QUEUE_MUTEX);
                
                for (int i = 0; i < MAX_PRODUKTOW; i++) {
                    sem_post(sem_id, SEM_DISPENSER + i);
                }
                
                kill(getpid(), SIGTERM);
            }
            
            break;
        }
        sleep(1);
    }
    return NULL;
}

int main() {
    setup_signal_handlers(cleanup_handler, evacuation_handler);
    chld_handler();
    initialize_shm_sklep(&shm_id, &sklep, SKLEP_KEY);

    sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    key_t key = ftok("/tmp", msq_klient);
    initialize_message_queue(&msqid_klient, key);

     if (pthread_create(&cleanup_thread, NULL, cleanup_thread_func, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }

    // Tworzenie procesów klientów (od 1 do 3 sekund między wejściami)
    while (1) {
    
        // Sprawdzanie czy sklep nie jest zamknięty lub nie ma ewakuacji
        sem_wait(sem_id, SEM_MUTEX_STORE);
        int sklep_zamkniety = sklep->sklep_zamkniety;
        int ewakuacja_w_trakcie = sklep->ewakuacja;
        sem_post(sem_id, SEM_MUTEX_STORE);
    
        if (sklep_zamkniety || ewakuacja_w_trakcie) {
            printf("Proces klientow: Sklep nie przyjmuje nowych klientow. Nikt więcej już nie wejdzie\n");
            break;
        }

        // Tworzenie nowego klienta
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

    // Czekanie na zakończenie wszystkich procesów potomnych
    // pid_t wpid;
    // while ((wpid = waitpid(-1, NULL, WNOHANG)) > 0);
    while (wait(NULL) > 0);
    
    // Wysłanie potwierdzenia do kierownika o zakonczeniu zakupów przez klientów
    send_acknowledgment_to_kierownik();

    cleanup_handler();
    return 0;
}
