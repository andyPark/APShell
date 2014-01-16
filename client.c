/* Written by Andrew J. Park
 *            COMP530 Fall 2013
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include "Socket.h"
#include "SharedData.h" /* definitions common to client and server */

#define DOMAIN_NAME 1
#define PORT_NUM 2

/* Client Program Description:
 * This client program just makes sure to only send commands that don't
 * go beyond the maximum size defined in SharedData.h and connect to the
 * server socket and send the not yet tokenized command to the server.
 * (the server will take care of parsing, tokenizing, and executing
 */
int main(int argc, char* argv[]) {
    int i, c, rc, j, received;
    int count = 0;
    int eof_called = 0;

    char current_line[ARRAY_SIZE];

    Socket connect_socket; //variable to hold socket descriptor

    if (argc < 3)
    {
        printf("No host and port\n");
        return (-1);
    }

    //create a new data transfer socket at 
    //specified hostname and port
    connect_socket = Socket_new(argv[DOMAIN_NAME], atoi(argv[PORT_NUM]));
    if (connect_socket < 0)
    {
        printf("Failed to connect to server\n");
        return (-1);
    }

    //as a policy i will make it so that the client guarentees that it won't send anything but a valid command
    //that is to say if i don't end the input line and have a \n on the last entry of the array, then i won't send
    while (true) {
        putc('%', stdout);
        while ((c = getchar()) != '\n' && c != EOF) { 
            current_line[count] = c;
            count++;
            if (count >= MAX_LINE) {
                while ((c = getchar()) != '\n' && c != EOF);
                current_line[0] = '\0';
                break;
            }
        }
        eof_called = (c == EOF);
        if (eof_called && count == 0)//Case where there is no input
                                    //and user enters EOF
            exit(EXIT_SUCCESS);
        current_line[count++] = '\0';

        //Case where user entered too many chars into the buffer
        //client will not send the command and just prompt user
        //for another command
        if (current_line[0] == '\0' && !eof_called) {
            count = 0;
            printf("Character limit exceeded\n");
            continue;
        }

        //Send the command characters to the server
        for (i = 0; i < count; i++) {
            c = current_line[i];
            rc = Socket_putc(c, connect_socket);
            if (rc == EOF) {
                printf("Socket_putc EOF or error\n");             
                Socket_close(connect_socket);
                exit(EXIT_FAILURE);  
            }
        }

        //Receive the output of the command from the server
        for (i = 0; i < ARRAY_SIZE; i++) {
            received = Socket_getc(connect_socket);
            if (received == EOF) {
                printf("Socket_getc EOF or error: Client Side\n");             
                Socket_close(connect_socket);
                exit (-1);  
            }
            else {
                current_line[i] = received;
                if (current_line[i] == '\0')
                    break;
            }
        }

        //print the output received from the server
        for (j = 0; j <= i; j++)
            putc(current_line[j], stdout);


        //If EOF was called break out
        if (eof_called) {
            break;
        }

        count = 0; //reset count and prompt another command
    } 
    Socket_close(connect_socket);
    exit(EXIT_SUCCESS);
}
