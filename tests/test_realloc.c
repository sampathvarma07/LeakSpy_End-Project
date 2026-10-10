#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    int *numbers = malloc(5 * sizeof(int));

    if (numbers == NULL) {
        printf("Initial allocation failed.\n");
        return 1;
    }

    for (int i = 0; i < 5; i++) {
        numbers[i] = i + 1;
    }

    printf("Initial allocation: %zu bytes\n",
           5 * sizeof(int));

    int *resized = realloc(numbers, 10 * sizeof(int));

    if (resized == NULL) {
        free(numbers);
        printf("Reallocation failed.\n");
        return 1;
    }

    numbers = resized;

    printf("Resized allocation: %zu bytes\n",
           10 * sizeof(int));
    printf("Preserved value: %d\n", numbers[0]);

    /* Intentionally leave the resized allocation unfreed. */

    return 0;
}
