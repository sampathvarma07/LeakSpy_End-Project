#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    volatile size_t count = SIZE_MAX / 2 + 1;
    volatile size_t size = 2;

    void *ptr = calloc(count, size);

    if (ptr == NULL) {
        printf("Overflow allocation rejected as expected.\n");
        return 0;
    }

    printf("Unexpectedly, calloc succeeded.\n");
    free(ptr);
    return 1;
}
