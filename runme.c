#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "src/c/whine.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

static bool is_fd_open(int fd) {
    errno = 0;
    if (fcntl(fd, F_GETFD) >= 0)
        return true;

    return errno != EBADF;
}

static enum whined whine_if_standard_fd_missing(void) {
    RETURN_IF_WHINED(
        require(
            is_fd_open(STDIN_FILENO),
            did_whine,
            "standard fd %s (%d) is not open",
            "STDIN_FILENO",
            STDIN_FILENO
        )
    );

    RETURN_IF_WHINED(
        require(
            is_fd_open(STDOUT_FILENO),
            did_whine,
            "standard fd %s (%d) is not open",
            "STDOUT_FILENO",
            STDOUT_FILENO
        )
    );

    RETURN_IF_WHINED(
        require(
            is_fd_open(STDERR_FILENO),
            did_whine,
            "standard fd %s (%d) is not open",
            "STDERR_FILENO",
            STDERR_FILENO
        )
    );

    return did_not_whine;
}

static enum whined cd_to_self_or_whine(
    char* buffer,
    size_t buffer_size
) {
    RETURN_IF_WHINED(
        require(
            buffer != NULL && buffer_size > 1,
            did_whine,
            "internal error: "
            "invalid buffer passed to cd_to_self_or_whine"
        )
    );

    // readlink does not null terminate,
    //  so reserve one byte in the buffer.
    size_t read_size = buffer_size - 1;

    static const char* link = "/proc/self/exe";
    ssize_t length = readlink(link, buffer, read_size);
    int e = errno;

    // null terminate
    buffer[length] = '\0';

    RETURN_IF_WHINED(
        require(
            length >= 0,
            did_whine,
            "readlink(%s) failed: %s",
            link,
            strerror(e)
        )
    );

    RETURN_IF_WHINED(
        require(
            (size_t)length < read_size,
            did_whine,
            "readlink(%s) returned truncated path",
            link
        )
    );

    RETURN_IF_WHINED(
        require(
            length != 0,
            did_whine,
            "%s resolved to an empty path",
            link
        )
    );

    char* last_slash = &buffer[length - 1];
    while (last_slash > buffer) {
        if (*last_slash == '/')
            break;
        --last_slash;
    }

    RETURN_IF_WHINED(
        require(
            *last_slash == '/',
            did_whine,
            "%s path has no directory component: '%s'",
            link,
            buffer
        )
    );

    // drop the basename
    *(last_slash + 1) = '\0';

    int chdir_status = chdir(buffer);
    e = errno;
    RETURN_IF_WHINED(
        require(
            chdir_status == 0,
            did_whine,
            "chdir('%s') failed: %s",
            buffer,
            strerror(e)
        )
    );

    return did_not_whine;
}

enum {
    s_ok,
    s_open_failed,
    s_fstat_failed,
    s_not_regular_file,
};

struct result {
    int status;
    int sys_error;
    int fd;
};

static struct result is_fd_regular_file(int fd) {
    struct result result = {0};
    result.fd = -1;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        result.status = s_fstat_failed;
        result.sys_error = errno;
    } else if (!S_ISREG(st.st_mode)) {
        result.status = s_not_regular_file;
    }

    errno = 0;
    return result;
}

static struct result open_path_fd(const char* path) {
    struct result result = {0};
    result.fd = open(path, O_PATH | O_CLOEXEC);

    if (result.fd < 0) {
        result.status = s_open_failed;
        result.sys_error = errno;
    }

    errno = 0;
    return result;
}

static enum whined whine_if_not_fs_blob_path(const char* path) {
    RETURN_IF_WHINED(
        require(
            path != NULL,
            did_whine,
            "internal error: "
            "null path passed to whine_if_not_fs_blob_path"
        )
    );

    struct result opened = open_path_fd(path);
    RETURN_IF_WHINED(
        require(
            opened.status == s_ok,
            did_whine,
            "open('%s') failed: %s", 
            path, 
            strerror(opened.sys_error)
        )
    );

    int fd = opened.fd;
    enum whined whined = did_not_whine;
    struct result file = is_fd_regular_file(fd);
    switch (file.status) {
    case s_ok:
        break;

    case s_fstat_failed:
        whined = require(
            false,
            did_whine,
            "fstat('%s') failed: %s",
            path,
            strerror(file.sys_error)
        );
        break;

    case s_not_regular_file:
        whined = require(
            false,
            did_whine,
            "'%s' is not a regular file",
            path
        );
        break;
    }

