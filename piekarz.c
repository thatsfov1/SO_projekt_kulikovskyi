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
int semop_wait_invalid_argument = 0;

// funkcja czyszcząca
void cleanup_handler(int signum){
    exit(0);
}

// obsługa sygnału ewakuacji
void evacuation_handler(int signum){
    printf("Piekarz: Ewakuacja!, kończę pracę.\n");
    cleanup_handler(SIGUSR1);
}

// piekarz kazde 5 sekund wypieka losową ilość produktów i próbuje dodać do podajników
void wypiekaj_produkty(Sklep *sklep, int sem_id){
    srand(time(NULL));
    message_buf rbuf;

    while (1){
        // sprawdza, czy sklep jest zamknięty
        if (sklep->sklep_zamkniety){
                printf("Piekarz: Sklep zamknięty, kończę pracę.\n");
                send_acknowledgment_to_kierownik();
                break;
        }

        printf("=============================\n");
        for (int i = 0; i < MAX_PRODUKTOW; i++){
            int ilosc_wypiekow = rand() % 5 + 1;
            
            printf("Piekarz: Wypiekłem %d sztuk produktu %s, próbuję dodać do podajnika.\n",
                    ilosc_wypiekow, sklep->podajniki[i].produkt.nazwa);
            
            sem_wait(sem_id, SEM_DISPENSER + i);
            // sprawdza, czy podajnik nie jest pełny, jeśli nie to dodaje produkty
            if (sklep->podajniki[i].produkt.ilosc < MAX_PRODUKTOW_W_PODAJNIKU){
                int wolne_miejsce = MAX_PRODUKTOW_W_PODAJNIKU - sklep->podajniki[i].produkt.ilosc;
                int do_dodania = (ilosc_wypiekow <= wolne_miejsce) ? ilosc_wypiekow : wolne_miejsce;

                sklep->podajniki[i].produkt.ilosc += do_dodania;
                
                sem_wait(sem_id, SEM_STATS_MUTEX);
                sklep->statystyki_piekarza.wyprodukowane[i] += do_dodania;
                sem_post(sem_id, SEM_STATS_MUTEX);

                printf("Piekarz: Dodałem %d sztuk %s do podajnika.\n", 
                        do_dodania, sklep->podajniki[i].produkt.nazwa);

                if (ilosc_wypiekow > do_dodania){
                    printf("Piekarz: Nie dodałem %d sztuk %s do podajnika, bo jest pełny.\n", 
                            ilosc_wypiekow - do_dodania, sklep->podajniki[i].produkt.nazwa);
                }
            }
            else{
                printf("Piekarz: Podajnik %s pełny, nie mogę dodać produktów.\n", 
                        sklep->podajniki[i].produkt.nazwa);
            }

            sem_post(sem_id, SEM_DISPENSER + i);
        }
        printf("=============================\n");

        //sleep(5);
    }
}

int main(){
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