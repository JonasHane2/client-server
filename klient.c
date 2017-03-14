/*H**********************************************************************
* FILENAME:		klient.c
*
* COMPILE:		Make
*
* RUN:			./klient <hostname> <port>
*
* NOTES:
*	ARGUMENTS: 	Host names are accepted as arguments and parsed to IP-
*			address.
*
* 	CONNECTION: 	Unlimited connection attempts, if initial connection to
*			server fails. 
*	
*			A user can ask for jobs as long as the server has more jobs
*			to read. However, when the server reaches the end, klient 
*			terminates.
*
*	PROTOCOL:	Initially sends two bytes to server seperatly. The first
*			is a GETJOB message (char), and the second is the number 
*			of jobs, wich i have set a max-limit to 255.
*
*
* AUTHOR: 		15119
*
*H*/

#define _POSIX_SOURCE

#include <errno.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "communication.h"

#define CHILDREN 2
#define TERMINATECHILDREN ((char) 'Q')
#define STDOUTCHILD1 ((char) 'O')
#define STDERRCHILD2 ((char) 'E')
#define ALIVE 0
#define DEAD -1
#define FINISHED 0
#define READ 0
#define WRITE 1

pid_t children[CHILDREN];
int clientSocket, childNR, parent;
int fd[CHILDREN][2];
char *address;
char *input;

int initializePipes();
int initializeChildren();
void closeUNPipes();
void closeNPipes();
int connectToServer();
void serverConnectionHelp();
char * hostToIP(char *address);
int sendMessageToServer(char msg);
int jobQuery();
int readLoop(int numJobs);
int askForJobs(int numJobs);
int executeJob();
int jobChooser(char jobType);
int childTask();
void childPrint(char *msg);
void terminateChildren();
int childStatus(pid_t c[]);

/*This is the main method wich executes functions that initializes the signal handler, 
*checks arguments, initializes pipes and children, closes unnecessary pipes. It then 
*splits the child and parent process by an if-sentence. 
*
*The parent process tries to create a socket and connect that socket to the server. If 
*either of these processes fail, the program terminates due to an error (intentionally). 
*Then a for-loop is entered that will run until user choses to exit (in query or by 
*hitting (ctrl+c)) or if an error occurs in any of the later functions). If not that 
*integer is sent as an argument to readLoop. If -1 is returned that means that theres an 
*error and the client is terminated as an error, if 1 is returned that means that the 
*file is finished, and the client terminates normally. 
*
*The child process calls the function childTask in a while loop and only returns when if 
*there occurs an error in that function, or the parent process informs it to stop. Then 
*it closes the reading end of his pipe.
*
*Input: 
*	a: number of arguments
*	b: arguments
*
*Return: 
*it returns an int value, but the value is not really relevant
*/
int main(int argc, char *argv[]) {

	/*Initialize sighandler, variables, pipes and children*/
	if (init_sig_handler() == -1) exit(EXIT_FAILURE);
	if (checkArguments(argc, argv[1], argv[2]) == -1) exit(EXIT_FAILURE);
	if (initializePipes() == -1) terminator(ERRORTERMINATE);
	parent = initializeChildren();
	if (parent == -1) terminator(ERRORTERMINATE);
	closeUNPipes();

	if (parent) { //Parent process	
	
		/*Connect to server*/
		if ((clientSocket = createSocket(address, port)) == -1) terminator(ERRORTERMINATE);
		serverConnectionHelp();

		for (;;) {	

			int numJobs = jobQuery();
			if (numJobs == 0) terminator(NORMALTERMINATE);

			testValue = readLoop(numJobs);
			if (testValue == -1) terminator(ERRORTERMINATE);
			else if (testValue == 1) terminator(NORMALTERMINATE);
		}

	} else { //Child process

		while(childTask() == 0);
		close(fd[childNR][READ]);
	}
	return 0;
}

