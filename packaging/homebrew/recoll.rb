require 'formula'

# Notes:
# - This formula is missing python-libxml2 and python-libxslt deps
#   which recoll needs for indexing many formats (e.g. libreoffice,
#   openxml). Homebrew does not include these packages.
#   So the user needs to install them with pip because I don't understand how
#   the "Resource" homebrew thing works.
# Still a bit of work then, but I did not investigate, because the macports
# version was an easier target.

class Recoll < Formula
  desc "Desktop search tool"
  homepage 'http://www.recoll.org'
  url 'http://www.recoll.org/recoll-1.24.4.tar.gz'
  sha256 "989c1ecdce36082020c0d404a6223005cd166011f9c9b28ac762af978ef9392c"

  depends_on "xapian"
  depends_on "qt"
  depends_on "antiword"
  depends_on "poppler"
  depends_on "unrtf"
  
  patch :p0, :DATA
  def install
    # homebrew has webengine, not webkit and we're not ready for this yet
    system "./configure", "--disable-python-module",
                          "--disable-webkit",
                          "QMAKE=/usr/local/opt/qt/bin/qmake",
                          "--prefix=#{prefix}"
    system "make", "install"
    bin.install "#{buildpath}/qtgui/recoll.app/Contents/MacOS/recoll"
  end

  test do
    system "#{bin}/recollindex", "-h"
  end
end

__END__
--- Makefile.in	2018-11-15 19:07:37.000000000 +0100
+++ Makefile.in.new	2018-11-29 17:08:19.000000000 +0100
@@ -720,8 +720,7 @@
 # We use -release: the lib is only shared
 # between recoll programs from the same release.
 # -version-info $(VERSION_INFO)
-librecoll_la_LDFLAGS = -release $(VERSION) \
-    -Wl,--no-undefined -Wl,--warn-unresolved-symbols
+librecoll_la_LDFLAGS = -release $(VERSION)
 
 librecoll_la_LIBADD = $(LIBXAPIAN) $(LIBICONV) $(LIBTHREADS)
 recollindex_SOURCES = \
diff --git filters/ppt-dump.py filters/ppt-dump.py
index f41a9f39..dc3085a4 100755
--- filters/ppt-dump.py
+++ filters/ppt-dump.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
diff --git filters/rcl7z.py filters/rcl7z.py
index c68c8bcb..ac50c4ec 100755
--- filters/rcl7z.py
+++ filters/rcl7z.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 
 # 7-Zip file filter for Recoll
 
diff --git filters/rclaudio.py filters/rclaudio.py
index 94ca0be7..08d6375a 100755
--- filters/rclaudio.py
+++ filters/rclaudio.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 
 # Audio tag filter for Recoll, using mutagen
 
diff --git filters/rclchm.py filters/rclchm.py
index f9811c37..3bc9b16d 100755
--- filters/rclchm.py
+++ filters/rclchm.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 """Extract Html files from a Microsoft Compiled Html Help file (.chm)
 Needs at least python 2.2 for HTMLParser (chmlib needs 2.2 too)"""
 
diff --git filters/rcldia.py filters/rcldia.py
index 282148eb..a480294b 100755
--- filters/rcldia.py
+++ filters/rcldia.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 # -*- coding: utf-8 -*-
 from __future__ import print_function
 
diff --git filters/rcldjvu.py filters/rcldjvu.py
index c5397195..0be01452 100755
--- filters/rcldjvu.py
+++ filters/rcldjvu.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 # Copyright (C) 2016 J.F.Dockes
 # This program is free software; you can redistribute it and/or modify
 # it under the terms of the GNU General Public License as published by
diff --git filters/rcldoc.py filters/rcldoc.py
index e8fa1831..b92b185d 100755
--- filters/rcldoc.py
+++ filters/rcldoc.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 from __future__ import print_function
 
 import rclexecm
diff --git filters/rclepub.py filters/rclepub.py
index 8042d7f9..51786af1 100755
--- filters/rclepub.py
+++ filters/rclepub.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 """Extract Html content from an EPUB file (.epub)"""
 from __future__ import print_function
 
diff --git filters/rclepub.py1 filters/rclepub.py1
index bd44f635..a7ea6c06 100755
--- filters/rclepub.py1
+++ filters/rclepub.py1
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 """Extract Html content from an EPUB file (.chm), concatenating all sections"""
 from __future__ import print_function
 
diff --git filters/rclics.py filters/rclics.py
index 0ef04f2d..de177024 100755
--- filters/rclics.py
+++ filters/rclics.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 from __future__ import print_function
 
 # Read an ICS file, break it into "documents" which are events, todos,
diff --git filters/rclimg.py filters/rclimg.py
index 7eb1da91..4eb6c9b0 100755
--- filters/rclimg.py
+++ filters/rclimg.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 
 # Python-based Image Tag extractor for Recoll. This is less thorough
 # than the Perl-based rclimg script, but useful if you don't want to
