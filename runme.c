#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

static const char* name = "runme";
static void errln(const char* format, ...) {
    va_list args;

    fprintf(stderr, "%s: ", name);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static bool parse_args(int argc, char** argv) {
    if (argc < 1 || argv[0] == NULL) {
        errln("missing argv[0]");
        return false;
    }
    name = argv[0];

    if (argc != 1) {
        errln(
            "expected no arguments, got %d",
            argc - 1
        );
        return false;
    }

    return true;
}

static bool check_host(void) {
    struct utsname uts;
    if (uname(&uts) != 0) {
        errln("uname failed");
        return false;
    }

    if (strcmp(uts.machine, "x86_64") != 0) {
        errln(
            "we only support x86_64 hosts for now, not '%s'",
            uts.machine
        );
        return false;
    }
    if (strcmp(uts.sysname, "Linux") != 0) {
        errln(
            "we only support Linux hosts for now, not '%s'",
            uts.sysname
        );
        return false;
    }

    return true;
}

int main(int argc, char** argv) {
    if (!parse_args(argc, argv))
        return EXIT_FAILURE;
    if (!check_host())
        return EXIT_FAILURE;

    puts("OK");
    return EXIT_SUCCESS;
}
