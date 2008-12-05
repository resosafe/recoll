#ifndef lint
static char	rcsid[] = "@(#$Id: csguess.cpp,v 1.7.4.1 2008-12-05 13:13:02 dockes Exp $ (C) 2004 J.F.Dockes";
#endif
/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef TEST_CSGUESS

// This code was converted from estraier / qdbm / myconf.c:

/**************************************************************************
 * Copyright (C) 2000-2004 Mikio Hirabayashi
 * 
 * This file is part of QDBM, Quick Database Manager.  
 * 
 * QDBM is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License or any later
 * version.  QDBM is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.  You should have received a copy of the GNU
 * Lesser General Public License along with QDBM; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA.
 * *********************************************************/

#include <errno.h>
#include <cstring>
#include <iostream>

#ifndef NO_NAMESPACES
using std::string;
#endif /* NO_NAMESPACES */

#include <iconv.h>

#include "csguess.h"
#include "autoconfig.h"
#ifdef RCL_ICONV_INBUF_CONST
#define ICV_P2_TYPE const char**
#else
#define ICV_P2_TYPE char**
#endif

// The values from estraier were 32768, 256, 0.001
const int ICONVCHECKSIZ = 32768;
const int ICONVMISSMAX  = 256;
const double ICONVALLWRAT = 0.001;

// Try to transcode and count errors (for charset guessing)
static int transcodeErrCnt(const char *ptr, int size, 
			   const char *icode, const char *ocode)
{
    iconv_t ic;
    char obuf[2*ICONVCHECKSIZ], *wp, *rp;
    size_t isiz, osiz;
    int miss;
    isiz = size;
    if((ic = iconv_open(ocode, icode)) == (iconv_t)-1) 
	return size;
    miss = 0;
    rp = (char *)ptr;
    while(isiz > 0){
	osiz = 2*ICONVCHECKSIZ;
	wp = obuf;
	if(iconv(ic, (ICV_P2_TYPE)&rp, &isiz, &wp, &osiz) == (size_t)-1){
	    if(errno == EILSEQ || errno == EINVAL){
		rp++;
		isiz--;
		miss++;
		if(miss >= ICONVMISSMAX) 
		    break;
	    } else {
		miss = size;
		break;
	    }
	}
    }
    if(iconv_close(ic) == -1) 
	return size;
    return miss;
}

// Try to guess character encoding. This could be optimized quite a
// lot by avoiding the multiple passes on the document, to be done
// after usefulness is demonstrated...
string csguess(const string &in, const string &dflt)
{
    const char     *hypo;
    int		i, miss;
    const char *text = in.c_str();
    bool cr = false;

    int size = in.length();
    if (size > ICONVCHECKSIZ)
	size = ICONVCHECKSIZ;

    // UTF-16 with normal prefix ?
    if (size >= 2 && (!memcmp(text, "\xfe\xff", 2) || 
		      !memcmp(text, "\xff\xfe", 2)))
	return "UTF-16";

    // If we find a zero at an appropriate position, guess it's UTF-16 
    // anyway. This is a quite expensive test for other texts as we'll 
    // have to scan the whole thing.
    for (i = 0; i < size - 1; i += 2) {
	if (text[i] == 0 && text[i + 1] != 0)
	    return "UTF-16BE";
	if (text[i + 1] == 0 && text[i] != 0)
	    return "UTF-16LE";
    }

    // Look for iso-2022 (rfc1468) specific escape sequences. As
    // iso-2022 begins in ascii, and typically soon escapes, these
    // succeed fast for a japanese text, but are quite expensive for
    // any other
    for (i = 0; i < size - 3; i++) {
	if (text[i] == 0x1b) {
	    i++;
	    if (text[i] == '(' && strchr("BJHI", text[i + 1]))
		return "ISO-2022-JP";
	    if (text[i] == '$' && strchr("@B(", text[i + 1]))
		return "ISO-2022-JP";
	}
    }

    // Try conversions from ascii and utf-8. These are unlikely to succeed
    // by mistake.
    if (transcodeErrCnt(text, size, "US-ASCII", "UTF-16BE") < 1) 
	return "US-ASCII";

    if (transcodeErrCnt(text, size, "UTF-8", "UTF-16BE") < 1)
	return "UTF-8";

    hypo = 0;
    for (i = 0; i < size; i++) {
	if (text[i] == 0xd) {
	    cr = true;
	    break;
	}
    }

    if (cr) {
	if ((miss = transcodeErrCnt(text, size, "Shift_JIS", "EUC-JP")) < 1)
	    return "Shift_JIS";
	if (!hypo && miss / (double)size <= ICONVALLWRAT)
	    hypo = "Shift_JIS";
	if ((miss = transcodeErrCnt(text, size, "EUC-JP", "UTF-16BE")) < 1)
	    return "EUC-JP";
	if (!hypo && miss / (double)size <= ICONVALLWRAT)
	    hypo = "EUC-JP";
    } else {
	if ((miss = transcodeErrCnt(text, size, "EUC-JP", "UTF-16BE")) < 1)
	    return "EUC-JP";
	if (!hypo && miss / (double)size <= ICONVALLWRAT)
	    hypo = "EUC-JP";
	if ((miss = transcodeErrCnt(text, size, "Shift_JIS", "EUC-JP")) < 1)
	    return "Shift_JIS";
	if (!hypo && miss / (double)size <= ICONVALLWRAT)
	    hypo = "Shift_JIS";
    }
    if ((miss = transcodeErrCnt(text, size, "UTF-8", "UTF-16BE")) < 1)
	return "UTF-8";
    if (!hypo && miss / (double)size <= ICONVALLWRAT)
	hypo = "UTF-8";
    if ((miss = transcodeErrCnt(text, size, "CP932", "UTF-16BE")) < 1)
	return "CP932";
    if (!hypo && miss / (double)size <= ICONVALLWRAT)
	hypo = "CP932";

    return hypo ? hypo : dflt;
}

#else

#include <errno.h>

#include <cstdlib>
#include <string>
#include <iostream>

using namespace std;

#include "readfile.h"
#include "csguess.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
	cerr << "Usage: trcsguess <filename> <default>" << endl;
	exit(1);
    }
    const string filename = argv[1];
    const string dflt = argv[2];
    string text;
    if (!file_to_string(filename, text)) {
	cerr << "Couldnt read file, errno " << errno << endl;
	exit(1);
    }
    cout << csguess(text, dflt) << endl;
    exit(0);
}
#endif
