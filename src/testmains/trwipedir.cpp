#include "wipedir.h"

#include <stdio.h>
#include <stdlib.h>


using namespace std;
static const char *thisprog;

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_r      0x2 
#define OPT_s      0x4 
static char usage [] =
"wipedir [-r -s] topdir\n"
" -r : recurse\n"
" -s : also delete topdir\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

int main(int argc, const char **argv)
{
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
    (*argv)++;
    if (!(**argv))
        /* Cas du "adb - core" */
        Usage();
    while (**argv)
        switch (*(*argv)++) {
        case 'r':    op_flags |= OPT_r; break;
        case 's':    op_flags |= OPT_s; break;
        default: Usage();    break;
        }
    b1: argc--; argv++;
    }

    if (argc != 1)
    Usage();

    string dir = *argv++;argc--;

    bool topalso = ((op_flags&OPT_s) != 0);
    bool recurse = ((op_flags&OPT_r) != 0);
    int cnt = wipedir(dir, topalso, recurse);
    printf("wipedir returned %d\n", cnt);
    exit(0);
}
