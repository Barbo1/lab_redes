#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef uint8_t mitipo;

void show(mitipo c) {
  for (int i = 8 * sizeof(mitipo) - 1; i > -1; i--) {
    printf("%d", (bool)(c & 1 << i));
  }
  printf("\n");
}

int main () {
  mitipo a = 0xF3;
  mitipo b = 0x21;
  mitipo c = a + b + (a & 0x8000 || b & 0x8000);
  show (a);
  show (b);
  printf("----------------\n");
  show (c);
}
