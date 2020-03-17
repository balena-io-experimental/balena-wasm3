/* Adapted from https://github.com/WebAssembly/wabt/blob/master/wasm2c/examples/fac/main.c */
#include <stdio.h>
#include <stdlib.h>
#include "wasm-rt.h"
#include "increment.h"

int main(int argc, char **argv)
{
  
  /* Make sure there is at least 2 command-line arguments. */
  if (argc < 3) return 1;
  /* the target value to increment */
  u32 value = atoi(argv[1]);
  /* where to store in wasm linear memory */
  u32 location = atoi(argv[2]);

  /* Initialize the module */
  init();

  Z_memory->data[location] = value;

 u32 result = Z_loadAndIncrementZ_ii(location);
 /* Print the result. */
  printf("%u\n", result);


  return 0;
}