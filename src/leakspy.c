
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include <inttypes.h>
#include <signal.h>

#define REPORT_PATH "reports/latest.log"
#define MAPS_PATH "reports/latest.maps"

static const char *test_names[] = {
    "test_leak",
    "test_normal",
    "test_calloc",
    "test_realloc"
};

static const char *test_descriptions[] = {
    "Intentional memory leak",
    "Normal allocation and free",
    "calloc allocation test",
    "realloc test"
};

static int read_choice(int *choice)
{
    if (scanf("%d", choice) == 1)
        return 1;

    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }

    return 0;
}

static void wait_for_enter(void)
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}

static void show_report_file(const char *path, const char *title)
{
    FILE *file = fopen(path, "r");

    if (!file) {
        printf("\nCannot open report: %s\n", path);
        perror("Reason");
        return;
    }

    char line[1024];
    printf("\n========== %s ==========\n", title);

    while (fgets(line, sizeof(line), file))
        fputs(line, stdout);

    if (ferror(file))
        perror("Error reading report");

    fclose(file);
}

static void display_report(
    const char *report_path,
    const char *symbolizer_path,
    const char *program_path,
    const char *maps_path);

static void show_previous_report(void)
{
    char project_path[PATH_MAX];
    char symbolizer_path[PATH_MAX];
    char program_path[PATH_MAX];
    char report_path[PATH_MAX];
    char maps_path[PATH_MAX];

    if (!getcwd(project_path, sizeof(project_path))) {
        perror("getcwd");
        return;
    }

    if (snprintf(symbolizer_path, sizeof(symbolizer_path),
                 "%s/leakspy-symbolize", project_path) >= (int)sizeof(symbolizer_path) ||
        snprintf(program_path, sizeof(program_path),
                 "%s/tests/test_leak", project_path) >= (int)sizeof(program_path) ||
        snprintf(report_path, sizeof(report_path),
                 "%s/%s", project_path, REPORT_PATH) >= (int)sizeof(report_path) ||
        snprintf(maps_path, sizeof(maps_path),
                 "%s/%s", project_path, MAPS_PATH) >= (int)sizeof(maps_path)) {
        fprintf(stderr, "Project path is too long.\\n");
        return;
    }

    display_report(report_path, symbolizer_path, program_path, maps_path);
}

static void analyze_existing_log(void)
{
    char path[PATH_MAX];

    printf("\n========== Analyze Existing Log ==========\n");
    printf("Enter log path (Enter for reports/latest.log):\n> ");
    fflush(stdout);

    wait_for_enter();

    if (!fgets(path, sizeof(path), stdin)) {
        printf("Could not read the path.\n");
        return;
    }

    path[strcspn(path, "\r\n")] = '\0';

    int use_latest_report = (path[0] == '\0');

    if (use_latest_report)
        snprintf(path, sizeof(path), "%s", REPORT_PATH);

    struct stat info;

    if (stat(path, &info) != 0) {
        perror("Log file not found");
        return;
    }

    if (!S_ISREG(info.st_mode)) {
        printf("Please provide a regular file.\n");
        return;
    }

    if (!use_latest_report) {
        show_report_file(path, "Log Analysis");
        return;
    }

    char project_path[PATH_MAX];
    char symbolizer_path[PATH_MAX];
    char program_path[PATH_MAX];
    char report_path[PATH_MAX];
    char maps_path[PATH_MAX];

    if (!getcwd(project_path, sizeof(project_path))) {
        perror("getcwd");
        return;
    }

    if (snprintf(symbolizer_path, sizeof(symbolizer_path),
                 "%s/leakspy-symbolize", project_path) >= (int)sizeof(symbolizer_path) ||
        snprintf(program_path, sizeof(program_path),
                 "%s/tests/test_leak", project_path) >= (int)sizeof(program_path) ||
        snprintf(report_path, sizeof(report_path),
                 "%s/%s", project_path, REPORT_PATH) >= (int)sizeof(report_path) ||
        snprintf(maps_path, sizeof(maps_path),
                 "%s/%s", project_path, MAPS_PATH) >= (int)sizeof(maps_path)) {
        fprintf(stderr, "Project path is too long.\\n");
        return;
    }

    display_report(report_path, symbolizer_path, program_path, maps_path);
}

