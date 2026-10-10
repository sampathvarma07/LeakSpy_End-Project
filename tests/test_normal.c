#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    int *numbers = malloc(10 * sizeof(int));

    if (numbers == NULL) {
        printf("Memory allocation failed.\n");
        return 1;
    }

    for (int i = 0; i < 10; i++) {
        numbers[i] = i + 1;
    }

    printf("Allocated memory: %zu bytes\n",
           10 * sizeof(int));
    printf("First value: %d\n", numbers[0]);
    printf("Last value: %d\n", numbers[9]);

    free(numbers);

    printf("Memory freed successfully.\n");

    return 0;
}
