/**
 * @file A2.c
 * @author Derek Duong
 * Student ID: 1186760
 * Class: CIS*3110 Operating Systems
 * @date 2023-03-09
 */
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>
#define ALPHABET_SIZE 26
int ** arrPipes;            // Store Pipes
pid_t * arrChildPid;        // Store the PID's of children
int * whichChildTerminated; // array to store which child is terminated 1 = terminated, 0 = not terminated

int fileAmount = 0;
void sigHandler(int sigNum);
int findChild(pid_t pid);
int main(int argc, char ** argv) {

    if (argc <= 1) {
        printf("No file names inputted");
        return -1;
    }   

    //initializing global variables
    fileAmount = argc-1;
    arrChildPid = (pid_t*)(malloc(sizeof(pid_t) * (fileAmount)));
    arrPipes = (int**)(malloc(sizeof(int*) * (fileAmount)));
    for (int i = 0; i < fileAmount; i++) {
        arrPipes[i] = (int*)malloc(sizeof(int) * 2);
    }
    whichChildTerminated = (int*)(calloc(sizeof(int), (argc-1)));
    signal(SIGCHLD, sigHandler);
    for (int i = 0; i < fileAmount; i++) {
        char * fileName = argv[i + 1]; 
        //check if the pipe was successful
        if (pipe(arrPipes[i]) == -1) {
            printf("- Pipe Creation Failed -\n");
            exit(1); // exit on failed pipe
        }
        pid_t pid = fork();
        if (pid == -1) {
            printf("- Fork Failed -\n");
            exit(1); 
        }
        if (pid == 0) { //child process
            //deal with SIGINT using default method
            signal(SIGINT, SIG_DFL); 
            int file = open(fileName, O_RDONLY);
            if (file == -1) {
                printf("- Could not open file %s -\n", fileName);
                //closes the writing end
                close(arrPipes[i][1]); 
                close(file);
                exit(1);
            }
 
            //find size of file
            off_t fileSize = lseek(file, 0, SEEK_END);
            lseek(file, 0, SEEK_SET);

            //store file in array
            char * fileArr = (char*)malloc((fileSize + 1) * sizeof(char));
            ssize_t readBytes = read(file, fileArr, fileSize);
            if (readBytes != fileSize) {
                printf("- File Reading Error -\n");
                close(arrPipes[i][1]);
                exit(1);
            }
            fileArr[fileSize] = '\0';
            close(file);

            //calculate the histogram
            int * hist = (int*)calloc(ALPHABET_SIZE, sizeof(int)); // allocate the histogram array
            for (int i = 0; i < fileSize; i++) {
                char temp = fileArr[i];
                if (isalpha(temp) != 0) {
                    temp = tolower(temp);
                    hist[temp - 'a']+=1; 
                }
            }
            // pass histogram through the pipe
            ssize_t writeBytes = write(arrPipes[i][1], hist, ALPHABET_SIZE * sizeof(int)); 
            if (writeBytes != ALPHABET_SIZE * sizeof(int)) {
                printf("- Write Failed -\n");
                close(arrPipes[i][1]);
                exit(1);
            }
            printf("PID: %d -> Sleeping\n", getpid()); //Display status
            sleep(10 + 2 * i);

            close(arrPipes[i][1]); 
            free(fileArr);
            free(hist); 
            free(arrChildPid);
            free(whichChildTerminated);
            for (int i = 0; i < fileAmount; i++) {
                free(arrPipes[i]);
            }
            free(arrPipes);
            printf("PID: %d -> Exiting\n", getpid()); //Display status
            exit(0); //terminate child process
            

        } else { // parent process
            arrChildPid[i] = pid;
            printf("PID: %ld\n", (long)arrChildPid[i]);
            if (strcmp("SIG", fileName) == 0) {
                close(arrPipes[i][1]);  //close the write side
                close(arrPipes[i][0]);  // close the read side
                kill(pid, SIGINT); //send the signal to the child process
                sleep(2);
            }
        } 

    }
    //wait until all children are done exiting
    int remainingProc = 1;
    while (remainingProc) {
        int numProcesses = 0;
        // No more child processes
        for (int i = 0; i < fileAmount; i++) {
            if (whichChildTerminated[i]) {
                numProcesses++;
            }
        }
        if (numProcesses == fileAmount) {
            break;
        }
    }
    printf("- Child Processes Have Finished -\n");
    printf("- Parent Process Finished -\n");
    
    //free global
    free(arrChildPid);
    free(whichChildTerminated);
    for (int i = 0; i < fileAmount; i++) {
        free(arrPipes[i]);
    }
    free(arrPipes);
    return 0;
}
/**
 * @brief returns the index of the child with the passed pid
 * 
 * @param pid   process id
 * @return int  index of child with passed process ID
 */
