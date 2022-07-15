#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct user_action{
  char* command;
  char* in_file;
  char* out_file;
  char* args[512];
}; 

struct user_action process_buffer(char* input_buffer, struct user_action action){
  char* input = strtok(input_buffer, " \n");
  char flag = '1';
  int count = 0;
  while (input){
    input = strtok(NULL, " \n");
    if (input != NULL){
      if (flag == '1'){
        action.command = input;
        flag = '0';
      }
      else if(flag == '<'){
        action.in_file = input;
      }
      else if(flag == '>'){
        action.out_file = input;
      }
      else if(strcmp(input, ">") == 0){
        flag = '>';
      }
      else if(strcmp(input, "<") == 0){
        flag = '<';
      }
      else{
        if (count < 512){
          action.args[count] = input;
          count += 1;
        }
        else{
          fprintf(stderr,"%s\n", "Too many arguments");
          exit(1);
        }
      }
    }
  }
  return action;
}

int run_basic_action(struct user_action action){
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
    
  }
  return(0);
}

int main(void) {
  while (1){
    char input_buffer[2048];
    struct user_action action;
    fgets(input_buffer, 2048, stdin);
    if (input_buffer[0] == ':'){
      action = process_buffer(input_buffer, action);
      run_basic_action(action);
    }
    else if (input_buffer[0] == '#' || input_buffer[0] == '\n'){
      // How do you want reprompt
    }
  }
}