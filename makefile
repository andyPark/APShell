all:
	gcc -g HW5client.c libsocket.c -o client
	gcc -g HW5server.c libsocket.c -o server
