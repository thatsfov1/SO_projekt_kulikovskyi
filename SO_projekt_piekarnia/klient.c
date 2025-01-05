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
    semop(semid, &op, 1);  // wejscie do sklepu
    
    printf("Klient %d: wszedł do sklepu\n", pid);
    
    struct msg_buf msg;
    msg.mtype = 1;  // typ komunikatu dla kasy
    utworz_liste_zakupow(&msg); // TODO: czy powinien klient tworzyc juz w sklepie?
    
    // Sprawdzenie dostepnosci produktow
    struct sembuf conv_op = {SEM_CONV, -1, 0};
    semop(semid, &conv_op, 1);
    
    for(int i = 0; i < msg.zakupy.liczba_prod; i++) {
        int prod_id = msg.zakupy.produkty[i];
        if(shm->podajniki[prod_id].liczba_prod < msg.zakupy.ilosci[i]) {
            msg.zakupy.ilosci[i] = 0;  // produkt niedostepny
        }
    }
    
    conv_op.sem_op = 1;
    semop(semid, &conv_op, 1);
    
    // Wyslanie zamowienia do kasy
    msgsnd(msgid, &msg, sizeof(msg.zakupy), 0);
    
    printf("Klient %d: zakończył zakupy\n", pid);
    
    op.sem_op = 1;
    semop(semid, &op, 1);  // wyjscie ze sklepu
}

int main() {
    int pid = getpid();
    srand(time(NULL) + pid);
    
    zakupy(pid);
    return 0;
}