/*This function checks that the user provided the correct argument size
*and if not prints a message infroming of correct use. -1 (error) is returned.
*
*Then it calls the hostToIP function. If there is an error a message is 
*printed and -1 (error) is returned.
*
*Then the char p (port) is parsed to an int end assigned to the variable port.
*If there is an error a message is printed and -1 (error) is returned
*
*Input: 
*	a: number of arguments
*	b: host name
*	c: port 
*
*Return: 
*0 for success, -1 for error
*/
int checkArguments(int argc, char *h, char *p) {

	if (argc != 3) {
		printf("Correct usage: ./klient <adress> <port>\n");
		return -1;
	}
	
	if ((address = hostToIP(h)) == NULL) {
		printf("hostToIP(): couldn't assign IP-address to host name: %s\n", h);
		return -1;
	}

	if ((port = atoi(p)) == 0) {
		perror("atoi()");
		return -1;
	}
	return 0;
}

/*This function initializes CHILDREN number of pipes, and prints an
*error message if the initialization fails.
*
*Input: 
*
*Return: 
*0 for success, -1 for error
*/
int initializePipes() {

	for (int i = 0; i < CHILDREN; i++) {
		if (pipe(fd[i]) == -1) {
			perror("pipe()");
			return -1;
		}
	}
	return 0;
}

/*This function initializes CHILDREN number of children, and prints an
*error message if the initialization fails. After a child is initialized
*successfully that child process returns. The parent returns once all the
*child processes are initialized.
*
*Input: 
*
*Return: 
*Sucsess returns the pid of the process returning (0 for children), -1 for error
*/
int initializeChildren() {

	int temp;
	for (int i = 0; i < CHILDREN; i++) {
		temp = fork();
		childNR = i;
		if (temp == -1) {
			perror("fork()");
			return -1;
		}
		if (!temp) return temp;
		children[i] = temp;				
	}
	return temp;
}

/*This function closes unnecessary pipes, wich for parent is the reading part
*and for children are the pipes that do not correspond to their childNR and
*the writing part of their pipe.
*
*Input: none
*
*Return: none
*/
void closeUNPipes() {

	if (parent) {
		for (int i = 0; i < CHILDREN; i ++) close(fd[i][READ]);
	} else {
		for (int i = 0; i < CHILDREN; i ++) close(fd[i][WRITE]);
		for (int i = 0; i < CHILDREN && i != childNR; i++) close(fd[i][READ]);
	}
}

/*This function closes necessary pipes, wich for parent is the writing part
*and for children are their reading part.
*
*Input: none
*
*Return: none
*/
void closeNPipes() {
	if (parent) for (int i = 0; i < CHILDREN; i ++) close(fd[i][WRITE]);
	else close(fd[childNR][READ]);
}

/*This function connects the socket to the server.
*
*Input: 
*
*Return: 
*0 for successful connection, and -1 for error.
*/
int connectToServer() {

	testValue = connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size);
	
	if (testValue == -1) perror("connect()");
	else printf("\n---Successfully connected to the server!---\n\n");
	
	return testValue;
}

/*This function helps the user if connecting to a server fails.
*If the connection is successful this function only sets the variable
*socketConnectionStatus to connected. If the connection fails, the user
*can try again by hitting the enter key, or exit by pressing (ctrl+c). 
*
*Input: none
*
*Return: none
*/
void serverConnectionHelp() {

	while (connectToServer() != 0) {

		char key = 0;
		printf("Press enter to retry, or (ctr+c) to exit!");				
		while (key != '\r' && key != '\n')  key = getchar( );
	}
	socketConnection = CONNECTED;
}

/*This function translates host name to IP-address by using the
*gethostbyname function.
*
*Input: 
*	a: host name
*
*Return: 
*IP-address, NULL means failure to find IP
*/
char * hostToIP(char *address) {

	struct hostent *temp;	
	if ((temp = gethostbyname(address)) == NULL) {
		perror("gethostbyname()");
		return NULL;
	}

	struct in_addr **addr_list = (struct in_addr **) temp->h_addr_list;
	for (int i = 0; addr_list[i] != NULL; i++) return (inet_ntoa(*addr_list[i]));

	return NULL;
}

/*This function sends a given message to server, and tests it for errors. If there
*is an error an appropriate message is outputed. It does so by inserting the message
*(a char) into a buffer and then calling the function  writeToFileDescriptor. This 
*functions purpose is to make it easier to send a message to a server. 
*
*Input: 
*	a: message to be sent to server
*
*Return: 
*0 on perfect execution, and -1 on error.
*/
int sendMessageToServer(char msg) {

	char buffer[1];	
	buffer[0] = msg;
	return writeToFileDescriptor(clientSocket, buffer, 1);
}

