#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/msg.h>
#include "struktury.h"
#include "funkcje.h"

Sklep *sklep;
Kosz *kosz;
int shm_id;
int sem_id;
int kosz_id;
int msqid;

// Inicjalizacja kolejki komunikatów do komunikacji z procesami
void initialize_message_queue()
{
    key_t key = ftok("/tmp", msq_kierownik);
    msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid == -1)
    {
        perror("msgget");
        exit(1);
    }
}

// Czyszczenie kolejki komunikatów
void cleanup_message_queue()
{
    if (msqid != -1)
    {
        msgctl(msqid, IPC_RMID, NULL);
    }
}

void drukuj_stan_podajnikow() {
    printf("Kierownik: Na podajnikach zostało: ");
    for (int i = 0; i < MAX_PRODUKTOW; i++) {
        if (sklep->podajniki[i].produkt.ilosc > 0) {
            drukuj_produkt(sklep->podajniki[i].produkt.nazwa, 
                          sklep->podajniki[i].produkt.ilosc);
        }
    }
    printf("\n");
}

void drukuj_statystyki_piekarza() {
    printf("Piekarz: Wyprodukował ");
    for (int i = 0; i < MAX_PRODUKTOW; i++) {
        drukuj_produkt(sklep->podajniki[i].produkt.nazwa, 
                      sklep->statystyki_piekarza.wyprodukowane[i]);
    }
    printf("\n");
}

void drukuj_sprzedaz_kasjera(int kasjer_id) {
    printf("Kasa %d: ", kasjer_id + 1);
    
    int sprzedal_cos = 0;
    for (int j = 0; j < MAX_PRODUKTOW; j++) {
        int ilosc = sklep->kasjerzy[kasjer_id].ilosc_sprzedanych[j];
        if (ilosc > 0) {
            if (!sprzedal_cos) {
                printf("Sprzedano: ");
                sprzedal_cos = 1;
            }
            drukuj_produkt(sklep->podajniki[j].produkt.nazwa, ilosc);
        }
    }
    
    if (!sprzedal_cos) {
        printf("brak sprzedanych produktów przez kasę");
    }
    printf("\n");
}

// Wydrukowanie stanu inwentaryzacji
void drukuj_inwentaryzacje()
{
    drukuj_stan_podajnikow();
    drukuj_statystyki_piekarza();
    
    for(int i = 0; i < MAX_KASJEROW; i++) {
        drukuj_sprzedaz_kasjera(i);
    }
}

// Wydrukowanie stanu kosza po zamknięciu sklepu
void drukuj_kosz()
{
    printf("Kierownik: Stan kosza po zamknięciu sklepu:\n");
    for (int i = 0; i < MAX_PRODUKTOW; i++)
    {
        if (kosz->produkty[i].ilosc > 0)
        {
            printf("%s %d szt.\n", kosz->produkty[i].nazwa, kosz->produkty[i].ilosc);
        }
    }
}

// Wysłanie komunikatu o zamknięciu sklepu do wszystkich procesów
void send_close_message()
{
    key_t keys[5] = {
        ftok("/tmp", 1),           // Kasa 1
        ftok("/tmp", 2),           // Kasa 2
        ftok("/tmp", 3),           // Kasa 3
        ftok("/tmp", msq_piekarz), // Piekarz
        ftok("/tmp", msq_klient)   // Klient
    };
    for (int i = 0; i < 5; i++)
    {
        int msqid = msgget(keys[i], 0666 | IPC_CREAT);
        if (msqid == -1)
        {
            perror("msgget");
            exit(1);
        }

        message_buf sbuf;
        sbuf.mtype = 1;
        strcpy(sbuf.mtext, close_store_message);
        if (msgsnd(msqid, &sbuf, sizeof(sbuf.mtext), 0) == -1)
        {
            perror("msgsnd");
            exit(1);
        }
    }

    printf(BLUE "Kierownik: Wkrótce sklep zamyka się, wszyscy klienci stojący w kolejce do kas będą obsłużeni \n" RESET);
}

// Oczekiwanie na potwierdzenia od wszystkich procesów
void wait_for_acknowledgments()
{
    message_buf rbuf;
    int ack_count = 0;
    while (ack_count < 5)
    {
        if (msgrcv(msqid, &rbuf, sizeof(rbuf.mtext), 0, 0) != -1)
        {
            if (strcmp(rbuf.mtext, acknowledgment_to_kierownik) == 0)
            {
                ack_count++;
            }
        }
    }
}

// Funkcja czyszcząca, która odłącza pamięć współdzieloną i wysyła potwierdzenia
void cleanup_handler(int signum){
    printf("Debug: Process %d: inwentaryzacja=%d, signum=%d\n", 
           getpid(), sklep->inwentaryzacja, signum);

    if (sklep->inwentaryzacja && signum != SIGUSR1){
        printf(BLUE "blablabla \n");
        drukuj_inwentaryzacje();
    }

    if (signum == SIGUSR1){
        drukuj_kosz();
    }

    shmdt(sklep);
    shmdt(kosz);

    printf(GREEN "Kierownik: Wszyscy klienci opuścili sklep. \n" RESET);
    if (signum == SIGUSR1){
        printf(GREEN "Kierownik: Ewakuacja zakończona. \n" RESET);
    }

    cleanup_message_queue();
    exit(0);
}

// Obsługa sygnału ewakuacji
void evacuation_handler(int signum){
    printf("Kierownik: Rozpoczynam ewakuację sklepu...\n");

    while (sklep->ilosc_klientow > 0){
        sleep(1);
    }

    cleanup_handler(SIGUSR1);
}

int main(){
    setup_signal_handlers(cleanup_handler, evacuation_handler);
    srand(time(NULL));

    initialize_message_queue();

    sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1)
    {
        perror("semget");
        exit(1);
    }

    shm_id = shmget(SHM_KEY, sizeof(Sklep), 0666);
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

    kosz_id = shmget(KOSZ_KEY, sizeof(Kosz), 0666);
    if (kosz_id < 0)
    {
        perror("shmget kosz");
        exit(1);
    }
    kosz = (Kosz *)shmat(kosz_id, NULL, 0);
    if (kosz == (Kosz *)-1)
    {
        perror("shmat kosz");
        exit(1);
    }

    

    int czy_bedzie_ewakuacja = 2;
    if (czy_bedzie_ewakuacja == 1)
    {
        sleep(rand() % CZAS_PRACY + 5);
        if (kill(0, SIGUSR1) == 0)
        {
            printf("Sygnał o ewakuację wysłany\n");
        }
        else
        {
            perror("Błąd wysyłania sygnału o ewakuację");
        }
    }

    sleep(CZAS_PRACY);
    send_close_message();

    wait_for_acknowledgments();

    int czy_bedzie_inwentaryzacja = 1;
        if (czy_bedzie_inwentaryzacja == 1)
        {
            printf(BLUE "Kierownik: w dniu dzisiejszym przeprowadzimy inwentaryzację.\n" RESET);
                sklep->inwentaryzacja = 1;
                printf("Debug: Ustawiono inwentaryzację=%d\n", sklep->inwentaryzacja);
        }
        else
        {
            printf(BLUE "Kierownik: Dzisiaj nie będziemy przeprowadzać inwentaryzacji.\n" RESET);
        }

    cleanup_handler(0);
    return 0;
}