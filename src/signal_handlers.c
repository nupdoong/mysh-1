#include <stdio.h>
#include <signal.h>
#include "signal_handlers.h"

void catch_sigint(int signalNo)
{
  // TODO: File this!
  printf("\nEnter \"exit\" to terminate this process..\n");
  signal(SIGINT, catch_sigint);
}

void catch_sigtstp(int signalNo)
{
  // TODO: File this!
  printf("\nCannot pause the process\n");
  signal(SIGTSTP, catch_sigtstp);
}
