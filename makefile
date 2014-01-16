all:
	gcc -g client.c libsocket.c -o client
	gcc -g server.c libsocket.c -o server
clean:
	rm client
	rm server
