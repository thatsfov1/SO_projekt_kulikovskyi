all: mainp piekarz klient kasa

mainp: mainp.c struktury.h
	gcc mainp.c -o mainp -lpthread

piekarz: piekarz.c struktury.h
	gcc piekarz.c -o piekarz -lpthread

klient: klient.c struktury.h
	gcc klient.c -o klient -lpthread

kasa: kasa.c struktury.h
	gcc kasa.c -o kasa -lpthread

clean:
	rm -r mainp piekarz klient kasa
