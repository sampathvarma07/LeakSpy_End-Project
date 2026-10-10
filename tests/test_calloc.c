#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    int *numbers = calloc(10, sizeof(int));

    if (numbers == NULL) {
        printf("Memory allocation failed.\n");
        return 1;
    }

    printf("Allocated memory: %zu bytes\n",
           10 * sizeof(int));
    printf("Initial value: %d\n", numbers[0]);

    /* Intentionally leave the allocation unfreed. */

    return 0;
}