static void show_settings(void)
{
    int choice;

    while (1) {
        printf("\n========== LeakSpy Settings ==========\n");
        printf("1. View report file location\n");
        printf("2. View available test programs\n");
        printf("3. Return to main menu\n");
        printf("Enter your choice: ");
        fflush(stdout);

        if (!read_choice(&choice)) {
            printf("Invalid input. Enter a number.\n");
            continue;
        }

        switch (choice) {
            case 1:
                printf("\nLatest report: %s\n", REPORT_PATH);
                printf("Latest process mappings: %s\n", MAPS_PATH);
                break;

            case 2:
                printf("\nAvailable test programs:\n");

                for (int i = 0; i < 4; i++) {
                    printf("%d. %-12s - %s\n",
                           i + 1,
                           test_names[i],
                           test_descriptions[i]);
                }
                break;

            case 3:
                return;

            default:
                printf("Invalid choice. Select 1 to 3.\n");
        }
    }
}

/*
 * Mapping lookup:
 * start-end perms offset ... pathname
 *
 * Return the pathname and object-relative address for a frame.
 */
static int find_mapping(
    const char *maps_path,
    uintptr_t address,
    char *object_path,
    size_t object_path_size,
    uintptr_t *object_address)
{
    FILE *file = fopen(maps_path, "r");

    if (!file)
        return 0;

    char line[PATH_MAX + 256];
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        unsigned long long start;
        unsigned long long end;
        unsigned long long offset;
        char permissions[8] = {0};
        char mapped_path[PATH_MAX] = {0};

        int fields = sscanf(
            line,
            "%llx-%llx %7s %llx %*s %*s %4095[^\n]",
            &start, &end, permissions, &offset, mapped_path
        );

        if (fields < 4)
            continue;

        if ((unsigned long long)address < start ||
            (unsigned long long)address >= end)
            continue;

        if (fields < 5 || mapped_path[0] != '/') {
            break;
        }

        mapped_path[strcspn(mapped_path, "\r\n")] = '\0';

        if (snprintf(object_path, object_path_size, "%s",
                     mapped_path) >= (int)object_path_size) {
            break;
        }

        *object_address =
            address - (uintptr_t)start + (uintptr_t)offset;

        found = 1;
        break;
    }

    fclose(file);
    return found;
}

static void symbolize_address(
    const char *symbolizer_path,
    const char *program_path,
    const char *maps_path,
    const char *address_text)
{
    uintptr_t address;
    char *end = NULL;

    errno = 0;
    unsigned long long parsed = strtoull(address_text, &end, 0);

    if (errno != 0 || end == address_text || *end != '\0') {
        printf("      Source location: invalid address\n");
        return;
    }

    address = (uintptr_t)parsed;

    char object_path[PATH_MAX] = {0};
    uintptr_t object_address = address;

    int has_mapping = find_mapping(
        maps_path, address,
        object_path, sizeof(object_path),
        &object_address
    );

    const char *selected_object =
        has_mapping ? object_path : program_path;

    uintptr_t selected_address =
        has_mapping ? object_address : address;

    char resolved_address[64];

    snprintf(resolved_address, sizeof(resolved_address),
             "0x%" PRIxPTR, selected_address);

    int pipefd[2];

    if (pipe(pipefd) < 0) {
        perror("pipe");
        return;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid == 0) {
        close(pipefd[0]);

        if (dup2(pipefd[1], STDOUT_FILENO) < 0)
            _exit(1);

        close(pipefd[1]);

        execl(symbolizer_path, symbolizer_path,
              selected_object, resolved_address, (char *)NULL);

        _exit(127);
    }

    close(pipefd[1]);

    char output[1024];
    ssize_t count;

    printf("      Object: %s\n", selected_object);
    printf("      Source location:\n");

    while ((count = read(pipefd[0], output,
                         sizeof(output) - 1)) > 0) {
        output[count] = '\0';
        printf("        %s", output);
    }

    close(pipefd[0]);

    int status;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;

        perror("waitpid");
        break;
    }
}

static void display_report(
    const char *report_path,
    const char *symbolizer_path,
    const char *program_path,
    const char *maps_path)
{
    FILE *file = fopen(report_path, "r");

    if (!file) {
        perror("Could not open LeakSpy report");
        return;
    }

    char line[1024];

    printf("\n========== LeakSpy Report with Source Locations ==========\n");

    while (fgets(line, sizeof(line), file)) {
        fputs(line, stdout);

        int frame_number;
        void *address;

        if (sscanf(line, " frame %d: %p",
                   &frame_number, &address) == 2) {
            char address_text[64];

            snprintf(address_text, sizeof(address_text),
                     "%p", address);

            symbolize_address(symbolizer_path,
                              program_path,
                              maps_path,
                              address_text);
        }
    }

    fclose(file);
}

