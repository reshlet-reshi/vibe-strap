#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

static const char* our_name = "runme";

static void whine(const char* format, ...) {
    va_list args;

    fprintf(stderr, "%s: ", our_name);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

enum whined {
    did_not_whine,
    did_whine,
};

static enum whined parse_args_or_whine(int argc, char** argv) {
    if (argc < 1 || argv[0] == NULL) {
        whine("missing program name");
        return did_whine;
    }
    our_name = argv[0];

    if (argc != 1) {
        whine(
            "expected no arguments, got %d",
            argc - 1
        );
        return did_whine;
    }

    return did_not_whine;
}

static enum whined whine_if_host_unsupported(void) {
    struct utsname uts;
    if (uname(&uts) != 0) {
        whine("uname failed");
        return did_whine;
    }

    if (strcmp(uts.machine, "x86_64") != 0) {
        whine(
            "we only support x86_64 hosts for now, not '%s'",
            uts.machine
        );
        return did_whine;
    }
    if (strcmp(uts.sysname, "Linux") != 0) {
        whine(
            "we only support Linux hosts for now, not '%s'",
            uts.sysname
        );
        return did_whine;
    }

    return did_not_whine;
}

static bool is_fd_open(int fd) {
    errno = 0;
    if (fcntl(fd, F_GETFD) >= 0)
        return true;

    return errno != EBADF;
}

static enum whined whine_if_standard_fd_missing(void) {
    enum whined whined = did_not_whine;

    if (!is_fd_open(STDIN_FILENO)) {
        whine(
            "standard fd %s (%d) is not open",
            "STDIN_FILENO",
            STDIN_FILENO
        );
        whined = did_whine;
    }

    if (!is_fd_open(STDOUT_FILENO)) {
        whine(
            "standard fd %s (%d) is not open",
            "STDOUT_FILENO",
            STDOUT_FILENO
        );
        whined = did_whine;
    }

    if (!is_fd_open(STDERR_FILENO)) {
        whine(
            "standard fd %s (%d) is not open",
            "STDERR_FILENO",
            STDERR_FILENO
        );
        whined = did_whine;
    }

    return whined;
}

static enum whined cd_to_self_or_whine(char* buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size <= 1) {
        whine(
            "internal error: invalid buffer passed to cd_to_self_or_whine"
        );
        return did_whine;
    }

    // readlink does not null terminate,
    //  so reserve one byte in the buffer.
    size_t read_size = buffer_size - 1;

    static const char* link = "/proc/self/exe";
    ssize_t length = readlink(link, buffer, read_size);

    if (length < 0) {
        int e = errno;
        whine(
            "readlink(%s) failed: %s",
            link,
            strerror(e)
        );
        return did_whine;
    }

    if ((size_t)length >= read_size) {
        whine(
            "readlink(%s) returned truncated path",
            link
        );
        return did_whine;
    }

    if (length == 0) {
        whine(
            "%s resolved to an empty path",
            link
        );
        return did_whine;
    }

    char* last_slash = &buffer[length - 1];
    while (last_slash > buffer) {
        if (*last_slash == '/')
            break;
        --last_slash;
    }

    if (*last_slash != '/') {
        // null terminate for whine
        buffer[length] = '\0';

        whine(
            "%s path has no directory component: '%s'",
            link,
            buffer
        );

        return did_whine;
    }

    // drop the basename
    *(last_slash + 1) = '\0';

    if (chdir(buffer) != 0) {
        int e = errno;
        whine(
            "chdir('%s') failed: %s",
            buffer,
            strerror(e)
        );
        return did_whine;
    }

    return did_not_whine;
}

static enum whined whine_if_distinct_files(const char* a, const char* b) {
    struct stat a_stat;
    if (stat(a, &a_stat) != 0) {
        int e = errno;
        whine("stat('%s') failed: %s", a, strerror(e));
        return did_whine;
    }

    struct stat b_stat;
    if (stat(b, &b_stat) != 0) {
        int e = errno;
        whine("stat('%s') failed: %s", b, strerror(e));
        return did_whine;
    }

    if (a_stat.st_dev != b_stat.st_dev || a_stat.st_ino != b_stat.st_ino) {
        whine("'%s' and '%s' are not the same file", a, b);
        return did_whine;
    }

