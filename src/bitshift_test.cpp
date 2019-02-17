#include <stdio.h>

int main(int argc, char* argv[]) {
  char c = 'A';

  c |= 0xF << 1;

  printf("%x\n", c);
  return 0;
}
