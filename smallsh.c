#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <errno.h>

// Set the two global variables for communicating
// with signal handler functions. background_messages
// records return information about background proscesses
// from from the SIGCHLD handler. foreground_only records
// the current foreground only state dictated by the SIGTSTP handler.
char background_messages[400] = "";
int foreground_only = 0;


// Defines the user_action structure. 
struct user_action{
  char* command;
  char* in_file;
  char* out_file; 
  char* args[512];
  int foreground;
  int arg_count;
};   

// Defines the status structure. The status structure is used
// to record the end state of the most recent foreground process
// so that it can be returned by the status command. It is separate
// from the user_action structure because the user action struct
// is reset each call of the while loop, and status information must
// persist. 
struct status{
  int type;
  int value;
};


// Handler for SIGTSTP used only for the parent process. Swaps 
// to/from foreground only mode and prints a message indicating
// the swap.
void handle_SIGTSTP(int signo){
  if (foreground_only == 0){
    foreground_only = 1;
    write(STDOUT_FILENO, "Entering foreground-only mode (& is now ignored)\n: ", 51);
    fflush(stdout);
  }
  else {
    foreground_only = 0;
    write(STDOUT_FILENO, "Exiting foreground-only mode\n: ", 31);
    fflush(stdout);
  }
}

// Int to string converter used for the SIGCHLD handler
// allocates a malloc which is freed at the end of use in 
// the SIGCHILD handler. MAKE SURE TO FREE THE MALLOC!
// 0 is treated independently since log10(0) is undefined. 
char* int_to_str(int input){
  char * ints = "0123456789";
  int str_len;
  char * output_str;
  if (input == 0){
    output_str = malloc(2);
    output_str[0] = '0';
    output_str[1] = '\0';
  }
  else if(input < 0){
    input = -1 * input;
    str_len = (int) log10(input) + 2;
    output_str = malloc(str_len + 1);
    for (int i = 0; i < str_len; i++){
      int out_digit = input % 10;
      output_str[str_len - 1 - i] = ints[out_digit];
      input = (input - out_digit) / 10;
    }
    output_str[0] = '-';
    output_str[str_len + 1] = '\0';
  }
  else{
    str_len = (int) log10(input) + 1;
    output_str = malloc(str_len + 1);
    for (int i = 0; i < str_len; i++){
      int out_digit = input % 10;
      output_str[str_len - 1 - i] = ints[out_digit];
      input = (input - out_digit) / 10;
    }
    output_str[str_len] = '\0';
  }
  return output_str;
}

// Handler for SIGCHLD. It would be much shorter if sprintf
// were reentrant. Foreground processes are caught first by
// the waitpid in the new process function, so when they reach
// the waitpid in the handler, an error is thrown and -1 is returned.
// Since pid =  -1 and 0 are ignored, only messages for background
// messages are sent to the the background_messages global variable.
void handle_SIGCHILD(int signo){
  int child_status;
  pid_t pid;
  pid = waitpid(-1, &child_status, WNOHANG);
  if (pid != -1 && pid != 0){
    char* pid_char = int_to_str(pid);
    if(WIFEXITED(child_status)){
      child_status = WEXITSTATUS(child_status);
      char* status_char = int_to_str(child_status);
      fflush(stdout);
      strcat(background_messages, "background pid ");
      strcat(background_messages, pid_char);
      strcat(background_messages, " is done: exit value ");
      strcat(background_messages, status_char);
      strcat(background_messages, "\n");
      free(status_char);
	  } 
    else{
      child_status = WTERMSIG(child_status);
      char* status_char = int_to_str(child_status);
      strcat(background_messages, "background pid ");
      strcat(background_messages, pid_char);
      strcat(background_messages, " is done: terminated by signal ");
      strcat(background_messages, status_char);
      strcat(background_messages, "\n");
      free(status_char);
	  }
  free(pid_char);
  }
}

