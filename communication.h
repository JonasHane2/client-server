/*H**********************************************************************
* FILENAME:	communication.h
*
* NOTES:	Just some global includes, definitions, variables and functions.
*
* AUTHOR: 	15119
*
*H*/

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define GETJOB ((char) 'G')
#define NORMALTERMINATE ((char) 'T')
#define ERRORTERMINATE ((char) 'E')
#define MAXJOBS 255
#define NOTCONNECTED 0
#define CONNECTED 1

int port, testValue, sigHandlerCalled, socketConnection;
struct sockaddr_in serverAddr;
socklen_t addr_size;

int readFromFileDescriptor(int fd, char *buffer, size_t length);
int writeToFileDescriptor(int fd, char *msg, size_t length);
int createSocket(char *adr, int prt);
int init_sig_handler();
void sig_handler(int signo);
int checkArguments(int argc, char *h, char *p);
void terminator(char msg);
