#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <iostream>
#include <vector>
#include <string>
using namespace std;

#include "subtreelist.h"
#include "rclconfig.h"
#include "rclinit.h"

static char *thisprog;
static char usage [] = " <path> : list document paths in this tree\n";
static void Usage(void)
{
    std::cerr << thisprog <<  ": usage:" << endl << usage;
    exit(1);
}

static int     op_flags;
#define OPT_o     0x2 

int main(int argc, char **argv)
{
    string top;

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            default: Usage();   break;
            }
        argc--; argv++;
    }
    if (argc < 1)
        Usage();
    top = *argv++;argc--;
    string reason;
    RclConfig *config = recollinit(0, 0, 0, reason, 0);
    if (!config || !config->ok()) {
        fprintf(stderr, "Recoll init failed: %s\n", reason.c_str());
        exit(1);
    }

    vector<string> paths;
    if (!subtreelist(config, top, paths)) {
        cerr << "subtreelist failed" << endl;
        exit(1);
    }
    for (vector<string>::const_iterator it = paths.begin(); 
         it != paths.end(); it++) {
        cout << *it << endl;
    }
    exit(0);
}