// Function for redirecting input/output signals to their proper location.
// If there is a specified input a/or output file from the user action, 
// stdin/out is redirected there, otherwise they are redirected to /dev/null
// if the process is a background process. 
int redirect(struct user_action action){
  int in_file;
  int out_file;

  // Redirect input stream if applicable. 
  if (strcmp(action.in_file, "") != 0){
    in_file = open(action.in_file, O_RDONLY);
    if (in_file == -1){
      fprintf(stderr, "%s %i\n", "File open failed with error", errno);
      fflush(stderr);
      exit(1);
    }
    dup2(in_file, STDIN_FILENO);
  }
  else if (action.foreground == 0){
    in_file = open("/dev/null", O_RDONLY);
    dup2(in_file, STDIN_FILENO);
  }

  // Redirect output stream if applicable.
  if (strcmp(action.out_file, "") != 0){
    out_file = open(action.out_file, O_WRONLY|O_TRUNC|O_CREAT, 0777);
    if (out_file == -1){
      fprintf(stderr, "%s %i\n", "File open failed with error", errno);
      fflush(stderr);
      exit(1);
    }
    dup2(out_file, STDOUT_FILENO);
  }
  else if (action.foreground == 0){
    out_file = open("/dev/null", O_WRONLY);
    dup2(out_file, STDOUT_FILENO);
  }
  return 0;
}

// Function for converting $$ to the pid of the parent process. 
// CREATES MALLOCS THAT PERSIST THROUGH RETURN. THESE MUST BE FREED.
// 
char* translate(char* input_word) {
  char* strstr_input = input_word;
  int $$count = 0;

  // The first section of the function counts the number of instances
  // of $$ in the input string, and allocates a malloc of the correct length
  // to hold the translated string.
  while ((strstr_input = strstr(strstr_input, "$$")) != NULL){
    $$count += 1;
    strstr_input += 2;
  }
  // Convert the PID to a string and get its length
  int pid_len = (int) log10(getpid()) + 1;
  char pid[pid_len];
  sprintf(pid, "%d", getpid());
  // Allocate a malloc to hold the final string.
  char* new_word = malloc(strlen(input_word) + $$count*(strlen(pid) - 2) + 1);

  // The second section of the function loops over the inital string,
  // replacing $$ with the PID string when it is found. last$ remembers
  // whether the last character was a $. j is the index counter for the 
  // translated string.
  int last$ = 0;
  int j = 0;
  for (unsigned int i=0; i < strlen(input_word); i++){
    // last char was $, but current char isnt. Write both the 
    // $ and the new char to the output string.
    if (input_word[i] != '$' && last$ == 1){
      last$ = 0;
      new_word[j] = '$';
      new_word[j + 1] = input_word[i];
      j += 2;
    }
    // last char was $ and this char was $. Write the PID string
    // to the output string.
    else if(input_word[i] == '$' && last$ == 1){
      last$ = 0;
      for (unsigned int k = 0; k < strlen(pid); k++){
        new_word[j] = pid[k];
        j += 1;
      }
    }
    // current char is $ and last char isnt. Set last$.
    else if(input_word[i] == '$' && last$ == 0){
      last$ = 1;
    }
    // current char and last char are regular characters.
    // Write the current char to the output string.
    else{
      new_word[j] = input_word[i];
      j += 1;
    }
  }
  // If the last char in the string was an untranslated $,
  // write it to the output string. 
  if (last$ == 1){
    new_word[j] = '$';
    j += 1;
  }
  // Null-terminate the string.
  new_word[j] = '\0';
  return new_word;
}

