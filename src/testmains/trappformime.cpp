#else // TEST_APPFORMIME

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <string>
#include <iostream>
#include <vector>
using namespace std;

#include "appformime.h"

static char *thisprog;

static char usage [] =
"  appformime <mime type>\n\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

int main(int argc, char **argv)
{
  thisprog = argv[0];
  argc--; argv++;

  if (argc != 1)
    Usage();
  string mime = *argv++;argc--;

  string reason;
  vector<DesktopDb::AppDef> appdefs;
  DesktopDb *ddb = DesktopDb::getDb();
  if (ddb == 0) {
      cerr << "Could not create desktop db\n";
      exit(1);
  }
  if (!ddb->appForMime(mime, &appdefs, &reason)) {
      cerr << "appForMime failed: " << reason << endl;
      exit(1);
  }
  if (appdefs.empty()) {
      cerr << "No application found for [" << mime << "]" << endl;
      exit(1);
  }
  cout << mime << " -> ";
  for (vector<DesktopDb::AppDef>::const_iterator it = appdefs.begin();
       it != appdefs.end(); it++) {
      cout << "[" << it->name << ", " << it->command << "], ";
  }
  cout << endl;

  exit(0);
}
