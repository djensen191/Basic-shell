/* Daniel Jensen 3 March 2019*/
/* This program will use the C89 version of the C compiler. */
/* Compile with gcc */

// Include Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>

// Function Declarations
void ChangeCurrentDirectory(char* argv);
int ExecuteCommands(char argv[]);
int GetStatus();
int TokenizeWords(char *input, char* inputWords[512]);
int StdOutRedirection(char argv[]);
int StdInRedirection(char argv[]);
int InAndOutRedirection(char argv[], char argw[]);
void catchSIGTSTP();


// Global variables
pid_t parentPID = -5; // Create a global PID for the PID of the main program. Initialize it to a bogus value.
                      // This is for the signals to be able to kill.
pid_t spawnPid = -5;  // Create a global variable for the PID of the child
int childExitMethod;  //Create a global variable holding the exit method of the child process 
int backgroundNum = 0;  // Create a background process counter.
pid_t background[20]= {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'};
int modeToggle = 0;   // Create a toggle variable for background/Foreground modes.  0 is Foreground 1 is background.

/*******************************************************************
//main() will call each of the starting functions, present a prompt
//to the user and allow for the user to enter commands.
//
//Input: None
//Output: The prompt for the shell is output to the user..
********************************************************************/
int main()
{
    // This code was from the notes.
    // Create the sigaction struct
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};  

    // Set up the SIGINT signal (initaially setting it to be ignored) 
    struct sigaction SIGINT_ignore = {0}; 
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_ignore.sa_flags = 0;   
    SIGINT_ignore.sa_handler = SIG_IGN;
    SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_ignore, NULL);
 
    // Set up the background/foreground signal (ctrl-z) with a signal handling function.
    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // generic variables
    size_t inputBufferSize = 0;    // inputBufferSize holds the size of the buffer allocated for user input.
    char *  userInput = NULL;      // userInput Points to a buffer which is allocated by the getline function
                                   // that contains the user input string and the newline and null termination character.
    char * ret;                    // Define a pointer to string for the string containing $$.
    char * tmp = malloc(1000);     // Create a temporary pointer to string to be able to copy the new string.
    int counter = 0; 
    int i;
    int backgroundNum=-1;

    pid_t theLastCommandTZ= -5 ;     // pid_t theLastCommandTZ stores the pid of the last non-build in command run.  
                                
    // variables for the cd command
    char changeDirectoryCommand[100];  // Create a string to hold the cd command.
    char changeDirectoryPath[50];      // Create a string to hold the path for cd.
    char * inputWordsSplit[512];       // Create the words array.
    memset(inputWordsSplit, '\0', sizeof(inputWordsSplit));  // Clear inputWordsSplit

    modeToggle = 0;

    parentPID = getpid(); // Get the pid of the parent process before fork() is called.

    
    
    // Use a while loop to run the prompt until the user enters the exit command. 
    while(1)  
    { 
       int numCharsEntered = -5;        // How many chars we entered
       theLastCommandTZ = spawnPid;     // Get the pid of the last run foreground process.

       printf(": ");                    // Print the prompt character.
       fflush(stdout);
       numCharsEntered = getline(&userInput, &inputBufferSize, stdin);  // Wait for and save input from the user.
       int enteredLength = strlen(userInput);                           // Get the length of the user input string.
       if (numCharsEntered == 0)
       {
             clearerr(stdin);
       }
       // Ignore the stand-alone character generated by the enter key.
       if (userInput[0]==0xA )
       {
           continue;
       }
       
       userInput[strcspn(userInput, "\n")] = '\0';  // Remove the trailing \n that getline adds
       
       char * ret  = strstr(userInput, "$$");       // Get the string from $$ to the end and store it in ret.
      
      // I was helped by the instructor for the pointer math here when I took this last term (though I did not get this working last term).
      if(ret != NULL)
      {     
            *ret = '%';                                // Replace the first $ with %
            *(ret + 1) = 'd';                          // Replace the second $ with d.
            sprintf(tmp,  userInput, parentPID );      // Replace %d with the PID 
            strcpy(userInput, tmp);                    // Put the new PID back in the full string  
      }

      int n = TokenizeWords(userInput,inputWordsSplit); // Tokenize the words.

      strcpy(changeDirectoryCommand, userInput); // Make a copy of the user input string.
         
      if (userInput[0] == '#') // Check if the first line is a commenting character.
      {
    
            continue; // Ignore the line, goto the next line and print a new prompt.
                      // (Leaves the current loop iteration without executing any further code.)
      }   
      else if (strcmp(userInput, "exit") == 0) // Check if user inputs the exit command.
      {
            exit(0); // If user input is exit break from the loop.
      }

      else if (changeDirectoryCommand[0] == 'c' && changeDirectoryCommand[1] == 'd')    // Check if the user inputs cd if so call the change directory Function.  
      {   
                  
            ChangeCurrentDirectory(userInput);         
      }
      else if (strcmp(inputWordsSplit[0], "status") == 0  )
      {
            if (theLastCommandTZ != -5 )      // Check if a foreground non-built-in command has been run.  
            {    
                GetStatus();  // If a non-built in foreground process has been run pass the last run PID to GetStatus.
            }
      else
            {
                printf("%d\n", 0);
                fflush(stdout);
            }
      }
        
      else  // If the command is not a built in or a comment pass it to the function that will fork and exec. 
      {
             ExecuteCommands(userInput); // Handle I/O Redirection, process forking and command execution for both foreground/background.
   
      }
      memset(userInput, '\0', sizeof(userInput));  // Clear userInput.
                   
      inputBufferSize = 0; // inputBufferSize holds the size of the buffer allocated for user input.
      userInput = NULL;     // userInput Points to a buffer which is allocated by the getline function

      counter++; 
      
      
      int i=0;
      memset(inputWordsSplit, '\0', sizeof(inputWordsSplit));  // Clear inputWordsSplit
      memset(tmp, '\0', sizeof(tmp));  //clear tmp
        
      // Free the dynamically allocated array.
      for(i = 0; i < n; i++) 
      {
              free(inputWordsSplit[i]);
      }
      
        // Check if there are background processes running and check if any are complete. 
       // if(backgroundNum>0) 
       // { 

            // If there are background processes in the aray check through all of them and see if any are complete.
            for(i=0; i<20; i++)
            {
                     

                     pid_t waitlist = waitpid(background[backgroundNum], &childExitMethod, WNOHANG);        
                     if(waitlist>0)
                     { 

                            // If any jobs have completed print message and decrement the number of background processes
                            if(WIFEXITED(childExitMethod)) 
                            {
                                 printf("\nBackground pid %d is done", waitlist);  // Print job completed message
                                 int exitStatus = WEXITSTATUS(childExitMethod); // Store the exit status.
                                 fflush(stdout);
                                 printf(" Exit value %d\n",exitStatus);                     // Print the exit status.
                                 fflush(stdout);
                                 backgroundNum--;                              // Decrement the background process counter
                            }
                            else
                            {
                            
                                int signalStatus = WIFSIGNALED(childExitMethod);  // Store the signal flag.
                                printf(" Terminated by signal: %d\n",signalStatus); // Print the signal status.
                                fflush(stdout); // Flush Stdout.
                                backgroundNum--;

                            }        
                     }    
            }
    
     
    }
    
    free(userInput);   // Free the string userInput.
    free(tmp);  // free tmp.

    return 0;  // Since main() is an int function it should return a value.  If everything runs correctly it will return 0. 
}

