#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>

#define COMMAND_LINES 2048
#define ARGUMENTS 512
#define MAX_WORD_LENGTH 100

int foregroundOnly = 0;


//Changes the directory based on user input
void setDir(char * fileName){
    //If no directory is provided, it redirects to home
	if(fileName == NULL){
		chdir(getenv("HOME"));	
	}	
	else{
		chdir(fileName); //Redirects to user input folder
	}
}

//Gets the status of child processes
void getStatus(int childStatus){
    if(WIFEXITED(childStatus)){
        printf("Exit Status %d\n", WEXITSTATUS(childStatus)); //child exits normally
    } else{
        printf("Terminated by signal %d\n", WTERMSIG(childStatus)); //child exits abnormally
    }
}

//Terminate process when user does ^C
void handle_SIGINT(int signo){
    printf("Terminated by signal 2\n");

}

//Switch between foreground mode when user does ^Z
void handle_SIGTSTP(int signo) {
    //User is in foreground mode
    if (foregroundOnly == 1) {
        printf("\nExiting foreground-only mode\n");
        fflush(stdout);

        foregroundOnly = 0; //Exit mode
    } else {
        //User is not in foreground mode
        printf("\nEntering foreground-only mode (& is now ignored)\n");
        fflush(stdout);

        foregroundOnly = 1; //Enter mode
    }
}


char* replaceMoneyMoney(char* word, int num) {
    char* result; 
    char newW[100];
    sprintf(newW, "%d", num); //Turn the pid into a string
    int i, cnt = 0; 
    int newWlen = strlen(newW); 
    int oldWlen = 2;

    // Counting the number of times old word 
    // occur in the string 
    for (i = 0; word[i] != '\0'; i++) { 
        if (strstr(&word[i], "$$") == &word[i]) { 
            cnt++; 
 
            i += oldWlen - 1;
        } 
    } 

     // Making new string of enough length 
    result = (char*)malloc(i + cnt * (newWlen - oldWlen) + 1); 

    i = 0; 
    while (*word) { 
        // compare the substring with the result 
        if (strstr(word, "$$") == word) { 
            strcpy(&result[i], newW); 
            i += newWlen; 
            word += oldWlen; 
        } 
        else
            result[i++] = *word++; 
    } 
 
    strcpy(&result[i], word); //Add the rest of the word to the result


    return result; 
}

