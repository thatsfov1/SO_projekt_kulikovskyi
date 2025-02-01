#include "funkcje.h"
#include "struktury.h"
#include <string.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <errno.h>
#include <signal.h>

// Operacje na semaforach
void sem_wait(int sem_id, int sem_num) {
    struct sembuf sem_op;
    sem_op.sem_num = sem_num;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    if (semop(sem_id, &sem_op, 1) == -1) {
        if (errno == EINVAL) {
            extern int semop_wait_invalid_argument;
            semop_wait_invalid_argument = 1;
        }
        perror("semop wait");
        exit(1);
    }
}

void sem_post(int sem_id, int sem_num) {
    struct sembuf sem_op;
    sem_op.sem_num = sem_num;
    sem_op.sem_op = 1;
    sem_op.sem_flg = 0;
    if (semop(sem_id, &sem_op, 1) == -1) {
        if (errno == EINVAL) {
            extern int semop_wait_invalid_argument;
            semop_wait_invalid_argument = 1;
        }
        perror("semop post");
        exit(1);
    }
}

void chld_handler(){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sigaction));
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_NOCLDWAIT;

    sigaction(SIGCHLD, &sa, NULL);
}

// Inicjalizacja pamięci współdzielonej dla sklepu
void initialize_shm_sklep(int *shm_id, Sklep **sklep, int key) {
    key_t shm_key = ftok("/tmp", key);
    if (shm_key == -1) {
        perror("ftok");
        exit(1);
    }

    *shm_id = shmget(shm_key, sizeof(Sklep), IPC_CREAT | 0666);
    if (*shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    *sklep = (Sklep *)shmat(*shm_id, NULL, 0);
    if (*sklep == (Sklep *)-1) {
        perror("shmat");
        exit(1);
    }
}

// Tworzenie semaforów
void init_semaphores(int *sem_id, int key, int num_semaphores) {
    *sem_id = semget(key, num_semaphores, IPC_CREAT | 0666);
    if (*sem_id == -1) {
        perror("semget");
        exit(1);
    }
}

void init_semaphore_values(int sem_id, int num_semaphores) {
    for(int i = 0; i < num_semaphores; i++) {
        semctl(sem_id, i, SETVAL, 1);
    }
}

// Inicjalizacja kolejki komunikatów
void initialize_message_queue(int *msqid, int key) {
    *msqid = msgget(key, 0666 | IPC_CREAT);
    if (*msqid == -1) {
        perror("msgget");
        exit(1);
    }
}

// Inicjalizacja sklepu (produkty i inne)
void init_produkty(Sklep *sklep) {
    memset(sklep, 0, sizeof(Sklep));
    char *nazwy[MAX_PRODUKTOW] = {
        "Chleb", "Bułka", "Rogalik", "Bagietka", "Pączek",
        "Ciasto", "Tort", "Muffin", "Keks", "Babka",
        "Sernik", "Makowiec"
    };
    float ceny[MAX_PRODUKTOW] = {
        3.0, 1.0, 2.5, 4.0, 2.0,
        10.0, 50.0, 3.5, 15.0, 20.0,
        25.0, 30.0
    };

    for (int i = 0; i < MAX_PRODUKTOW; i++) {
        strcpy(sklep->podajniki[i].produkt.nazwa, nazwy[i]);
        sklep->podajniki[i].produkt.cena = ceny[i];
        sklep->podajniki[i].produkt.ilosc = 0;
        sklep->podajniki[i].produkt.id = i;
        sklep->kosz.produkty[i].id = i;
        strcpy(sklep->kosz.produkty[i].nazwa, sklep->podajniki[i].produkt.nazwa);
        sklep->kosz.produkty[i].ilosc = 0;
        sklep->kosz.produkty[i].cena = sklep->podajniki[i].produkt.cena;
    }
}

// funkcja wysyłająca potwierdzenie do kierownika
void send_acknowledgment_to_kierownik() {
    key_t kierownik_key = ftok("/tmp", msq_kierownik);
    int kierownik_msqid = msgget(kierownik_key, 0666 | IPC_CREAT);
    if (kierownik_msqid == -1) {
        perror("msgget kierownik");
        exit(1);
    }
    message_buf sbuf;
    sbuf.mtype = 1;
    strcpy(sbuf.mtext, acknowledgment_to_kierownik);
    if (msgsnd(kierownik_msqid, &sbuf, sizeof(sbuf.mtext), 0) == -1) {
        perror("msgsnd do kierownika");
        exit(1);
    }
}

void drukuj_produkt(const char* nazwa, int ilosc) {
    printf("%s %d szt. ", nazwa, ilosc);
}

void zwroc_produkty_do_podajnikow(Klient *klient, Sklep *sklep, int sem_id) {
    for(int i=0; i<klient->ilosc_zakupow; i++){
        Produkt *p = &klient->lista_zakupow[i];
        sem_wait(sem_id, SEM_DISPENSER + p->id);
        sklep->podajniki[p->id].produkt.ilosc += p->ilosc;
        sem_post(sem_id, SEM_DISPENSER + p->id);
    }
}

// Losowanie listy zakupów dla klienta
void losuj_liste_zakupow(Sklep *sklep, Produkt lista_zakupow[], int *liczba_produktow)
{
    *liczba_produktow = rand() % 3 + 2; // Min. 2, max. 4 produkty
    for (int i = 0; i < *liczba_produktow; i++)
    {
        int produkt_id = rand() % MAX_PRODUKTOW;
        Produkt produkt = sklep->podajniki[produkt_id].produkt;
        lista_zakupow[i] = produkt; // Kopiowanie całej struktury wraz z ID
        lista_zakupow[i].ilosc = rand() % 5 + 1;
    }
}

// Znalezienie kasy z najmniejszą kolejką
int znajdz_kase_z_najmniejsza_kolejka(Sklep *sklep, int sem_id)
{
    int min_klienci = MAX_KLIENTOW + 1;
    int wybrana_kasa = -1;
    for (int i = 0; i < MAX_KASJEROW; i++)
    {
        sem_wait(sem_id, 13 + i);
        if (sklep->kasjerzy[i].ilosc_klientow != -1)
        { // Sprawdzenie, czy kasa jest otwarta
            int liczba_klientow = (sklep->kasjerzy[i].tail + MAX_KLIENTOW - sklep->kasjerzy[i].head) % MAX_KLIENTOW;
            if (liczba_klientow < min_klienci)
            {
                min_klienci = liczba_klientow;
                wybrana_kasa = i;
            }
        }
        sem_post(sem_id, 13 + i);
    }
    return wybrana_kasa;
}


// Zarządzanie sygnałami
void setup_signal_handlers(void (*cleanup_handler)(int), void (*evacuation_handler)(int)) {
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);
    signal(SIGUSR1, evacuation_handler);
}