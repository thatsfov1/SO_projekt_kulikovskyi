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

Sklep *sklep;
int msqid;

// Inicjalizacja kolejki komunikatów do komunikacji z kierownikiem
void initialize_message_queue()
{
    key_t key = ftok("/tmp", msq_piekarz);
    msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid == -1)
    {
        perror("msgget");
        exit(1);
    }
}

void cleanup_message_queue()
{
    if (msqid != -1)
    {
        msgctl(msqid, IPC_RMID, NULL);
    }
}

// Wysłanie potwierdzenia do kierownika, że piekarz zakończył swoje zadania
void send_acknowledgment()
{
    key_t kierownik_key = ftok("/tmp", msq_kierownik);
    int kierownik_mdqid = msgget(kierownik_key, 0666 | IPC_CREAT);
    message_buf sbuf;
    sbuf.mtype = 1;
    strcpy(sbuf.mtext, acknowledgment_to_kierownik);
    if (msgsnd(kierownik_mdqid, &sbuf, sizeof(sbuf.mtext), 0) == -1)
    {
        perror("msgsnd piekarz do kierownika");
        exit(1);
    }
}

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
    send_acknowledgment();
    
    shmdt(sklep);
    cleanup_message_queue();
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
    // send_acknowledgment();
}

int main()
{
    setup_signal_handlers(cleanup_handler, evacuation_handler);

    int shm_id = shmget(SHM_KEY, sizeof(Sklep), 0666);
    if (shm_id < 0)
    {
        perror("shmget");
        exit(1);
    }

    sklep = (Sklep *)shmat(shm_id, NULL, 0);
    if (sklep == (Sklep *)-1)
    {
        perror("shmat");
        exit(1);
    }

    int sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1)
    {
        perror("semget");
        exit(1);
    }

    initialize_message_queue();
    wypiekaj_produkty(sklep, sem_id);
    cleanup_handler(0);
    return 0;
}