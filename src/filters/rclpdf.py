#!/usr/bin/env python
# Copyright (C) 2014 J.F.Dockes
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the
# Free Software Foundation, Inc.,
# 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

# Recoll PDF extractor, with support for attachments
#
# pdftotext sometimes outputs unescaped text inside HTML text sections.
# We try to correct.
#
# If pdftotext produces no text and tesseract is available, we try to
# perform OCR. As this can be very slow and the result not always
# good, we only do this if a file named $RECOLL_CONFDIR/ocrpdf exists
#
# We guess the OCR language in order of preference:
#  - From the content of a ".ocrpdflang" file if it exists in the same
#    directory as the PDF
#  - From an RECOLL_TESSERACT_LANG environment variable
#  - From the content of $RECOLL_CONFDIR/ocrpdf
#  - Default to "eng"

from __future__ import print_function

import os
import sys
import re
import rclexecm
import subprocess
import tempfile
import atexit
import signal
import rclconfig
import glob

tmpdir = None

def finalcleanup():
    if tmpdir:
        vacuumdir(tmpdir)
        os.rmdir(tmpdir)

def signal_handler(signal, frame):
    sys.exit(1)

atexit.register(finalcleanup)

# Not all signals necessary exist on all systems, use catch
try: signal.signal(signal.SIGHUP, signal_handler)
except: pass
try: signal.signal(signal.SIGINT, signal_handler)
except: pass
try: signal.signal(signal.SIGQUIT, signal_handler)
except: pass
try: signal.signal(signal.SIGTERM, signal_handler)
except: pass

def vacuumdir(dir):
    if dir:
        for fn in os.listdir(dir):
            path = os.path.join(dir, fn)
            if os.path.isfile(path):
                os.unlink(path)
    return True