diff --git filters/rclinfo.py filters/rclinfo.py
index f353d19e..36cf34e0 100755
--- filters/rclinfo.py
+++ filters/rclinfo.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 
 # Read a file in GNU info format and output its nodes as subdocs,
 # interfacing with recoll execm
diff --git filters/rclkar.py filters/rclkar.py
index d6570dd5..34b8d2a2 100755
--- filters/rclkar.py
+++ filters/rclkar.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 
 # Read a .kar midi karaoke file and translate to recoll indexable format
 # This does not work with Python3 yet because python:midi doesn't 
diff --git filters/rcllatinclass.py filters/rcllatinclass.py
index 3f8b8634..e6b0fbee 100755
--- filters/rcllatinclass.py
+++ filters/rcllatinclass.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 """Try to guess a text's language and character set by checking how it matches lists of
 common words. This is not a primary method of detection because it's slow and unreliable, but it
 may be a help in discrimating, for exemple, before european languages using relatively close
diff --git filters/rclopxml.py filters/rclopxml.py
index b7f7fe83..4f1803c1 100755
--- filters/rclopxml.py
+++ filters/rclopxml.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 # Copyright (C) 2015 J.F.Dockes
 #   This program is free software; you can redistribute it and/or modify
 #   it under the terms of the GNU General Public License as published by
diff --git filters/rclpdf.py filters/rclpdf.py
index 1e6852ea..47b09534 100755
--- filters/rclpdf.py
+++ filters/rclpdf.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 # Copyright (C) 2014 J.F.Dockes
 # This program is free software; you can redistribute it and/or modify
 # it under the terms of the GNU General Public License as published by
diff --git filters/rclppt.py filters/rclppt.py
index a4e50265..993bc56c 100755
--- filters/rclppt.py
+++ filters/rclppt.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 
 # Recoll PPT text extractor
 # Mso-dumper is not compatible with Python3. We use sys.executable to
diff --git filters/rclpython filters/rclpython
index 615455b3..1e411890 100755
--- filters/rclpython
+++ filters/rclpython
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 # -*- coding: iso-8859-1 -*-
 """
     MoinMoin - Python source parser and colorizer
diff --git filters/rclrar.py filters/rclrar.py
index 8f723fa5..5f6adfb0 100755
--- filters/rclrar.py
+++ filters/rclrar.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 
 # Rar file filter for Recoll
 # Adapted from the Zip archive filter by mroark.
diff --git filters/rclrtf.py filters/rclrtf.py
index e4b56d54..ffd0560e 100755
--- filters/rclrtf.py
+++ filters/rclrtf.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 from __future__ import print_function
 
 import rclexecm
diff --git filters/rclsoff-flat.py filters/rclsoff-flat.py
index 337a5f94..65bfa73a 100755
--- filters/rclsoff-flat.py
+++ filters/rclsoff-flat.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 # Copyright (C) 2014 J.F.Dockes
 #   This program is free software; you can redistribute it and/or modify
 #   it under the terms of the GNU General Public License as published by
diff --git filters/rclsoff.py filters/rclsoff.py
index 5730d97c..4404a14b 100755
--- filters/rclsoff.py
+++ filters/rclsoff.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 # Copyright (C) 2014 J.F.Dockes
 #   This program is free software; you can redistribute it and/or modify
 #   it under the terms of the GNU General Public License as published by
diff --git filters/rclsvg.py filters/rclsvg.py
index 8c1b8aea..cee17324 100755
--- filters/rclsvg.py
+++ filters/rclsvg.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 # Copyright (C) 2014 J.F.Dockes
 #   This program is free software; you can redistribute it and/or modify
 #   it under the terms of the GNU General Public License as published by
diff --git filters/rcltar.py filters/rcltar.py
index d8bf100d..ab4b306e 100755
--- filters/rcltar.py
+++ filters/rcltar.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 
 # Tar-file filter for Recoll
 # Thanks to Recoll user Martin Ziegler
diff --git filters/rcltext.py filters/rcltext.py
index 77359ff6..be410984 100755
--- filters/rcltext.py
+++ filters/rcltext.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 
 # Wrapping a text file. Recoll does it internally in most cases, but
 # this is for use by another filter.
diff --git filters/rcltxtlines.py filters/rcltxtlines.py
index 220151fd..b2907364 100755
--- filters/rcltxtlines.py
+++ filters/rcltxtlines.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 """Index text lines as document (execm handler sample). This exists
 to demonstrate the execm interface and is not meant to be useful or
 efficient"""
diff --git filters/rcluncomp.py filters/rcluncomp.py
index 32a11c1a..eab3b257 100644
--- filters/rcluncomp.py
+++ filters/rcluncomp.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 from __future__ import print_function
 
 import rclexecm
