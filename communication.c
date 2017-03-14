/*H**********************************************************************
* FILENAME:		communication.c
*
* COMPILE:		Make
*
* NOTES: 	
*	COMMUNICATION:	This program is used by C programs that communicate 
*			with sockets so frequently that having one place 
*			where the error messages are printed saves time 
*			and space. 
*
*	SOCKET:		This program provides a flexible function for 
*			creating sockets.
*
*	SIGNAL HANDLER:	One function for initializing the signal handler
*			and one for the actual signal handler that calls
*			the function terminator() with NORMALTERMINATE as 
*			an argument.
*
*
* AUTHOR: 		15119
*
*H*/

#include "communication.h"

/*This function reads from file descriptor and outputs appropriate
*error message if it fails. If it reads fewer bytes that it was supposed
*to then i treat that as an error.
*
*Input: 
*	a: file-descriptor
*	b: string for read text to be written to
* 	c: length of text to be read
*
*Return: string of read text, NULL means error
*/
int readFromFileDescriptor(int fd, char *buffer, size_t length) {
	
	int testValue = read(fd, buffer, length);

	if (testValue <= 0) {
		if (testValue == -1) perror("read()");
		return -1;
	}
	if (testValue != (int)length) {
		printf("Only read %d of %d bytes requested\n", testValue, (int)length);
		return -1;
	}

	return 0;
}

/*This function sends a given message to a given file descriptor, and tests it for errors. 
*If there is an error an appropriate message is outputed. If it writes fewer bytes that it 
*was supposed to then i treat that as an error.
*
*Input: 
*	a: file descriptor
*	b: message to be sent to file descriptor
*	c: length of message to be sent
*
*Return: 
*0 on perfect execution, and -1 on error.
*/
int writeToFileDescriptor(int fd, char *msg, size_t length) {

	int testValue = write(fd, msg, length);
	if (testValue <= 0) {
		if (testValue == -1) perror("write()");
		return -1;
	}
	if (testValue != (int)length) {
		printf("Only wrote %d of %d bytes requested\n", testValue, (int)length);
		return -1;
	}

	return 0;
}

/*This function creates the socket, and prints an error message if
*the initialization fails. If the address is NULL then the serverAddr
*struct binds to any ip (INADDR_ANY). If not then the address is set
*to the provided address. 
*
*Input: 
*		a: address
*		b: port
*
*Return:
*0 for successful creation, and -1 for error.
*/
int createSocket(char *adr, int prt) {

	int sock;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket()");
		return -1;
	}	

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(prt);

	if (adr == NULL) {
		serverAddr.sin_addr.s_addr = INADDR_ANY;
	} else {
		testValue = inet_pton(AF_INET, adr, &serverAddr.sin_addr.s_addr);
		if (testValue != 1) {
			if (testValue == -1) perror("inet_pton()");
			return -1;
		} 
	}
	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
	addr_size = sizeof serverAddr;

	return sock;
}

/*This function is a signal handler that treats the signal (Ctrl+c) as a normal termination
*
*Input: 
*	a: signal that is being handeled
*
*Return: none
*/
void sig_handler (int signo) {
	if (signo == SIGINT) {
		sigHandlerCalled = 1;
		terminator(NORMALTERMINATE);
	}
}

/*This initializes the signal handler and prints an error message
*if the initialization fails.
*
*Input: none
*
*Return: 
*0 for successful initialization, and -1 for error.
*/
int init_sig_handler() {

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		perror("signal()");
		return -1;
	}
	return 0;
}
