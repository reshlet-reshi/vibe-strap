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

static bool is_fd_open(int fd) {
    errno = 0;
    if (fcntl(fd, F_GETFD) >= 0)
        return true;

    return errno != EBADF;
}

static bool has_standard_fds(void) {
    bool has_fds = true;

    if (!is_fd_open(STDIN_FILENO)) {
        errln(
            "standard fd %s (%d) is not open",
            "STDIN_FILENO",
            STDIN_FILENO
        );
        has_fds = false;
    }

    if (!is_fd_open(STDOUT_FILENO)) {
        errln(
            "standard fd %s (%d) is not open",
            "STDOUT_FILENO",
            STDOUT_FILENO
        );
        has_fds = false;
    }

    if (!is_fd_open(STDERR_FILENO)) {
        errln(
            "standard fd %s (%d) is not open",
            "STDERR_FILENO",
            STDERR_FILENO
        );
        has_fds = false;
    }

    return has_fds;
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

enum whined {
    did_not_whine,
    whined,
};

static enum whined whine_if_not_fs_blob(const char* path) {
    if (!path) {
        errln("internal error: null path passed to whine_if_not_fs_blob");
        return whined;
    }

    int error;
    enum fs_blob_status stat = fs_blob_status(path, &error);

    if (stat == fs_blob_exists)
        return did_not_whine;

    if (stat == fs_blob_stat_error) {
        errln("stat('%s') failed: %s", path, strerror(error));
    } else if (stat == fs_blob_wrong_mode) {
        errln("'%s' is not a regular file", path);
    } else {
        errln(
            "internal error: unexpected fs_blob_status '%d'.\n"
            "from: whine_if_not_fs_blob",
            stat
        );
    }

    return whined;
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

    if (whine_if_not_fs_blob("./.gitignore") == whined)
        return false;

    return file_has_exact_content("./.gitignore", expected);
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

static bool copy_fd_to_target(
    const char* value,
    int fd_value,
    const char* target,
    int fd_target
) {
    if (fd_value == fd_target)
        return true;

    if (dup2(fd_value, fd_target) < 0) {
        int e = errno;
        errln(
            "dup2(%s (%d), %s (%d)) failed: %s",
            value,
            fd_value,
            target,
            fd_target,
            strerror(e)
        );
        return false;
    }

    return true;
}

#define COPY_FD_TO_TARGET(value, target) copy_fd_to_target(#value, value, #target, target)

static bool command_succeeds(char* const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        int e = errno;
        errln("fork failed: %s", strerror(e));
        return false;
    }

    if (pid == 0) {
        if (!has_standard_fds())
            _exit(EXIT_FAILURE);

        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd < 0) {
            errln("failed to open '/dev/null'");
            _exit(EXIT_FAILURE);
        }

        // NOTE this check is mostly redundant, since we
        //  check has_standard_fds above, so null_fd >= 3
        //  at this point. But, in esoteric multi threading/etc cases,
        //  we could get surprised, so check anyway.
        if (is_standard_fd(null_fd)) {
            errln("'/dev/null' opened as standard fd");
            _exit(EXIT_FAILURE);
        }

        if (!COPY_FD_TO_TARGET(null_fd, STDOUT_FILENO))
            _exit(EXIT_FAILURE);

        if (!COPY_FD_TO_TARGET(null_fd, STDERR_FILENO))
            _exit(EXIT_FAILURE);

        // this is safe because we know it is not a standard fd
        close(null_fd);

        execvp(argv[0], argv);
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
        errln("waitpid failed: %s", strerror(e));
        return false;
    }

    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code == 0)
            return true;

        errln("'%s' exited with status %d", argv[0], code);
        return false;
    }

    if (WIFSIGNALED(status)) {
        errln("'%s' was killed by signal %d", argv[0], WTERMSIG(status));
        return false;
    }

    errln("'%s' ended unexpectedly", argv[0]);
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
        errln(
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
        errln("mkdir('%s') failed: %s", path, strerror(e));
        return false;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        e = errno;
        errln("stat('%s') failed: %s", path, strerror(e));
        return false;
    }

    if (!S_ISDIR(st.st_mode)) {
        errln("'%s' is not a directory", path);
        return false;
    }

    return true;
}

int main(int argc, char** argv) {
    if (!parse_args(argc, argv))
        return EXIT_FAILURE;
    if (!check_host())
        return EXIT_FAILURE;

    if (!has_standard_fds())
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

    if (whine_if_not_fs_blob("./runme") == whined)
        return EXIT_FAILURE;
    if (!same_file("/proc/self/exe", "./runme"))
        return EXIT_FAILURE;

    if (whine_if_not_fs_blob("./runme.c") == whined)
        return EXIT_FAILURE;

    if (!expected_gitignore_exists())
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
