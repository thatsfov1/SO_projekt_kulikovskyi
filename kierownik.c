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
int shm_id;
int sem_id;
int msqid;

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
        if (sklep->kosz.produkty[i].ilosc > 0)
        {
            printf("%s %d szt.\n", sklep->kosz.produkty[i].nazwa, sklep->kosz.produkty[i].ilosc);
        }
    }
}

// Wysłanie komunikatu o zamknięciu sklepu do wszystkich procesów
void send_close_message()
{
    sem_wait(sem_id, SEM_CLOSE);

    sklep->sklep_zamkniety = 1;

    key_t keys[5] = {
        ftok("/tmp", 1),         
        ftok("/tmp", 2),          
        ftok("/tmp", 3),          
        ftok("/tmp", msq_piekarz), 
        ftok("/tmp", msq_klient)   
    };
    for (int i = 0; i < 5; i++)
    {
        int msqid;
        initialize_message_queue(&msqid, keys[i]);

        message_buf sbuf;
        sbuf.mtype = 1;
        strcpy(sbuf.mtext, close_store_message);
        if (msgsnd(msqid, &sbuf, sizeof(sbuf.mtext), 0) == -1)
        {
            perror("msgsnd");
            exit(1);
        }
    }
    sem_post(sem_id, SEM_CLOSE);

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

    if (sklep->inwentaryzacja && signum != SIGUSR1){
        drukuj_inwentaryzacje();
    }

    if (signum == SIGUSR1){
        drukuj_kosz();
    }

    printf(GREEN "Kierownik: Wszyscy klienci opuścili sklep. \n" RESET);
    if (signum == SIGUSR1){
        printf(GREEN "Kierownik: Ewakuacja zakończona. \n" RESET);
    }
    exit(0);
}

// Obsługa sygnału ewakuacji
void evacuation_handler(int signum){
    printf("Kierownik: Rozpoczynam ewakuację sklepu...\n");

    while (sklep->ilosc_klientow > 0){
        sleep(1);
    }

    cleanup_handler(signum);
}

int main(){
    setup_signal_handlers(cleanup_handler, evacuation_handler);
    srand(time(NULL));

    key_t key = ftok("/tmp", msq_kierownik);
    initialize_message_queue( &msqid, key);

    sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    initialize_shm_sklep(&shm_id, &sklep, SKLEP_KEY);

    int czy_bedzie_ewakuacja = rand() % 5 + 1;
    if (czy_bedzie_ewakuacja == 1)
    {
        sleep(rand() % CZAS_PRACY + 10);
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

    int czy_bedzie_inwentaryzacja = rand() % 5 + 1;
        if (czy_bedzie_inwentaryzacja == 1)
        {
            printf(BLUE "Kierownik: w dniu dzisiejszym przeprowadzimy inwentaryzację.\n" RESET);
            sklep->inwentaryzacja = 1;
        }
        else
        {
            printf(BLUE "Kierownik: Dzisiaj nie będziemy przeprowadzać inwentaryzacji.\n" RESET);
    }

    cleanup_handler(0);
    return 0;
}