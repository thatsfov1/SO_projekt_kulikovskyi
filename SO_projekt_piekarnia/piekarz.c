#include "struktury.h"

int shmid, semid, msgid;
struct SharedMemory *shm;
void dodaj_do_podajnika(int pid, int produkt_id, int ilosc) {
    struct sembuf op = {SEM_CONV, -1, 0};
    semop(semid, &op, 1); // blokada dostepu do podajnikow

    struct Podajnik *p = &shm->podajniki[produkt_id];
    
    // Sprawdzenie czy jest miejsce
    if(p->liczba_prod + ilosc <= p->pojemnosc) {
        for(int i = 0; i < ilosc; i++) {
            p->produkty[p->tail] = produkt_id;
            p->tail = (p->tail + 1) % p->pojemnosc;
            p->liczba_prod++;
        }
        printf("Piekarz %d: dodano %d sztuk produktu %d\n", pid, ilosc, produkt_id);
    } else {
        printf("Piekarz %d: brak miejsca na podajniku dla produktu %d\n", pid, produkt_id);
    }

    op.sem_op = 1;
    semop(semid, &op, 1); // zwolnienie blokady
}

void sprawdz_stan(int pid) {
    struct sembuf mem_op = {SEM_MEM, -1, 0};
    semop(semid, &mem_op, 1);
    
    if(shm->ewakuacja) {
        printf("Piekarz %d: Przerwanie pracy z powodu ewakuacji\n", pid);
        mem_op.sem_op = 1;
        semop(semid, &mem_op, 1);
        exit(0);
    }
    
    if(shm->inwentaryzacja) {
        printf("Piekarz %d: Kończę pracę - inwentaryzacja\n", pid);
        mem_op.sem_op = 1;
        semop(semid, &mem_op, 1);
        exit(0);
    }
    
    mem_op.sem_op = 1;
    semop(semid, &mem_op, 1);
}

int main() {
    int pid = getpid() % P; // kazdy piekarz odpowiada za jeden rodzaj produktu
    srand(time(NULL) + pid);
    
    printf("Piekarz %d: Rozpoczynam pracę\n", pid);
    
    while(1) {
        sprawdz_stan(pid);
        
        sleep(rand() % 5 + 1); // losowy czas produkcji
        int ilosc = rand() % 5 + 1; // losowa liczba sztuk (1-5)
        
        dodaj_do_podajnika(pid, pid, ilosc);
    }
    
    return 0;
}