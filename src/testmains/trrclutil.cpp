
#include "rclutil.h"

#include <getopt.h>

#include <iostream>
#include <map>

#include "pathut.h"

using namespace std;

static std::map<std::string, int> options {
    {"path_to_thumb", 0},
    {"url_encode", 0},
        };

static const char *thisprog;
static void Usage(void)
{
    string sopts;
    for (const auto& opt: options) {
        sopts += "--" + opt.first + "\n";
    }
    fprintf(stderr, "%s: usage: %s\n%s", thisprog, thisprog, sopts.c_str());
    exit(1);
}

void path_to_thumb(const string& _input)
{
    string input(_input);
    // Make absolute path if needed
    if (input[0] != '/') {
        input = path_absolute(input);
    }

    input = string("file://") + path_canon(input);

    string path;
    //path = url_encode(input, 7);
    thumbPathForUrl(input, 7, path);
    cout << path << endl;
}
        
int main(int argc, char **argv)
{
    thisprog = *argv;
    std::vector<struct option> long_options;

    for (auto& entry : options) {
        struct option opt;
        opt.name = entry.first.c_str();
        opt.has_arg = 0;
        opt.flag = &entry.second;
        opt.val = 1;
        long_options.push_back(opt);
    }
    long_options.push_back({0, 0, 0, 0});

    while (getopt_long(argc, argv, "", &long_options[0], nullptr) != -1) {
    }
    if (options["path_to_thumb"]) {
        if (optind >= argc) {
            cerr <<  "Usage: trrcutil --path_to_thumb <filepath>" << "\n";
            return 1;
        }
        string input = argv[optind];
        optind++;
        if (optind != argc) {
            return 1;
        }
        path_to_thumb(input);
    } else if (options["url_encode"]) {
        if (optind >= argc) {
            cerr << "Usage: trsmallut --url_encode <arg> [offs=0]\n";
            return 1;
        }
        string s = argv[optind];
        optind++;
        int offs = 0;
        if (optind != argc) {
            offs = atoi(argv[optind]);
            optind++;
        }
        if (optind != argc) {
            return 1;
        }
        cout << "url_encode(" << s << ", " << offs << ") -> [" << url_encode(s, offs) << "]\n";
    } else {
        Usage();
    }
}