class PDFExtractor:
    def __init__(self, em):
        self.currentindex = 0
        self.pdftotext = None
        self.em = em

        self.confdir = rclconfig.RclConfig().getConfDir()
        cf_doocr = rclconfig.RclConfig().getConfParam("pdfocr")
        cf_attach = rclconfig.RclConfig().getConfParam("pdfattach")
        
        self.pdftotext = rclexecm.which("pdftotext")
        if not self.pdftotext:
            self.pdftotext = rclexecm.which("poppler/pdftotext")

        # Check if we need to escape portions of text where old
        # versions of pdftotext output raw HTML special characters.
        self.needescape = True
        try:
            version = subprocess.check_output([self.pdftotext, "-v"],
                                              stderr=subprocess.STDOUT)
            major,minor,rev = version.split()[2].split('.')
            # Don't know exactly when this changed but it's fixed in
            # jessie 0.26.5
            if int(major) > 0 or int(minor) >= 26:
                self.needescape = False
        except:
            pass
        
        # See if we'll try to perform OCR. Need the commands and the
        # either the presence of a file in the config dir (historical)
        # or a set config variable.
        self.ocrpossible = False
        if cf_doocr or os.path.isfile(os.path.join(self.confdir, "ocrpdf")):
            self.tesseract = rclexecm.which("tesseract")
            if self.tesseract:
                self.pdftoppm = rclexecm.which("pdftoppm")
                if self.pdftoppm:
                    self.ocrpossible = True
                    self.maybemaketmpdir()
        # self.em.rclog("OCRPOSSIBLE: %d" % self.ocrpossible)

        # Pdftk is optionally used to extract attachments. This takes
        # a hit on perfmance even in the absence of any attachments,
        # so it can be disabled in the configuration.
        self.attextractdone = False
        self.attachlist = []
        if cf_attach:
            self.pdftk = rclexecm.which("pdftk")
        else:
            self.pdftk = None
        if self.pdftk:
            self.maybemaketmpdir()
        
    # Extract all attachments if any into temporary directory
    def extractAttach(self):
        if self.attextractdone:
            return True
        self.attextractdone = True

        global tmpdir
        if not tmpdir or not self.pdftk:
            # no big deal
            return True

        try:
            vacuumdir(tmpdir)
            subprocess.check_call([self.pdftk, self.filename, "unpack_files",
                                   "output", tmpdir])
            self.attachlist = sorted(os.listdir(tmpdir))
            return True
        except Exception as e:
            self.em.rclog("extractAttach: failed: %s" % e)
            # Return true anyway, pdf attachments are no big deal
            return True

    def extractone(self, ipath):
        #self.em.rclog("extractone: [%s]" % ipath)
        if not self.attextractdone:
            if not self.extractAttach():
                return (False, "", "", rclexecm.RclExecM.eofnow)
        path = os.path.join(tmpdir, ipath)
        if os.path.isfile(path):
            f = open(path)
            docdata = f.read();
            f.close()
        if self.currentindex == len(self.attachlist) - 1:
            eof = rclexecm.RclExecM.eofnext
        else:
            eof = rclexecm.RclExecM.noteof
        return (True, docdata, ipath, eof)


    # Try to guess tesseract language. This should depend on the input
    # file, but we have no general way to determine it. So use the
    # environment and hope for the best.
    def guesstesseractlang(self):
        tesseractlang = ""
        pdflangfile = os.path.join(os.path.dirname(self.filename), ".ocrpdflang")
        if os.path.isfile(pdflangfile):
            tesseractlang = open(pdflangfile, "r").read().strip()
        if tesseractlang:
            return tesseractlang

        tesseractlang = os.environ.get("RECOLL_TESSERACT_LANG", "");
        if tesseractlang:
            return tesseractlang
        
        tesseractlang = \
                      open(os.path.join(self.confdir, "ocrpdf"), "r").read().strip()
        if tesseractlang:
            return tesseractlang

        # Half-assed trial to guess from LANG then default to english
        localelang = os.environ.get("LANG", "").split("_")[0]
        if localelang == "en":
            tesseractlang = "eng"
        elif localelang == "de":
            tesseractlang = "deu"
        elif localelang == "fr":
            tesseractlang = "fra"
        if tesseractlang:
            return tesseractlang

        if not tesseractlang:
            tesseractlang = "eng"
        return tesseractlang

    # PDF has no text content and tesseract is available. Give OCR a try
    def ocrpdf(self):

        global tmpdir
        if not tmpdir:
            return ""

        tesseractlang = self.guesstesseractlang()
        # self.em.rclog("tesseractlang %s" % tesseractlang)

        tesserrorfile = os.path.join(tmpdir, "tesserrorfile")
        tmpfile = os.path.join(tmpdir, "ocrXXXXXX")

        # Split pdf pages
        try:
            vacuumdir(tmpdir)
            subprocess.check_call([self.pdftoppm, "-r", "300", self.filename,
                                   tmpfile])
        except Exception as e:
            self.em.rclog("pdftoppm failed: %s" % e)
            return ""

        files = glob.glob(tmpfile + "*")
        for f in files:
            try:
                out = subprocess.check_output([self.tesseract, f, f, "-l",
                                               tesseractlang],
                                              stderr = subprocess.STDOUT)
            except Exception as e:
                self.em.rclog("tesseract failed: %s" % e)

            errlines = out.split('\n')
            if len(errlines) > 2:
                self.em.rclog("Tesseract error: %s" % out)

        # Concatenate the result files
        files = glob.glob(tmpfile + "*" + ".txt")
        data = ""
        for f in files:
            data += open(f, "r").read()

        if not data:
            return ""
        return '''<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"></head><body><pre>''' + \
        self.em.htmlescape(data) + \
        '''</pre></body></html>'''

    # pdftotext (used to?) badly escape text inside the header
    # fields. We do it here. This is not an html parser, and depends a
    # lot on the actual format output by pdftotext.
    # We also determine if the doc has actual content, for triggering OCR
    def _fixhtml(self, input):
        #print input
        inheader = False
        inbody = False
        didcs = False
        output = b''
        isempty = True
        for line in input.split(b'\n'):
            if re.search(b'</head>', line):
                inheader = False
            if re.search(b'</pre>', line):
                inbody = False
            if inheader:
                if not didcs:
                    output += b'<meta http-equiv="Content-Type"' + \
                              b'content="text/html; charset=UTF-8">\n'
                    didcs = True
                if self.needescape:
                    m = re.search(b'''(.*<title>)(.*)(<\/title>.*)''', line)
                    if not m:
                        m = re.search(b'''(.*content=")(.*)(".*/>.*)''', line)
                    if m:
                        line = m.group(1) + self.em.htmlescape(m.group(2)) + \
                               m.group(3)

                # Recoll treats "Subject" as a "title" element
                # (based on emails). The PDF "Subject" metadata
                # field is more like an HTML "description"
                line = re.sub(b'name="Subject"', b'name="Description"', line, 1)

            elif inbody:
                s = line[0:1]
                if s != "\x0c" and s != "<":
                    isempty = False
                # We used to remove end-of-line hyphenation (and join
                # lines), but but it's not clear that we should do
                # this as pdftotext without the -layout option does it ?
                line = self.em.htmlescape(line)

            if re.search(b'<head>', line):
                inheader = True
            if re.search(b'<pre>', line):
                inbody = True

            output += line + b'\n'

        return output, isempty
            
    def _selfdoc(self):
        self.em.setmimetype('text/html')

        if self.attextractdone and len(self.attachlist) == 0:
            eof = rclexecm.RclExecM.eofnext
        else:
            eof = rclexecm.RclExecM.noteof
            
        data = subprocess.check_output([self.pdftotext, "-htmlmeta", "-enc",
                                        "UTF-8", "-eol", "unix", "-q",
                                        self.filename, "-"])

        data, isempty = self._fixhtml(data)
        #self.em.rclog("ISEMPTY: %d : data: \n%s" % (isempty, data))
        if isempty and self.ocrpossible:
            data = self.ocrpdf()
        return (True, data, "", eof)

    def maybemaketmpdir(self):
        global tmpdir
        if tmpdir:
            if not vacuumdir(tmpdir):
                self.em.rclog("openfile: vacuumdir %s failed" % tmpdir)
                return False
        else:
            tmpdir = tempfile.mkdtemp(prefix='rclmpdf')
        
    ###### File type handler api, used by rclexecm ---------->
    def openfile(self, params):
        self.filename = params["filename:"]
        #self.em.rclog("openfile: [%s]" % self.filename)
        self.currentindex = -1
        self.attextractdone = False

        if not self.pdftotext:
            print("RECFILTERROR HELPERNOTFOUND pdftotext")
            sys.exit(1);

        if self.pdftk:
            preview = os.environ.get("RECOLL_FILTER_FORPREVIEW", "no")
            if preview != "yes":
                # When indexing, extract attachments at once. This
                # will be needed anyway and it allows generating an
                # eofnext error instead of waiting for actual eof,
                # which avoids a bug in recollindex up to 1.20
                self.extractAttach()
        else:
            self.attextractdone = True
        return True

    def getipath(self, params):
        ipath = params["ipath:"]
        ok, data, ipath, eof = self.extractone(ipath)
        return (ok, data, ipath, eof)
        
    def getnext(self, params):
        # self.em.rclog("getnext: current %d" % self.currentindex)
        if self.currentindex == -1:
            self.currentindex = 0
            return self._selfdoc()
        else:
            self.em.setmimetype('')

            if not self.attextractdone:
                if not self.extractAttach():
                    return (False, "", "", rclexecm.RclExecM.eofnow)

            if self.currentindex >= len(self.attachlist):
                return (False, "", "", rclexecm.RclExecM.eofnow)
            try:
                ok, data, ipath, eof = \
                    self.extractone(self.attachlist[self.currentindex])
                self.currentindex += 1

                #self.em.rclog("getnext: returning ok for [%s]" % ipath)
                return (ok, data, ipath, eof)
            except:
                return (False, "", "", rclexecm.RclExecM.eofnow)


# Main program: create protocol handler and extractor and run them
proto = rclexecm.RclExecM()
extract = PDFExtractor(proto)
rclexecm.main(proto, extract)
