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

// zmienne procedur
Sklep *sklep;
int shm_id;
int sem_id;
int msqid;
int semop_wait_invalid_argument = 0;
time_t start_time = 0;
time_t pause_time = 0;
int accumulated_time = 0;


void handle_sigtstp(int signum) {
    if (start_time != 0) {
        pause_time = time(NULL);
        printf("\nKierownik: Sklep wstrzymany po %d sekundach działania\n", 
               (int)(pause_time - start_time) + accumulated_time);
    }
    signal(SIGTSTP, SIG_DFL);
    raise(SIGTSTP);
}

// Handler dla SIGCONT (fg)
void handle_sigcont(int signum) {
    if (pause_time != 0) {
        accumulated_time += pause_time - start_time;
        start_time = time(NULL);
        printf("Kierownik: Sklep wznawia działanie, pozostało %d sekund\n", 
               CZAS_PRACY - accumulated_time);
    }
    signal(SIGTSTP, handle_sigtstp);
}

// Wydrukowanie stanu podajników
void drukuj_stan_podajnikow() {
    sem_wait(sem_id, SEM_STATS_MUTEX);
    printf("Kierownik: Na podajnikach zostało: ");
    for (int i = 0; i < MAX_PRODUKTOW; i++) {
        if (sklep->podajniki[i].produkt.ilosc > 0) {
            drukuj_produkt(sklep->podajniki[i].produkt.nazwa, 
                          sklep->podajniki[i].produkt.ilosc);
        }
    }
    printf("\n");
    sem_post(sem_id, SEM_STATS_MUTEX);
}

// Wydrukowanie ilosci wyprodukowanych produktów przez piekarza
void drukuj_statystyki_piekarza() {
    sem_wait(sem_id, SEM_STATS_MUTEX);
    printf("Piekarz: Wyprodukował ");
    for (int i = 0; i < MAX_PRODUKTOW; i++) {
        drukuj_produkt(sklep->podajniki[i].produkt.nazwa, 
                      sklep->statystyki_piekarza.wyprodukowane[i]);
    }
    printf("\n");
    sem_post(sem_id, SEM_STATS_MUTEX);
}

// Wydrukowanie sprzedaży przez kazdego kasjera (ile kazdego produktu sprzedano)
void drukuj_sprzedaz_kasjera(int kasjer_id) {
    sem_wait(sem_id, SEM_STATS_MUTEX);
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
    sem_post(sem_id, SEM_STATS_MUTEX);
}

// Wydrukowanie stanu inwentaryzacji (razem podajniki, piekarz, kasy)
void drukuj_inwentaryzacje()
{
    drukuj_stan_podajnikow();
    drukuj_statystyki_piekarza();
    
    for(int i = 0; i < MAX_KASJEROW; i++) {
        drukuj_sprzedaz_kasjera(i);
    }
}

// Wydrukowanie stanu kosza (do którego trafiają produkty podczas ewakuacji)
void drukuj_kosz() {
    printf("Kierownik: Stan kosza po zamknięciu sklepu:\n");
    for (int i = 0; i < MAX_PRODUKTOW; i++) {
        if (sklep->kosz.produkty[i].ilosc > 0) {
            printf("%s %d szt.\n", sklep->kosz.produkty[i].nazwa, 
                   sklep->kosz.produkty[i].ilosc);
        }
    }
}