/************************************************************************
//ChangeCurrentDirectory() will change the current working directory of
//the shell to whatever valid directory the user passes in.
//Input: argv the directory that the user passes in as a string.
//Output: NONE
*************************************************************************/
void ChangeCurrentDirectory(char * argv)
{

    char * cdWords[512];                    // Create the words array.
    int n = TokenizeWords(argv,cdWords);    // Tokenize the words.
    char * homePath;                        // Create a string for the path contained in the home environment variable.
    int i = 0;

    if (n == 1)                             // Check if cd is passed without any arguments.
    {
        homePath = getenv("HOME");          // Store the the path from the HOME variable and store it in a string. 
        chdir(homePath);                    // Change the directory to the path contained in the HOME envirionment variable.
    }
    else
    {
        chdir(cdWords[1]);                  // Change to the given directory.
    }        

} 

/********************************************************************************************
//ExecuteCommands() this function will take in the commands from the user that are not built   
//in and execute them through execvp().  This function will also hand I/O redirection, 
//and handle background processes.
//
//Input: argv which is the command the user initially entered as a string.
//Output: spawnPid, the PID of the child process 
********************************************************************************************/
int ExecuteCommands(char * argv)
{
    int userInputLength = strlen(argv);  // Get the strlen of the user input.
    char sourceFile[30]= {"\0"};         // Create a string to hold the source file.
    char destFile[30]= {"\0"};           // Create a string to hold the destination file.
    int flag = -5, backgroundFlag = -5;  // Created two flags.
    char operator1[3] = {"\0"};          // Create a string for the first redirection operator.
    char operator2[3] = {"\0"};          // Create a string for the second redirection operator.
    int count = 0;                       // Create a counter.
    char tmp[30] = {"\0"} ;              // Create a tmp string.
    char * delineatedWords[512];         // Create the words array.
    int numberOfTokens;                  // Create an int to hold the number of tokens. 
    int fd[2], nbytes;                   // Create file descriptor for piping and an int for sizing
    pid_t child = 0;

    char * copiedWords[512];             // Create an array of string pointers to copy the delineated words

    int i,j,k,m;                         // Define for loop counters.
    
    memset(delineatedWords, '\0', sizeof(delineatedWords));  // Clear inputWordsSplit
    memset(copiedWords, '\0', sizeof(copiedWords));          // Clear inputWordsSplit
    
    pipe(fd); // Create the pipe

    // Check for the background process symbol.
    if(argv[userInputLength-1] == '&' )
    {
        backgroundFlag = 1;  // set a flag notating a background process.
        argv[strcspn(argv, "&")] = '\0'; // Replace the background process symbol with the null terminator. 
        
    }
    
    numberOfTokens = TokenizeWords(argv, delineatedWords); // Use the TokenizeWords() function to split words up by spaces./
    int out = -5;
    int saved_stdout = dup(1);  // Save stdout to point back to later   
    int saved_stdin = dup(1);   // save stdin to point back to later.
    
    // Iterate through the words and see if they match the redirection operator.
    for(j=0; j < numberOfTokens; j++) 
    {

        // The if statement checks for occurences of the file redirection operator.
       if (strcmp(delineatedWords[j], "<") == 0 || strcmp(delineatedWords[j], ">" ) == 0 )
       {
           flag = 1;  // Set the flag equal to 1.
           break; // If the redirection operator is found break from the loop.
       } 
    count++;  // Increment count at the end of each iteration.  
    }

    //  Copy the words into a temporary string.
    for(m = 0; m < count ; m++)
    {
       copiedWords[m] = delineatedWords[m];  // Copy the word into a tmp string.
    }

    // Copy redirection information from the separated words to strings for usage. 
    for(k = count; k < numberOfTokens; k++) 
    {
        
        // Check for the flag and check to see if k is equal to the first redirection operator.
        if ((k == count) && flag == 1)         {    
            strcpy(operator1, delineatedWords[count]);  // Copy the first operator into a string.
        }
         // Check for the flag and check if k = count + 1
        else if ( k == (count+1) && flag == 1) 
        {

            strcpy(sourceFile, delineatedWords[count + 1]); // Copy the source filename. 
        }
        // Check if there is another redirection operator and another filename.
        else if((strcmp(delineatedWords[count + 2], "<" )==0 || strcmp(delineatedWords[count + 2], ">" ) == 0 ) && (k == (count + 2))) 
        {
            strcpy(operator2, delineatedWords[count + 2]); // Copy the second redirection operator into a string.
            strcpy(destFile, delineatedWords[count + 3]);  // Copy the destination file into a string.
            flag = 2; // Set flag = 2 which will allow redirection of both stdin and stdout.
        }
        // If the flag is not set do nothing.
        else if(flag == -5)        
        {
                
        }
    }

    spawnPid = fork();  // Fork from the current process saving the value in spawnPid.

    // CHILD FAIL
    if (spawnPid == -1) // check if fork failed.
    {
        perror("ERROR: Fork failed\n"); // Write error message.
        fflush(stdout);
        exit(1); // Exit with error.
    }
    // CHILD SUCCESS
    else if (spawnPid == 0) // If in the child process run exec with the given command.
    {    

        if(strcmp(operator1, ">") == 0 && flag == 1 )  // Check if operator is stdout redirection
        {
             out = StdOutRedirection(sourceFile);  // Call stdout redirection function
        }
         else if(strcmp(operator1, "<") == 0 && flag == 1) // Check if operator is stdin redirection.
        {
            StdInRedirection(sourceFile); // Call stdin redirection function.
        }
        else if(flag == 2) // If flag = 2 call stdout & stdin redirection function.
        {
             InAndOutRedirection(sourceFile, destFile); // Call redirection function and pass source and destination files.
        }
        else if(flag == -5)
        {

        }
    
        // If the process is not a background process allow the use of SIGINT
        if(backgroundFlag != 1) 
        {
            struct sigaction SIGINT_action = {0};
            SIGINT_action.sa_handler = SIG_DFL;
            sigaction(SIGINT, &SIGINT_action, NULL);

        
        }
      
       child = getpid();

       // If background allowed and background flag is set print the background PID
       if(backgroundFlag==1 && modeToggle==0)
       {
           printf("Background PID is:%d",child);  // Print background process message
           fflush(stdout);
           close(fd[0]); // Close the input side of the pipe 
           write(fd[1],&child,sizeof(child)); // Write the information to the pipe  
       } 
        
        // Deal with redirection of stdout for background processes
        if(backgroundFlag == 1 && strcmp(operator1, "<") != 1 && modeToggle == 0)
        {

            int backOut = StdInRedirection("/dev/null"); // call stdin redirection and redirect
            fflush(stdout); // Flush stdout.
        }
        // Deal with stdout redirection for background processes
        if(backgroundFlag == 1 && strcmp(operator1, ">") != 1 && modeToggle == 0)
        {
    
            int backOut = StdOutRedirection("/dev/null"); // call stdout redirection and redirect
            fflush(stdout); // Flush stdout.
        } 
        
         // pass char* [512] that you pass execvp
        int result = execvp(copiedWords[0], copiedWords); // Execute the given commands.   
    
        //  Deal with exec function error case and exit with error.
        if (result < 0)
        {
            char* file = copiedWords[0];
            perror(file); // If exec fails print error.
            // set status varaible to 1
            exit(1); // If exec fails exit with error.
        }
        
    }
    
    // This is the parent case deal with any pipes from the child and with the waitpid portion for foreground processes
    else
    {
        // Gather the information that was piped from the child and pass to global variables
        close(fd[1]); 
        nbytes = read(fd[0], &child, sizeof(child));
    
        
        // If background, waitpid with WNOHANG
        if(backgroundFlag == 1  && modeToggle == 0)
        {
            dup2(saved_stdout ,1); // Reset stdout to point to the terminal.
            background[backgroundNum] = child; // Save the 
            backgroundNum++;
            waitpid(-1, &childExitMethod, WNOHANG);    

        }
        // If Foreground wait for the specified child to exit before continuing back to the main loop. 
        else
        {    
            waitpid(spawnPid, &childExitMethod, 0);
        } 
    }
    memset(copiedWords, '\0', sizeof(copiedWords));  // Clear copiedWords

    // Free the dynamically allocated array
    for(i = 0; i < numberOfTokens; i++) 
    {
          free(copiedWords[i]);
    }
    
    // Free the dynamically allocated array.
    for(i = 0; i < numberOfTokens; i++) 
    {
          free(delineatedWords[i]);
    }


    return spawnPid; //exit the function normally.   
}

