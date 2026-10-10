
#define _GNU_SOURCE

#include <dlfcn.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>

#define MAX_RECORDS 100000
#define MAX_FRAMES 10
#define MESSAGE_SIZE 512

typedef struct {
    void *pointer;
    size_t size;
    int active;
    int frame_count;
    void *frames[MAX_FRAMES];
} AllocationRecord;

static AllocationRecord records[MAX_RECORDS];

static void *(*real_malloc)(size_t);
static void *(*real_calloc)(size_t, size_t);
static void *(*real_realloc)(void *, size_t);
static void (*real_free)(void *);

static unsigned long allocation_count;
static unsigned long free_count;
static unsigned long dropped_records;

static int initialized;
static int initializing;
static __thread int in_backtrace;

/* Declare this before the constructor that calls it. */
static void initialize_allocator(void);

static void capture_process_maps(void)
{
    const char *destination = getenv("LEAKSPY_MAPS_PATH");

    if (destination == NULL || destination[0] == '\0')
        return;

    int output_fd = open(
        destination,
        O_WRONLY | O_CREAT | O_TRUNC,
        0644
    );

    if (output_fd < 0)
        return;

    int input_fd = open("/proc/self/maps", O_RDONLY);

    if (input_fd < 0) {
        close(output_fd);
        return;
    }

    char buffer[4096];

    for (;;) {
        ssize_t bytes_read = read(input_fd, buffer, sizeof(buffer));

        if (bytes_read == 0)
            break;

        if (bytes_read < 0)
            break;

        ssize_t offset = 0;

        while (offset < bytes_read) {
            ssize_t bytes_written = write(
                output_fd,
                buffer + offset,
                (size_t)(bytes_read - offset)
            );

            if (bytes_written <= 0) {
                close(input_fd);
                close(output_fd);
                return;
            }

            offset += bytes_written;
        }
    }

    close(input_fd);
    close(output_fd);
}

__attribute__((constructor))
static void leakspy_initialize(void)
{
    capture_process_maps();
    initialize_allocator();
}

static void initialize_allocator(void)
{
    if (initialized || initializing)
        return;

    initializing = 1;

    real_malloc = dlsym(RTLD_NEXT, "malloc");
    real_calloc = dlsym(RTLD_NEXT, "calloc");
    real_realloc = dlsym(RTLD_NEXT, "realloc");
    real_free = dlsym(RTLD_NEXT, "free");

    initialized = real_malloc && real_calloc &&
                  real_realloc && real_free;

    initializing = 0;
}

static size_t find_record(void *ptr)
{
    for (size_t i = 0; i < MAX_RECORDS; i++) {
        if (records[i].active && records[i].pointer == ptr)
            return i;
    }

    return MAX_RECORDS;
}

static void capture_stack(AllocationRecord *record)
{
    record->frame_count = 0;

    if (in_backtrace)
        return;

    void *frames[MAX_FRAMES + 4];

    in_backtrace = 1;
    int count = backtrace(frames, MAX_FRAMES + 4);
    in_backtrace = 0;

    /* Skip LeakSpy's internal frames. */
    int start = count < 3 ? count : 3;

    for (int i = start;
         i < count && record->frame_count < MAX_FRAMES;
         i++) {
        record->frames[record->frame_count++] = frames[i];
    }
}

static void record_allocation(void *ptr, size_t size)
{
    if (ptr == NULL)
        return;

    size_t i = find_record(ptr);

    if (i == MAX_RECORDS) {
        for (i = 0; i < MAX_RECORDS; i++) {
            if (!records[i].active)
                break;
        }
    }

    if (i == MAX_RECORDS) {
        dropped_records++;
        return;
    }

    records[i].pointer = ptr;
    records[i].size = size;
    records[i].active = 1;

    capture_stack(&records[i]);
}

void *malloc(size_t size)
{
    if (!initialized)
        initialize_allocator();

    if (!real_malloc)
        return NULL;

    void *ptr = real_malloc(size);

    if (ptr && initialized && !in_backtrace) {
        allocation_count++;
        record_allocation(ptr, size);
    }

    return ptr;
}

