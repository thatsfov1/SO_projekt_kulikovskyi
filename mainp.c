#include "struktury.h"

int shmid, semid, msgid;
struct SharedMemory *shm;

void cleanup() {
    printf("\nMAIN: Rozpoczynam sprzątanie...\n");

    // Zwalnianie zasobów
    msgctl(msgid, IPC_RMID, NULL);
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID, NULL);

    printf("MAIN: Zasoby zwolnione, kończę program.\n");
}

void koniec(int sig) {
    printf("\nMAIN: Otrzymano sygnał %d - sprzątanie...\n", sig);
    cleanup();
    exit(0);
}

void obsluz_sygnal(int typ) {
    struct sembuf op = {SEM_MEM, -1, 0};
    semop(semid, &op, 1);
    
    if(typ == 1) {  // ewakuacja
        shm->ewakuacja = 1;
        printf("MAIN: Ogłoszono ewakuację!\n");
    } else if(typ == 2) {  // inwentaryzacja
        shm->inwentaryzacja = 1;
        printf("MAIN: Rozpoczynam inwentaryzację.\n");
        
        // Podsumowanie sprzedaży
        printf("\nPodsumowanie sprzedaży:\n");
        for(int i = 0; i < P; i++) {
            printf("Produkt %d: sprzedano %d sztuk\n", i, shm->sprzedane[i]);
        }
        
        printf("\nUtarg na kasach:\n");
        for(int i = 0; i < 3; i++) {
            printf("Kasa %d: %lld zł\n", i, shm->utarg[i]);
        }
    }
    
    op.sem_op = 1;
    semop(semid, &op, 1);
}

int main() {
    // Obsługa sygnałów
    struct sigaction act;
    act.sa_handler = koniec;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, 0);

    // Tworzenie kluczy i zasobów IPC
    key_t key = ftok(".", 'A');
    if(key == -1) {
        perror("BLad ftok");
        exit(1);
    }

    // Pamiec wspoldzielona
    shmid = shmget(key, sizeof(struct SharedMemory), IPC_CREAT | IPC_EXCL | 0666);
    if(shmid == -1) {
        perror("Blad shmget");
        exit(1);
    }
    shm = (struct SharedMemory *)shmat(shmid, NULL, 0);
    if(shm == (void *)-1) {
        perror("Blad shmat");
        exit(1);
    }

    // Semafory
    semid = semget(key, 4, IPC_CREAT | IPC_EXCL | 0666);
    if(semid == -1) {
        perror("Blad semget");
        exit(1);
    }
    
    // Inicjalizacja semaforow
    semctl(semid, SEM_SHOP, SETVAL, N);
    semctl(semid, SEM_CONV, SETVAL, 1);
    semctl(semid, SEM_MEM, SETVAL, 1);
    semctl(semid, SEM_KASY, SETVAL, 3);

    // Kolejka komunikatow
    msgid = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    if(msgid == -1) {
        perror("Blad msgget");
        exit(1);
    }

    // Inicjalizacja pamieci wspoldzielonej
    shm->liczba_klientow = 0;
    shm->ewakuacja = 0;
    shm->inwentaryzacja = 0;
    shm->czas_otwarcia = time(NULL);
    
    for(int i = 0; i < P; i++) {
        shm->podajniki[i].pojemnosc = 10 + rand() % 10;
        shm->podajniki[i].liczba_prod = 0;
        shm->podajniki[i].produkty = malloc(shm->podajniki[i].pojemnosc * sizeof(int));
        shm->podajniki[i].head = shm->podajniki[i].tail = 0;
        shm->sprzedane[i] = 0;
    }
    
    for(int i = 0; i < 3; i++) {
        shm->czynne_kasy[i] = 1;
        shm->utarg[i] = 0;
    }

    // Inicjalizacja kolejki pustych komunikatów
    struct msg_buf msg;
    msg.mtype = 1;
    for(int i = 0; i < N; i++) {
        if(msgsnd(msgid, &msg, sizeof(msg.zakupy), 0) == -1) {
            perror("Blad msgsnd");
            exit(1);
        }
    }

    // Uruchomienie procesow piekarzy
    for(int i = 0; i < P; i++) {
        if(fork() == 0) {
            execl("./piekarz", "piekarz", NULL);
            perror("Blad execl piekarz");
            exit(1);
        }
    }

    // Uruchomienie procesow kas
    for(int i = 0; i < 3; i++) {
        if(fork() == 0) {
            char kasa_id[2];
            sprintf(kasa_id, "%d", i);
            execl("./kasa", "kasa", kasa_id, NULL);
            perror("Błąd execl kasa");
            exit(1);
        }
    }

    // Glowna petla programu
    while(1) {
        // Sprawdzenie czasu pracy
        if(time(NULL) - shm->czas_otwarcia >= CZAS_PRACY) {
            printf("MAIN: Koniec czasu pracy piekarni.\n");
            obsluz_sygnal(2);  // Inwentaryzacja na koniec dnia
            break;
        }

        // Symulacja przychodzenia klientow
        if(fork() == 0) {
            execl("./klient", "klient", NULL);
            perror("Blad execl klient");
            exit(1);
        }
        
        sleep(rand() % 5 + 1);

        // Obsluga polecen użytkownika
        char cmd;
        if(scanf(" %c", &cmd) == 1) {
            switch(cmd) {
                case 'e':
                    obsluz_sygnal(1);  // ewakuacja
                    break;
                case 'i':
                    obsluz_sygnal(2);  // inwentaryzacja
                    break;
                case 'q':
                    goto koniec_programu;
            }
        }
    }

koniec_programu:
    while(wait(NULL) > 0);
    
    cleanup();
    return 0;
}