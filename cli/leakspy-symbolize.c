#define _GNU_SOURCE

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int parse_address(const char *text, uintptr_t *result)
{
    errno = 0;
    char *end = NULL;
    unsigned long long value = strtoull(text, &end, 0);

    if (errno != 0 || end == text || *end != '\0')
        return 0;

    *result = (uintptr_t)value;
    return 1;
}

/*
 * src/leakspy.c currently passes file-offset-style addresses for
 * mapped objects. For ET_EXEC files, addr2line needs the executable's
 * virtual address instead. Recover the lowest PT_LOAD virtual address
 * and add it when the supplied address is an offset.
 */
static uintptr_t correct_executable_address(
    const char *path,
    uintptr_t address)
{
    int fd = open(path, O_RDONLY);

    if (fd < 0)
        return address;

    Elf64_Ehdr header;

    if (read(fd, &header, sizeof(header)) != sizeof(header) ||
        memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
        header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_type != ET_EXEC ||
        header.e_phentsize != sizeof(Elf64_Phdr) ||
        header.e_phnum == 0) {
        close(fd);
        return address;
    }

    if (lseek(fd, (off_t)header.e_phoff, SEEK_SET) < 0) {
        close(fd);
        return address;
    }

    uintptr_t lowest = UINTPTR_MAX;

    for (unsigned int i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr segment;

        if (read(fd, &segment, sizeof(segment)) != sizeof(segment))
            break;

        if (segment.p_type == PT_LOAD &&
            segment.p_vaddr < lowest) {
            lowest = (uintptr_t)segment.p_vaddr;
        }
    }

    close(fd);

    if (lowest != UINTPTR_MAX && address < lowest) {
        if (address <= UINTPTR_MAX - lowest)
            address += lowest;
    }

    return address;
}

static int run_addr2line(const char *object, uintptr_t address)
{
    char address_text[32];

    snprintf(address_text, sizeof(address_text),
             "0x%" PRIxPTR, address);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        char *args[] = {
            "addr2line",
            "-f",
            "-C",
            "-e",
            (char *)object,
            address_text,
            NULL
        };

        execvp("addr2line", args);
        perror("execvp");
        _exit(127);
    }

    int status;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;

        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return 1;
}

int main(int argc, char *argv[])
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr,
                "Usage: %s <executable-or-library> <address> "
                "[base-address]\n",
                argv[0]);
        return 1;
    }

    uintptr_t address;

    if (!parse_address(argv[2], &address)) {
        fprintf(stderr, "Invalid address: %s\n", argv[2]);
        return 1;
    }

    if (argc == 4) {
        uintptr_t base_address;

        if (!parse_address(argv[3], &base_address)) {
            fprintf(stderr, "Invalid base address: %s\n", argv[3]);
            return 1;
        }

        if (address < base_address) {
            fprintf(stderr,
                    "Address is below the supplied base address.\n");
            return 1;
        }

        address -= base_address;
    }

    address = correct_executable_address(argv[1], address);

    return run_addr2line(argv[1], address);
}