int findChild(pid_t pid) {
    int childIndex = -1;
    //locate index of the child process
    for (int i = 0; i < fileAmount; i++) {
        if (arrChildPid[i] == pid) {
            childIndex = i;
            break;
        }
    }
    return childIndex;
}

/**
 * @brief Custom signal handler for the program
 * 
 * @param sigNum    inputted signal
 */
void sigHandler(int sigNum) {
    pid_t pid;
    int status = 0;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int index = findChild(pid);
        printf("PID: %d -> SIGCHLD Caught\n", pid);
        //checks the exit code of the child status
        if(WIFSIGNALED(status)){
            char * signalName = strsignal(status);
            int signalNum = WTERMSIG(status);
            if (signalName == NULL) {
                printf("Invalid signal code\n");
            } else {
                printf("PID: %d -> killed by %s (Signal %d)\n", pid, signalName, signalNum);
                close(arrPipes[index][0]); 
                close(arrPipes[index][1]); 
                signal(SIGCHLD, sigHandler); 
            }
        } 
        if (index == -1) {
            printf("- No Child Process With PID: %d -\n", pid);
            //close both ends of pipe (read & write)
            close(arrPipes[index][1]); 
            close(arrPipes[index][0]); 
            //reinstate the signal handler
            signal(SIGCHLD, sigHandler); 
            return;
        } else {
            // set the child to be terminated
            whichChildTerminated[index] = 1; 
            int * hist = (int*)calloc(ALPHABET_SIZE, sizeof(int)); 
            //read the histogram from pipe
            int readbytes = read(arrPipes[index][0], hist, ALPHABET_SIZE * sizeof(int)); 
            if (readbytes < 0) {
                printf("PID: %d -> READ Error \n", pid);
                free(hist); // free the histogram array
                close(arrPipes[index][1]);
                signal(SIGCHLD, sigHandler); // reinstall the signal handler
                return;
            } 
            close(arrPipes[index][0]); // close the reading end of the pipe
            
            char histFileName[50];
            snprintf(histFileName, 50, "file%d.hist", pid);
            int histFile = open(histFileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            
            if (histFile == -1) {
                printf("- Open() Error - \n");
                close(arrPipes[index][0]); 
                close(arrPipes[index][1]); 
                signal(SIGCHLD, sigHandler);
                exit(1);
            }
            printf("Outputted file is %s\n", histFileName);
            char temp[256];
            for (int i = 0; i < ALPHABET_SIZE; i++) {
                int n = snprintf(temp, sizeof(temp), "%c %d\n", 'a' + i, hist[i]);
                if (write(histFile, temp, n) == -1) {
                    fprintf(stderr, "- write() Error - \n");
                    close(histFile);
                    close(arrPipes[index][0]);
                    close(arrPipes[index][1]);
                    signal(SIGCHLD, sigHandler);
                    exit(1);
                }
            }
            //close file
            close(histFile); 
            //free array
            free(hist); 
        } 
        signal(SIGCHLD, sigHandler); 
    }
}