void commandLine (){
    pid_t spawnpid = -5;
	int intVal = 10;
    int childStatus = 0;
    int bg = 0;

    
    // Initialize SIGINT_action struct to be empty
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};

    // Fill out the SIGINT_action struct
	// Register handle_SIGINT as the signal handler
    SIGINT_action.sa_handler = handle_SIGINT;
    // Block all catchable signals while handle_SIGINT is running
    sigfillset(&SIGINT_action.sa_mask);
    // No flags set
    SIGINT_action.sa_flags = 0;

    // Install our signal handler
    sigaction(SIGINT, &SIGINT_action, NULL);

    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);


    char readIn[COMMAND_LINES];

    //continues until user exits
    while (1) {
        //new line
        printf(": ");
        fflush(stdout);
        fgets(readIn, COMMAND_LINES, stdin);

        char* args[ARGUMENTS];
        int argCount = 0;
        char* inputFile = NULL;
	    char* outputFile = NULL;

        //Reads in user input

        char *token = strtok(readIn, " \n");

        while (token != NULL && argCount < ARGUMENTS) {
            //Detects input file
            if (strcmp(token, "<") == 0) {
                token = (strtok(NULL, " \n"));
				inputFile = strdup(token); //Stores the input file
            } else if (strcmp(token, ">") == 0) {
                //Detects output file
                token = (strtok(NULL, " \n"));
				outputFile = strdup(token); //Stores the output file
            } else {
                //Check if string contains $$
                if (strstr(token, "$$") != NULL){
                    args[argCount++] = strdup(replaceMoneyMoney(token, getpid())); //Looks for $$ and Stores args in argument array
                } else {
                    args[argCount++] = strdup(token); //Add token normally
                }
            }

            token = strtok(NULL, " \n");
        }
        

        //Check for comment line or if user input is empty
        if (args[0] != NULL && args[0][0] != '#') {
            //Checks if command should be executed in the background
            if (args[argCount - 1] != NULL && strcmp(args[argCount - 1],"&") == 0){
                
                //removes & from args
                args[argCount - 1] = NULL;

                //if not in foreground only mode, it will make the command executed in the background
                if (foregroundOnly == 0){
                    bg = 1;
                }
            } else {
                //if its in foreground mode, it will make it run in foreground
				args[argCount++] = NULL;
				bg = 0;
			}

            if (strcmp(args[0],"cd") == 0){
                setDir(args[1]);
            } else if (strcmp(args[0],"exit") == 0){
                exit(0); //Exits program
            } else if (strcmp(args[0],"status") == 0){
                getStatus(childStatus);
            } else {
                //Source: Exploration: Process API - Monitoring Child Processes

                spawnpid = fork();
                switch (spawnpid){
                    case -1:
                // Code in this branch will be exected by the parent when fork() fails and the creation of child process fails as well
                        perror("fork() failed!\n");
                        exit(1);
                        break;
                    case 0:
                        // spawnpid is 0. This means the child will execute the code in this branch

                        //enables ^C, resets back to default values
                        //Source: Exploration: Signal Handling API
                        if (bg == 0) {
                            SIGINT_action.sa_handler = SIG_DFL;
                            sigaction(SIGINT, &SIGINT_action, NULL);
                        }
                        // Handle input redirection

                        //Input is specified
                        if (inputFile != NULL) {
                            int inputFileStat = open(inputFile, O_RDONLY); //Open file for reading
                            if (inputFileStat == -1) {
                                //File cant be opened
                                printf("bash: %s: No such file or directory\n", inputFile);
                                fflush(stdout);
                                exit(1); //Exit status 1
                            }
                            //returns error if dup2 fails
                            if (dup2(inputFileStat, STDIN_FILENO) == -1) {
                                perror("dup2 failed\n");
                                close(inputFileStat);
                                exit(1); //Exit status 1
                            }
                            close(inputFileStat);
                        } else if (bg == 1) {
                            //No input file specified
                            int nullInput = open("/dev/null", O_RDONLY); //uses /dev/null when no input specified
                            if (nullInput == -1) {
                                perror("open failed\n");
                                exit(1);
                            }
                            //returns error if dup2 fails
                            if (dup2(nullInput, STDIN_FILENO) == -1) {
                                perror("dup2 failed\n");
                                close(nullInput);
                                exit(1);
                            }
                            close(nullInput);
                        }

                        // Handle output redirection

                        //output is specified
                        if (outputFile != NULL) {
                            int outputFileStat = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666); //Writing only, Create if it doesnt exist, truncate if it does
                            if (outputFileStat == -1) {
                                printf("bash: %s: No such file or directory\n", outputFile); //File doesnt exist
                                fflush(stdout);
                                exit(1);
                            }
                            if (dup2(outputFileStat, STDOUT_FILENO) == -1) {
                                perror("dup2 failed\n");
                                close(outputFileStat);
                                exit(1);
                            }
                            close(outputFileStat);
                        } else if (bg == 1) {
                            //No output file specified
                            //uses /dev/null when no input specified
                            int nullOutput = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0666); //Writing only, Create if it doesnt exist, truncate if it does
                            if (nullOutput == -1) {
                                perror("open failed\n");
                                exit(1);
                            }
                            if (dup2(nullOutput, STDOUT_FILENO) == -1) {
                                perror("dup2 failed\n");
                                close(nullOutput);
                                exit(1);
                            }
                            close(nullOutput);
                        }

                        //Runs the command held in the args array
                        execvp(args[0], args);
                        printf("bash: %s: command not found\n", args[0]);
                        exit(1);
                        break;

                    default:
                        // spawnpid is the pid of the child. This means the parent will execute the code in this branch
                        //Runs child in foreground
                        if (!bg || foregroundOnly) {
                            waitpid(spawnpid, &childStatus, 0); //Waits for child process to execute
                        } else {
                            //Runs child in background
                            printf("background pid is %d\n", spawnpid);
                            fflush(stdout);
                        }
                        break;
                }
            }

            spawnpid = waitpid(-1, &childStatus, WNOHANG); //Waits for child process to terminate
			while(spawnpid > 0){
				printf("background pid %i is done: ", spawnpid);
				fflush(stdout);
				if (WIFEXITED(childStatus)) {
					printf("exit value %i\n", WEXITSTATUS(childStatus)); //Exits normally
					fflush(stdout);
				} else {
					printf("terminated by signal %i\n", childStatus); //Exits abnormally
					fflush(stdout);
				}
				spawnpid = waitpid(-1, &childStatus, WNOHANG);
            }
        }
    }
}


int main () {

    commandLine();
    return 0;
};