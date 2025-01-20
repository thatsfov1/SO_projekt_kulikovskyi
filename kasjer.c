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

// Funkcja czyszcząca, która odłącza pamięć współdzieloną i wysyła potwierdzenia
void cleanup_handler(int signum) {
    if (sklep->inwentaryzacja) {
        int sprzedal_cos = 0;
        printf("Kasjer %d: ", kasa_id + 1);
        
        for (int j = 0; j < MAX_PRODUKTOW; j++) {
            if (sklep->kasjerzy[kasa_id].ilosc_sprzedanych[j] > 0) {
                if (!sprzedal_cos) {
                    printf("Sprzedał: ");
                    sprzedal_cos = 1;
                }
                printf("%s: %d szt. ", 
                    sklep->podajniki[j].produkt.nazwa, 
                    sklep->kasjerzy[kasa_id].ilosc_sprzedanych[j]);
            }
        }
        
        if (!sprzedal_cos) {
            printf("brak sprzedanych produktów przez kasę");
        }
        printf("\n");
    }
    shmdt(sklep);
    for (int i = 0; i < MAX_KASJEROW; i++) {
        key_t key = ftok("/tmp", i + 1);
        int msqid = msgget(key, 0666);
        if (msqid != -1) {
            msgctl(msqid, IPC_RMID, NULL);
        }
    }
    exit(0);
}

// Obsługa sygnału ewakuacji
void evacuation_handler(int signum) {
    printf("Kasjer: Otrzymałem sygnał ewakuacji, kończę pracę.\n");
    cleanup_handler(signum);
}

// Funkcja monitorująca liczbę klientów i zarządzająca kasami
void monitoruj_kasy(Sklep *sklep, int sem_id) {
    while (!sklep->sklep_zamkniety) { // Używamy pola ze struktury Sklep
        sem_wait(sem_id, 12);
        int ilosc_klientow = sklep->ilosc_klientow;
        sem_post(sem_id, 12);

        // Zarządzanie kasami
        if (ilosc_klientow <= 10) {
            sem_wait(sem_id, 15);
            if (!sklep->sklep_zamkniety) {
                sklep->kasjerzy[2].ilosc_klientow = -1;
                printf("Kasa 3: Zamknięta, mniej niż 11 klientów w sklepie.\n");
            }
            sem_post(sem_id, 15);
        } else if (ilosc_klientow > 10) {
            sem_wait(sem_id, 15);
            if (sklep->kasjerzy[2].ilosc_klientow == -1 && !sklep->sklep_zamkniety) {
                sklep->kasjerzy[2].ilosc_klientow = 0;
                printf("Kasa 3: Otwarto, więcej niż 10 klientów w sklepie.\n");
            }
            sem_post(sem_id, 15);
        }

        sleep(2);
    }
}

// Pobranie ID klienta z kolejki
int pobierz_id_klienta_z_kolejki(Sklep *sklep, int kasa_id) {
    if (sklep->kasjerzy[kasa_id].head == sklep->kasjerzy[kasa_id].tail) {
        // Kolejka jest pusta
        return -1;
    }

    int klient_index = sklep->kasjerzy[kasa_id].kolejka_klientow[sklep->kasjerzy[kasa_id].head];
    sklep->kasjerzy[kasa_id].head = (sklep->kasjerzy[kasa_id].head + 1) % MAX_KLIENTOW;

    return klient_index;
}

