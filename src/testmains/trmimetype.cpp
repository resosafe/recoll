#include "mimetype.h"

#include <stdio.h>

#include <cstdlib>
#include <iostream>

#include "log.h"
#include "rclconfig.h"
#include "rclinit.h"
#include "pathut.h"

using namespace std;
int main(int argc, const char **argv)
{
    string reason;
    RclConfig *config = recollinit(0, 0, 0, reason);

    if (config == 0 || !config->ok()) {
    string str = "Configuration problem: ";
    str += reason;
    fprintf(stderr, "%s\n", str.c_str());
    exit(1);
    }

    while (--argc > 0) {
    string filename = *++argv;
    struct PathStat st;
    if (path_fileprops(filename, &st)) {
        fprintf(stderr, "Can't stat %s\n", filename.c_str());
        continue;
    }
    cout << filename << " -> " << 
        mimetype(filename, &st, config, true) << endl;

    }
    return 0;
}
