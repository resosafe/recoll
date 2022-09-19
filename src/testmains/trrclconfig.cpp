/* Copyright (C) 2017-2019 J.F.Dockes
 *
 * License: GPL 2.1
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
#include <stdio.h>
#include <signal.h>

#include <iostream>
#include <vector>
#include <string>

using namespace std;

#include "log.h"

#include "rclinit.h"
#include "rclconfig.h"
#include "cstr.h"

void showFields(RclConfig *config)
{
    set<string> stored = config->getStoredFields();
    set<string> indexed = config->getIndexedFields();
    cout << "Stored fields: ";
    for (const auto& field: stored) {
        cout << "[" << field << "] ";
    }
    cout << "\n";   
    cout << "Indexed fields: ";
    for (const auto& field : indexed) {
        const FieldTraits *ftp;
        config->getFieldTraits(field, &ftp);
        if (ftp)
            cout << "[" << field << "]" << " -> [" << ftp->pfx << "] ";
        else 
            cout << "[" << field << "]" << " -> [" << "(none)" << "] ";

    }
    cout << "\n";   
}

void checkMtypesConsistency(RclConfig *config)
{
    // Checking the configuration consistency
    
    // Find and display category names
    vector<string> catnames;
    config->getMimeCategories(catnames);
    cout << "Categories: ";
    for (const auto& catg : catnames) {
        cout << catg << " ";
    }
    cout << "\n";

    // Compute union of all types from each category. Check that there
    // are no duplicates while we are at it.
    set<string> allmtsfromcats;
    for (const auto& catg : catnames) {
        vector<string> cts;
        config->getMimeCatTypes(catg, cts);
        for (const auto& cattype : cts) {
            // Already in map -> duplicate
            if (allmtsfromcats.find(cattype) != allmtsfromcats.end()) {
                cout << "Duplicate: [" << cattype << "]" << "\n";
            }
            allmtsfromcats.insert(cattype);
        }
    }

    // Retrieve complete list of mime types 
    vector<string> mtypes = config->getAllMimeTypes();

    // And check that each mime type is found in exactly one category
    // And have an icon
    for (const auto& mtype: mtypes) {
        if (allmtsfromcats.find(mtype) == allmtsfromcats.end()) {
            cout << "Not found in catgs: [" << mtype << "]" << "\n";
        }
        // We'd like to check for types with no icons, but
        // getMimeIconPath() returns the valid 'document' by
        // default, we'd have to go look into the confsimple
        // directly.
        // string path = config->getMimeIconPath(mtype, string());
        // cout << mtype << " -> " << path << "\n";
    }
        
    // List mime types not in mimeview
    for (const auto & mtype : mtypes) {
        if (config->getMimeViewerDef(mtype, "", false).empty()) {
            cout << "No viewer: [" << mtype << "]" << "\n";
        }
    }

    // Check that each mime type has an indexer
    for (const auto & mtype : mtypes) {
        if (config->getMimeHandlerDef(mtype, false).empty()) {
            cout << "No filter: [" << mtype << "]" << "\n";
        }
    }

    // Check that each mime type has a defined icon
    for (const auto & mtype : mtypes) {
        if (config->getMimeIconPath(mtype, "") == "document") {
            cout << "No or generic icon: [" << mtype << "]" << "\n";
        }
    }
}



static char *thisprog;

static char usage [] = "\n"
    " -h : print help\n"
    "-c: check a few things in the configuration files\n"
    "[-s subkey] -q param : query parameter value\n"
    "-f : print some field data\n"
    "-l : list recoll.conf parameters\n"
    "  : default: nothing\n"
    ;
static void
Usage(FILE *fp = stderr)
{
    fprintf(fp, "%s: usage: %s\n", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_s     0x2 
#define OPT_q     0x4 
#define OPT_c     0x8
#define OPT_f     0x10
#define OPT_l     0x20
#define OPT_h     0x40

int main(int argc, char **argv)
{
    string pname, skey;
    
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
            case 'f':   op_flags |= OPT_f; break;
            case 'h':   op_flags |= OPT_h;
                Usage(stdout);
                break;
            case 'l':   op_flags |= OPT_l; break;
            case 's':   op_flags |= OPT_s; if (argc < 2)  Usage();
                skey = *(++argv);
                argc--; 
                goto b1;
            case 'q':   op_flags |= OPT_q; if (argc < 2)  Usage();
                pname = *(++argv);
                argc--; 
                goto b1;
            default: Usage();   break;
            }
    b1: argc--; argv++;
    }

    if (argc != 0)
        Usage();

    string reason;
    RclConfig *config = recollinit(0, 0, 0, reason);
    if (config == 0 || !config->ok()) {
        cerr << "Configuration problem: " << reason << "\n";
        exit(1);
    }
    if (op_flags & OPT_s)
        config->setKeyDir(skey);
    if (op_flags & OPT_q) {
        string value;
        if (!config->getConfParam(pname, value)) {
            fprintf(stderr, "getConfParam failed for [%s]\n", pname.c_str());
            exit(1);
        }
        printf("[%s] -> [%s]\n", pname.c_str(), value.c_str());
    } else if (op_flags & OPT_f) {
        showFields(config);
    } else if (op_flags & OPT_c) {
        checkMtypesConsistency(config);
    } else  if (op_flags & OPT_l) {
        config->setKeyDir(cstr_null);
        vector<string> names = config->getConfNames();
        for (const auto& name : names) {
            string value;
            config->getConfParam(name, value);
            cout << name << " -> [" << value << "]" << "\n";
        }
    } else if (1) {

        std::vector<std::string> samples {
            "ebook-viewer %F;ignoreipath=1",
            "rclshowinfo %F %(title);ignoreipath=1",
            "xterm -u8 -e \"groff -T ascii -man %f | more\"",
            "xterm -e \"info -f %f\"",

            "\"C:/Program Files/Tracker Software/PDF Editor/PDFXEdit.exe\" /A "
            "\"page=%p;search=%s\" \"%f\"",

            "\"C:/Program Files/Tracker Software/PDF Editor/PDFXEdit.exe\" /A "
            "\"page=%p;search=\\\"%s\\\"\" \"%f\"",
            
            "\"C:/Program Files/Tracker Software/PDF Editor/PDFXEdit.exe\" /A "
            "\"page=%p;search=%s\" \"%f\"; someattr = somevalue;quotedattr = \"hello world\"",
        };

        for (const auto& sample: samples) {
            string value;
            ConfSimple attrs;
            config->valueSplitAttributes(sample, value, attrs);
            std::cout << "Input: ["<< sample << "]\n";
            std::cout << "Value: [" << value << "]\n";
            std::cout << "Attrs: ";
            for (const auto& nm : attrs.getNames("")) {
                std::string attr;
                attrs.get(nm, attr);
                std::cout << "{[" << nm << "] -> [" << attr << "]} ";
            }
            std::cout << "\n\n";
        }
    }
    return 0;
}
