#!/bin/sh
q() {
    printf "$2" "$1" "$1" "$2" "$1"
}
q "'" '#!/bin/sh
q() {
    printf "$2" "$1" "$1" "$2" "$1"
}
q "%s" %s%s%s
'