// Function to convert the inputs from the user into commands, arguments, 
// input and output files, and determine if the command should be run in
// the foreground or background. 
struct user_action process_buffer(char* input_buffer, struct user_action action){
  // Get the command from the buffer and store it in action.command.
  action.command = strtok(input_buffer, " \n");
  // Initilize infile and outfile for comparison in new_process.
  action.in_file = "";
  action.out_file = "";
  // Intilize first argument to NULL for comparision in run_action.
  action.args[0] = NULL; 
  char* input;
  // flag variable is used to determine if last character was special
  // character < or > so input can be directed to in_file or out_file 
  // respectively. 
  char flag = '0';
  action.foreground = 1;
  while ((input = strtok(NULL, " \n")) != NULL){
    // This boolean handles the case of & key as a (non-final) argument
    // If an & set the action to background, resets it to foreground
    // and adds & to the argument array.
    if (flag == '&'){
      action.foreground = 1;
      if (action.arg_count < 512){
        char* persand = translate("&");
        action.args[action.arg_count] = persand;
        action.arg_count += 1;
      }
      else{
        fprintf(stderr,"%s\n", "Too many arguments");
        fflush(stderr);
        exit(1);
      }
    }
    // Sets the flag to > or < when those characters appear.
    else if(strcmp(input, ">") == 0){flag = '>';}
    else if(strcmp(input, "<") == 0){flag = '<';}
    // If most recent character was < or >, sends the current input 
    // to action.in_file or action.out_file respectively.
    if(flag == '<'){
      action.in_file = input;
      flag = 0;
    }
    else if(flag == '>'){
      action.out_file = input;
      flag = 0;
    }
    // Sets the action to background mode if foreground_only
    // is not set, and sets the flag to '&'.
    else if(strcmp(input, "&") == 0){
      if (foreground_only == 0){
        action.foreground = 0;
      }
      flag = '&';
    }
    // Writes arguments to the argument array, up to 
    // a maximum of 512.
    else{
      if (action.arg_count < 512){
        input = translate(input);
        action.args[action.arg_count] = input;
        action.arg_count += 1;
      }
      else{
        fprintf(stderr,"%s\n", "Too many arguments");
        fflush(stderr);
        exit(1);
      }
    }
  }
  return action;
}

// This function is called when the input command must be run via
// exec. 
int new_process(struct user_action action, struct status *status){
  char* arg_vec[action.arg_count + 2];
  int childStatus;
  // fork a new process for command to be executed in. 
  pid_t spawnPid = fork();
  switch (spawnPid){
    
    // Error case.
    case -1:
      fprintf(stderr, "fork() failed\n");
      fflush(stderr);
      exit(1);
      break;
    
    case 0:
      // Call redirect to send stdin and stdout to locations
      // dicated by action.in_file/out_file and whether the 
      // process is executed in background. 
      redirect(action);

      // Set child process to ignore SIGTSTP. 
      struct sigaction SIGTSTP_action = {0};
      SIGTSTP_action.sa_handler = SIG_IGN;
      sigaction(SIGTSTP, &SIGTSTP_action, NULL);

      // If process is a foreground process, set it to 
      // exit when it recieves SIGINT. 
      if (action.foreground == 1){
        struct sigaction SIGINT_action = {0};
        SIGINT_action.sa_handler = SIG_DFL;
	      SIGINT_action.sa_flags = SA_RESTART;
        sigaction(SIGINT, &SIGINT_action, NULL);
      }

      // Transfer command and arguments to an argument
      // array to be sent to execvp. Null the last element of
      // the argument array, and run execvp to execute the command.
      arg_vec[0] = action.command;
      for (int i = 0; i < action.arg_count; i++ ){
        arg_vec[i+1] = action.args[i];
      }
      arg_vec[action.arg_count + 1] = NULL;
      execvp(action.command, arg_vec);

      // error handling for exec.
      perror("execvp");
      fflush(stderr);
	    exit(EXIT_FAILURE);
      break;
    
    default:
      // Print output message if process being executed in background.
      if (action.foreground == 0 && foreground_only == 0){
        printf("%s %i\n", "background pid is", spawnPid);
        fflush(stdout);
      }
      else {
        // Wait for process to finish.
        waitpid(spawnPid, &childStatus, 0);
        // If process exits normally, send exit information to status.
        if(WIFEXITED(childStatus)){
          (*status).type = 0;
          childStatus = WEXITSTATUS(childStatus);
		      (*status).value = childStatus;
	      } 
        // If process is terminated, print a message indicating the 
        // termination signal and send exit information to status. 
        else{
          (*status).type = 1;
          childStatus = WTERMSIG(childStatus);
		      (*status).value = childStatus;
          printf("%s %d\n", "terminated by signal", WTERMSIG(childStatus));
          fflush(stdout);
	      }
      }
      break;
  }
  return(0);
}
  
