#ifndef VIBE_STRAP_WHINE_H
#define VIBE_STRAP_WHINE_H

#include <stdbool.h>

enum whined {
    did_not_whine,
    did_whine,
};

void whine(const char* format, ...);
enum whined require(
    bool cond,
    enum whined complaint,
    const char* format,
    ...
);

#define RETURN_IF_WHINED(whined)        \
    do {                                \
        if ((whined) != did_not_whine)  \
            return (whined);            \
    } while (0)

#endif
