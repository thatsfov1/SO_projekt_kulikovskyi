all: main kierownik klient piekarz kasjer 

main: main.c
	gcc main.c funkcje.c -o main -lpthread

kierownik: kierownik.c
	gcc kierownik.c funkcje.c -o kierownik -lpthread

klient: klient.c
	gcc klient.c funkcje.c -o klient -lpthread

piekarz: piekarz.c
	gcc piekarz.c funkcje.c -o piekarz -lpthread

kasjer: kasjer.c
	gcc kasjer.c funkcje.c -o kasjer -lpthread

clean:
	rm -r main kierownik klient piekarz kasjer 