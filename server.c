/*H**********************************************************************
* FILENAME:		server.c
* 
* COMPILE:		Make
*
* RUN:			./server <address> <port>
* 
* NOTES:
* 	CONNECTION: 	As i understood the task, the server should only be 
*			able to have one connection per execution. But if you
*			wanted to expand, and make the server able to outlive
*			multiple connections that is possible using a simple 
*			loop.
*
*			If the client connected to the server experiences an 
*			error mid-connection, then i think that is most likely
*			an error caused while communicating with the server, and
*			therfore i choose to terminate the server as well when
*			that happens.
*
*
* AUTHOR: 		15119
*
*H*/

#include <fcntl.h>
#include "communication.h"

#define EMPTYFILE ((char) 'Q')

char *filename;
int welcomeSocket, newSocket, fp;
struct sockaddr_storage serverStorage;

int bindAndListen();
int acceptConnection();
int openFile();
int executeJob();
int getJob(int numJobs);
int sendTerminationMsgToClient();
int readFile(int newSocket, int fp);
int msgInterp(char msg);

/*This is the main method wich first calls checkArguments and init_sig_handler and
*exits due to failure if any of these functions are == -1. Then the sockets are created, 
*and as soon as a connection is found, the socket that listens for connections are closed. 
*Then the functions executeJob is called, and when that returns the program is terminated.
*
*Input: 
*	a: number of arguments
*	b: arguments
*
*Return: 
*it returns an int value, but the value is not really relevant
*/
int main(int argc, char *argv[]) {
	
	if ((checkArguments(argc, argv[1], argv[2]) + init_sig_handler()) != 0) exit(EXIT_FAILURE);

	/*Initialize socket and connections*/
	if ((welcomeSocket = createSocket(NULL, port)) == -1) terminator(ERRORTERMINATE);
	if (bindAndListen() == -1) terminator(ERRORTERMINATE);
	if (acceptConnection() == -1) terminator(ERRORTERMINATE);
	close(welcomeSocket);

	socketConnection = CONNECTED;

	/*Execute clients requests*/
	if (executeJob() == -1) terminator(ERRORTERMINATE);

	/*Finished*/
	terminator(NORMALTERMINATE);
	return 0;
}

/*This function checks that the user provided the correct argument size
*and if not prints a message infroming of correct use. -1 (error) is returned.
*
*Then it sets filename to the variable h.
*
*Then the char p (port) is parsed to an int end assigned to the variable port.
*If there is an error a message is printed and -1 (error) is returned
*
*Input: 
*	a: number of arguments
*	b: file name
*	c: port 
*
*Return: 
*0 for success, -1 for error
*/
int checkArguments(int argc, char *h, char *p) {

	if (argc != 3) {
		printf("Correct usage: ./server <filename> <port>\n");
		return -1;
	}

	filename = h;

	if ((port = atoi(p)) == 0) {
		perror("atoi()");	
		return -1;
	}	
	return 0;
}

/*This function binds the socket and listens for attempts at connecting, if any
*of those functions fail an error message is printed and -1 (error) is returned.
*
*Input: none
*
*Return: 
*0 for successfull execution, -1 for error
*/
int bindAndListen() {

	if ((bind(welcomeSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr))) == -1) {
		perror("bind()");
		return -1;
	}

	if (listen(welcomeSocket, 5) == 0) printf("Listening\n");
	else {
		perror("listen()");
		return -1;
	}
	return 0;
}

/*This function accepts connections from client and prints a message if there is an error
*
*Input: none
*
*Return: 
*0 for successfull execution, -1 for error
*/
int acceptConnection() {

	addr_size = sizeof serverStorage;
	if ((newSocket = accept(welcomeSocket, (struct sockaddr *) &serverStorage, &addr_size)) == -1) {
		perror("accept()");	
		return -1;
	}
	printf("\n---Connection established!---\n\n");
	return 0;
}

