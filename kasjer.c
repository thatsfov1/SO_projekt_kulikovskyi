#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/msg.h>
#include "struktury.h"
#include "funkcje.h"

Sklep *sklep;
int sem_id;
int sklep_zamkniety = 0;
int kasa_id;

// Funkcja czyszcząca
void cleanup_handler(int signum)
{
    exit(0);
}

// Obsługa sygnału ewakuacji
void evacuation_handler(int signum)
{
    printf("Kasa %d: Ewakuacja!, zamykam się.\n", kasa_id + 1);
    cleanup_handler(SIGUSR1);
}

// monitorowanie ilości klientów w sklepie, jesli jest ich mniej niż 10 to kasa 3 jest zamknięta
void monitoruj_kasy(Sklep *sklep, int sem_id)
{
    while (!sklep->sklep_zamkniety)
    {
        sem_wait(sem_id, SEM_SKLEP);
        int ilosc_klientow = sklep->ilosc_klientow;
        sem_post(sem_id, SEM_SKLEP);

        // Zarządzanie kasami
        if (ilosc_klientow <= 10)
        {
            sem_wait(sem_id, 15);
            if (!sklep->sklep_zamkniety)
            {
                sklep->kasjerzy[2].ilosc_klientow = -1;
                printf("Kasa 3: Zamknięta, mniej niż 11 klientów w sklepie.\n");
            }
            sem_post(sem_id, 15);
        }
        else if (ilosc_klientow > 10)
        {
            sem_wait(sem_id, 15);
            if (sklep->kasjerzy[2].ilosc_klientow == -1 && !sklep->sklep_zamkniety)
            {
                sklep->kasjerzy[2].ilosc_klientow = 0;
                printf("Kasa 3: Otwarto, więcej niż 10 klientów w sklepie.\n");
            }
            sem_post(sem_id, 15);
        }

        sleep(2);
    }
}


// pobiera id klienta który jest pierwszy w kolejce
int pobierz_id_klienta_z_kolejki(Sklep *sklep, int kasa_id)
{
    if (sklep->kasjerzy[kasa_id].head == sklep->kasjerzy[kasa_id].tail) {
        return -1;
    }
    
    int klient_index = sklep->kasjerzy[kasa_id].kolejka_klientow[sklep->kasjerzy[kasa_id].head];
    if (sklep->klienci[klient_index].klient_id == 0) {
        sklep->kasjerzy[kasa_id].head = (sklep->kasjerzy[kasa_id].head + 1) % MAX_KLIENTOW;
        return pobierz_id_klienta_z_kolejki(sklep, kasa_id);
    }

    sklep->kasjerzy[kasa_id].head = (sklep->kasjerzy[kasa_id].head + 1) % MAX_KLIENTOW;
    return klient_index;
}

// Obsługa klienta w kasie
void obsluz_klienta(Sklep *sklep, int kasa_id, int sem_id) {
    key_t key = ftok("/tmp", kasa_id + 1);
    int msqid;
    initialize_message_queue(&msqid, key);

    message_buf rbuf;
    int kasa_zamknieta = 0;
    while (!kasa_zamknieta) {

        // Sprawdzenie, czy otrzymano komunikat o zamknięciu sklepu
        if (msgrcv(msqid, &rbuf, sizeof(rbuf.mtext), 0, IPC_NOWAIT) != -1) {
            if (strcmp(rbuf.mtext, close_store_message) == 0) {
                printf("Kasa %d: Otrzymałem komunikat o zamknięciu sklepu.\n", kasa_id + 1);
                kasa_zamknieta=1;
            }
        }
        sem_wait(sem_id, 13 + kasa_id);
        int kolejka_pusta = (sklep->kasjerzy[kasa_id].head == sklep->kasjerzy[kasa_id].tail);

        // jesli kasa otrzymała komunikat o zamknięciu sklepu i nie ma klientów w kolejce to zamyka się
        if (kasa_zamknieta && kolejka_pusta) {
            sem_post(sem_id, 13 + kasa_id);
            printf("Kasa %d: Zamykam się, sklep zamknięty i brak klientów w kolejce.\n", kasa_id + 1);
            break;
        }
        // jesli nie, to obsługuje klienta
        if (!kolejka_pusta) {
            int klient_index = pobierz_id_klienta_z_kolejki(sklep, kasa_id);
            if (klient_index != -1) {
                // obliczenie sumy zakupów klienta
                float suma = 0;
                for (int i = 0; i < sklep->klienci[klient_index].ilosc_zakupow; i++) {
                    Produkt *produkt = &sklep->klienci[klient_index].lista_zakupow[i];
                    if (produkt->ilosc > 0) {
                        suma += produkt->ilosc * produkt->cena;
                
                        // Aktualizacja statystyk sprzedaży
                        if (produkt->id >= 0 && produkt->id < MAX_PRODUKTOW) {
                            sklep->kasjerzy[kasa_id].ilosc_sprzedanych[produkt->id] += produkt->ilosc;
                        } else {
                            printf("Kasa %d: Niepoprawny ID produktu %d, pomijam...\n", kasa_id + 1, produkt->id);
                        }
                    }
                }
                // symulacja obsługi klienta
                sleep(3);  
                printf("Kasa %d: Klient %d Obsłużony, Suma zakupu %.2f zł\n",
                       kasa_id + 1, sklep->klienci[klient_index].klient_id, suma);
            
                sklep->kasjerzy[kasa_id].ilosc_klientow--;
                // wysłanie potwierdzenia do klienta
                message_buf sbuf = {.mtype = sklep->klienci[klient_index].klient_id};
                strcpy(sbuf.mtext, klient_rozliczony);
                msgsnd(msqid, &sbuf, sizeof(sbuf.mtext), 0);
                sklep->klienci[klient_index].klient_id = 0;
            }
        }
        sem_post(sem_id, 13 + kasa_id);

        usleep(100000);
    }

    // Wysłanie potwierdzenia do kierownika, że kasa się zamknęła
    send_acknowledgment_to_kierownik();
}

int main() {
    setup_signal_handlers(cleanup_handler, evacuation_handler);

    int shm_id;
    initialize_shm_sklep(&shm_id, &sklep, SKLEP_KEY);

    int sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    // Uruchomienie kasjerów
    for (int i = 0; i < MAX_KASJEROW; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            kasa_id = i;
            obsluz_klienta(sklep, i, sem_id);
            exit(0);
        }
    }

    monitoruj_kasy(sklep, sem_id);
    return 0;
}