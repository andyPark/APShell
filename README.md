Try running this on two separate machines connected via the web.

One machine needs to have access to the binaries of server.c and another with client.c

example execution:

    Server starts by running the program (named "server" at this point) with the port number from which it will the welcoming socket will be listening for following:
        server 12345

    Client then runs:
        client server-address.com 12345
    
The client is now free to enter commands which the server will then run and return the results of said commands to the client
