#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <math.h>

struct user_action{
  char* command;
  char* in_file;
  char* out_file;
  char* args[512];
  int foreground;
  int arg_count;
}; 

const char* translate(char* input_word) {
  char* strstr_input;
  int $$count = 0;
  while ((strstr_input = strstr(input_word, "$$")) != NULL){
    $$count += 1;
    strstr_input += 2;
  }
  int pid_num = getpid();
  char pid[(int)(log10(pid_num)) + 1];
  sprintf(pid, "%d", getpid());
  char* new_word =  malloc(strlen(input_word) + $$count*(strlen(pid) - 2) + 1);
  int last$ = 0;
  int j = 0;
  for (int i=0; i < strlen(input_word); i++){
    if (input_word[i] != '$' && last$ == 1){
      last$ = 0;
      new_word[j] = '$';
      new_word[j + 1] = input_word[i];
      j += 2;
    }
    else if(input_word[i] == '$' && last$ == 1){
      last$ = 0;
      for (int k = 0; k < strlen(pid); k++){
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
  return(new_word);
}

struct user_action process_buffer(char* input_buffer, struct user_action action){
  action.command = strtok(input_buffer, " \n");
  action.in_file = "";
  action.out_file = "";
  char* input;
  char flag = '0';
  action.arg_count = 0;
  action.foreground = 1;
  while ((input = strtok(NULL, " \n")) != NULL){
      const char* trans_input = translate(input);
      if(flag == '<'){
        action.in_file = trans_input;
        flag = 0;
      }
      else if(flag == '>'){
        action.out_file = trans_input;
        flag = 0;
      }
      else if(strcmp(input, ">") == 0){
        flag = '>';
      }
      else if(strcmp(input, "<") == 0){
        flag = '<';
      }
      else if(strcmp(input, "&") == 0){
        action.foreground = 0;
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

int new_process(struct user_action action){
  char* arg_vec[action.arg_count + 2];
  int childStatus;
  int save_stdin = dup(STDIN_FILENO);
  int save_stdout = dup(STDOUT_FILENO);
  int in_file;
  int out_file;
  if (strcmp(action.in_file, "") != 0){
    in_file = open(action.in_file, O_RDONLY);
    dup2(in_file, STDIN_FILENO);
  }
  if (strcmp(action.out_file, "") != 0){
    out_file = open(action.out_file, O_WRONLY|O_TRUNC|O_CREAT, 0777);
    dup2(out_file, STDOUT_FILENO);
  }
  pid_t spawnPid = fork();
  switch (spawnPid){
    case -1:
      fprintf(stderr, "fork() failed\n");
      fflush(stderr);
      exit(1);
      break;
    case 0:
      arg_vec[0] = action.command;
      for (int i = 0; i < action.arg_count; i++ ){
        arg_vec[i+1] = action.args[i];
      }
      arg_vec[action.arg_count + 1] = NULL;
      execvp(action.command, arg_vec);
      perror("execv");   /* execve() returns only on error */
      fflush(stderr);
	    exit(EXIT_FAILURE);
      break;
    default:
      if (action.foreground == 0){
        spawnPid = waitpid(spawnPid, &childStatus, WNOHANG);
      } 
      else {
        spawnPid = waitpid(spawnPid, &childStatus, 0);
      }
      if (strcmp(action.out_file, "") != 0){
        fflush(stdout);
        close(out_file);
        dup2(save_stdin, STDOUT_FILENO);
      }
      if (strcmp(action.in_file, "") != 0){
        fflush(stdin);
        close(in_file);
        dup2(save_stdin, STDIN_FILENO);
      }
      close(save_stdin);
      close(save_stdout);
      break;
  }
  return(0);
}
  

int run_action(struct user_action action){
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
    printf("%i\n", 0);
  }
  else{
    new_process(action);
  }
  return(0);
}

int main(void) {
  while (1){
    printf("%c ", ':');
    char input_buffer[2048];
    struct user_action action;
    fgets(input_buffer, 2048, stdin);
    if (input_buffer[0] != '#' && input_buffer[0] != '\n'){
      action = process_buffer(input_buffer, action);
      run_action(action);
    }
  }
}
