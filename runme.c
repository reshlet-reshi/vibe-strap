#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

static const char *name = "runme";
static void errln(const char *format, ...) {
    va_list args;

    fprintf(stderr, "%s: ", name);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

int main(int argc, char **argv) {
    if (argc < 1 || argv[0] == NULL) {
        errln("missing argv[0]");
        return 1;
    }
    name = argv[0];

    if (argc != 1) {
        errln(
            "expected no arguments, got %d",
            argc - 1
        );
        return 1;
    }

    struct utsname uts;
    if (uname(&uts) != 0) {
        errln("uname failed");
        return 1;
    }

    if (strcmp(uts.machine, "x86_64") != 0) {
        errln(
            "we only support x86_64 hosts for now, not '%s'",
            uts.machine
        );
        return 1;
    }
    if (strcmp(uts.sysname, "Linux") != 0) {
        errln(
            "we only support Linux hosts for now, not '%s'",
            uts.sysname
        );
        return 1;
    }

    puts("OK");
    return 0;
}
