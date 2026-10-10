#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    int *numbers = malloc(10 * sizeof(int));

    if (numbers == NULL) {
        printf("Initial allocation failed.\n");
        return 1;
    }

    for (int i = 0; i < 10; i++) {
        numbers[i] = i + 1;
    }

    printf("Initial allocation: 40 bytes\n");

    volatile size_t request_size = (size_t)PTRDIFF_MAX + 1;
    void *resized = realloc(numbers, request_size);

    if (resized == NULL) {
        printf("Reallocation failed as expected.\n");
        printf("Original allocation is still valid: %d\n",
               numbers[0]);

        /* Intentionally leave the original allocation unfreed. */
        return 0;
    }

    printf("Unexpectedly, reallocation succeeded.\n");
    free(resized);
    return 2;
}
