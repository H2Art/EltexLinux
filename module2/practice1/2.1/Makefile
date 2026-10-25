all: Contacts.o main.o
	gcc main.o Contacts.o -o Contacts
Contacts.o: src/Contacts.c
	gcc -c src/Contacts.c -o Contacts.o
main.o: src/main.c Contacts.o
	gcc -c src/main.c -o main.o
clean:
	rm -rf *.o Contacts