/*********************************************************************************************
//GetStatus() gets the exit status or the terminating signal from the last run foreground  
//process.
//
//Input: NONE
//Output: Prints signal status to the terminal using stdout.
***********************************************************************************************/
int GetStatus()
{
    

    if (WIFEXITED(childExitMethod))  // If WIFEXITED of childExitMethod is non-zero termination is normal. 
    {
        int exitStatus = WEXITSTATUS(childExitMethod); // Store the exit status.
        fflush(stdout);
        printf("Exit value %d\n",exitStatus);                     // Print the exit status.
        fflush(stdout);
    }
    else   // Otherwise the termination was from a signal.
    {
        int signalStatus = WIFSIGNALED(childExitMethod);  // Store the signal flag.
        printf("Terminated by signal: %d\n",signalStatus); // Print the signal status.
        fflush(stdout); // Flush Stdout.
    }

}


/***********************************************************************************************
//TokenizeWords() tokenizes a space delineated string into an array of words. It will take in the 
//user input string and output an array containing the separate words. 
//NOTE: This work is from  https://stackoverflow.com/questions/26330219/split-line-into-array-of-words-c
************************************************************************************************/
int TokenizeWords(char* str, char* delineatedWords[512])
{

    int start = 0; // Create a start index value.
    int end; // Create an end index value.
    int maxWords = 512;  // create a max words value
    int wordCnt = 0; // Create the value word count to be passed back.

    while(1)
    {
            while(isspace(str[start]))  // while isspace() of the string[beginindex] pre-increment start
            {
                        ++start; 
            }
            if(str[start] == '\0') // check the string for the null terminator if so break
                break;
            end = start; // Set 
            while (str[end] && !isspace(str[end]))
            {
                        ++end; // Increment end index
            }
            
            int len = end - start;  // Length = last index - first index
            char *tmp = calloc(len + 1, sizeof(char));  // Dynamically allocate tmp which will dynamically allocate delineated words.
            memcpy(tmp, &str[start], len);  // Copy the 
            delineatedWords[wordCnt++] = tmp; // 1D for loop to free and then set each pointer to NULL after each while loop iteration
            start = end;
            if (wordCnt == maxWords) // If word count is equalto the max number of words break.
                break;
    }
    return wordCnt; // Return a count of the words.
}