void *calloc(size_t count, size_t size)
{
    if (!initialized)
        initialize_allocator();

    if (!real_calloc)
        return NULL;

    void *ptr = real_calloc(count, size);

    if (ptr && initialized && !in_backtrace) {
        allocation_count++;

        if (count == 0 || size <= SIZE_MAX / count)
            record_allocation(ptr, count * size);
        else
            dropped_records++;
    }

    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    if (!initialized)
        initialize_allocator();

    if (!real_realloc)
        return NULL;

    if (in_backtrace)
        return real_realloc(ptr, size);

    if (ptr == NULL)
        return malloc(size);

    if (size == 0) {
        size_t old_index = find_record(ptr);
        void *result = real_realloc(ptr, size);

        if (old_index != MAX_RECORDS) {
            records[old_index].active = 0;
            free_count++;
        }

        return result;
    }

    size_t old_index = find_record(ptr);
    void *new_ptr = real_realloc(ptr, size);

    /* On failure, the original allocation remains valid. */
    if (!new_ptr)
        return NULL;

    allocation_count++;

    if (old_index != MAX_RECORDS) {
        /* Preserve the original allocation stack across realloc(). */
        records[old_index].pointer = new_ptr;
        records[old_index].size = size;
    } else {
        record_allocation(new_ptr, size);
    }

    return new_ptr;
}

void free(void *ptr)
{
    if (ptr == NULL)
        return;

    if (!initialized)
        initialize_allocator();

    if (!real_free)
        return;

    if (in_backtrace) {
        real_free(ptr);
        return;
    }

    size_t i = find_record(ptr);

    if (i != MAX_RECORDS) {
        records[i].active = 0;
        free_count++;
    }

    real_free(ptr);
}

static void write_message(const char *message, size_t size)
{
    while (size > 0) {
        ssize_t written = write(STDERR_FILENO, message, size);

        if (written <= 0)
            return;

        message += written;
        size -= (size_t)written;
    }
}

__attribute__((destructor))
static void print_summary(void)
{
    char message[MESSAGE_SIZE];

    int length = snprintf(
        message, sizeof(message),
        "\n========== LeakSpy Report ==========\n"
        "Allocation operations: %lu\n"
        "Tracked free operations: %lu\n"
        "Untracked allocations: %lu\n",
        allocation_count, free_count, dropped_records
    );

    if (length > 0) {
        size_t bytes = (size_t)length;

        if (bytes >= sizeof(message))
            bytes = sizeof(message) - 1;

        write_message(message, bytes);
    }

    if (dropped_records > 0) {
        const char warning[] =
            "WARNING: Leak tracking is incomplete. "
            "Some allocation records could not be stored.\n";

        write_message(warning, sizeof(warning) - 1);
    }

    size_t leak_count = 0;
    size_t leak_bytes = 0;

    for (size_t i = 0; i < MAX_RECORDS; i++) {
        if (!records[i].active)
            continue;

        leak_count++;
        leak_bytes += records[i].size;

        length = snprintf(
            message, sizeof(message),
            "\n[LEAK] pointer=%p size=%zu bytes\n",
            records[i].pointer, records[i].size
        );

        if (length > 0) {
            size_t bytes = (size_t)length;

            if (bytes >= sizeof(message))
                bytes = sizeof(message) - 1;

            write_message(message, bytes);
        }

        for (int j = 0; j < records[i].frame_count; j++) {
            Dl_info info = {0};

            if (dladdr(records[i].frames[j], &info) &&
                info.dli_sname != NULL) {
                length = snprintf(
                    message, sizeof(message),
                    "  frame %d: %p (%s)\n",
                    j, records[i].frames[j], info.dli_sname
                );
            } else {
                length = snprintf(
                    message, sizeof(message),
                    "  frame %d: %p (symbol unavailable)\n",
                    j, records[i].frames[j]
                );
            }

            if (length > 0) {
                size_t bytes = (size_t)length;

                if (bytes >= sizeof(message))
                    bytes = sizeof(message) - 1;

                write_message(message, bytes);
            }
        }
    }

    length = snprintf(
        message, sizeof(message),
        "\nLeaked blocks: %zu\n"
        "Leaked bytes: %zu\n"
        "====================================\n",
        leak_count, leak_bytes
    );

    if (length > 0) {
        size_t bytes = (size_t)length;

        if (bytes >= sizeof(message))
            bytes = sizeof(message) - 1;

        write_message(message, bytes);
    }
}