diff --git filters/rclwar.py filters/rclwar.py
index b654f3b3..301e28e9 100755
--- filters/rclwar.py
+++ filters/rclwar.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 
 # WAR web archive filter for recoll. War file are gzipped tar files
 
diff --git filters/rclxls.py filters/rclxls.py
index c7b2343a..f8f10f8b 100755
--- filters/rclxls.py
+++ filters/rclxls.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 
 # Extractor for Excel files.
 # Mso-dumper is not compatible with Python3. We use sys.executable to
diff --git filters/rclxml.py filters/rclxml.py
index 33ae8e3e..507851db 100755
--- filters/rclxml.py
+++ filters/rclxml.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 # Copyright (C) 2014 J.F.Dockes
 #   This program is free software; you can redistribute it and/or modify
 #   it under the terms of the GNU General Public License as published by
diff --git filters/rclxmp.py filters/rclxmp.py
index 158e1222..602769af 100755
--- filters/rclxmp.py
+++ filters/rclxmp.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 # Copyright (C) 2016 J.F.Dockes
 #   This program is free software; you can redistribute it and/or modify
 #   it under the terms of the GNU General Public License as published by
diff --git filters/rclzip.py filters/rclzip.py
index 35739625..0c597fbd 100755
--- filters/rclzip.py
+++ filters/rclzip.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 # Copyright (C) 2014 J.F.Dockes
 #   This program is free software; you can redistribute it and/or modify
 #   it under the terms of the GNU General Public License as published by
diff --git filters/xls-dump.py filters/xls-dump.py
index abffa330..57a8f113 100755
--- filters/xls-dump.py
+++ filters/xls-dump.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
diff --git filters/xlsxmltocsv.py filters/xlsxmltocsv.py
index 0c9a5047..90ab06f2 100755
--- filters/xlsxmltocsv.py
+++ filters/xlsxmltocsv.py
@@ -1,4 +1,4 @@
-#!/usr/bin/env python2
+#!/usr/bin/env python2.7
 # Copyright (C) 2015 J.F.Dockes
 #   This program is free software; you can redistribute it and/or modify
 #   it under the terms of the GNU General Public License as published by
diff --git aspell/rclaspell.cpp aspell/rclaspell.cpp-new
index 698832df..4fd8b6b8 100644
--- aspell/rclaspell.cpp
+++ aspell/rclaspell.cpp-new
@@ -71,12 +71,16 @@ static std::mutex o_aapi_mutex;
 	badnames += #NM + string(" ");					\
     }
 
