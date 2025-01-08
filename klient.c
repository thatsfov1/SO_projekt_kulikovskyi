#include "struktury.h"

int shmid, semid, msgid;
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

void zakupy(int pid) {
    struct sembuf op = {SEM_SHOP, -1, 0};
    printf("Klient %d: przed semop SEM_SHOP\n", pid);
    semop(semid, &op, 1);  // wejscie do sklepu
    printf("Klient %d: po semop SEM_SHOP\n", pid);
    
    printf("Klient %d: wszedł do sklepu\n", pid);
    
    struct sembuf mem_op = {SEM_MEM, -1, 0};
    printf("Klient %d: przed semop SEM_MEM (zwiększenie liczby klientów)\n", pid);
    semop(semid, &mem_op, 1);
    printf("Klient %d: po semop SEM_MEM (zwiększenie liczby klientów)\n", pid);
    printf("Klient %d: liczba_klientow przed zwiększeniem = %d\n", pid, shm->liczba_klientow);
    shm->liczba_klientow++;
    printf("Klient %d: liczba_klientow po zwiększeniu = %d\n", pid, shm->liczba_klientow);
    mem_op.sem_op = 1;
    printf("Klient %d: przed semop SEM_MEM (zwolnienie semafora)\n", pid);
    semop(semid, &mem_op, 1);
    printf("Klient %d: po semop SEM_MEM (zwolnienie semafora)\n", pid);
    
    struct msg_buf msg;
    //msg.mtype = 1;  // typ komunikatu dla kasy
    int kasa_id = rand() % 3;  // losowy numer kasy (0, 1, 2)
    msg.mtype = kasa_id + 1;  // typ komunikatu dla kasy
    utworz_liste_zakupow(&msg); // TODO: czy powinien klient tworzyc juz w sklepie?
    
    // Sprawdzenie dostepnosci produktow
    struct sembuf conv_op = {SEM_CONV, -1, 0};
    printf("Klient %d: przed semop SEM_CONV\n", pid);
    semop(semid, &conv_op, 1);
    printf("Klient %d: po semop SEM_CONV\n", pid);
    
    for(int i = 0; i < msg.zakupy.liczba_prod; i++) {
        int prod_id = msg.zakupy.produkty[i];
        if(shm->podajniki[prod_id].liczba_prod < msg.zakupy.ilosci[i]) {
            msg.zakupy.ilosci[i] = 0;  // produkt niedostepny
        }
    }
    
    conv_op.sem_op = 1;
    printf("Klient %d: przed semop SEM_CONV (zwolnienie semafora)\n", pid);
    semop(semid, &conv_op, 1);
    printf("Klient %d: po semop SEM_CONV (zwolnienie semafora)\n", pid);
    
    // Wyslanie zamowienia do kasy
    printf("Klient %d: przed msgsnd\n", pid);
    if (msgsnd(msgid, &msg, sizeof(msg.zakupy), 0) == -1) {
        perror("Blad msgsnd");
        exit(1);
    }
    printf("Klient %d: po msgsnd\n", pid);

    // Powiadomienie kas o nowym kliencie
    struct sembuf new_client_op = {SEM_NEW_CLIENT, 1, 0};
    printf("Klient %d: powiadomienie kas o nowym kliencie\n", pid);
    semop(semid, &new_client_op, 1);
    
    printf("Klient %d: zakończył zakupy\n", pid);

    printf("Klient %d: przed semop SEM_MEM (zmniejszenie liczby klientów)\n", pid);
    semop(semid, &mem_op, 1);
    printf("Klient %d: po semop SEM_MEM (zmniejszenie liczby klientów)\n", pid);
    printf("Klient %d: liczba_klientow przed zmniejszeniem = %d\n", pid, shm->liczba_klientow);
    shm->liczba_klientow--;
    printf("Klient %d: liczba_klientow po zmniejszeniu = %d\n", pid, shm->liczba_klientow);
    mem_op.sem_op = 1;
    semop(semid, &mem_op, 1);
    
    op.sem_op = 1;
    printf("Klient %d: przed semop SEM_SHOP (wyjście ze sklepu)\n", pid);
    semop(semid, &op, 1);  // wyjscie ze sklepu
    printf("Klient %d: po semop SEM_SHOP (wyjście ze sklepu)\n", pid);
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

    int pid = getpid();
    srand(time(NULL) + pid);
    
    zakupy(pid);
    return 0;
}