// Wysłanie komunikatu o decyzji zamknięcia sklepu do wszystkich procesów
void send_close_message()
{
    sem_wait(sem_id, SEM_STORE_CLOSE);
    sem_wait(sem_id, SEM_MUTEX_STORE);
    sklep->sklep_zamkniety = 1;
    sem_post(sem_id, SEM_MUTEX_STORE);

    key_t keys[5] = {
        ftok("/tmp", msq_kasa1),         
        ftok("/tmp", msq_kasa2),          
        ftok("/tmp", msq_kasa3),          
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
    sem_post(sem_id, SEM_STORE_CLOSE);

    printf(BLUE "Kierownik: Wkrótce sklep zamyka się, wszyscy klienci stojący w kolejce do kas będą obsłużeni \n");
}

// Oczekiwanie na potwierdzenia gotowości na zamknięcie od wszystkich procesów
void wait_for_acknowledgments(){
    message_buf rbuf;
    int ack_count = 0;
    while (ack_count < 5){
        if (msgrcv(msqid, &rbuf, sizeof(rbuf.mtext), 0, 0) != -1)
        {
            if (strcmp(rbuf.mtext, acknowledgment_to_kierownik) == 0)
            {
                ack_count++;
                printf("Kierownik: Otrzymano potwierdzenie od procesu %d/5\n", ack_count);
            }
        }
    }
}

// Funkcja czyszcząca, drukująca inwentaryzację jeśli była i stan kosza przy ewakuacji
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
void evacuation_handler(int signum) {
    printf("Kierownik: Rozpoczynam ewakuację sklepu...\n");

    sem_wait(sem_id, SEM_EVACUATION_MUTEX);

    while (1) {
        if (sklep->ilosc_klientow == 0) {
            break;
        }
        sleep(1);
    }
    sem_post(sem_id, SEM_EVACUATION_MUTEX);
    cleanup_handler(signum);
}

void inventory_handler(int signum) {
    sem_wait(sem_id, SEM_MUTEX_STORE);
    sklep->inwentaryzacja = 1;
    sem_post(sem_id, SEM_MUTEX_STORE);
}

int main(){
    setup_signal_handlers(cleanup_handler, evacuation_handler);
    signal(SIGUSR2, inventory_handler);
    signal(SIGTSTP, handle_sigtstp);
    signal(SIGCONT, handle_sigcont);
    srand(time(NULL));

    key_t key = ftok("/tmp", msq_kierownik);
    initialize_message_queue( &msqid, key);

    sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    initialize_shm_sklep(&shm_id, &sklep, SKLEP_KEY);

    start_time = time(NULL);


    // losowanie czy będzie ewakuacja i wysłanie sygnału do ewakuacji
    int czy_bedzie_ewakuacja = rand() % 5 + 1;
    if (czy_bedzie_ewakuacja == 1)
    {
        int czas_do_ewakuacji = rand() % CZAS_PRACY + 10;
        time_t evacuation_start = time(NULL);
        
        while ((time(NULL) - evacuation_start + accumulated_time) < czas_do_ewakuacji) {
            sleep(1);
        }
        if (kill(0, SIGUSR1) == 0)
        {
            sem_wait(sem_id, SEM_MUTEX_STORE);
            sklep->ewakuacja = 1;
            sem_post(sem_id, SEM_MUTEX_STORE);
            printf("Sygnał o ewakuację wysłany\n");
            
        }
        else
        {
            perror("Błąd wysyłania sygnału o ewakuację");
        }
    }

    // wysłanie komunikatu o zamknięciu sklepu innym procesom 
    while ((time(NULL) - start_time + accumulated_time) < CZAS_PRACY) {
        sleep(1);
    }
    send_close_message();
    
    wait_for_acknowledgments();
    // losowanie czy będzie inwentaryzacja po zamknięciu sklepu
    int czy_bedzie_inwentaryzacja = rand() % 5 + 1; 
    if (czy_bedzie_inwentaryzacja == 1) {
    printf(BLUE "Kierownik: w dniu dzisiejszym przeprowadzimy inwentaryzację.\n" RESET);
    if (kill(0, SIGUSR2) == 0) {
        printf("Kierownik: Sygnał SIGUSR2 wysłany.\n");
    } else {
        perror("Kierownik: Błąd wysyłania sygnału SIGUSR2");
    }
} else {
    printf(BLUE "Kierownik: Dzisiaj nie będziemy przeprowadzać inwentaryzacji.\n" RESET);
}

    cleanup_handler(0);
    return 0;
}