// Obsługa klienta w kasie
void obsluz_klienta(Sklep *sklep, int kasa_id, int sem_id) {
    key_t key = ftok("/tmp", kasa_id + 1);
    int msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid == -1) {
        perror("msgget");
        exit(1);
    }
    message_buf rbuf;

    key_t kierownik_key = ftok("/tmp", msq_kierownik);
    int kierownik_msqid = msgget(kierownik_key, 0666 | IPC_CREAT);
    if (kierownik_msqid == -1) {
        perror("msgget kierownik");
        exit(1);
    }

    while (1) {
        // Sprawdzenie, czy otrzymano komunikat o zamknięciu sklepu
        if (msgrcv(msqid, &rbuf, sizeof(rbuf.mtext), 0, IPC_NOWAIT) != -1) {
            if (strcmp(rbuf.mtext, close_store_message) == 0) {
                printf("Kasa %d: Otrzymałem komunikat o zamknięciu sklepu.\n", kasa_id + 1);
                sem_wait(sem_id, 12);
                sklep->sklep_zamkniety = 1;  // Ustawiamy flagę w pamięci współdzielonej
                sem_post(sem_id, 12);
            }
        }
        sem_wait(sem_id, 13 + kasa_id);
        int kolejka_pusta = (sklep->kasjerzy[kasa_id].head == sklep->kasjerzy[kasa_id].tail);
        
        if (sklep->sklep_zamkniety && kolejka_pusta) {
            sem_post(sem_id, 13 + kasa_id);
            printf("Kasa %d: Zamykam się, sklep zamknięty i brak klientów w kolejce.\n", kasa_id + 1);
            break;
        }
        
        if (sklep->kasjerzy[kasa_id].ilosc_klientow > 0) {
            int klient_index = pobierz_id_klienta_z_kolejki(sklep, kasa_id);
            if (klient_index != -1) {
                float suma = 0;
                for (int i = 0; i < sklep->klienci[klient_index].ilosc_zakupow; i++) {
                    Produkt *produkt = &sklep->klienci[klient_index].lista_zakupow[i];
                    if (produkt->ilosc > 0) {
                        // Używamy ceny z produktu w liście zakupów
                        suma += produkt->ilosc * produkt->cena;
                
                        // Aktualizacja statystyk sprzedaży
                        sklep->kasjerzy[kasa_id].ilosc_sprzedanych[produkt->id] += produkt->ilosc;
                    }
                }
                sleep(3);  // Symulacja obsługi klienta
                printf("Kasa %d: Klient %d Obsłużony, Suma zakupu %.2f zł\n",
                       kasa_id + 1, sklep->klienci[klient_index].klient_id, suma);

                sklep->kasjerzy[kasa_id].suma += suma;
                sklep->kasjerzy[kasa_id].ilosc_klientow--;

                message_buf sbuf = {.mtype = sklep->klienci[klient_index].klient_id};
                strcpy(sbuf.mtext, "OK");
                msgsnd(msqid, &sbuf, sizeof(sbuf.mtext), 0);
            }
        }
        sem_post(sem_id, 13 + kasa_id);

        usleep(100000);
    }

    // Wysłanie potwierdzenia do kierownika
    message_buf sbuf;
    sbuf.mtype = 1;
    strcpy(sbuf.mtext, acknowledgment_to_kierownik);
    if (msgsnd(kierownik_msqid, &sbuf, sizeof(sbuf.mtext), 0) == -1) {
        perror("msgsnd");
        exit(1);
    }
    msgctl(msqid, IPC_RMID, NULL);
}

int main() {
    signal(SIGTERM, cleanup_handler);
    signal(SIGUSR1, evacuation_handler);

    int shm_id = shmget(SHM_KEY, sizeof(Sklep), 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    sklep = (Sklep *)shmat(shm_id, NULL, 0);
    if (sklep == (Sklep *)-1) {
        perror("shmat");
        exit(1);
    }

    sem_id = semget(SEM_KEY, 0, 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    // Uruchamianie wątków lub procesów dla każdej kasy
    for (int i = 0; i < MAX_KASJEROW; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            kasa_id = i;
            obsluz_klienta(sklep, i, sem_id);
            exit(0);
        }
    }

    // Monitorowanie liczby klientów i zarządzanie kasami
    monitoruj_kasy(sklep, sem_id);
    shmdt(sklep);
    return 0;
}