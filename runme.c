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

enum {
    s_ok,
    s_invalid_buffer,
    s_null_path,
    s_open_failed,
    s_fcntl_failed,
    s_fd_not_open,
    s_fstat_failed,
    s_not_regular_file,
    s_readlink_failed,
    s_readlink_truncated,
    s_readlink_empty,
    s_no_directory_component,
    s_chdir_failed,
    s_close_failed,
    s_lseek_failed,
    s_read_failed,
    s_ended_before_expected_content,
    s_wrong_content,
    s_unexpected_extra_content,
};

struct result {
    const char* in_path;
    int in_fd;
    int in_flags;
    mode_t in_mode;
    size_t in_buffer_size;

    char* in_out_buffer;
    
    int out_status;
    int out_sys_error;
    int out_fd;
};

static void clear_result(struct result* result) {
    *result = (struct result) {0};
    result->in_fd = -1;
    result->out_fd = -1;
}

static enum whined whine_result(struct result result) {
    switch (result.out_status) {
    case s_ok:
        return did_not_whine;

    case s_invalid_buffer:
        return require(
            false,
            did_whine,
            "internal error: "
            "invalid buffer passed to cd_to_self"
        );

    case s_null_path:
        return require(
            false,
            did_whine,
            "internal error: "
            "null path passed to is_path_regular_file"
        );

    case s_open_failed:
        return require(
            false,
            did_whine,
            "open('%s') failed: %s",
            result.in_path,
            strerror(result.out_sys_error)
        );

    case s_fcntl_failed:
        return require(
            false,
            did_whine,
            "fcntl(%s (%d), F_GETFD) failed: %s",
            result.in_path,
            result.in_fd,
            strerror(result.out_sys_error)
        );

    case s_fd_not_open:
        return require(
            false,
            did_whine,
            "standard fd %s (%d) is not open",
            result.in_path,
            result.in_fd
        );

    case s_fstat_failed:
        return require(
            false,
            did_whine,
            "fstat('%s') failed: %s",
            result.in_path,
            strerror(result.out_sys_error)
        );

    case s_not_regular_file:
        return require(
            false,
            did_whine,
            "'%s' is not a regular file",
            result.in_path
        );

    case s_readlink_failed:
        return require(
            false,
            did_whine,
            "readlink(%s) failed: %s",
            result.in_path,
            strerror(result.out_sys_error)
        );

    case s_readlink_truncated:
        return require(
            false,
            did_whine,
            "readlink(%s) returned truncated path",
            result.in_path
        );

    case s_readlink_empty:
        return require(
            false,
            did_whine,
            "%s resolved to an empty path",
            result.in_path
        );

    case s_no_directory_component:
        return require(
            false,
            did_whine,
            "%s path has no directory component: '%s'",
            result.in_path,
            result.in_out_buffer
        );

    case s_chdir_failed:
        return require(
            false,
            did_whine,
            "chdir('%s') failed: %s",
            result.in_out_buffer,
            strerror(result.out_sys_error)
        );

    case s_close_failed:
        return require(
            false,
            did_whine,
            "close('%s') failed: %s",
            result.in_path,
            strerror(result.out_sys_error)
        );

    case s_lseek_failed:
        return require(
            false,
            did_whine,
            "lseek('%s') failed: %s",
            result.in_path,
            strerror(result.out_sys_error)
        );

    case s_read_failed:
        return require(
            false,
            did_whine,
            "read('%s') failed: %s",
            result.in_path,
            strerror(result.out_sys_error)
        );

    case s_ended_before_expected_content:
        return require(
            false,
            did_whine,
            "'%s' ended before expected content",
            result.in_path
        );

    case s_wrong_content:
        return require(
            false,
            did_whine,
            "'%s' does not contain the expected content",
            result.in_path
        );

    case s_unexpected_extra_content:
        return require(
            false,
            did_whine,
            "'%s' has unexpected extra content",
            result.in_path
        );
    }

    return require(
        false,
        did_whine,
        "internal error: unknown result status %d",
        result.out_status
    );
}

static enum whined whine_results(
    const struct result* const results[],
    size_t count
) {
    enum whined whined = did_not_whine;

    for (size_t i = 0; i < count; ++i) {
        if (whine_result(*results[i]) == did_whine)
            whined = did_whine;
    }

    return whined;
}

#define WHINE_RESULTS(...)                                      \
    whine_results(                                              \
        (const struct result* const[]) { __VA_ARGS__ },          \
        sizeof((const struct result* const[]) { __VA_ARGS__ }) / \
            sizeof(const struct result*)                         \
    )