/*This function prompts the user with a query asking what the user wants to do out of 4
*alternatives. A string will be read from the user and then compared to the options. If
*an invalid option are chosen 0 is returned and a message is printed. If alternative 2
*is chosen, an additional query is presented where the user have to name the amount of
*jobs he/she wants to recieve from the server. If the user chooses alternative 3 then
*the maximum ammount of jobs that can be printed out wich i set at 255 is returned. 
*
*Input: none
*
*Return:
*The number of jobs to look up.
*/
int jobQuery() {
	
	int value = 0;
	input = malloc(sizeof(char)*8);
	
	printf("1) Get 1 job from server\n2) Get X job(s) from server\n3) Get all jobs (%d) from server\n0) Exit\n> ", MAXJOBS);
	scanf(" %s", input);
	getchar( );

	if (strcmp(input, "1") == 0) value = 1;
	else if (strcmp(input, "2") == 0) {

		printf("How many jobs?\n> ");
		scanf(" %s", input);
		getchar( );
		value = atoi(input);
		if (value > MAXJOBS) value = MAXJOBS;
		else if (value < 0) value = 0;
	} 
	else if (strcmp(input, "3") == 0) value = MAXJOBS;
	else if (strcmp(input, "0") != 0) printf("%s, is not an alternative.\n", input);

	free(input);
	input = NULL;

	return value;
}

/*This function calls askForJobs wich sends a message to the server asking for numJobs jobs.
*It then enters a for loop wich is ment to call executeJob numJobs times, but will be broken
*it executeJob returns -1 (error). The returnValue is then returned, and it will be 0 if 
*the ammount of jobs the user wanted actually got executed, and -1 if not.
*
*Input: 
*	a: number of jobs to be executed
*
*Return: none
*/
int readLoop(int numJobs) {

	testValue = askForJobs(numJobs);
	for (int i = numJobs; i != 0 && testValue == 0; i--) testValue = executeJob();
	return testValue;
}

/*This function sends a message to the server asking for numJobs messages.
*The maximum ammount of messages i decided to be possible to ask for at once
*is 255. A char array is created and filled with a GETJOB message and a char 
*with a numJob int value
*
*Input:
*	a: number of jobs to ask for
*
*Return:
*0 for success, -1 for failure. 
*/
int askForJobs(int numJobs) {

	char jobs[2];
	jobs[0] = GETJOB;
	jobs[1] = (unsigned char)numJobs;

	return writeToFileDescriptor(clientSocket, jobs, sizeof(jobs));
}

/*This function performs the jobs given by the server. First it reads in jobType and textLength
*from server. Then it determines what type of job to execute by calling the function jobChooser.
*If jobChooser returns -1 it means that there were an error, and -1 is returned. If jobChooser 
*returns 2 then that means that children are being terminated and 1 is returned. The only values
*jobChooser can then return is 0 or 1 and that is the child/pipe nr. that is being written to.
*Then the jobtext is read, and textlength and jobtext is written to pipe/child with nr. jobValue.
*
*jobType = buffer[0];
*textLength = (int)buffer[1];
*
*
*Input: none
*
*Return: 
*0 from perfect execution, -1 for errors, 1 for end of file
*/
int executeJob() {

	/*Read jobtype and textlength from server*/
	char buffer[2];
	if (readFromFileDescriptor(clientSocket, buffer, 2) == -1) return -1;

	/*Determine what type to execute*/
	int jobValue = jobChooser(buffer[0]);
	if (jobValue == -1) return -1;
	else if (jobValue == 2) { 
		terminateChildren();
		return 1;
	} 

	/*Read text from server*/
	char jobText[(int)((unsigned char)buffer[1])+1];
	jobText[0] = buffer[1];
	if (readFromFileDescriptor(clientSocket, jobText+1, sizeof(jobText)-1) == -1) return -1;

	/*First write text length and then jobtext to pipe*/
	return writeToFileDescriptor(fd[jobValue][WRITE], jobText, sizeof(jobText));
}

