#include "struktury.h"

int msgid, shmid, semid;
struct SharedMemory *shm;

void utworz_liste_zakupow(struct msg_buf *msg) {
    msg->zakupy.liczba_prod = rand() % (MAX_ZAKUPY-1) + 2;
    
    // Wybieranie losowych roznych produktow
    int wybrane[P] = {0};
    for(int i = 0; i < msg->zakupy.liczba_prod; i++) {
        int prod;
        do {
            prod = rand() % P;
        } while(wybrane[prod]);
        wybrane[prod] = 1;
        msg->zakupy.produkty[i] = prod;
        msg->zakupy.ilosci[i] = rand() % 3 + 1;  // 1-3 sztuki
    }
}

void sprawdz_dostepnosc(struct msg_buf *msg) {
    struct sembuf op = {SEM_CONV, -1, 0};
    semop(semid, &op, 1);

    for(int i = 0; i < msg->zakupy.liczba_prod; i++) {
        int prod_id = msg->zakupy.produkty[i];
        if(shm->podajniki[prod_id].liczba_prod < msg->zakupy.ilosci[i]) {
            msg->zakupy.ilosci[i] = 0;
        }
    }

    op.sem_op = 1;
    semop(semid, &op, 1);
}

void zakupy(int pid) {
    struct sembuf shop_op = {SEM_SHOP, -1, 0};
    semop(semid, &shop_op, 1);

    struct sembuf mem_op = {SEM_MEM, -1, 0};
    semop(semid, &mem_op, 1);

    shm->liczba_klientow++;
    printf("Klient %d: Wchodzę do sklepu, liczba klientów: %d\n", pid, shm->liczba_klientow);

    mem_op.sem_op = 1;
    semop(semid, &mem_op, 1);

    struct msg_buf msg;
    int kasa_id = rand() % 3;
    msg.mtype = kasa_id + 1;
    utworz_liste_zakupow(&msg);
    sprawdz_dostepnosc(&msg);
    
    if(msgsnd(msgid, &msg, sizeof(msg.zakupy), 0) == -1) {
        perror("Blad msgsnd");
        exit(1);
    }
    
    printf("Klient %d: wyslal zamowienie do kasy %d\n", pid, kasa_id);

    mem_op.sem_num = SEM_MEM;
    mem_op.sem_op = -1;
    semop(semid, &mem_op, 1);

    shm->liczba_klientow--;

    mem_op.sem_op = 1;
    semop(semid, &mem_op, 1);

    shop_op.sem_op = 1;
    semop(semid, &shop_op, 1);
}

int main() {
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

    semid = semget(key, 4, 0666);
    if (semid == -1) {
        perror("Blad semget");
        exit(1);
    }

    msgid = msgget(key, 0666);
    if (msgid == -1) {
        perror("Blad msgget");
        exit(1);
    }

    int pid = getpid();
    srand(time(NULL) + pid);
    
    zakupy(pid);
    return 0;
}