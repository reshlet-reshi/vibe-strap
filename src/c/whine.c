#include "whine.h"

#include <stdarg.h>
#include <stdio.h>

static const char* our_name = "runme";

void set_whine_name(const char* name) {
    our_name = name;
}

static void vwhine(const char* format, va_list args) {
    fprintf(stderr, "%s: ", our_name);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

void whine(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vwhine(format, args);
    va_end(args);
}

static enum whined vrequire(
    bool cond,
    enum whined complaint,
    const char* format,
    va_list args
) {
    if (cond)
        return did_not_whine;

    vwhine(format, args);
    return complaint;
}

enum whined require(
    bool cond,
    enum whined complaint,
    const char* format,
    ...
) {
    va_list args;
    va_start(args, format);
    enum whined whined = vrequire(cond, complaint, format, args);
    va_end(args);

    return whined;
}
