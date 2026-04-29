#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static bool same_file(const char* a, const char* b) {
    struct stat a_stat;
    struct stat b_stat;

    if (stat(a, &a_stat) != 0) {
        int e = errno;
        errln("stat('%s') failed: %s", a, strerror(e));
        return false;
    }

    if (stat(b, &b_stat) != 0) {
        int e = errno;
        errln("stat('%s') failed: %s", b, strerror(e));
        return false;
    }

    if (a_stat.st_dev != b_stat.st_dev || a_stat.st_ino != b_stat.st_ino) {
        errln("'%s' and '%s' are not the same file", a, b);
        return false;
    }

    return true;
}

static bool regular_file_exists(const char* path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        int e = errno;
        errln("stat('%s') failed: %s", path, strerror(e));
        return false;
    }

    if (!S_ISREG(st.st_mode)) {
        errln("'%s' is not a regular file", path);
        return false;
    }

    return true;
}

static bool file_has_exact_content(const char* path, const char* expected) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        int e = errno;
        errln("fopen('%s') failed: %s", path, strerror(e));
        return false;
    }

    for (size_t i = 0; expected[i] != '\0'; ++i) {
        int ch = fgetc(file);
        if (ch == EOF) {
            if (ferror(file)) {
                int e = errno;
                errln("fgetc('%s') failed: %s", path, strerror(e));
            } else {
                errln("'%s' ended before expected content", path);
            }
            fclose(file);
            return false;
        }

        if ((char)ch != expected[i]) {
            errln("'%s' does not contain the expected content", path);
            fclose(file);
            return false;
        }
    }

    int ch = fgetc(file);
    if (ch != EOF) {
        errln("'%s' has unexpected extra content", path);
        fclose(file);
        return false;
    }
    if (ferror(file)) {
        int e = errno;
        errln("fgetc('%s') failed: %s", path, strerror(e));
        fclose(file);
        return false;
    }

    if (fclose(file) != 0) {
        int e = errno;
        errln("fclose('%s') failed: %s", path, strerror(e));
        return false;
    }

    return true;
}

static bool expected_gitignore_exists(void) {
    static const char expected[] =
        ".ignore/\n"
        "runme\n"
        ".codex\n";

    if (!regular_file_exists("./.gitignore"))
        return false;

    return file_has_exact_content("./.gitignore", expected);
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

    if (!regular_file_exists("./runme"))
        return EXIT_FAILURE;
    if (!same_file("/proc/self/exe", "./runme"))
        return EXIT_FAILURE;

    if (!regular_file_exists("./runme.c"))
        return EXIT_FAILURE;
    if (!expected_gitignore_exists())
        return EXIT_FAILURE;

    puts("OK");
    return EXIT_SUCCESS;
}
