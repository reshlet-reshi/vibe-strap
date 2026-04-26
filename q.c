#include <stdio.h>

void printf2(
    const char * pChzFmt, 
    const char * pChz1, 
    const char * pChz2
) {
    printf(pChzFmt, pChz1, pChz2)
}

void q(const char * pChz1, const char * pChz2)
{
    printf(
        pChz2,
        "'",
        "'",
        pChz2,
        "'"
    );
}

void main(void)
{
    q(
        // tick
        "'", 

        // fmt
        "#!/bin/sh\n"
        "q() {\n"
        "    printf \"$2\" \"$1\" \"$1\" \"$2\" \"$1\"\n"
        "}\n"
        "q \"%1$\" %s%s%s\n"
        "'"
    )
    return 0;
}