/*This function first opens the file given in the arguments at runtime. Then a byte is read
*from the client and the meaning of that byte is sent as an argument to msgInterp. The return-
*value from that function is used to decide weather the user asks for a job or terminated. 
*If the client terminated normally 0 is returned, if it was due to an error -1 is returned. 
*If the client asks for a job then another byte is read, wich is the number of jobs
*the user wants (numJobs). Then getJob is called with numJobs as argument. If that returns -1
*that means that there was an error and -1 is returned if 1 is returned that means that the
*end-of-file is reached, and 0 is returned.
*
*Input: none
*
*Return: 
*0 for success, -1 for error
*/
int executeJob() {
	
	char clientMsg[1];	
	if (openFile() == -1) return -1;

	for(;;) {	

		/*Read first msg from client*/
		if (readFromFileDescriptor(newSocket, clientMsg, sizeof(clientMsg)) == -1) return -1;
		testValue = msgInterp(clientMsg[0]);

		if (testValue == 0) { //Client asks for job

			if (readFromFileDescriptor(newSocket, clientMsg, sizeof(clientMsg)) == -1) return -1;

			testValue = getJob((int)((unsigned char)clientMsg[0]));
			if (testValue == -1) return -1; //Error when reading file
			else if (testValue == 1) return 0; //No more jobs to read
		}
		else if (testValue == -2) return -1; //Client terminated due to an error/ or didn't understand msg
		else break; //Client terminater normally
	}

	close(fp);
	return 0;
}

/*This function takes an argument numJobs and makes a for loop that loops
*numJobs times, or is broken by an error or end of file. In the loop readFile
*is called and if that returns -1 that means that there is an error and -1 is
*returned, if not 0 is always returned. Even if readFile returns 1 wich 
*indicates end-of-file.
*
*Input: 
*	a: number of jobs to read from file
*
*Return: 
*0 on success, -1 for error
*/
int getJob(int numJobs) {
		
	for (int i = 0; i < numJobs && testValue != 1; i++) {
		testValue = readFile(newSocket, fp);
		if (testValue == -1) return -1;
	}
	return 0;
}

/*This function opens a file with a filename given by user. 
*If the open() function fails, then an error message is printed.
*
*Input: none
*
*Return: 
*A file pointer on success, -1 for error
*/
int openFile() {

	fp = open(filename, O_RDONLY);
	if (fp == -1) perror("open()");
	return fp;
}

/*This function starts with reading two bytes from the file and checking for errors in an 
*if-test. If there are errors sendTerminationMsgToClient is called, wich indicates that 
*the file is empty/finished. If not then another 'textLength' number of bytes is read from 
*the file. Then jobtype, textlength and jobtext is written to client.
*
*Input:
*	a: socket to communicate with client
*	b: file to read from
*
*Return: 
0 successful execution, 1 for end of file, -1 for error
*/
int readFile(int newSocket, int fp) {

	char buffer[2];	
	testValue = readFromFileDescriptor(fp, buffer, sizeof(buffer));
	int textLength = (int)((unsigned char) buffer[1]);

	if (testValue != -1 && textLength > 0) { //Send job to client

		/*Read jobtext*/
		char jobText[textLength+2];
		if (readFromFileDescriptor(fp, jobText+2, textLength) == -1) return -1;

		jobText[0] = buffer[0];
		jobText[1] = textLength;

		/*Writing jobtype, textlength and jobtext to client*/
		if (writeToFileDescriptor(newSocket, jobText, textLength+2) == -1) return -1;

	} else { //Inform client that there are no jobs left	

		if (sendTerminationMsgToClient() == -1) return -1;
		return 1;
	}
	return 0;	
}

/*This function sends a message to client with 'Q' and the number 0. Wich
*will make the client terminate. 
*
*Input: none
*
*Return:
*0 for success, -1 for error
*/
int sendTerminationMsgToClient() {

	char buffer[2];
	buffer[0] = EMPTYFILE;
	buffer[1] = 0;
	return writeToFileDescriptor(newSocket, buffer, sizeof(buffer));
}

/*This function compares the message given as an argument to know messages from the client.
*If the message doesn't compare to any of the known message then -2 is returned. Otherwise
*the corresponding number to the message that the argument compares to is returned (*-1). 
*That means that -2 can also be returned by the message being an ERRORTERMINATE message.
*
*Input:
*	a: the message to be interpreted
*
*Return:
*0 for successfull execution, -1 for normal error, and -2 for fatal error
*/
int msgInterp(char msg) {

	char messages[3] = {GETJOB, NORMALTERMINATE, ERRORTERMINATE};

	for (int i = 0; i < (int)(sizeof(messages)/sizeof(messages[0])); i++) {
		if (msg == messages[i]) return (-1*i);
	}
	return -2;
}

/*This function sends a message to client that tells client
*that the file is empty, closes file, and sockets and 
*terminates the program according to what type of termination 
*it is (error/normal).
*
*Input: 
*	a: type of termination
*
*Return: none
*/
void terminator(char msg) {

	if (socketConnection == CONNECTED) sendTerminationMsgToClient();
	close(fp);
	close(welcomeSocket);
	close(newSocket);
	if (msg == NORMALTERMINATE) exit(EXIT_SUCCESS);
	else exit(EXIT_FAILURE);
}
