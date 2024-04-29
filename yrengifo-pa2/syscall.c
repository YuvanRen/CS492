#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#define yrengifo_syscall 451

int main(){

  char msg1[] = "The quick brown fox jumps over the lazy dog";
  printf("STR1 before: %s\n", msg1);
  
  long res1 = syscall(yrengifo_syscall, msg1);

  printf("syscall result: %ld\n", res1);
						
  printf("STR1 after: %s\n",msg1);
    
  char msg2[] = "hequicrofoxjumoverhelazydog";
  printf("STR2 before: %s\n", msg2);
  
  long res2 = syscall(yrengifo_syscall, msg2);
  printf("syscall result: %ld\n",res2);

  printf("STR2 after: %s\n", msg2);
  return 0;
}
