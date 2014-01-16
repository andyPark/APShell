/* Written by Andrew J. Park */
   /* COMP 530 Fall 2013 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "Socket.h"
#include "SharedData.h" //definitions shared by client and server

#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* main actors */
char** parse(char* input);
int execute(char** args);
void shell_service();

/* helper functions for the main actors */
char* int2str(int argct);
char* negative_int2str(int argct);
char* createstr(char* str, int length);
int isEndOfString(char* str, int index);

ServerSocket welcome_socket;
Socket connect_socket;

/* Server Program Description:
 * This server program will create a welcoming socket which will take in
 * incoming connections from clients and then create another socket which
 * will be the main pipe through which the server will send the results of
 * the command which the client requested
 *
 * In order to do that the server will parse, tokenize and execute the
 * command sent by the client. IT will send the results of the exec
 * upon success and return an error message upon a failed exec
 */
int main(int argc, char* argv[]) {
    pid_t spid, term_pid;
    int child_status;
    if (argc < 2) {
        printf("No port specified\n");
        return (-1);
    }

    welcome_socket = ServerSocket_new(atoi(argv[1]));
    if (welcome_socket < 0)
    {
        printf("Failed new server socket\n");
        return (-1);
    }

    while (true) {
        connect_socket = ServerSocket_accept(welcome_socket);
        if (connect_socket < 0) {
            printf("Failed accept on server socket\n");
            exit (-1);
        }
        spid = fork();  /* create child == service process */
        if (spid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (spid == 0) { //child will run the shell service
            shell_service();
            Socket_close(connect_socket);
            exit(EXIT_SUCCESS);
        }
        else {
            Socket_close(connect_socket);  
            term_pid = waitpid(-1, &child_status, WNOHANG);
        }
    }
}

void shell_service() { //child will run this and read, p5arse, execute
                       //the commands received from the client and then
                       //send them back through the connect_socket
    int i, receivedFromClient, status;
    int currentArrayLength = 0;
    char currentLine[ARRAY_SIZE];
    char** args;

    Socket_close(welcome_socket);

    while (true) {
        i = 0;
        //My policy states that the client process will make sure
        //to only send a maximum of ARRAY_SIZE chars which are
        //null terminated to the server process

        while (true) {
        //Put while(true) here instead of while(i < ARRAY_SIZE) as a 
        //form of debugging. That is, since my policy states
        //that received input will always be null terminated.
        //I should always break out below, else the bug will be obvious

            //this loop will receive all the chars and save them
            //into an array
            receivedFromClient = Socket_getc(connect_socket);
            if (receivedFromClient == EOF) {
                printf("Socket_getc EOF or error: Server Side\n"); 
                return; /* assume socket EOF ends service for this client */           
            }
            else {
                if (receivedFromClient == '\0') {
                    currentLine[i] = receivedFromClient;
                    currentArrayLength = i+1;
                    break;
                }
                currentLine[i] = receivedFromClient;
            }
            i++;
        }

        //now I need to parse and execute the program and then
        //send the output of the execution back to the client
        args = parse(currentLine);
        execute(args);
    }
}

/*
 * This function returns an array of strings and parses using whitespace as delimiters
 * The whitespace delimiters can be variable meaning "a b" will return the same array as "a      b"
 */
/* Precondition: input will always be terminated with '\0'
 * Postcondition: nothing will change about input
 */
/* The return value args will return an array of strings. where args[0] will return the number of arguments
 * the user entered (formatted as a string)
 */
char** parse(char* input) {
    int input_length = strlen(input);
    int i;
    char* temp = (char*) malloc((MAX_LINE+1)*sizeof(char)); //temporary storage for arguments
    int temp_len = 0; //length of temp

    char** args = (char**) malloc((MAX_LINE+1)*sizeof(char*));
    int argct = 0;
    int insideQuote = false;

    for (i = 0; i < input_length; i++) {
        if (temp_len == 0) { 
            switch (input[i]){
                case ' ':
                    continue;
                case '\t':
                    continue;
                case '"':
                    insideQuote = true;
                    temp[temp_len++] = input[i]; //pre-increment (i.e. ++argct) here is important because args[0] will be assigned the string version of argct in order for that information to be preserved across the function call
                    if (isEndOfString(input, i)) { //for the case that your only space left in the buffer is that single character
                        args[++argct] = createstr(temp, temp_len);
                    }
                    break;
                case '\'':
                    insideQuote = true;
                    temp[temp_len++] = input[i];
                    if (isEndOfString(input, i)) {
                        args[++argct] = createstr(temp, temp_len);
                    }
                    break;
                default:
                    temp[temp_len++] = input[i];
                    if (isEndOfString(input, i)) {
                        args[++argct] = createstr(temp, temp_len);
                    }
            }
        }
        else if (insideQuote) {//This case is meant for the case where you execute an 
                               //"echo" command which can take in quoted arguments
            temp[temp_len++] = input[i];
            if (input[i] == temp[0]) {//if you have a matching closing ' or "
                args[++argct] = createstr(temp, temp_len);
                temp_len = 0;
                insideQuote = false;
            }
            else if (isEndOfString(input, i)) {
                fprintf(stderr, "Unmatched %c\n", temp[0]);
                args[0] = "0";
                return args;
            }
        }
        else { //This is the default case where you are waiting for all the chars of the command 
               //to be written or if you're going to reach the delimiter and save it
            if (input[i] == ' ' || input[i] == '\t') {
                args[++argct] = createstr(temp, temp_len); 
                temp_len = 0;
            }
            else {
                temp[temp_len++] = input[i];
                if (isEndOfString(input, i)) {
                    args[++argct] = createstr(temp, temp_len);
                }
            }
        }
    }
    args[0] = int2str(argct);
    return args;
}

int execute(char** args) {
    int pipefd[2]; //named pipe to be the temporary file
                   //through which I hold the output of execvp
    pipe(pipefd);
    int pid, exitStatus, execvpStatus;

    if (atoi(args[0]) == 0) {
        return 0;
    }

    pid = fork();
    switch (pid) {
        case -1:
            exit(EXIT_FAILURE);
            break;
        case 0:
            close(pipefd[0]); //close read end of the pipe file descriptor
            dup2(pipefd[1], STDOUT_FILENO); //redirect STDOUT and STDERR
                                            //to the write end of the pipe
            dup2(pipefd[1], STDERR_FILENO);

            //regardless of whether or not you make the exit call.
            //you will run execvp with these two arguments
            close(pipefd[1]); //this end is no longer needed;

            execvpStatus = execvp(args[1], &args[1]); //index 1 because args[0] holds the argct

            if (execvpStatus < 0) {
                printf("%s: Command not found.\n", args[1]);
                exit(EXIT_FAILURE);
            }
            break;
        default:
            while (wait(&exitStatus) != pid); //wait for the command to terminate then send the output to the client...
            close(pipefd[1]);  // close the write end of the pipe in the parent

    //read from the pipe using the read() function and send the output 
    //of execvp to the client using Socket_putc
            char read_char;
            int rc;
            while (read(pipefd[0], &read_char, sizeof(char)) != 0) {
                rc = Socket_putc(read_char, connect_socket);
                if (rc == EOF) {
                    printf("Socket_putc EOF or error\n");             
                    return; 
                }
            }
            Socket_putc('\0', connect_socket); //terminate the message
    }
    return 0;
}

// Helper Functions Below This Line //

//precondition: integer is positive and less than 5 digits
char* int2str(int argct) { 
//This is just a private helper function which I will use to put argct
//as the first element of my array which holds the array of arguments.
//It turns things of type int into strings.
    const int MAX_DIGITS = 9;
    char* strargc = (char*) malloc(MAX_DIGITS*sizeof(char));
    const char* digits = "0123456789";
    int num_of_digits;
    int temp = argct;
    for (num_of_digits = 0; temp != 0; num_of_digits++) { //this line is just to grab the number of digits
        temp /= 10;
    }
    temp = argct; //restore temp
    int i;
    for (i = num_of_digits; i > 0; i--) {
        strargc[i-1] = digits[temp%10];
        temp /= 10;
    }
    strargc[num_of_digits] = '\0';
    return strargc;
}

char* negative_int2str(int argct) {
    const int MAX_DIGITS = 9;
    char* strargc = (char*) malloc(MAX_DIGITS*sizeof(char));
    const char* digits = "0123456789";
    int num_of_digits;
    int temp = argct*-1;
    for (num_of_digits = 0; temp != 0; num_of_digits++) { //this line is just to grab the number of digits
        temp /= 10;
    }
    temp = argct*-1; //restore temp
    int i;
    for (i = num_of_digits; i > 0; i--) {
        strargc[i-1] = digits[temp%10];
        temp /= 10;
    }
    strargc[num_of_digits] = '\0';
    return strargc;
}

char* createstr(char* str, int length) {
    char* newstr = (char*) malloc((length+1)*sizeof(char));
    str[length] = '\0';
    strcpy(newstr, str);
    return newstr;
}

int isEndOfString(char* str, int index) {
    return str[index+1] == '\0';
}
