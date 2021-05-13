#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
   int i = 0;

   //srand(50);
   srand(time(NULL));
   printf("Time %ld\n", time(NULL));

   for (i=0; i < 10; i++) {
      printf("%2d randNum: %d\n", i, rand() % 10);
   }
}