    return did_not_whine;
}

enum fs_blob_status {
    fs_blob_0,

    fs_blob_null_path,
    fs_blob_null_error_pointer,

    fs_blob_exists,
    fs_blob_stat_error,
    fs_blob_wrong_mode,
};

static enum fs_blob_status fs_blob_status(
    const char* path,
    int* p_error
) {
    if (!path)
        return fs_blob_null_path;

    if (!p_error)
        return fs_blob_null_error_pointer;

    struct stat st;
    if (stat(path, &st) != 0) {
        *p_error = errno;
        return fs_blob_stat_error;
    }

    if (!S_ISREG(st.st_mode))
        return fs_blob_wrong_mode;

    return fs_blob_exists;
}

static enum whined whine_if_not_fs_blob(const char* path) {
    if (!path) {
        whine("internal error: null path passed to whine_if_not_fs_blob");
        return did_whine;
    }

    int error;
    enum fs_blob_status blob_stat = fs_blob_status(path, &error);

    if (blob_stat == fs_blob_exists)
        return did_not_whine;

    if (blob_stat == fs_blob_stat_error) {
        whine("stat('%s') failed: %s", path, strerror(error));
    } else if (blob_stat == fs_blob_wrong_mode) {
        whine("'%s' is not a regular file", path);
    } else {
        whine(
            "internal error: unexpected fs_blob_status '%d'.\n"
            "from: whine_if_not_fs_blob",
            blob_stat
        );
    }

    return did_whine;
}

static enum whined whine_if_wrong_text_in_file(
    const char* path, 
    FILE* file, 
    const char* expected
) {
    for (size_t i = 0; expected[i] != '\0'; ++i) {
        int ch = fgetc(file);
        if (ch == EOF) {
            if (ferror(file)) {
                int e = errno;
                whine("fgetc('%s') failed: %s", path, strerror(e));
            } else {
                whine("'%s' ended before expected content", path);
            }
            return did_whine;
        }

        if ((char)ch != expected[i]) {
            whine("'%s' does not contain the expected content", path);
            return did_whine;
        }
    }

    int ch = fgetc(file);
    if (ch != EOF) {
        whine("'%s' has unexpected extra content", path);
        return did_whine;
    }
    if (ferror(file)) {
        int e = errno;
        whine("fgetc('%s') failed: %s", path, strerror(e));
        return did_whine;
    }

    return did_not_whine;
}

static enum whined whine_if_wrong_text_at_path(const char* path, const char* expected) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        int e = errno;
        whine("fopen('%s') failed: %s", path, strerror(e));
        return did_whine;
    }

    enum whined whined = whine_if_wrong_text_in_file(
        path,
        file,
        expected
    );

    if (fclose(file) != 0) {
        int e = errno;
        whine("fclose('%s') failed: %s", path, strerror(e));
        return did_whine;
    }

    return whined;
}

static bool whine_if_wrong_gitignore(void) {
    static const char expected[] =
        ".ignore/\n"
        "runme\n"
        ".codex\n";

    if (whine_if_not_fs_blob("./.gitignore") == did_whine)
        return did_whine;

    return whine_if_wrong_text_at_path("./.gitignore", expected);
}

static bool is_standard_fd(int fd) {
    if (fd == STDIN_FILENO)
        return true;
    if (fd == STDOUT_FILENO)
        return true;
    if (fd == STDERR_FILENO)
        return true;
    return false;
}

static enum whined copy_fd_to_target_or_whine(
    const char* value,
    int fd_value,
    const char* target,
    int fd_target
) {
    if (fd_value == fd_target)
        return did_not_whine;

    if (dup2(fd_value, fd_target) < 0) {
        int e = errno;
        whine(
            "dup2(%s (%d), %s (%d)) failed: %s",
            value,
            fd_value,
            target,
            fd_target,
            strerror(e)
        );
        return did_whine;
    }

    return did_not_whine;
}

