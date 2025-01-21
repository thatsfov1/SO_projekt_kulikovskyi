#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
#include "struktury.h"
#include <string.h>
#include "funkcje.h"

int shm_id;
Sklep *sklep;
int msqid;

// Wysłanie danych inwentaryzacyjnych do kierownika, jeśli inwentaryzacja jest włączona
void print_inventory()
{
    for (int i = 0; i < MAX_PRODUKTOW; i++)
    {
        printf("Piekarz: Wyprodukował %s %d szt.\n", sklep->podajniki[i].produkt.nazwa, sklep->statystyki_piekarza.wyprodukowane[i]);
    }
}

// Funkcja czyszcząca, która odłącza pamięć współdzieloną i wysyła potwierdzenie
void cleanup_handler(int signum)
{
    if (sklep->inwentaryzacja && signum != SIGUSR1)
    {
        print_inventory();
    }
    exit(0);
}

// Obsługa sygnału ewakuacji
void evacuation_handler(int signum)
{
    printf("Piekarz: Ewakuacja!, kończę pracę.\n");
    cleanup_handler(SIGUSR1);
}

// Funkcja wypiekająca produkty i dodająca je do podajników
void wypiekaj_produkty(Sklep *sklep, int sem_id)
{
    srand(time(NULL));
    message_buf rbuf;

    while (1)
    {
        // Sprawdzenie, czy otrzymano komunikat o zamknięciu sklepu
        if (msgrcv(msqid, &rbuf, sizeof(rbuf.mtext), 0, IPC_NOWAIT) != -1)
        {
            if (strcmp(rbuf.mtext, close_store_message) == 0)
            {
                printf("Piekarz: Otrzymałem komunikat o zamknięciu sklepu, kończę pracę.\n");
                send_acknowledgment_to_kierownik();
                break;
            }
        }
        for (int i = 0; i < MAX_PRODUKTOW; i++)
        {
            sem_wait(sem_id, i);

            int ilosc_wypiekow = rand() % 5 + 1;
            printf("Piekarz: Wypiekłem %d sztuk produktu %s, próbuję dodać do podajnika.\n", ilosc_wypiekow, sklep->podajniki[i].produkt.nazwa);

            if (sklep->podajniki[i].produkt.ilosc < MAX_PRODUKTOW_W_PODAJNIKU)
            {
                int wolne_miejsce = MAX_PRODUKTOW_W_PODAJNIKU - sklep->podajniki[i].produkt.ilosc;
                int do_dodania = (ilosc_wypiekow <= wolne_miejsce) ? ilosc_wypiekow : wolne_miejsce;

                sklep->podajniki[i].produkt.ilosc += do_dodania;
                sklep->statystyki_piekarza.wyprodukowane[i] += do_dodania;
                printf("Piekarz: Dodałem %d sztuk %s do podajnika.\n", do_dodania, sklep->podajniki[i].produkt.nazwa);

                if (ilosc_wypiekow > do_dodania)
                {
                    printf("Piekarz: Nie dodałem %d sztuk %s do podajnika, bo jest pełny.\n", ilosc_wypiekow - do_dodania, sklep->podajniki[i].produkt.nazwa);
                }
            }
            else
            {
                printf("Piekarz: Podajnik %s pełny, nie mogę dodać produktów.\n", sklep->podajniki[i].produkt.nazwa);
            }

            sem_post(sem_id, i);
        }

        sleep(5);
    }
}

int main()
{
    setup_signal_handlers(cleanup_handler, evacuation_handler);

    initialize_shm_sklep(&shm_id, &sklep, SKLEP_KEY);

    int sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    key_t key = ftok("/tmp", msq_piekarz);
    initialize_message_queue(&msqid, key);
    
    wypiekaj_produkty(sklep, sem_id);
    cleanup_handler(0);
    return 0;
}