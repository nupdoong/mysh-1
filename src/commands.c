#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "commands.h"
#include "built_in.h"
#include "signal_handlers.h"

static struct built_in_command built_in_commands[] = {
  { "cd", do_cd, validate_cd_argv },
  { "pwd", do_pwd, validate_pwd_argv },
  { "fg", do_fg, validate_fg_argv }
};

static int is_built_in_command(const char* command_name)
{
  static const int n_built_in_commands = sizeof(built_in_commands) / sizeof(built_in_commands[0]);

  for (int i = 0; i < n_built_in_commands; ++i) {
    if (strcmp(command_name, built_in_commands[i].command_name) == 0) {
      return i;
    }
  }

  return -1; // Not found
}

void* client(void* data){
   int client_len;
   int client_sockfd;
   char** command = (char**)data;
   char* Port_File = "/tmp/port";
   struct sockaddr_un clientaddr;
   int pid;
   int status;
   int tempfd = 100;

   client_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
   if(client_sockfd == -1){
      perror("error : ");
      exit(0);
   }

   memset(&clientaddr, 0x00, sizeof(clientaddr));
   clientaddr.sun_family = AF_UNIX;
   strcpy(clientaddr.sun_path, Port_File);
   client_len = sizeof(clientaddr);
  
   if(connect(client_sockfd, (struct sockaddr*)&clientaddr, client_len) < 0){
      perror("connect error: ");
      exit(0);
   }
   
   tempfd = dup(STDOUT_FILENO);
   dup2(client_sockfd, STDOUT_FILENO);

   pid = fork();
   if(pid < 0){
      perror("fork failed in thread : ");
      exit(0);
   }
   else if (pid > 0){
      waitpid(-1, &status, 0);
      dup2(tempfd, STDOUT_FILENO);
      close(tempfd);
   }
   else{
      execv(command[0], command);
      exit(0);
   }
   close(client_sockfd);
   
}

/*
 * Description: Currently this function only handles single built_in commands. You should modify this structure to launch process and offer pipeline functionality.
 */
int evaluate_command(int n_commands, struct single_command (*commands)[512])
{
  if (n_commands > 0) {
    struct single_command* com = (*commands);

    assert(com->argc != 0);

    int built_in_pos = is_built_in_command(com->argv[0]);
    int pid;
    int flag = 0;
    int flag2 = 0;
    int status;
    int isBack = 0;
    struct stat buf[2];

    stat(com->argv[0], &buf[0]);
    if((access(com->argv[0], 0) != -1) && !S_ISDIR(buf[0].st_mode))
       flag = 1;
    if(n_commands == 2){
       stat((com + 1)->argv[0], &buf[1]);
       if((access((com + 1)->argv[0], 0) != -1) && !S_ISDIR(buf[1].st_mode))
          flag2 = 1;
    }
    for(int i = 0; i < n_commands; i++){
       if((strchr((com + i)->argv[(com + i)->argc -1], '&')!= NULL) && (com + i)->argc > 1){
          (com + i)->argv[(com + i)-> argc - 1] = NULL;
          isBack = 1;
       }
    }

    pid = fork();
    if(pid < 0){
       fputs("fork fail\n", stderr);
       return -1;
    }
    else if(pid > 0){
       if(isBack){
          bg.pid = pid;
          setpgid(pid, pid);
       }
       else{
          waitpid(-1, &status, 0);
       }

       if(flag) return 0;


    if (built_in_pos != -1) {
      if (built_in_commands[built_in_pos].command_validate(com->argc, com->argv) && n_commands != 2) {
        if (built_in_commands[built_in_pos].command_do(com->argc, com->argv) != 0) {
          fprintf(stderr, "%s: Error occurs\n", com->argv[0]);
        }else if(built_in_pos == 2){
           kill(bg.pid, SIGCONT);
           tcsetpgrp(0, bg.pid);
           pause();
        }
      } else if(n_commands != 2){
        fprintf(stderr, "%s: Invalid arguments\n", com->argv[0]);
        return -1;
      }
    } else if (strcmp(com->argv[0], "") == 0) {
      return 0;
    } else if (strcmp(com->argv[0], "exit") == 0 && n_commands != 2) {
      return 1;
    } else if(n_commands != 2){
      fprintf(stderr, "%s: command not found\n", com->argv[0]);
      return -1;
    }
  }
  else{
     if(isBack){
        bg.pid = getpid();
        setpgid(0, 0);
        printf("[1] %d\n", getpid());
     }
     if(n_commands != 2){
        execv(com->argv[0], com->argv);
        signal(SIGINT, (void*)catch_sigint);
        signal(SIGTSTP, (void*)catch_sigtstp);
        exit(0);
     }
     else if(flag && flag2){
        int thread_status;
        pthread_t thread_t;
        if(pthread_create(&thread_t, NULL, client, com->argv) < 0){
           perror("thread create eroor : ");
           exit(0);
        }

        int server_sockfd, client_sockfd;
        int state, client_len;
        struct sockaddr_un clientaddr, serveraddr;
        char* Port_File = "/temp/port";

        if(!access(Port_File, F_OK))
           unlink(Port_File);
        client_len = sizeof(clientaddr);
        if((server_sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
           perror("socket error: ");
           exit(0);
        }
        memset(&serveraddr, 0x00, sizeof(serveraddr));
        serveraddr.sun_family = AF_UNIX;
        strcpy(serveraddr.sun_path, Port_File);
        state = bind(server_sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
        if(state == -1){
           perror("bind error: ");
           exit(0);
        }
        state = listen(server_sockfd, 1);
        if(state == -1){
           perror("listen error: ");
           exit(0);
        }
        pthread_join(thread_t, (void**)&thread_status);
        client_sockfd = accept(server_sockfd, (struct sockaddr*)&clientaddr, &client_len);
        if(client_sockfd == -1){
           perror("accept error : ");
           exit(0);
        }
        dup2(client_sockfd, STDIN_FILENO);
        close(client_sockfd);
        if(execv((com + 1)->argv[0], (com + 1)->argv) == -1)
           exit(0);
     }
     else if(!flag || !flag2){
        if(!flag)
           fprintf(stderr, "%s: command not found\n", com->argv[0]);
        if(!flag2)
           fprintf(stderr, "%s: command not found\n", (com + 1)->argv[0]);
        exit(0);
     }
  }
}

  return 0;
}

void free_commands(int n_commands, struct single_command (*commands)[512])
{
  for (int i = 0; i < n_commands; ++i) {
    struct single_command *com = (*commands) + i;
    int argc = com->argc;
    char** argv = com->argv;

    for (int j = 0; j < argc; ++j) {
      free(argv[j]);
    }

    free(argv);
  }

  memset((*commands), 0, sizeof(struct single_command) * n_commands);
}
