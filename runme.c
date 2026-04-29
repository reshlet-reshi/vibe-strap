#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

static const char* called_as = "runme";

static void errln(const char* format, ...) {
    va_list args;

    fprintf(stderr, "%s: ", called_as);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static bool parse_args(int argc, char** argv) {
    if (argc < 1 || argv[0] == NULL) {
        errln("missing program name");
        return false;
    }
    called_as = argv[0];

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

static bool cd_to_self_dir(char* buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size <= 1) {
        errln(
            "internal error: invalid buffer passed to cd_to_self_dir"
        );
        return false;
    }

    // readlink does not null terminate,
    //  so reserve one byte in the buffer.
    size_t read_size = buffer_size - 1;

    static const char* link = "/proc/self/exe";
    ssize_t length = readlink(link, buffer, read_size);

    if (length < 0) {
        int e = errno;
        errln(
            "readlink(%s) failed: %s",
            link,
            strerror(e)
        );
        return false;
    }

    if ((size_t)length >= read_size) {
        errln(
            "readlink(%s) returned truncated path",
            link
        );
        return false;
    }

    if (length == 0) {
        errln(
            "%s resolved to an empty path",
            link
        );
        return false;
    }

    char* last_slash = &buffer[length - 1];
    while (last_slash > buffer) {
        if (*last_slash == '/')
            break;
        --last_slash;
    }

    if (*last_slash != '/') {
        // null terminate for errln
        buffer[length] = '\0';

        errln(
            "%s path has no directory component: '%s'",
            link,
            buffer
        );

        return false;
    }

    // drop the basename
    *(last_slash + 1) = '\0';

    if (chdir(buffer)) {
        int e = errno;
        errln(
            "chdir('%s') failed: %s",
            buffer,
            strerror(e)
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

    enum { buffer_size = 65536, };
    char* buffer = malloc(buffer_size);
    if (!buffer) {
        errln("out of memory");
        return EXIT_FAILURE;
    }

    bool ok = cd_to_self_dir(buffer, buffer_size);
    free(buffer);
    if (!ok)
        return EXIT_FAILURE;

    puts("OK");
    return EXIT_SUCCESS;
}
