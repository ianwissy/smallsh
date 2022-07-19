#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <errno.h>

char background_messages[400] = "";
int foreground_only = 0;

struct user_action{
  char* command;
  char* in_file;
  char* out_file; 
  char* args[512];
  int foreground;
  int arg_count;
};   

struct status{
  int type;
  int value;
};

void handle_SIGINT(int signo){
  exit(1);
}

void handle_SIGTSTP(int signo){
  if (foreground_only == 0){
    foreground_only = 1;
    write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n: ", 52);
  }
  else {
    foreground_only = 0;
    write(STDOUT_FILENO, "\nExiting foreground-only mode\n: ", 32);
  }
}

void handle_SIGCHILD(int signo){
  int status;
  char message[60];
  pid_t pid;
  pid = waitpid(0, &status, 0);
  if (pid != -1 && pid != 0){
    if(WIFEXITED(status)){
      strcat(background_messages, "background pid %d is done: exit value %d\n");
		  //sprintf(message, "background pid %d is done: exit value %d\n", pid, WEXITSTATUS(status));
	  } 
    else{
      strcat(background_messages, "background pid %d is done: terminated by signal %d\n");
		  //sprintf(message,"background pid %d is done: terminated by signal %d\n", pid, WTERMSIG(status));
	  }
  }
  //strcat(background_messages, message);
}


int redirect(struct user_action action){
  int in_file;
  int out_file;
  if (strcmp(action.in_file, "") != 0){
    in_file = open(action.in_file, O_RDONLY);
    if (in_file == -1){
      printf("%s %i\n", "File open failed with error", errno);
      exit(1);
    }
    dup2(in_file, STDIN_FILENO);
  }
  else if (action.foreground == 0){
    in_file = open("/dev/null", O_RDONLY);
    dup2(in_file, STDIN_FILENO);
  }
  if (strcmp(action.out_file, "") != 0){
    out_file = open(action.out_file, O_WRONLY|O_TRUNC|O_CREAT, 0777);
    if (out_file == -1){
      printf("%s %i\n", "File open failed with error", errno);
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

char* translate(char* input_word) {
  char* strstr_input;
  int $$count = 0;
  while ((strstr_input = strstr(input_word, "$$")) != NULL){
    $$count += 1;
    strstr_input += 2;
  }
  int pid_len = (int) log10(getpid()) + 1;
  char pid[pid_len];
  sprintf(pid, "%d", getpid());
  char* new_word =  malloc(strlen(input_word) + $$count*(strlen(pid) - 2) + 1);
  int last$ = 0;
  int j = 0;
  for (unsigned int i=0; i < strlen(input_word); i++){
    if (input_word[i] != '$' && last$ == 1){
      last$ = 0;
      new_word[j] = '$';
      new_word[j + 1] = input_word[i];
      j += 2;
    }
    else if(input_word[i] == '$' && last$ == 1){
      last$ = 0;
      for (unsigned int k = 0; k < strlen(pid); k++){
        new_word[j] = pid[k];
        j += 1;
      }
    }
    else if(input_word[i] == '$' && last$ == 0){
      last$ = 1;
    }
    else{
      new_word[j] = input_word[i];
      j += 1;
    }
  }
  if (last$ == 1){
    new_word[j] = '$';
    j += 1;
  }
  new_word[j] = '\0';
  strcpy(input_word, new_word);
  free(new_word);
  return(input_word);
}

struct user_action process_buffer(char* input_buffer, struct user_action action){
  action.command = strtok(input_buffer, " \n");
  action.in_file = "";
  action.out_file = "";
  action.args[0] = NULL;
  char* input;
  char flag = '0';
  action.arg_count = 0;
  action.foreground = 1;
  while ((input = strtok(NULL, " \n")) != NULL){
    char* trans_input = translate(input);
    if(flag == '<'){
      action.in_file = trans_input;
      flag = 0;
    }
    else if(flag == '>'){
      action.out_file = trans_input;
      flag = 0;
    }
    else if(strcmp(input, ">") == 0){flag = '>';}
    else if(strcmp(input, "<") == 0){flag = '<';}
    else if(strcmp(input, "&") == 0){
      if (foreground_only == 0){
        action.foreground = 0;
      }
    }
    else{
      if (action.arg_count < 512){
        action.args[action.arg_count] = trans_input;
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

int new_process(struct user_action action, struct status *status){
  char* arg_vec[action.arg_count + 2];
  int childStatus;
  pid_t spawnPid = fork();
  switch (spawnPid){
    case -1:
      fprintf(stderr, "fork() failed\n");
      fflush(stderr);
      exit(1);
      break;
    case 0:
      redirect(action);
      struct sigaction SIGTSTP_action = {0};
      SIGTSTP_action.sa_handler = SIG_IGN;
      sigaction(SIGTSTP, &SIGTSTP_action, NULL);
      if (action.foreground != 0){
        struct sigaction SIGINT_action = {0};
        SIGINT_action.sa_handler = handle_SIGCHILD;
	      SIGINT_action.sa_flags = SA_RESTART;
        sigaction(SIGINT, &SIGINT_action, NULL);
      }
      arg_vec[0] = action.command;
      for (int i = 0; i < action.arg_count; i++ ){
        arg_vec[i+1] = action.args[i];
      }
      arg_vec[action.arg_count + 1] = NULL;
      execvp(action.command, arg_vec);
      perror("execv");/* execve() returns only on error */
      fflush(stderr);
	    exit(EXIT_FAILURE);
      break;
    default:
      if (action.foreground != 0){
        waitpid(spawnPid, &childStatus, 0);
        if(WIFEXITED(childStatus)){
          (*status).type = 0;
		      (*status).value = WEXITSTATUS(childStatus);
	      } 
        else{
          (*status).type = 1;
		      (*status).value = WTERMSIG(childStatus);
          printf("%s %d\n", "terminated by signal", WTERMSIG(childStatus));
	      }
      }
      else {
        fprintf(stdout, "%s %i\n", "background pid is", spawnPid);
        fflush(stdout);
        struct sigaction SIGCHLD_action = {0};
        SIGCHLD_action.sa_handler = handle_SIGCHILD;
	      SIGCHLD_action.sa_flags = SA_RESTART|SA_RESETHAND;
        sigaction(SIGCHLD, &SIGCHLD_action, NULL);
      }
      break;
  }
  return(0);
}
  

int run_action(struct user_action action, struct status *status){
  if (strcmp(action.command, "exit") == 0){
    exit(0);
  }
  else if (strcmp(action.command, "cd") == 0){
    if (action.args[0] == NULL){
      chdir(getenv("HOME"));
    }
    else{
      chdir(action.args[0]);
    }
  }
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
  else{
    new_process(action, status);
  }
  return(0);
}

int main(void) {
  
  struct sigaction SIGINT_inaction = {0};
  SIGINT_inaction.sa_handler = SIG_IGN;
  sigaction(SIGINT, &SIGINT_inaction, NULL);

  struct sigaction SIGTSTP_action;
  SIGTSTP_action.sa_handler = handle_SIGTSTP;
  SIGTSTP_action.sa_flags = SA_RESTART;
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);
  
  struct status status;
  status.value = 0;
  status.type = 0;
  while (1){
    printf("%c ", ':');
    fflush(stdout);
    char input_buffer[2048];
    struct user_action action;
    fgets(input_buffer, 2048, stdin);
    if (input_buffer[0] != '#' && input_buffer[0] != '\n'){
      action = process_buffer(input_buffer, action);
      run_action(action, &status);
    }
    printf("%s", background_messages);
    fflush(stdout);
    background_messages[0] = '\0';
  }
}