/*************************************************************************************
//StdOutRedirection() conducts the redirection of stdout before executing any commands.
//It takes in two arguments, an int to check how many arguments are passed (argc) and 
//the actual arguments themselves in an array (argv[]).
//
//Input:argv which is a string containing the file to open.
//Output:the opened File descriptor
**************************************************************************************/
int StdOutRedirection(char argv[])
{
    int targetFD = open(argv, O_WRONLY | O_CREAT | O_TRUNC, 0644);  // Open the file to prepare for redirection.
    
    //Check for error
    if (targetFD == -1)  
    { 
        perror("cannot open"); // Print error to the user.
        fflush(stdout);   // Flush stdout
        //exit(1);  // exit with error.
    }
    int result = dup2(targetFD, 1); // If successful pass file decriptor to dup2().
    if (result == -1)  // Check for error with dup2().
    { 
        perror("dup2 error!");  // Print error to the user
        fflush(stdout);   // Flush stdout.
        //exit(2);  // Exit with error.
    }

    return(targetFD); 
        
}

/*************************************************************************************
//StdInRedirection() conducts the redirection of stdin before executing any commands.
//It takes in one argument which is the source file name.
//Input: argv, the string containing the filename.
//Output: The file descriptor
**************************************************************************************/
int StdInRedirection(char argv[])
{
    int sourceFD = open(argv, O_RDONLY);  // Open the file to prepare for redirection.
    if (sourceFD == -1)  // Check for error.
    { 
        perror("Cannot open");  // Print error message.
        exit(1);  // Exit with error.
    }
    
    int result = dup2(sourceFD, 0);  // Pass the file descriptor to dup2()
    if (result == -1)  // Check for error.
    { 
        perror("dup2 error!"); // Print error message to the screen.
        fflush(stdout);   // Flush stdout
        exit(2);  // exit with error.
    }

    return(0);       
}
/*****************************************************************************************************
//InAndOutRedirection() will take in sourceFile and a destFile as strings and redirect stdin & stdout 
//to those files rescpectively.
//Input:argv, filename to redirect stdin to argw, the filename to redirect stdout to. 
//Output: The file descriptors.
******************************************************************************************************/
int InAndOutRedirection(char argv[], char argw[])
{

    int sourceFD, targetFD, result; // Create ints for file descriptors and results
    
    sourceFD = open(argv, O_RDONLY);  // Open the file for source redirection
    if (sourceFD == -1)  // Check for error.
    { 
        perror("Cannot Open"); // Print error message.
        fflush(stdout);  // flush stdout.
        exit(1); // Exit with error.
    }

    targetFD = open(argw, O_WRONLY | O_CREAT | O_TRUNC, 0644); // Open file for target redirection.
    if (targetFD == -1) // check for error.
    { 
        perror("Cannot open"); // Print error message.
        fflush(stdout);  // Flush stdout.
        exit(1);  // Exit with error.
    } 
    result = dup2(sourceFD, 0);  // Redirect stdin using dup2()
    if (result == -1)  // Check for error.
    { 
        perror("Source Error dup2()!"); // Print error message.
        fflush(stdout);  // Flush stdout.
        exit(2); // Exit with error.
    }
    result = dup2(targetFD, 1); // Redirect stdout using dup2
    if (result == -1) // check for error.
    { 
        perror("Target error dup2()!"); // Print error message.
        fflush(stdout); // Flush stdout.
        exit(2);  // exit with error.
    }
}

/***********************************************************************************************************
//CatchSIGTSTP() is the signal handler function for handling SIGTSTP. 
//Signal handling function to toggle between foreground only mode on/off.
//Input: The global variable modeToggle()
//Output:Changes the value of the global variable modeToggle.
***********************************************************************************************************/
void catchSIGTSTP()
{
    
        if(modeToggle == 0)
        
        {
            fflush(stdout);
            char* message1 = "Entering Foreground only mode!"; // Create message to write to stdout.
            write(STDOUT_FILENO, message1, 30); // Write message to stdout
            fflush(stdout);
            
            modeToggle = 1;
        }
        else
        {
            fflush(stdout);
            char* message2 = "Exiting Foreground only Mode!"; //Create message to write to stdout.
            write(STDOUT_FILENO, message2, 30); // Write message to stdout.
            fflush(stdout);
             modeToggle = 0;
        
        }

        
}