    int status = close(fd);
    int e = errno;
    RETURN_IF_WHINED(
        require(
            status == 0,
            did_whine,
            "close('%s') failed: %s", 
            path, 
            strerror(e)
        )
    );

    return whined;
}

static enum whined whine_if_wrong_text_at_fd(
    const char* path,
    int fd,
    const char* expected
) {
    enum { buffer_size = 4096, };
    char buffer[buffer_size];

    off_t offset = lseek(fd, 0, SEEK_SET);
    int e = errno;
    RETURN_IF_WHINED(
        require(
            offset == 0,
            did_whine,
            "lseek('%s') failed: %s",
            path,
            strerror(e)
        )
    );

    size_t length = 0;
    size_t expected_length = strlen(expected);
    while (length < expected_length) {
        size_t remaining = expected_length - length;
        size_t chunk_size = remaining < sizeof(buffer)
            ? remaining
            : sizeof(buffer);

        ssize_t n = read(fd, buffer, chunk_size);
        if (n < 0) {
            e = errno;
            if (e == EINTR)
                continue;

            whine(
                "read('%s') failed: %s",
                path,
                strerror(e)
            );
            return did_whine;
        }

        RETURN_IF_WHINED(
            require(
                n != 0,
                did_whine,
                "'%s' ended before expected content",
                path
            )
        );

        RETURN_IF_WHINED(
            require(
                memcmp(buffer, expected + length, (size_t)n) == 0,
                did_whine,
                "'%s' does not contain the expected content",
                path
            )
        );

        length += (size_t)n;
    }

    for (;;) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n < 0) {
            if (errno == EINTR)
                continue;

            e = errno;
            whine(
                "read('%s') failed: %s",
                path,
                strerror(e)
            );
            return did_whine;
        }

        if (n == 0)
            break;

        whine(
            "'%s' has unexpected extra content",
            path
        );
        return did_whine;
    }

    return did_not_whine;
}

static enum whined whine_if_wrong_text_at_path(
    const char* path,
    const char* expected
) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    int e = errno;
    RETURN_IF_WHINED(
        require(
            fd >= 0,
            did_whine,
            "open('%s') failed: %s",
            path,
            strerror(e)
        )
    );

    enum whined whined;
    whined = whine_if_wrong_text_at_fd(
        path,
        fd,
        expected
    );

    int status = close(fd);
    e = errno;
    RETURN_IF_WHINED(
        require(
            status == 0,
            did_whine,
            "close('%s') failed: %s",
            path,
            strerror(e)
        )
    );

    return whined;
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

static enum whined dup2_or_whine(
    const char* value,
    int fd_value,
    const char* target,
    int fd_target
) {
    if (fd_value == fd_target)
        return did_not_whine;

    int status = dup2(fd_value, fd_target);
    int e = errno;
    RETURN_IF_WHINED(
        require(
            status >= 0,
            did_whine,
            "dup2(%s (%d), %s (%d)) failed: %s",
            value,
            fd_value,
            target,
            fd_target,
            strerror(e)
        )
    );

    return did_not_whine;
}

