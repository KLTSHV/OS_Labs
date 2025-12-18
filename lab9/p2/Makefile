all: receiver sender
receiver: receiver.c shm.h
	gcc -o receiver receiver.c
sender: sender.c shm.h
	gcc -o sender sender.c
clean: 
	rm -f receiver sender