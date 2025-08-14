#include "libparser.h"
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
//#include "test_parser.c"
//the cmd args lists may need to be null terminated

char* next_token(char** str) {
    char* start = *str;
    char* end;

    // Skip leading whitespace
    while (*start == ' ') start++;
    if (*start == '\0') return NULL;

    if (*start == '"') {
        // Quoted string
        start++; // Skip opening quote
        end = strchr(start, '"');
        if (end) {
            *end = '\0';
            *str = end + 1;
            return start;
        } else {
            // No closing quote, treat rest of string as token
            *str = start + strlen(start);
            return start;
        }
    } else {
        // Non-quoted token
        end = start;
        while (*end && *end != ' ') end++;
        if (*end) {
            *end = '\0';
            *str = end + 1;
        } else {
            *str = end;
        }
        return start;
    }
}

void get_command(char* line, struct Cmd* cmdline)
{
    char *token;
    int num_cmds = 0;
    int i;
    char* ptr;
    int file_idx = -1;

    cmdline->cmd1_argv = (char**)malloc(10*sizeof(char*));
    cmdline->cmd2_argv = (char**)malloc(10*sizeof(char*));
    
    for (i = 0; i < 3; i++) {
        cmdline->cmd1_fds[i] = NULL;
        cmdline->cmd2_fds[i] = NULL;
    }

    //token = strtok(line, " ");
    token = next_token(&line);

    //first command sequence
    while (token != NULL && (strcmp(token, "|") != 0)) {
        //the token is the file name. This applies after the file index has been found in the if else below
        if (file_idx > -1) {
            cmdline->cmd1_fds[file_idx] = token + '\0';
            //printf("file %s\n", cmdline->cmd1_fds[file_idx]);
            file_idx = -1;
            
        // there will be an input or output file given
        } else if ((ptr = strstr(token, ">")) != NULL || (ptr = strstr(token, "<")) != NULL) {
            
            /*if (token[0] == '1') {
                file_idx = 1;
            } else if( token[0] == '2') {
                file_idx = 2;
            }else {
                file_idx = 0;
            }*/
            //file_idx 1 means >>, file idx 0 means >
            if (strlen(token) == 2) {
                file_idx = 1;
            } else  {
                file_idx = 0;
            }
            
        //regular command, not file related
        } else {
            cmdline->cmd1_argv[num_cmds] = token;
            //printf("%s\n", cmdline->cmd1_argv[num_cmds]);   
            num_cmds++;
        }
        //token = strtok(NULL, " ");
        token = next_token(&line);
    }
    //add null terminator to args;
    cmdline->cmd1_argv[num_cmds] = NULL;
    num_cmds = 0;
    /*
    //next command, true if token == "|"
    if (token != NULL) {
        //skip the pipe
        token = strtok(NULL, " ");
        while (token != NULL) {
            //the token is the file name
            if (file_idx > -1) {
                cmdline->cmd2_fds[file_idx] = token;
                //printf("file %s\n", cmdline->cmd2_fds[file_idx]);
                file_idx = -1;
                
            // there will be an input or output file given
            } else if ((ptr = strstr(token, ">")) != NULL || (ptr = strstr(token, "<")) != NULL) {
                if (token[0] == '1') {
                    file_idx = 1;
                } else if( token[0] == '2') {
                    file_idx = 2;
                }else {
                    file_idx = 0;
                }
                
            //regular command, not file related
            } else {
                cmdline->cmd2_argv[num_cmds] = token;
                //printf("%s\n", cmdline->cmd2_argv[num_cmds]);   
                num_cmds++;
            }
            token = strtok(NULL, " ");
        }
    }
    //add null terminator to args;
    cmdline->cmd2_argv[num_cmds] = NULL;*/
    
    //print_cmd(cmdline);

    //free memory
    //free(cmdline->cmd1_argv);
    //free(cmdline->cmd2_argv);
}








/*
int main() {
    char line[2048];
    struct Cmd cmd;

    strncpy(line, "ls -l", 2048);
    get_command(line, &cmd);
    return 0;
}*/

//https://www.educative.io/answers/splitting-a-string-using-strtok-in-c