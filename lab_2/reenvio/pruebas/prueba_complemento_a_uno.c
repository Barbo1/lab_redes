#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

uint16_t ones_comp_sum (uint16_t a, uint16_t b) {
  return 
}

int main () {
  uint16_t a = 0xF3A1;
  uint16_t b = 0x2155;
  uint16_t c = ones_comp_sum (a, b);
  for (int i = 16; i > -1; i--) {
    printf("%d", (bool)(c & 1 << i));
  }
  printf("\n");
}