// Function to run commands sent by the user. If the command is one 
// of the three basic commands, they are run within this function, 
// otherwise new_process is called. 
int run_action(struct user_action action, struct status *status){

  // exit command for the program. Exits exit code 0. 
  if (strcmp(action.command, "exit") == 0){
    exit(0);
  }

  // Processes change directory commands. If no arguments are given,
  // moves to home directory. Otherwise moves to directory given by 
  // first argument, if that directory exists.
  else if (strcmp(action.command, "cd") == 0){
    if (action.args[0] == NULL){
      chdir(getenv("HOME"));
    }
    else{
      chdir(action.args[0]);
    }
  }

  // Processes status command. Gets status information from the 
  // status structure, and prints the message associated with 
  // the exit status of the last exec foreground call. 
  else if (strcmp(action.command, "status") == 0){
    if ((*status).type == 0){
      printf("%s %i\n", "exit value", (*status).value);
      fflush(stdout);
    }
    else {
      printf("%s %i\n", "terminated by signal", (*status).value);
      fflush(stdout);
    }
  }

  // If the command was not any of the three above, call new_process
  // to execute the command. 
  else{
    new_process(action, status);
  }
  return(0);
}

int main(void) {

  // Initilization of SIGINT action.
  // Hander ignores SIGINT signal, the 
  // handler is pointed to the actual handler
  // for foreground processes.
  struct sigaction SIGINT_inaction = {0};
  SIGINT_inaction.sa_handler = SIG_IGN;
  SIGINT_inaction.sa_flags = SA_RESTART;
  sigaction(SIGINT, &SIGINT_inaction, NULL);

  // Initilization of SIGTSTP action
  // Mask blocks all signals while signal is processed
  // SA_RESTART prevents signal catches from
  // interfering with reads and writes. 
  struct sigaction SIGTSTP_action;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_handler = handle_SIGTSTP;
  SIGTSTP_action.sa_flags = SA_RESTART;
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

  // Intitilization of the SIGCHLD action.
  // Mask blocks all signals while signal is processed
  // SA_RESTART prevents signal catches from
  // interfering with reads and writes.
  struct sigaction SIGCHLD_action = {0};
  SIGCHLD_action.sa_handler = handle_SIGCHILD;
  sigfillset(&SIGCHLD_action.sa_mask);
	SIGCHLD_action.sa_flags = SA_RESTART;
  sigaction(SIGCHLD, &SIGCHLD_action, NULL);

  // Initilize the structure status. The structure
  // type value is 0 when the most recent return
  // exited normally, and 1 when it was terminated.
  // the value is the return value or terminating signal number.
  struct status status;
  status.value = 0;
  status.type = 0;
  
  while (1){
    // Write new line prompt to STDOUT.
    printf("%c ", ':');
    fflush(stdout);

    // Intitilize input buffer 
    char input_buffer[2049];

    // Reinitilize action each time the while loop 
    struct user_action action;
    action.arg_count = 0;

    // get information from user and call process_buffer
    // and run action to execute the user's command. 
    fgets(input_buffer, 2048, stdin);
    if (input_buffer[0] != '#' && input_buffer[0] != '\n'){
      action = process_buffer(input_buffer, action);
      run_action(action, &status);
    }

    // Print messages from the SIGCHLD handler about
    // finished background messages.
    printf("%s", background_messages);
    fflush(stdout);
    // reset the background messages to print an empty string.
    background_messages[0] = '\0';
    // free mallocs allocated in the translate function.
    for (int i = 0; i < action.arg_count; i ++){
      free(action.args[i]);
    }
  }
}