#define DUP2_OR_WHINE(value, target) \
    dup2_or_whine(#value, (value), #target, (target))

static void die(enum whined complaint) {
    _exit(complaint);
}

static void die_if_whined(enum whined whined) {
    if (whined != did_whine)
        return;

    die(whined);
}

static enum whined run_command_or_whine(
    char* const argv[],
    int* p_exit_code
) {
    RETURN_IF_WHINED(
        require(
            p_exit_code != NULL,
            did_whine,
            "internal error: "
            "null p_exit_code passed to run_command_or_whine"
        )
    );

    pid_t pid = fork();
    int e = errno;
    RETURN_IF_WHINED(
        require(
            pid >= 0,
            did_whine,
            "fork failed: %s",
            strerror(e)
        )
    );

    if (pid == 0) {
        die_if_whined(whine_if_standard_fd_missing());

        int null_fd = open("/dev/null", O_WRONLY);
        die_if_whined(
            require(
                null_fd >= 0,
                did_whine,
                "failed to open '/dev/null'"
            )
        );

        // NOTE this check is mostly redundant, since we
        //  whine_if_standard_fd_missing above, so null_fd >= 3
        //  at this point. But, in esoteric threaded cases,
        //  we could get surprised, so check anyway.
        die_if_whined(
            require(
                is_standard_fd(null_fd) == false,
                did_whine,
                "'/dev/null' opened as standard fd"
            )
        );

        die_if_whined(DUP2_OR_WHINE(null_fd, STDOUT_FILENO));
        die_if_whined(DUP2_OR_WHINE(null_fd, STDERR_FILENO));

        // this is safe because we know it is not a standard fd
        close(null_fd);

        execvp(argv[0], argv);

        // if we get here execvp failed
        die(did_whine);
    }

    int status;
    for (;;) {
        if (waitpid(pid, &status, 0) >= 0)
            break;

        // We may get interrupted by a signal while
        // waiting on our child. If that happens,
        // just wait again.
        if (errno == EINTR)
            continue;

        e = errno;
        whine("waitpid failed: %s", strerror(e));
        return did_whine;
    }

    if (WIFEXITED(status)) {
        *p_exit_code = WEXITSTATUS(status);
        return did_not_whine;
    }

    RETURN_IF_WHINED(
        require(
            WIFSIGNALED(status) == 0,
            did_whine,
            "'%s' was killed by signal %d",
            argv[0],
            WTERMSIG(status)
        )
    );

    whine("'%s' ended unexpectedly", argv[0]);
    return did_whine;
}

static enum whined whine_if_file_untracked(char* path) {
    char* const argv[] = {
        "git",
        "ls-files",
        "--error-unmatch",
        "--",
        path,
        NULL
    };

    int exit_code;
    if (run_command_or_whine(argv, &exit_code) == did_not_whine) {
        if (exit_code == 0)
            return did_not_whine;

        whine(
            "'%s' exited with status %d",
            argv[0],
            exit_code
        );
    }

    whine(
        "could not verify '%s' is tracked by git",
        path
    );
    return did_whine;
}

static enum whined mkdir_or_whine(const char* path) {
    // mode of 0777 mimics shell
    if (mkdir(path, 0777) == 0)
        return did_not_whine;

    int e = errno;
    RETURN_IF_WHINED(
        require(
            e == EEXIST,
            did_whine,
            "mkdir('%s') failed: %s",
            path,
            strerror(e)
        )
    );

    struct stat st;
    int status = stat(path, &st);
    e = errno;
    RETURN_IF_WHINED(
        require(
            status == 0,
            did_whine,
            "stat('%s') failed: %s",
            path,
            strerror(e)
        )
    );

    RETURN_IF_WHINED(
        require(
            S_ISDIR(st.st_mode),
            did_whine,
            "'%s' is not a directory",
            path
        )
    );

    return did_not_whine;
}

static enum whined open_blob_or_whine(
    const char* path,
    int* p_fd
) {
    RETURN_IF_WHINED(
        require(
            path != NULL,
            did_whine,
            "internal error: "
            "null path passed to open_blob_or_whine"
        )
    );

    RETURN_IF_WHINED(
        require(
            p_fd != NULL,
            did_whine,
            "internal error: "
            "null p_fd passed to open_blob_or_whine"
        )
    );

    // mode of 0666 mimics shell
    int fd = open(path, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0666);
    int e = errno;
    if (fd < 0) {
        RETURN_IF_WHINED(
            require(
                e == EEXIST,
                did_whine,
                "open('%s') failed: %s",
                path,
                strerror(e)
            )
        );
    }

    if (fd < 0) {
        fd = open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
        e = errno;
        RETURN_IF_WHINED(
            require(
                fd >= 0,
                did_whine,
                "open('%s') failed: %s",
                path,
                strerror(e)
            )
        );
    }

    struct result file = is_fd_regular_file(fd);
    switch (file.status) {
    case s_ok:
        break;

    case s_fstat_failed:
        require(
            false,
            did_whine,
            "fstat('%s') failed: %s",
            path,
            strerror(file.sys_error)
        );
        break;

    case s_not_regular_file:
        require(
            false,
            did_whine,
            "'%s' is not a regular file",
            path
        );
        break;
    }

    if (file.status != s_ok) {
        int status = close(fd);
        e = errno;
        require(
            status == 0,
            did_whine,
            "close('%s') failed: %s",
            path,
            strerror(e)
        );
        return did_whine;
    }

    *p_fd = fd;
    return did_not_whine;
}

enum whined main_check_lock_fd(
    const char* lock_path,
    int lock_fd
) {
    struct flock lock = {
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0,
    };

    int status = fcntl(lock_fd, F_SETLK, &lock);
    int e = errno;
    RETURN_IF_WHINED(
        require(
            status == 0,
            did_whine,
            "lock('%s') failed: %s",
            lock_path,
            strerror(e)
        )
    );

    RETURN_IF_WHINED(
        whine_if_wrong_text_at_fd(lock_path, lock_fd, "")
    );

    char* const argv[] = {
        "./runme",
        NULL
    };

    int exit_code;
    RETURN_IF_WHINED(
        run_command_or_whine(argv, &exit_code)
    );

    // XXX this does not distinguish all the different ways it can fail...

    return require(
        exit_code == EXIT_FAILURE, 
        did_whine,
        "child process did not fail like we expected"
    );
}

int main(int argc, char** argv) {
    RETURN_IF_WHINED(
        require(
            (argc >= 1) && (argv[0] != NULL),
            did_whine,
            "missing program name: argc (%d), argv[0] (%p)",
            argc,
            argv[0]
        )
    );

    RETURN_IF_WHINED(
        require(
            argc == 1,
            did_whine,
            "expected no arguments, got %d",
            argc - 1
        )
    );

    struct utsname uts;
    RETURN_IF_WHINED(
        require(
            uname(&uts) == 0,
            did_whine,
            "uname failed"
        )
    );

    RETURN_IF_WHINED(
        require(
            strcmp(uts.machine, "x86_64") == 0,
            did_whine,
            "we only support x86_64 hosts for now, not '%s'",
            uts.machine
        )
    );

    RETURN_IF_WHINED(
        require(
            strcmp(uts.sysname, "Linux") == 0,
            did_whine,
            "we only support Linux hosts for now, not '%s'",
            uts.sysname
        )
    );

    RETURN_IF_WHINED(whine_if_standard_fd_missing());

    enum { buffer_size = 65536, };
    char* buffer = malloc(buffer_size);
    RETURN_IF_WHINED(require(buffer, did_whine, "out of memory"));

    enum whined whined;
    whined = cd_to_self_or_whine(buffer, buffer_size);
    free(buffer);
    RETURN_IF_WHINED(whined);

    RETURN_IF_WHINED(whine_if_not_fs_blob_path("./runme"));
    {
        const char* a = "/proc/self/exe";
        const char* b = "./runme";

        struct stat a_stat;
        int status = stat(a, &a_stat);
        int e = errno;
        RETURN_IF_WHINED(
            require(
                status == 0,
                did_whine,
                "stat('%s') failed: %s",
                a,
                strerror(e)
            )
        );

        struct stat b_stat;
        status = stat(b, &b_stat);
        e = errno;
        RETURN_IF_WHINED(
            require(
                status == 0,
                did_whine,
                "stat('%s') failed: %s",
                b,
                strerror(e)
            )
        );

        RETURN_IF_WHINED(
            require(
                a_stat.st_dev == b_stat.st_dev &&
                a_stat.st_ino == b_stat.st_ino,
                did_whine,
                "'%s' and '%s' are not the same file",
                a,
                b
            )
        );
    }

    RETURN_IF_WHINED(whine_if_not_fs_blob_path("./runme.c"));
    RETURN_IF_WHINED(whine_if_not_fs_blob_path("./.gitignore"));

    RETURN_IF_WHINED(
        whine_if_wrong_text_at_path(
            "./.gitignore",

            ".ignore/\n"
            "runme\n"
            ".codex\n"
        )
    );

    RETURN_IF_WHINED(whine_if_file_untracked("runme.c"));
    RETURN_IF_WHINED(whine_if_file_untracked(".gitignore"));

    RETURN_IF_WHINED(mkdir_or_whine("./.ignore"));

    const char* lock_path = "./.ignore/runme.lock";
    int lock_fd;
    RETURN_IF_WHINED(open_blob_or_whine(lock_path, &lock_fd));

    whined = main_check_lock_fd(lock_path, lock_fd);

    int status = close(lock_fd);
    int e = errno;
    RETURN_IF_WHINED(
        require(
            status == 0,
            did_whine,
            "close('%s') failed: %s",
            lock_path,
            strerror(e)
        )
    );

    RETURN_IF_WHINED(whined);

    puts("OK");
    return did_not_whine;
}