static void run_test(void)
{
    int choice;

    printf("\n========== Select a Test ==========\n");

    for (int i = 0; i < 4; i++) {
        printf("%d. %s - %s\n",
               i + 1, test_names[i], test_descriptions[i]);
    }

    printf("5. Back to Main Menu\n");
    printf("Enter your choice: ");
    fflush(stdout);

    if (!read_choice(&choice)) {
        printf("Invalid input.\n");
        return;
    }

    if (choice == 5)
        return;

    if (choice < 1 || choice > 4) {
        printf("Invalid choice. Select 1 to 5.\n");
        return;
    }

    char project_path[PATH_MAX];

    if (!getcwd(project_path, sizeof(project_path))) {
        perror("getcwd");
        return;
    }

    char library_path[PATH_MAX];
    char program_path[PATH_MAX];
    char symbolizer_path[PATH_MAX];
    char report_path[PATH_MAX];
    char maps_path[PATH_MAX];

    if (snprintf(library_path, sizeof(library_path),
                 "%s/leakspy.so", project_path) >= (int)sizeof(library_path) ||
        snprintf(program_path, sizeof(program_path),
                 "%s/tests/%s", project_path,
                 test_names[choice - 1]) >= (int)sizeof(program_path) ||
        snprintf(symbolizer_path, sizeof(symbolizer_path),
                 "%s/leakspy-symbolize", project_path) >= (int)sizeof(symbolizer_path) ||
        snprintf(report_path, sizeof(report_path),
                 "%s/%s", project_path, REPORT_PATH) >= (int)sizeof(report_path) ||
        snprintf(maps_path, sizeof(maps_path),
                 "%s/%s", project_path, MAPS_PATH) >= (int)sizeof(maps_path)) {
        fprintf(stderr, "Project path is too long.\n");
        return;
    }

    if (access(library_path, R_OK) != 0 ||
        access(program_path, X_OK) != 0 ||
        access(symbolizer_path, X_OK) != 0) {
        printf("Required files are missing. Run 'make' first.\n");
        return;
    }

    int log_fd = open(report_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (log_fd < 0) {
        perror("Could not create report log");
        return;
    }

    int maps_fd = open(maps_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (maps_fd < 0) {
        perror("Could not create mappings file");
        close(log_fd);
        return;
    }

    printf("\nStarting %s...\n\n", test_names[choice - 1]);
    fflush(NULL);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        close(log_fd);
        close(maps_fd);
        return;
    }

    if (pid == 0) {
        close(maps_fd);

        if (dup2(log_fd, STDERR_FILENO) < 0)
            _exit(1);

        close(log_fd);

        /*
         * The child is the test process after exec.
         * Its constructor in the preload library snapshots /proc/self/maps.
         */
        if (setenv("LEAKSPY_MAPS_PATH", maps_path, 1) != 0)
            _exit(1);

        if (setenv("LD_PRELOAD", library_path, 1) != 0)
            _exit(1);

        execl(program_path, program_path, (char *)NULL);
        perror("execl");
        _exit(127);
    }

    close(log_fd);
    close(maps_fd);

    int status;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;

        perror("waitpid");
        return;
    }

    display_report(report_path, symbolizer_path,
                   program_path, maps_path);

    if (WIFEXITED(status))
        printf("\nTarget exited with status %d.\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        printf("\nTarget terminated by signal %d.\n", WTERMSIG(status));
}

static void show_menu(void)
{
    printf("\n========================================\n");
    printf("        LeakSpy Memory Profiler         \n");
    printf("========================================\n");
    printf("1. Run a program\n");
    printf("2. Analyze an existing log\n");
    printf("3. View previous reports\n");
    printf("4. Settings\n");
    printf("5. Help\n");
    printf("6. Exit\n");
}

int main(void)
{
    int choice;

    while (1) {
        show_menu();
        printf("\nEnter your choice: ");
        fflush(stdout);

        if (!read_choice(&choice)) {
            printf("Please enter a valid number.\n");
            continue;
        }

        switch (choice) {
            case 1:
                run_test();
                break;

            case 2:
                analyze_existing_log();
                break;

            case 3:
                show_previous_report();
                break;

            case 4:
                show_settings();
                break;

            case 5:
                printf("\nLeakSpy detects memory leaks in C programs.\n");
                printf("It intercepts allocation calls using LD_PRELOAD.\n");
                printf("Reports are stored in reports/latest.log.\n");
                break;

            case 6:
                printf("\nExiting LeakSpy. Goodbye!\n");
                return 0;

            default:
                printf("\nInvalid choice. Select 1 to 6.\n");
        }

        printf("\nPress Enter to return to the menu...");
        fflush(stdout);

        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
        }

        if (ch == EOF)
            break;
    }

    return 0;
}
