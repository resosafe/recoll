/* Copyright (C) 2017-2019 J.F.Dockes
 *
 * License: LGPL 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "autoconfig.h"

#include "textsplit.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>

#include <iostream>

#include "readfile.h"
#include "log.h"
#include "transcode.h"
#include "unacpp.h"
#include "termproc.h"
#include "rclutil.h"
#include "rclconfig.h"

using namespace std;

#define OPT_C 0x1   
#define OPT_c 0x2       
#define OPT_d 0x4       
#define OPT_I 0x8       
#define OPT_k 0x10      
#define OPT_l 0x20  
#define OPT_L 0x40  
#define OPT_n 0x80      
#define OPT_p 0x100     
#define OPT_q 0x200     
#define OPT_S 0x400     
#define OPT_s 0x800 
#define OPT_t 0x1000
#define OPT_u 0x2000     
#define OPT_w 0x4000    


static string thisprog;

static string usage =
    " textsplit [opts] [filename]\n"
    "   -I : use internal data. Else read filename or stdin if no param.\n"
    "   -q : no output\n"
    "   -d : print position and byte lists for input to hldata\n"
    "   -s :  only spans\n"
    "   -w :  only words\n"
    "   -n :  no numbers\n"
    "   -k :  preserve wildcards (?*)\n"
    "   -c : just count words\n"
    "   -u : use unac\n"
    "   -t [tagger] : korean tagger name (Mecab/Okt/Komoran)\n"
    "   -C [charset] : input charset\n"
    "   -S [stopfile] : stopfile to use for commongrams\n"
    "   -l <maxtermlen> : set max term length (bytes)\n"
    "   -L <loglevel> : set max term length (bytes)\n"
    "   -p somephrase : display results from stringToStrings()\n"
    "\n"
    "    if filename is 'stdin', will read stdin for data (end with ^D)\n"
    ;

static void
Usage(void)
{
    cerr << thisprog  << ": usage:\n" << usage;
    exit(1);
}

static int        op_flags;


class myTermProc : public Rcl::TermProc {
    int first;
    bool nooutput;
public:
    myTermProc() : TermProc(0), first(1), nooutput(false) {}
    void setNoOut(bool val) {nooutput = val;}
    virtual bool takeword(const string &term, int pos, int bs, int be) {
        m_plists[term].push_back(pos);
        m_gpostobytes[pos] = pair<int,int>(bs, be);
        if (nooutput)
            return true;
        FILE *fp = stdout;
        if (first) {
            fprintf(fp, "%3s %-20s %4s %4s\n", "pos", "Term", "bs", "be");
            first = 0;
        }
        fprintf(fp, "%3d %-20s %4d %4d\n", pos, term.c_str(), bs, be);
        return true;
    }

    void printpos() {
        cout << "{";
        for (const auto& lst : m_plists) {
            cout << "{\"" << lst.first << "\", {";
            for (int pos : lst.second) {
                cout << pos << ",";
            }
            cout << "}}, ";
        }
        cout << "};\n";
        cout << "{";
        for (const auto& ent : m_gpostobytes) {
            cout << "{" << ent.first << ", {";
            cout << ent.second.first << ", " << ent.second.second << "}}, ";
        }
        cout << "};\n";
    }
private:
    // group/near terms word positions.
    map<string, vector<int> > m_plists;
    map<int, pair<int, int> > m_gpostobytes;
};


bool dosplit(const string& data, TextSplit::Flags flags, int op_flags)
{
    myTermProc printproc;

    Rcl::TermProc *nxt = &printproc;

//    Rcl::TermProcCommongrams commonproc(nxt, stoplist);
//    if (op_flags & OPT_S)
//        nxt = &commonproc;

    Rcl::TermProcPrep preproc(nxt);
    if (op_flags & OPT_u) 
        nxt = &preproc;

    Rcl::TextSplitP splitter(nxt, flags);

    if (op_flags & OPT_q)
        printproc.setNoOut(true);

    splitter.text_to_words(data);
    if (op_flags & OPT_d) {
        printproc.printpos();
    }

#ifdef TEXTSPLIT_STATS
    TextSplit::Stats::Values v = splitter.getStats();
    cout << "Average length: " 
         <<  v.avglen
         << " Standard deviation: " 
         << v.sigma
         << " Coef of variation "
         << v.sigma / v.avglen
         << endl;
#endif
    return true;
}

static const char *teststrings[] = {
    "Un bout de texte \nnormal. 2eme phrase.3eme;quatrieme.\n",
    "\"Jean-Francois Dockes\" <jfd@okyz.com>\n",
    "n@d @net .net net@ t@v@c c# c++ o'brien 'o'brien'",
    "_network_ some_span",
    "data123\n",
    "134 +134 -14 0.1 .1 2. -1.5 +1.5 1,2 1.54e10 1,2e30 .1e10 1.e-8\n",
    "@^#$(#$(*)\n",
    "192.168.4.1 one\n\rtwo\r",
    "[olala][ululu]  (valeur) (23)\n",
    "utf-8 ucs-4© \\nodef\n",
    "A b C 2 . +",
    "','this\n",
    " ,able,test-domain",
    " -wl,--export-dynamic",
    " ~/.xsession-errors",
    "this_very_long_span_this_very_long_span_this_very_long_span",
    "soft\xc2\xadhyphen",
    "soft\xc2\xad\nhyphen",
    "soft\xc2\xad\n\rhyphen",
    "real\xe2\x80\x90hyphen",
    "real\xe2\x80\x90\nhyphen",
    "hyphen-\nminus",
};
const int teststrings_cnt = sizeof(teststrings)/sizeof(char *);

static string teststring1 = " nouvel-an ";

int main(int argc, char **argv)
{
    string charset, stopfile, kotagger;
    int loglevel = 4;
    int maxtermlen{-1};

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            case 'c':   op_flags |= OPT_c; break;
            case 'C':   op_flags |= OPT_C; if (argc < 2)  Usage();
                charset = *(++argv); argc--;  goto b1;
            case 'd':   op_flags |= OPT_d|OPT_q; break;
            case 'I':   op_flags |= OPT_I; break;
            case 'k':   op_flags |= OPT_k; break;
            case 'l':   op_flags |= OPT_l; if (argc < 2)  Usage();
                maxtermlen = atoi(*(++argv)); argc--; goto b1;
            case 'L':   op_flags |= OPT_L; if (argc < 2)  Usage();
                loglevel = atoi(*(++argv)); argc--; goto b1;
            case 'n':   op_flags |= OPT_n; break;
            case 'p':   op_flags |= OPT_p; break;
            case 'q':   op_flags |= OPT_q; break;
            case 's':   op_flags |= OPT_s; break;
            case 'S':   op_flags |= OPT_S; if (argc < 2)  Usage();
                stopfile = *(++argv); argc--; goto b1;
            case 't':   op_flags |= OPT_t; if (argc < 2)  Usage();
                kotagger = *(++argv); argc--; goto b1;
            case 'u':   op_flags |= OPT_u; break;
            case 'w':   op_flags |= OPT_w; break;
            default: Usage();   break;
            }
    b1: argc--; argv++;
    }

    TextSplit::Flags flags = TextSplit::TXTS_NONE;

    if (op_flags&OPT_s)
        flags = TextSplit::TXTS_ONLYSPANS;
    else if (op_flags&OPT_w)
        flags = TextSplit::TXTS_NOSPANS;
    if (op_flags & OPT_k) 
        flags = (TextSplit::Flags)(flags | TextSplit::TXTS_KEEPWILD); 


    // We need a configuration file, which we build in a temp file
    TempDir tmpconf;
    string cffn(path_cat(tmpconf.dirname(), "recoll.conf"));
    FILE *fp = fopen(cffn.c_str(), "w");
    fprintf(fp, "loglevel = %d\n", loglevel);
    if (op_flags & OPT_n) {
        fprintf(fp, "nonumbers = 1\n");
    }
    if (op_flags & OPT_l) {
        fprintf(fp, "maxtermlength = %d\n", maxtermlen);
    }
    if (!kotagger.empty()) {
        fprintf(fp, "hangultagger = %s\n", kotagger.c_str());
    }
    fprintf(fp, "underscoreasletter = 0\n");
    fclose(fp);

    Logger::getTheLog("")->setLogLevel(Logger::LogLevel(loglevel));

    string dn(tmpconf.dirname());
    RclConfig *config = new RclConfig(&dn);
    if (!config->ok()) {
        cerr << "Could not build configuration: " << config->getReason() <<endl;
    }

    TextSplit::staticConfInit(config);
    LOGDEB("Trtextsplit starting up\n");
    
    Rcl::StopList stoplist;
    if (op_flags & OPT_S) {
        if (!stoplist.setFile(stopfile)) {
            cerr << "Can't read stopfile: " << stopfile << endl;
            exit(1);
        }
    }

    if (op_flags & OPT_I) {
        if (argc)
            Usage();
        if (op_flags & OPT_p)
            Usage();
        for (int i = 0; i < teststrings_cnt; i++) {
            cout << endl << teststrings[i] << endl;  
            dosplit(teststrings[i], flags, op_flags);
        }
        exit(0);
    } else if (op_flags& OPT_p) {
        if (!argc)
            Usage();
        vector<string> tokens;
        TextSplit::stringToStrings(argv[0], tokens);
        for (vector<string>::const_iterator it = tokens.begin();
             it != tokens.end(); it++) {
            cout << "[" << *it << "] ";
        }
        cout << endl;
        exit(0);
    }


    string odata, reason;
    if (argc == 1) {
        const char *filename = *argv++; argc--;
        if (!file_to_string(filename, odata, &reason)) {
            cerr << "Failed: file_to_string(" << filename << ") failed: " 
                 << reason << endl;
            exit(1);
        }
    } else {
        char buf[1024];
        int nread;
        while ((nread = read(0, buf, 1024)) > 0) {
            odata.append(buf, nread);
        }
    }

    string& data = odata;
    string ndata;
    if ((op_flags & OPT_C)) {
        if (!transcode(odata, ndata, charset, "UTF-8")) {
            cerr << "Failed: transcode error" << endl;
            exit(1);
        } else {
            data = ndata;
        }
    }

    if (op_flags & OPT_c) {
        int n = TextSplit::countWords(data, flags);
        cout << n << " words" << endl;
    } else {
        dosplit(data, flags, op_flags);
    }    
}