-static const char *aspell_lib_suffixes[] = {
-  ".so",
-  ".so.15",
-  ".so.16"
+static const vector<string> aspell_lib_suffixes {
+#if defined(__APPLE__) 
+        ".15.dylib",
+        ".dylib",
+#else
+        ".so",
+        ".so.15",
+        ".so.16",
+#endif
 };
-static const unsigned int nlibsuffs  = sizeof(aspell_lib_suffixes) / sizeof(char *);
 
 // Stuff that we don't wish to see in the .h (possible sysdeps, etc.)
 class AspellData {
@@ -160,16 +164,39 @@ bool Aspell::init(string &reason)
 	return false;
     }
 
+
+    // Don't know what with Apple and (DY)LD_LIBRARY_PATH. Does not work
+    // So we look in all ../lib in the PATH...
+#if defined(__APPLE__) 
+    vector<string> path;
+    const char *pp = getenv("PATH");
+    if (pp) {
+        stringToTokens(pp, path, ":");
+    }
+#endif
+    
     reason = "Could not open shared library ";
     string libbase("libaspell");
     string lib;
-    for (unsigned int i = 0; i < nlibsuffs; i++) {
-        lib = libbase + aspell_lib_suffixes[i];
+    for (const auto& suff : aspell_lib_suffixes) {
+        lib = libbase + suff;
         reason += string("[") + lib + "] ";
         if ((m_data->m_handle = dlopen(lib.c_str(), RTLD_LAZY)) != 0) {
             reason.erase();
             goto found;
         }
+#if defined(__APPLE__) 
+        // Above was the normal lookup: let dlopen search the directories.
+        // Here is for Apple. Also look at all ../lib along the PATH
+        for (const auto& dir : path) {
+            string lib1 = path_canon(dir + "/../lib/" + lib);
+            if ((m_data->m_handle = dlopen(lib1.c_str(), RTLD_LAZY)) != 0) {
+                reason.erase();
+                lib=lib1;
+                goto found;
+            }
+        }
+#endif
     }
     
  found:
--- sampleconf/mimeview	2018-11-29 13:33:09.000000000 +0100
+++ sampleconf/mimeview.mac	2018-11-29 16:38:52.000000000 +0100
@@ -2,7 +2,8 @@
 
 ## ##########################################
 # External viewers, launched by the recoll GUI when you click on a result
-# 'edit' link
+# 'Open' link - MAC version
+# On the MAC, we use "open" for everything...
 
 # Mime types which we should not uncompress if they are found gzipped or
 # bzipped because the native viewer knows how to handle. These would be
@@ -16,22 +17,17 @@
 #    search string
 #  - For pages of CHM and EPUB documents where we can choose to open the 
 #    parent document instead of a temporary html file.
-# Use xallexcepts- and xallexcepts+ in a user file to add or remove from
-# the default xallexcepts list
-
-xallexcepts = application/pdf application/postscript application/x-dvi \
- text/html|gnuinfo text/html|chm text/html|epub \
- application/x-fsdirectory|parentopen inode/directory|parentopen
-
+#xallexcepts = application/pdf application/postscript application/x-dvi \
+#            text/html|gnuinfo text/html|chm text/html|epub
 
 [view]
 # Pseudo entry used if the 'use desktop' preference is set in the GUI
-application/x-all = xdg-open %u
+application/x-all = open %f
 
 application/epub+zip = ebook-viewer %f
-# Open the parent epub document for epub parts instead of opening them as
-# html documents. This is almost always what we want.
-text/html|epub = ebook-viewer %F;ignoreipath=1
+# If you want to open the parent epub document for epub parts instead of
+# opening them as html documents:
+#text/html|epub = ebook-viewer %F;ignoreipath=1
 
 application/x-gnote = gnote %f
 
@@ -146,12 +142,11 @@
 application/zip = ark %f
 application/x-7z-compressed = ark %f
 
-application/x-awk = emacsclient --no-wait %f
-application/x-perl = emacsclient --no-wait %f
-text/x-perl = emacsclient --no-wait %f
-application/x-shellscript = emacsclient --no-wait %f
-text/x-shellscript = emacsclient --no-wait %f
-text/x-srt = emacsclient --no-wait %f
+application/x-awk = emacsclient  %f
+application/x-perl = emacsclient  %f
+text/x-perl = emacsclient  %f
+application/x-shellscript = emacsclient  %f
+text/x-shellscript = emacsclient  %f
 
 # Or firefox -remote "openFile(%u)"
 text/html = firefox %u
@@ -163,16 +158,15 @@
 
 application/x-webarchive = konqueror %f
 text/x-fictionbook = ebook-viewer %f
-application/x-tex = emacsclient --no-wait  %f
-application/xml = emacsclient --no-wait  %f
-text/xml = emacsclient --no-wait  %f
-text/x-tex = emacsclient --no-wait  %f
-text/plain = emacsclient --no-wait  %f
-text/x-awk = emacsclient --no-wait  %f
-text/x-c = emacsclient --no-wait  %f
-text/x-lua = emacsclient --no-wait  %f
-text/x-c+ = emacsclient --no-wait  %f
-text/x-c++ = emacsclient --no-wait  %f
+application/x-tex = emacsclient  %f
+application/xml = emacsclient  %f
+text/xml = emacsclient  %f
+text/x-tex = emacsclient  %f
+text/plain = emacsclient  %f
+text/x-awk = emacsclient  %f
+text/x-c = emacsclient  %f
+text/x-c+ = emacsclient  %f
+text/x-c++ = emacsclient  %f
 text/x-csv = libreoffice %f
 text/x-html-sidux-man = konqueror %f
 text/x-html-aptosid-man = iceweasel %f
@@ -183,22 +177,21 @@
 # file at the right place
 text/html|chm = kchmviewer --url %i %F
 
-text/x-ini = emacsclient --no-wait %f
+text/x-ini = emacsclient %f
 text/x-man = xterm -u8 -e "groff -T ascii -man %f | more"
 text/x-python = idle %f
-text/x-gaim-log = emacsclient --no-wait  %f
-text/x-purple-html-log = emacsclient --no-wait  %f
-text/x-purple-log = emacsclient --no-wait  %f
+text/x-gaim-log = emacsclient  %f
+text/x-purple-html-log = emacsclient  %f
+text/x-purple-log = emacsclient  %f
 
 # The video types will usually be handled by the desktop default, but they
 # need entries here to get an "Open" link
-video/3gpp = vlc %f
-video/mp2p = vlc %f
-video/mp2t = vlc %f
-video/mp4 = vlc %f
-video/mpeg = vlc %f
-video/quicktime = vlc %f
-video/x-matroska = vlc %f
-video/x-ms-asf = vlc %f
-video/x-msvideo = vlc %f
-
+video/3gpp = open %f
+video/mp2p = open %f
+video/mp2t = open %f
+video/mp4 = open %f
+video/mpeg = open %f
+video/quicktime = open %f
+video/x-matroska = open %f
+video/x-ms-asf = open %f
+video/x-msvideo = open %f