/*This function creates an char array with all know job-types.
*It then loops through the array and sees if it's a match between
*a job-type and the job type provided as an argument.
*
*Input: 
*	a: type of job
*
*Return: 
*0 for job-type 1 (STDOUTCHILD1)
*1 for job-type 2 (STDERRCHILD2)
*2 for job-type 3 (TERMINATECHILDREN)
*-1 if none of the alternatives matched
*/
int jobChooser(char jobType) {

	char jobs[3] = {STDOUTCHILD1, STDERRCHILD2, TERMINATECHILDREN};

	for (int i = 0; i < (int)(sizeof(jobs)/sizeof(jobs[0])); i++) {
		if (jobType == jobs[i]) return i;
	}
	printf("ERROR: Don't know job-type: %c\n", jobType);
	return -1;
}

/*This function initializes a loop wich in practice means that as long as none of the 
*two reading operations in the loop returns an error, it continiues. In the loop 
*the length of the text is read before the whole text is read. There is one extra space
*allocated for the nullbyte. Then childPrint is called with the jobtext as an argument.
*
*Input: none
*
*Return:
*0 for successful execution, -1 for error
*/
int childTask() {

	char buffer[1];
	if (readFromFileDescriptor(fd[childNR][READ], buffer, sizeof(buffer)) == -1) return -1;

	char b[(int)((unsigned char)buffer[0])+1];
	if (readFromFileDescriptor(fd[childNR][READ], b, (sizeof(b)-1)) == -1) return -1; 
	b[(int)((unsigned char)buffer[0])] = '\0';

	childPrint(b);
	return 0;
}

/*This function prints out message to stdout if its child 0 who is
*trying to print or stderr if its child 1.
*
*Input: 
*	a: message to be printed out
*
*Return: none
*/
void childPrint(char *msg) {

	if (strcmp(msg, "") != 0) {
		if (childNR == 0) fprintf(stdout, "%s\n", msg);
		else if (childNR == 1) fprintf(stderr, "%s\n", msg);
	}
}

/*This function can execute two different ways; 
*Scenario 1; called from parent process. If its called from the parent process, then its only 
*the parent running this function and then as well as the other functions and if the children 
*are alive terminateChildren will be called.
*
*Scenario 2; called from signald handler. If its called from the signal handler, parent process
*and all child processes are sent here. They all close the remaining open pipes and then the 
*child processes terminate themself. Therfore terminateChildren isn't called by parent, since
*they take care of that themself. The parent does however wait for the children to terminate
*before terminating the whole program. 
*
*Apart from that the previously malloced space is freed if necessary. The server gets sent a 
*message informing about the termination before the socket is closed. After that the program terminates. 
*
*Input: 
*	a: termination message
*
*Return: none
*/
void terminator(char msg) {

 	closeNPipes();
	
	if (parent) {

		if (input != NULL) free(input);
		if (childStatus(children) == ALIVE && sigHandlerCalled != 1) terminateChildren();

		if (socketConnection == CONNECTED) {
			char buffer[1] = {msg};
			send(clientSocket, buffer, sizeof(buffer), MSG_NOSIGNAL);	
		}
		close(clientSocket);

		if (sigHandlerCalled == 1) wait(NULL);

		if (msg == NORMALTERMINATE) exit(EXIT_SUCCESS);
		else exit(EXIT_FAILURE);
	}
	else {
		raise(SIGKILL);
	}
}

/*This function first checks if the children are alive. If not it 
*terminates all child processes by looping through all children, wich 
*are already saved in an array from when they were initialized, and 
*waiting for that child to finish before sendig a kill signal. If 
*this termination isn't from a sig handler then a message is sent to 
*the pipe to make it finish. 
*
*Input: none
*
*Return: none
*/
void terminateChildren() {

	if (childStatus(children) == DEAD) return;

	char buffer[1] = {FINISHED};

	for (int i = 0; i < CHILDREN; i++) {

		if (sigHandlerCalled != 1) write(fd[i][WRITE], buffer, sizeof(buffer));
		waitpid(children[i], NULL, 0);
		kill(children[i], SIGKILL);
	}
}

/*This function takes an array of PID's of child id's, and checks if they
*are still "alive". If they are 0 is returned, however if one is not alive
*-1 is returned.
*
*Input:
*	a: array of PID's to be checked.
*
*Return:
*0 = children are alive, -1 = one or more children are not alive.
*/
int childStatus(pid_t c[]) {

	for (int i = 0; i < (int)(sizeof(c)/sizeof(c[0])); i++) {
		if (waitpid(c[i], NULL, WNOHANG) != 0) return -1;
	}
	return 0;
}
