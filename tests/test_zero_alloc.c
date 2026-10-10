#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    void *zero_malloc = malloc(0);

    if (zero_malloc != NULL) {
        printf("malloc(0) returned a valid pointer.\n");
        free(zero_malloc);
    } else {
        printf("malloc(0) returned NULL.\n");
    }

    int *numbers = malloc(10 * sizeof(int));

    if (numbers == NULL) {
        printf("Initial allocation failed.\n");
        return 1;
    }

    numbers[0] = 42;

    void *result = realloc(numbers, 0);

    printf("realloc(ptr, 0) returned %s.\n",
           result == NULL ? "NULL" : "a non-NULL pointer");

    /*
     * On Linux/glibc, realloc(ptr, 0) normally frees the original
     * allocation. Do not access or free numbers afterward.
     */
    return 0;
}