#define COPY_FD_TO_TARGET_OR_WHINE(value, target) \
    copy_fd_to_target_or_whine(#value, value, #target, target)

static bool command_succeeds(char* const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        int e = errno;
        whine("fork failed: %s", strerror(e));
        return false;
    }

    if (pid == 0) {
        if (whine_if_standard_fd_missing() == did_whine)
            _exit(EXIT_FAILURE);

        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd < 0) {
            whine("failed to open '/dev/null'");
            _exit(EXIT_FAILURE);
        }

        // NOTE this check is mostly redundant, since we
        //  whine_if_standard_fd_missing above, so null_fd >= 3
        //  at this point. But, in esoteric multi threading/etc cases,
        //  we could get surprised, so check anyway.
        if (is_standard_fd(null_fd)) {
            whine("'/dev/null' opened as standard fd");
            _exit(EXIT_FAILURE);
        }

        if (COPY_FD_TO_TARGET_OR_WHINE(null_fd, STDOUT_FILENO) == did_whine)
            _exit(EXIT_FAILURE);

        if (COPY_FD_TO_TARGET_OR_WHINE(null_fd, STDERR_FILENO) == did_whine)
            _exit(EXIT_FAILURE);

        // this is safe because we know it is not a standard fd
        close(null_fd);

        execvp(argv[0], argv);

        // if we get here execvp failed
        _exit(EXIT_FAILURE);
    }

    int status;
    for (;;) {
        if (waitpid(pid, &status, 0) >= 0)
            break;

        // We may get interrupted by a signal while waiting on our child.
        // If that happens, just wait again.
        if (errno == EINTR)
            continue;

        int e = errno;
        whine("waitpid failed: %s", strerror(e));
        return false;
    }

    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code == 0)
            return true;

        whine("'%s' exited with status %d", argv[0], code);
        return false;
    }

    if (WIFSIGNALED(status)) {
        whine("'%s' was killed by signal %d", argv[0], WTERMSIG(status));
        return false;
    }

    whine("'%s' ended unexpectedly", argv[0]);
    return false;
}

static bool expected_files_are_tracked(void) {
    char* const argv[] = {
        "git",
        "ls-files",
        "--error-unmatch",
        "--",
        "runme.c",
        ".gitignore",
        NULL
    };

    if (!command_succeeds(argv)) {
        whine(
            "could not verify runme.c and "
            ".gitignore are tracked by git"
        );
        return false;
    }

    return true;
}

static bool ensure_directory_exists(const char* path) {
    // mode of 0777 mimics shell
    if (mkdir(path, 0777) == 0)
        return true;

    int e = errno;
    if (e != EEXIST) {
        whine("mkdir('%s') failed: %s", path, strerror(e));
        return false;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        e = errno;
        whine("stat('%s') failed: %s", path, strerror(e));
        return false;
    }

    if (!S_ISDIR(st.st_mode)) {
        whine("'%s' is not a directory", path);
        return false;
    }

    return true;
}

int main(int argc, char** argv) {
    if (parse_args_or_whine(argc, argv) == did_whine)
        return EXIT_FAILURE;
    if (whine_if_host_unsupported() == did_whine)
        return EXIT_FAILURE;

    if (whine_if_standard_fd_missing() == did_whine)
        return EXIT_FAILURE;

    enum { buffer_size = 65536, };
    char* buffer = malloc(buffer_size);
    if (!buffer) {
        whine("out of memory");
        return EXIT_FAILURE;
    }

    enum whined cd_whined = cd_to_self_or_whine(buffer, buffer_size);
    free(buffer);
    if (cd_whined == did_whine)
        return EXIT_FAILURE;

    if (whine_if_not_fs_blob("./runme") == did_whine)
        return EXIT_FAILURE;
    if (whine_if_distinct_files("/proc/self/exe", "./runme") == did_whine)
        return EXIT_FAILURE;

    if (whine_if_not_fs_blob("./runme.c") == did_whine)
        return EXIT_FAILURE;

    if (whine_if_wrong_gitignore() == did_whine)
        return EXIT_FAILURE;

    if (!expected_files_are_tracked())
        return EXIT_FAILURE;

    if (!ensure_directory_exists("./.ignore"))
        return EXIT_FAILURE;
    if (!ensure_directory_exists("./.ignore/.cache"))
        return EXIT_FAILURE;

    puts("OK");
    return EXIT_SUCCESS;
}
