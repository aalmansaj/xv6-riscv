#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int tickets = 3;

  if(settickets(tickets) != 0){
    printf("Syscall error");
  }
  else{
    printf("Succesful syscall");
  }

  exit(0);
}
