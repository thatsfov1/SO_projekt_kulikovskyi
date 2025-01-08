#include "struktury.h"

int shmid, semid, msgid;
struct SharedMemory *shm;
void obsluz_klienta(int kasa_id, struct msg_buf *msg) {
    struct sembuf conv_op = {SEM_CONV, -1, 0};
    semop(semid, &conv_op, 1);
    
    float suma = 0;
    printf("Kasa %d: rozpoczecie obslugi\n", kasa_id);
    
    for(int i = 0; i < msg->zakupy.liczba_prod; i++) {
        if(msg->zakupy.ilosci[i] > 0) {
            int prod_id = msg->zakupy.produkty[i];
            struct Podajnik *p = &shm->podajniki[prod_id];
            
            // Pobranie produktow z podajnika
            for(int j = 0; j < msg->zakupy.ilosci[i]; j++) {
                if(p->liczba_prod > 0) {
                    p->head = (p->head + 1) % p->pojemnosc;
                    p->liczba_prod--;
                    shm->sprzedane[prod_id]++;
                }
            }
        }
    }
    
    conv_op.sem_op = 1;
    semop(semid, &conv_op, 1);
    
    printf("Kasa %d: zakonczenie obslugi, suma: %.2f\n", kasa_id, suma);
}

void kasa_praca(int kasa_id) {
    struct sembuf kasa_op = {SEM_KASY, -1, 0};
    struct msg_buf msg;
    
    while(1) {
        // Sprawdzenie czy kasa powinna byc czynna
        struct sembuf mem_op = {SEM_MEM, -1, 0};
        printf("Kasa %d: przed semop SEM_MEM\n", kasa_id);
        semop(semid, &mem_op, 1);
        printf("Kasa %d: po semop SEM_MEM\n", kasa_id);
        
        int liczba_klientow = shm->liczba_klientow;
        int potrzebne_kasy = (liczba_klientow + K - 1) / K;
        printf("Kasa %d: liczba_klientow = %d, potrzebne_kasy = %d\n", kasa_id, liczba_klientow, potrzebne_kasy);
        
        if(kasa_id >= potrzebne_kasy) {
            printf("Kasa %d: zamknieta\n", kasa_id);
            mem_op.sem_op = 1;
            printf("Kasa %d: przed semop SEM_MEM (zwolnienie semafora)\n", kasa_id);
            semop(semid, &mem_op, 1);
            printf("Kasa %d: po semop SEM_MEM (zwolnienie semafora)\n", kasa_id);
            struct sembuf new_client_op = {SEM_NEW_CLIENT, -1, 0};
            printf("Kasa %d: czeka na nowego klienta\n", kasa_id);
            semop(semid, &new_client_op, 1);
            printf("Kasa %d: nowy klient, sprawdzanie liczby klientów\n", kasa_id);
            continue;
        }
        
        mem_op.sem_op = 1;
        printf("Kasa %d: przed semop SEM_MEM (zwolnienie semafora)\n", kasa_id);
        semop(semid, &mem_op, 1);
        printf("Kasa %d: po semop SEM_MEM (zwolnienie semafora)\n", kasa_id);
        
        
        // Obsluga klienta
        printf("Kasa %d: oczekuje na klienta\n", kasa_id);
        if(msgrcv(msgid, &msg, sizeof(msg.zakupy), kasa_id+1, 0) == -1) {
            perror("Blad msgrcv");
            exit(1);
        }
        printf("Kasa %d: odebrała zamówienie od klienta\n", kasa_id);
        obsluz_klienta(kasa_id, &msg);
    }
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("Uzycie: %s kasa_id\n", argv[0]);
        exit(1);
    }

    key_t key = ftok(".", 'A');
    if (key == -1) {
        perror("Blad ftok");
        exit(1);
    }

    shmid = shmget(key, sizeof(struct SharedMemory), 0666);
    if (shmid == -1) {
        perror("Blad shmget");
        exit(1);
    }

    shm = (struct SharedMemory *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("Blad shmat");
        exit(1);
    }

    semid = semget(key, 5, 0666);
    if (semid == -1) {
        perror("Blad semget");
        exit(1);
    }

    msgid = msgget(key, 0666);
    if (msgid == -1) {
        perror("Blad msgget");
        exit(1);
    }

    int kasa_id = atoi(argv[1]);
    kasa_praca(kasa_id);
    return 0;
}