static void sys_open(
    const char* path,
    int flags,
    mode_t mode,
    struct result* result
) {
    clear_result(result);
    result->in_path = path;
    result->in_flags = flags;
    result->in_mode = mode;

    bool needs_mode =
        ((result->in_flags & O_CREAT) == O_CREAT) ||
        ((result->in_flags & O_TMPFILE) == O_TMPFILE);

    if (needs_mode)
        result->out_fd = open(path, result->in_flags, result->in_mode);
    else
        result->out_fd = open(path, result->in_flags);

    if (result->out_fd < 0) {
        result->out_status = s_open_failed;
        result->out_sys_error = errno;
    }

    errno = 0;
}

static void is_fd_open(
    const char* path,
    int fd,
    struct result* result
) {
    clear_result(result);
    result->in_path = path;
    result->in_fd = fd;

    errno = 0;
    if (fcntl(fd, F_GETFD) >= 0)
        result->out_status = s_ok;
    else if (errno == EBADF)
        result->out_status = s_fd_not_open;
    else {
        result->out_status = s_fcntl_failed;
        result->out_sys_error = errno;
    }

    errno = 0;
}

static void cd_to_self(
    char* buffer,
    size_t buffer_size,
    struct result* result
) {
    clear_result(result);
    result->in_path = "/proc/self/exe";
    result->in_buffer_size = buffer_size;
    result->in_out_buffer = buffer;

    if (buffer == NULL || buffer_size <= 1) {
        result->out_status = s_invalid_buffer;
        return;
    }

    // readlink does not null terminate,
    //  so reserve one byte in the buffer.
    size_t read_size = result->in_buffer_size - 1;

    ssize_t length = readlink(result->in_path, buffer, read_size);
    if (length < 0) {
        result->out_status = s_readlink_failed;
        result->out_sys_error = errno;
        errno = 0;
        return;
    }

    // null terminate
    buffer[length] = '\0';

    if ((size_t)length >= read_size) {
        result->out_status = s_readlink_truncated;
        errno = 0;
        return;
    }

    if (length == 0) {
        result->out_status = s_readlink_empty;
        errno = 0;
        return;
    }

    char* last_slash = &buffer[length - 1];
    while (last_slash > buffer) {
        if (*last_slash == '/')
            break;
        --last_slash;
    }

    if (*last_slash != '/') {
        result->out_status = s_no_directory_component;
        errno = 0;
        return;
    }

    // drop the basename
    *(last_slash + 1) = '\0';

    if (chdir(buffer) < 0) {
        result->out_status = s_chdir_failed;
        result->out_sys_error = errno;
        errno = 0;
        return;
    }

    errno = 0;
}

static void is_fd_regular_file(
    const char* path,
    int fd,
    struct result* result
) {
    clear_result(result);
    result->in_path = path;
    result->in_fd = fd;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        result->out_status = s_fstat_failed;
        result->out_sys_error = errno;
    } else if (!S_ISREG(st.st_mode)) {
        result->out_status = s_not_regular_file;
    }

    errno = 0;
}

static void is_path_regular_file(
    const char* path,
    struct result* result,
    struct result* close_result
) {
    clear_result(result);
    result->in_path = path;
    clear_result(close_result);
    close_result->in_path = path;

    if (path == NULL) {
        result->out_status = s_null_path;
        return;
    }

    sys_open(path, O_PATH | O_CLOEXEC, 0, result);
    if (result->out_status != s_ok)
        return;

    int fd = result->out_fd;
    is_fd_regular_file(path, fd, result);

    int status = close(fd);
    int e = errno;
    if (status != 0) {
        clear_result(close_result);
        close_result->in_path = path;
        close_result->in_fd = fd;
        close_result->out_status = s_close_failed;
        close_result->out_sys_error = e;
    }

    errno = 0;
}

static void has_expected_text_at_fd(
    const char* path,
    int fd,
    const char* expected,
    struct result* result
) {
    clear_result(result);
    result->in_path = path;
    result->in_fd = fd;

    enum { buffer_size = 4096, };
    char buffer[buffer_size];

    errno = 0;
    off_t offset = lseek(fd, 0, SEEK_SET);
    if (offset != 0) {
        result->out_status = s_lseek_failed;
        result->out_sys_error = errno;
        errno = 0;
        return;
    }

    size_t length = 0;
    size_t expected_length = strlen(expected);
    while (length < expected_length) {
        size_t remaining = expected_length - length;
        size_t chunk_size = remaining < sizeof(buffer)
            ? remaining
            : sizeof(buffer);

        ssize_t n = read(fd, buffer, chunk_size);
        if (n < 0) {
            if (errno == EINTR)
                continue;

            result->out_status = s_read_failed;
            result->out_sys_error = errno;
            errno = 0;
            return;
        }

        if (n == 0) {
            result->out_status = s_ended_before_expected_content;
            errno = 0;
            return;
        }

        if (memcmp(buffer, expected + length, (size_t)n) != 0) {
            result->out_status = s_wrong_content;
            errno = 0;
            return;
        }

        length += (size_t)n;
    }

    for (;;) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n < 0) {
            if (errno == EINTR)
                continue;

            result->out_status = s_read_failed;
            result->out_sys_error = errno;
            errno = 0;
            return;
        }

        if (n == 0)
            break;

        result->out_status = s_unexpected_extra_content;
        errno = 0;
        return;
    }

    errno = 0;
}

