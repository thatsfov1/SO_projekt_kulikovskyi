all: main kierownik klient piekarz kasjer 

main: main.c funkcje.c funkcje.h
	gcc main.c funkcje.c -o main -lpthread

kierownik: kierownik.c funkcje.c funkcje.h
	gcc kierownik.c funkcje.c -o kierownik -lpthread

klient: klient.c funkcje.c funkcje.h
	gcc klient.c funkcje.c -o klient -lpthread
 
piekarz: piekarz.c funkcje.c funkcje.h
	gcc piekarz.c funkcje.c -o piekarz -lpthread

kasjer: kasjer.c funkcje.c funkcje.h
	gcc kasjer.c funkcje.c -o kasjer -lpthread

clean:
	rm -r main kierownik klient piekarz kasjer 