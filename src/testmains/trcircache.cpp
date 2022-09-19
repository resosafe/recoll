#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <iostream>
#include <memory>

#include "circache.h"
#include "fileudi.h"
#include "conftree.h"
#include "readfile.h"
#include "log.h"

#include "smallut.h"

using namespace std;

static char *thisprog;

static char usage [] =
    " -c [-u] <dirname> <sizekbs>: create new store or possibly resize existing one\n"
    "   -u: set the 'unique' flag (else unset it)\n"
    "   None of this changes the existing data\n"
    " -p <dirname> <apath> [apath ...] : put files\n"
    " -d <dirname> : dump\n"
    " -g [-i instance] [-D] <dirname> <udi>: get\n"
    "   -D: also dump data\n"
    " -e <dirname> <udi> : erase\n"
    " -a <targetdir> <dir> [<dir> ...]: append content from existing cache(s) to target\n"
    "  The target should be first resized to hold all the data, else only\n"
    "  as many entries as capacity permit will be retained\n"
    " -C <dir> : recover space from erased entries. May temporarily need twice the current "
    "    space used by the cache\n";
    ;

static void
Usage(FILE *fp = stderr)
{
    fprintf(fp, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_a     0x1
#define OPT_b     0x2  
#define OPT_C     0x4  
#define OPT_c     0x8  
#define OPT_D     0x10 
#define OPT_d     0x20 
#define OPT_e     0x40 
#define OPT_g     0x80 
#define OPT_i     0x100
#define OPT_p     0x200
#define OPT_u     0x400

bool storeFile(CirCache& cc, const std::string fn);

int main(int argc, char **argv)
{
    int instance = -1;

    thisprog = argv[0];
    argc--;
    argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
        {
            Usage();
        }
        while (**argv)
            switch (*(*argv)++) {
            case 'a': op_flags |= OPT_a; break;
            case 'b': op_flags |= OPT_b; break;
            case 'C': op_flags |= OPT_C; break;
            case 'c': op_flags |= OPT_c; break;
            case 'D': op_flags |= OPT_D; break;
            case 'd': op_flags |= OPT_d; break;
            case 'e': op_flags |= OPT_e; break;
            case 'g': op_flags |= OPT_g; break;
            case 'i':
                op_flags |= OPT_i;
                if (argc < 2) {
                    Usage();
                }
                if ((sscanf(*(++argv), "%d", &instance)) != 1) {
                    Usage();
                }
                argc--;
                goto b1;
            case 'p': op_flags |= OPT_p; break;
            case 'u': op_flags |= OPT_u; break;
            default: Usage(); break;
            }
b1:
        argc--;
        argv++;
    }

    Logger::getTheLog("")->setLogLevel(Logger::LLDEB1);

    if (argc < 1) {
        Usage();
    }
    string dir = *argv++;
    argc--;

    CirCache cc(dir);

    if (op_flags & OPT_c) {
        if (argc != 1) {
            Usage();
        }
        int64_t sizekb = atoi(*argv++);
        argc--;
        int flags = 0;
        if (op_flags & OPT_u) {
            flags |= CirCache::CC_CRUNIQUE;
        }
        if (!cc.create(sizekb * 1024, flags)) {
            cerr << "Create failed:" << cc.getReason() << endl;
            exit(1);
        }
    } else if (op_flags & OPT_C) {
        if (argc) {
            Usage();
        }
        std::string reason;
        if (!CirCache::compact(dir, &reason)) {
            std::cerr << "Compact failed: " << reason << "\n";
            return 1;
        }
        return 0;
    } else if (op_flags & OPT_a) {
        if (argc < 1) {
            Usage();
        }
        while (argc) {
            string reason;
            if (CirCache::appendCC(dir, *argv++, &reason) < 0) {
                cerr << reason << endl;
                return 1;
            }
            argc--;
        }
    } else if (op_flags & OPT_b) {
        if (argc < 1) {
            Usage();
        }
        std::string ddir = *argv++; argc--;
        string reason;
        if (!CirCache::burst(dir, ddir, &reason)) {
            cerr << reason << endl;
            return 1;
        }
    } else if (op_flags & OPT_p) {
        if (argc < 1) {
            Usage();
        }
        if (!cc.open(CirCache::CC_OPWRITE)) {
            cerr << "Open failed: " << cc.getReason() << endl;
            exit(1);
        }
        while (argc) {
            string fn = *argv++;
            argc--;
            if (!storeFile(cc, fn)) {
                return 1;
            }
        }
        cc.open(CirCache::CC_OPREAD);
    } else if (op_flags & OPT_g) {
        if (!cc.open(CirCache::CC_OPREAD)) {
            cerr << "Open failed: " << cc.getReason() << endl;
            exit(1);
        }
        while (argc) {
            string udi = *argv++;
            argc--;
            string dic, data;
            if (!cc.get(udi, dic, &data, instance)) {
                cerr << "Get failed: " << cc.getReason() << endl;
                exit(1);
            }
            cout << "Dict: [" << dic << "]" << endl;
            if (op_flags & OPT_D) {
                cout << "Data: [" << data << "]" << endl;
            }
        }
    } else if (op_flags & OPT_e) {
        if (!cc.open(CirCache::CC_OPWRITE)) {
            cerr << "Open failed: " << cc.getReason() << endl;
            exit(1);
        }
        while (argc) {
            string udi = *argv++;
            argc--;
            string dic, data;
            if (!cc.erase(udi)) {
                cerr << "Erase failed: " << cc.getReason() << endl;
                exit(1);
            }
        }
    } else if (op_flags & OPT_d) {
        if (!cc.open(CirCache::CC_OPREAD)) {
            cerr << "Open failed: " << cc.getReason() << endl;
            exit(1);
        }
        cc.dump();
    } else {
        Usage();
    }

    exit(0);
}


bool storeFile(CirCache& cc, const std::string fn)
{
    char dic[1000];
    string data, reason;
    if (!file_to_string(fn, data, &reason)) {
        std::cerr << "File_to_string: " << reason << endl;
        return false;
    }
    string udi;
    make_udi(fn, "", udi);
    string cmd("xdg-mime query filetype ");
    // Should do more quoting here...
    cmd += "'" + fn + "'";
    FILE *fp = popen(cmd.c_str(), "r");
    char* buf=0;
    size_t sz = 0;
    if (::getline(&buf, &sz, fp) -1) {
        std::cerr << "Could not read from xdg-mime output\n";
        return false;
    }
    pclose(fp);
    string mimetype(buf);
    free(buf);
    trimstring(mimetype, "\n\r");
    //std::cerr << "Got [" << mimetype << "]\n";

    string s;
    ConfSimple conf(s);
    conf.set("udi", udi);
    conf.set("mimetype", mimetype);
    //ostringstream str; conf.write(str); cout << str.str() << endl;

    if (!cc.put(udi, &conf, data, 0)) {
        std::cerr << "Put failed: " << cc.getReason() << endl;
        std::cerr << "conf: [";
        conf.write(std::cerr);
        std::cerr << "]" << endl;
        return false;
    }
    return true;
}