static void has_expected_text_at_path(
    const char* path,
    const char* expected,
    struct result* result,
    struct result* close_result
) {
    clear_result(result);
    result->in_path = path;
    clear_result(close_result);
    close_result->in_path = path;

    sys_open(path, O_RDONLY | O_CLOEXEC, 0, result);
    if (result->out_status != s_ok)
        return;

    int fd = result->out_fd;

    has_expected_text_at_fd(
        path,
        fd,
        expected,
        result
    );

    int status = close(fd);
    int e = errno;
    if (status != 0) {
        close_result->in_fd = fd;
        close_result->out_status = s_close_failed;
        close_result->out_sys_error = e;
    }

    errno = 0;
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
        struct result result;
        enum whined whined;

        is_fd_open("STDIN_FILENO", STDIN_FILENO, &result);
        whined = whine_result(result);
        if (whined == did_whine)
            _exit(whined);

        is_fd_open("STDOUT_FILENO", STDOUT_FILENO, &result);
        whined = whine_result(result);
        if (whined == did_whine)
            _exit(whined);

        is_fd_open("STDERR_FILENO", STDERR_FILENO, &result);
        whined = whine_result(result);
        if (whined == did_whine)
            _exit(whined);

        // NOTE dup2 does not maintain O_CLOEXEC, which is what we want here.
        sys_open("/dev/null", O_WRONLY | O_CLOEXEC, 0, &result);
        whined = whine_result(result);
        if (whined == did_whine)
            _exit(whined);

        int null_fd = result.out_fd;

        // NOTE this check is mostly redundant, since we
        //  checked the standard fds above, so null_fd >= 3 at
        //  this point. But, in esoteric threaded cases,
        //  we could get surprised, so check anyway.
        whined = require(
            is_standard_fd(null_fd) == false,
            did_whine,
            "'/dev/null' opened as standard fd"
        );
        if (whined == did_whine)
            _exit(whined);

        whined = DUP2_OR_WHINE(null_fd, STDOUT_FILENO);
        if (whined == did_whine)
            _exit(whined);

        whined = DUP2_OR_WHINE(null_fd, STDERR_FILENO);
        if (whined == did_whine)
            _exit(whined);

        // this is safe because we know it is not a standard fd
        close(null_fd);

        execvp(argv[0], argv);

        // if we get here execvp failed
        _exit(did_whine);
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

    struct result result;

    // mode of 0666 mimics shell
    sys_open(path, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0666, &result);
    if (result.out_status != s_ok) {
        if (
            result.out_status != s_open_failed ||
            result.out_sys_error != EEXIST
        ) {
            RETURN_IF_WHINED(whine_result(result));
        }

        sys_open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0, &result);
        RETURN_IF_WHINED(whine_result(result));
    }

    int fd = result.out_fd;
    is_fd_regular_file(path, fd, &result);
    enum whined whined = whine_result(result);
    if (whined == did_whine) {
        int status = close(fd);
        int e = errno;
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

    struct result result;
    has_expected_text_at_fd(lock_path, lock_fd, "", &result);
    RETURN_IF_WHINED(whine_result(result));

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

    struct result result;
    is_fd_open("STDIN_FILENO", STDIN_FILENO, &result);
    RETURN_IF_WHINED(whine_result(result));

    is_fd_open("STDOUT_FILENO", STDOUT_FILENO, &result);
    RETURN_IF_WHINED(whine_result(result));

    is_fd_open("STDERR_FILENO", STDERR_FILENO, &result);
    RETURN_IF_WHINED(whine_result(result));

    enum { buffer_size = 65536, };
    char* buffer = malloc(buffer_size);
    RETURN_IF_WHINED(require(buffer, did_whine, "out of memory"));

    enum whined whined;
    cd_to_self(buffer, buffer_size, &result);
    whined = whine_result(result);
    free(buffer);
    RETURN_IF_WHINED(whined);

    struct result path_result;
    struct result close_result;
    is_path_regular_file("./runme", &path_result, &close_result);
    RETURN_IF_WHINED(WHINE_RESULTS(&path_result, &close_result));
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

    is_path_regular_file("./runme.c", &path_result, &close_result);
    RETURN_IF_WHINED(WHINE_RESULTS(&path_result, &close_result));

    is_path_regular_file("./.gitignore", &path_result, &close_result);
    RETURN_IF_WHINED(WHINE_RESULTS(&path_result, &close_result));

    has_expected_text_at_path(
        "./.gitignore",

        ".ignore/\n"
        "runme\n"
        ".codex\n",

        &path_result,
        &close_result
    );
    RETURN_IF_WHINED(WHINE_RESULTS(&path_result, &close_result));

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
