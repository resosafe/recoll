#ifndef lint
static char rcsid[] = "@(#$Id: textsplit.cpp,v 1.36.4.2 2008-12-12 12:28:24 dockes Exp $ (C) 2004 J.F.Dockes";
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
#ifndef TEST_TEXTSPLIT

#include <iostream>
#include <string>
#include <set>
#include <cstring>

#include "textsplit.h"
#include "debuglog.h"
#include "assert.h"

//#define UTF8ITER_CHECK
#include "utf8iter.h"

#include "uproplist.h"

#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

/**
 * Splitting a text into words. The code in this file works with utf-8
 * in a semi-clean way (see uproplist.h)
 *
 * We are also not using capitalization information.
 *
 * There are a few remnants of the initial utf8-ignorant version in this file.
 */

// Character classes: we have three main groups, and then some chars
// are their own class because they want special handling.
// 
// We have an array with 256 slots where we keep the character types. 
// The array could be fully static, but we use a small function to fill it 
// once.
// The array is actually a remnant of the original version which did no utf8
// It could be reduced to 128, because real (over 128) utf8 chars are now 
// handled with a set holding all the separator values.
enum CharClass {LETTER=256, SPACE=257, DIGIT=258, WILD=259};
static int charclasses[256];

static set<unsigned int> unicign;
static set<unsigned int> visiblewhite;
static void setcharclasses()
{
    static int init = 0;
    if (init)
	return;
    unsigned int i;
    for (i = 0 ; i < 256 ; i ++)
	charclasses[i] = LETTER;

    for (i = 0; i < ' ';i++)
	charclasses[i] = SPACE;

    char digits[] = "0123456789";
    for (i = 0; i  < strlen(digits); i++)
	charclasses[int(digits[i])] = DIGIT;

    char blankspace[] = "\t\v\f ";
    for (i = 0; i < strlen(blankspace); i++)
	charclasses[int(blankspace[i])] = SPACE;

    char seps[] = "!\"$%&()/<=>\\^{|}~:;`";
    for (i = 0; i  < strlen(seps); i++)
	charclasses[int(seps[i])] = SPACE;

    char wild[] = "*?[]";
    for (i = 0; i  < strlen(wild); i++)
	charclasses[int(wild[i])] = WILD;

    char special[] = ".@+-,#'\n\r";
    for (i = 0; i  < strlen(special); i++)
	charclasses[int(special[i])] = special[i];

    for (i = 0; i < sizeof(uniign) / sizeof(int); i++) {
	unicign.insert(uniign[i]);
    }
    unicign.insert((unsigned int)-1);

    for (i = 0; i < sizeof(avsbwht) / sizeof(int); i++) {
	visiblewhite.insert(avsbwht[i]);
    }

    init = 1;
}

static inline int whatcc(unsigned int c)
{
    if (c <= 127) {
	return charclasses[c]; 
    } else {
	if (unicign.find(c) != unicign.end())
	    return SPACE;
	else
	    return LETTER;
    }
}

// 2E80..2EFF; CJK Radicals Supplement
// 3000..303F; CJK Symbols and Punctuation
// 3040..309F; Hiragana
// 30A0..30FF; Katakana
// 3100..312F; Bopomofo
// 3130..318F; Hangul Compatibility Jamo
// 3190..319F; Kanbun
// 31A0..31BF; Bopomofo Extended
// 31C0..31EF; CJK Strokes
// 31F0..31FF; Katakana Phonetic Extensions
// 3200..32FF; Enclosed CJK Letters and Months
// 3300..33FF; CJK Compatibility
// 3400..4DBF; CJK Unified Ideographs Extension A
// 4DC0..4DFF; Yijing Hexagram Symbols
// 4E00..9FFF; CJK Unified Ideographs
// A700..A71F; Modifier Tone Letters
// AC00..D7AF; Hangul Syllables
// F900..FAFF; CJK Compatibility Ideographs
// FE30..FE4F; CJK Compatibility Forms
// FF00..FFEF; Halfwidth and Fullwidth Forms
// 20000..2A6DF; CJK Unified Ideographs Extension B
// 2F800..2FA1F; CJK Compatibility Ideographs Supplement
#define UNICODE_IS_CJK(p)						\
    (((p) >= 0x2E80 && (p) <= 0x2EFF)                                   \
     || ((p) >= 0x3000 && (p) <= 0x9FFF)                                \
     || ((p) >= 0xA700 && (p) <= 0xA71F)                                \
     || ((p) >= 0xAC00 && (p) <= 0xD7AF)                                \
     || ((p) >= 0xF900 && (p) <= 0xFAFF)                                \
     || ((p) >= 0xFE30 && (p) <= 0xFE4F)                                \
     || ((p) >= 0xFF00 && (p) <= 0xFFEF)                                \
     || ((p) >= 0x20000 && (p) <= 0x2A6DF)                              \
     || ((p) >= 0x2F800 && (p) <= 0x2FA1F))

bool TextSplit::isCJK(int c)
{
    return UNICODE_IS_CJK(c);
}

bool          TextSplit::o_processCJK = true;
unsigned int  TextSplit::o_CJKNgramLen = 2;

// Do some checking (the kind which is simpler to do here than in the
// main loop), then send term to our client.
inline bool TextSplit::emitterm(bool isspan, string &w, int pos, 
			 int btstart, int btend)
{
    LOGDEB3(("TextSplit::emitterm: [%s] pos %d\n", w.c_str(), pos));

    unsigned int l = w.length();
    if (l > 0 && l < (unsigned)m_maxWordLength) {
	// 1 char word: we index single letters and digits, but
	// nothing else. We might want to turn this into a test for a single
	// utf8 character instead.
	if (l == 1) {
	    int c = (int)w[0];
	    if (charclasses[c] != LETTER && charclasses[c] != DIGIT) {
		//cerr << "ERASING single letter term " << c << endl;
		return true;
	    }
	}
	if (pos != m_prevpos || l != m_prevlen) {
	    bool ret = m_cb->takeword(w, pos, btstart, btend);
	    m_prevpos = pos;
	    m_prevlen = w.length();
	    return ret;
	}
	LOGDEB2(("TextSplit::emitterm:dup: [%s] pos %d\n", w.c_str(), pos));
    }
    return true;
}

/**
 * A routine called from different places in text_to_words(), to
 * adjust the current state of the parser, and call the word
 * handler/emitter. Emit and reset the current word, possibly emit the current
 * span (if different). In query mode, words are not emitted, only final spans
 * 
 * This is purely for factoring common code from different places
 * text_to_words(). 
 * 
 * @return true if ok, false for error. Splitting should stop in this case.
 * @param spanerase Set if the current span is at its end. Reset it.
 * @param bp        The current BYTE position in the stream
 */
inline bool TextSplit::doemit(bool spanerase, int bp)
{
    LOGDEB3(("TextSplit::doemit:spn [%s] sp %d wrdS %d wrdL %d spe %d bp %d\n",
	     span.c_str(), spanpos, wordStart, wordLen, spanerase, bp));

    // Emit span. When splitting for query, we only emit final spans
    bool spanemitted = false;
    if (spanerase && !(m_flags & TXTS_NOSPANS)) {
	// Maybe trim at end. These are chars that we would keep inside 
	// a span, but not at the end
	while (m_span.length() > 0) {
	    switch (m_span[m_span.length()-1]) {
	    case '.':
	    case ',':
	    case '@':
	    case '\'':
		m_span.resize(m_span.length()-1);
		if (--bp < 0) 
		    bp = 0;
		break;
	    default:
		goto breakloop1;
	    }
	}
    breakloop1:
	spanemitted = true;
	if (!emitterm(true, m_span, m_spanpos, bp - m_span.length(), bp))
	    return false;
    }

    // Emit word if different from span and not 'no words' mode
    if (!(m_flags & TXTS_ONLYSPANS) && m_wordLen && 
	(!spanemitted || m_wordLen != m_span.length())) {
	string s(m_span.substr(m_wordStart, m_wordLen));
	if (!emitterm(false, s, m_wordpos, bp - m_wordLen, bp))
	    return false;
    }

    // Adjust state
    m_wordpos++;
    m_wordLen = 0;
    if (spanerase) {
	m_span.erase();
	m_spanpos = m_wordpos;
	m_wordStart = 0;
    } else {
	m_wordStart = m_span.length();
    }

    return true;
}

/** 
 * Splitting a text into terms to be indexed.
 * We basically emit a word every time we see a separator, but some chars are
 * handled specially so that special cases, ie, c++ and dockes@okyz.com etc, 
 * are handled properly,
 */
bool TextSplit::text_to_words(const string &in)
{
    LOGDEB1(("TextSplit::text_to_words: docjk %d (%d) %s%s%s [%s]\n", 
	     o_processCJK, o_CJKNgramLen,
	     m_flags & TXTS_NOSPANS ? " nospans" : "",
	     m_flags & TXTS_ONLYSPANS ? " onlyspans" : "",
	     m_flags & TXTS_KEEPWILD ? " keepwild" : "",
	     in.substr(0,50).c_str()));

    setcharclasses();

    m_span.erase();
    m_inNumber = false;
    m_wordStart = m_wordLen = m_prevpos = m_prevlen = m_wordpos = m_spanpos = 0;

    Utf8Iter it(in);

    for (; !it.eof(); it++) {
	unsigned int c = *it;

	if (c == (unsigned int)-1) {
	    LOGERR(("Textsplit: error occured while scanning UTF-8 string\n"));
	    return false;
	}

	if (o_processCJK && UNICODE_IS_CJK(c)) {
	    // CJK character hit. 
	    // Do like at EOF with the current non-cjk data.
	    if (m_wordLen || m_span.length()) {
		if (!doemit(true, it.getBpos()))
		    return false;
	    }

	    // Hand off situation to the cjk routine.
	    if (!cjk_to_words(&it, &c)) {
		LOGERR(("Textsplit: scan error in cjk handler\n"));
		return false;
	    }

	    // Check for eof, else c contains the first non-cjk
	    // character after the cjk sequence, just go on.
	    if (it.eof())
		break;
	}

	int cc = whatcc(c);
	switch (cc) {
	case LETTER:
	    m_wordLen += it.appendchartostring(m_span);
	    break;

	case DIGIT:
	    if (m_wordLen == 0)
		m_inNumber = true;
	    m_wordLen += it.appendchartostring(m_span);
	    break;

	case SPACE:
	SPACE:
	    if (m_wordLen || m_span.length()) {
		if (!doemit(true, it.getBpos()))
		    return false;
		m_inNumber = false;
	    }
	    break;
	case WILD:
	    if (m_flags & TXTS_KEEPWILD)
		goto NORMALCHAR;
	    else
		goto SPACE;
	    break;
	case '-':
	case '+':
	    if (m_wordLen == 0) {
		if (whatcc(it[it.getCpos()+1]) == DIGIT) {
		    m_inNumber = true;
		    m_wordLen += it.appendchartostring(m_span);
		} else {
		    m_wordStart += it.appendchartostring(m_span);
		}
	    } else {
		if (!doemit(false, it.getBpos()))
		    return false;
		m_inNumber = false;
		m_wordStart += it.appendchartostring(m_span);
	    }
	    break;
	case '.':
	case ',':
	    if (m_inNumber) {
		// 132.jpg ?
		if (whatcc(it[it.getCpos()+1]) != DIGIT)
		    goto SPACE;
		m_wordLen += it.appendchartostring(m_span);
		break;
	    } else {
		// If . inside a word, keep it, else, this is whitespace. 
		// We also keep an initial '.' for catching .net, but this adds
		// quite a few spurious terms !
                // Another problem is that something like .x-errs 
		// will be split as .x-errs, x, errs but not x-errs
		// A final comma in a word will be removed by doemit
		if (cc == '.') {
		    if (m_wordLen) {
			if (!doemit(false, it.getBpos()))
			    return false;
			// span length could have been adjusted by trimming
			// inside doemit
			if (m_span.length())
			    m_wordStart += it.appendchartostring(m_span);
			break;
		    } else {
			m_wordStart += it.appendchartostring(m_span);
			break;
		    }
		}
	    }
	    goto SPACE;
	    break;
	case '@':
	    if (m_wordLen) {
		if (!doemit(false, it.getBpos()))
		    return false;
		m_inNumber = false;
	    }
	    m_wordStart += it.appendchartostring(m_span);
	    break;
	case '\'':
	    // If in word, potential span: o'brien, else, this is more 
	    // whitespace
	    if (m_wordLen) {
		if (!doemit(false, it.getBpos()))
		    return false;
		m_inNumber = false;
		m_wordStart += it.appendchartostring(m_span);
	    }
	    break;
	case '#': 
	    // Keep it only at end of word ... Special case for c# you see...
	    if (m_wordLen > 0) {
		int w = whatcc(it[it.getCpos()+1]);
		if (w == SPACE || w == '\n' || w == '\r') {
		    m_wordLen += it.appendchartostring(m_span);
		    break;
		}
	    }
	    goto SPACE;
	    break;
	case '\n':
	case '\r':
	    if (m_span.length() && m_span[m_span.length() - 1] == '-') {
		// if '-' is the last char before end of line, just
		// ignore the line change. This is the right thing to
		// do almost always. We'd then need a way to check if
		// the - was added as part of the word hyphenation, or was 
		// there in the first place, but this would need a dictionary.
		// Also we'd need to check for a soft-hyphen and remove it,
		// but this would require more utf-8 magic
	    } else {
		// Handle like a normal separator
		goto SPACE;
	    }
	    break;

	default:
	NORMALCHAR:
	    m_wordLen += it.appendchartostring(m_span);
	    break;
	}
    }
    if (m_wordLen || m_span.length()) {
	if (!doemit(true, it.getBpos()))
	    return false;
    }
    return true;
}

// Using an utf8iter pointer just to avoid needing its definition in
// textsplit.h
//
// We output ngrams for exemple for char input a b c and ngramlen== 2, 
// we generate: a ab b bc c as words
//
// This is very different from the normal behaviour, so we don't use
// the doemit() and emitterm() routines
//
// The routine is sort of a mess and goes to show that we'd probably
// be better off converting the whole buffer to utf32 on entry...
bool TextSplit::cjk_to_words(Utf8Iter *itp, unsigned int *cp)
{
    LOGDEB1(("cjk_to_words: m_wordpos %d\n", m_wordpos));
    Utf8Iter &it = *itp;

    // We use an offset buffer to remember the starts of the utf-8
    // characters which we still need to use.
    assert(o_CJKNgramLen < o_CJKMaxNgramLen);
    unsigned int boffs[o_CJKMaxNgramLen+1];

    // Current number of valid offsets;
    unsigned int nchars = 0;
    unsigned int c = 0;
    for (; !it.eof(); it++) {
	c = *it;
	if (!UNICODE_IS_CJK(c)) {
	    // Return to normal handler
	    break;
	}

	if (nchars == o_CJKNgramLen) {
	    // Offset buffer full, shift it. Might be more efficient
	    // to have a circular one, but things are complicated
	    // enough already...
	    for (unsigned int i = 0; i < nchars-1; i++) {
		boffs[i] = boffs[i+1];
	    }
	}  else {
	    nchars++;
	}

	// Take note of byte offset for this character.
	boffs[nchars-1] = it.getBpos();

	// Output all new ngrams: they begin at each existing position
	// and end after the new character. onlyspans->only output
	// maximum words, nospans=> single chars
	if (!(m_flags & TXTS_ONLYSPANS) || nchars == o_CJKNgramLen) {
	    unsigned int btend = it.getBpos() + it.getBlen();
	    unsigned int loopbeg = (m_flags & TXTS_NOSPANS) ? nchars-1 : 0;
	    unsigned int loopend = (m_flags & TXTS_ONLYSPANS) ? 1 : nchars;
	    for (unsigned int i = loopbeg; i < loopend; i++) {
		if (!m_cb->takeword(it.buffer().substr(boffs[i], 
						       btend-boffs[i]),
				m_wordpos - (nchars-i-1), boffs[i], btend)) {
		    return false;
		}
	    }

	    if ((m_flags & TXTS_ONLYSPANS)) {
		// Only spans: don't overlap: flush buffer
		nchars = 0;
	    }
	}
	// Increase word position by one, other words are at an
	// existing position. This could be subject to discussion...
	m_wordpos++;
    }

    // If onlyspans is set, there may be things to flush in the buffer
    // first
    if ((m_flags & TXTS_ONLYSPANS) && nchars > 0 && nchars != o_CJKNgramLen)  {
	unsigned int btend = it.getBpos(); // Current char is out
	if (!m_cb->takeword(it.buffer().substr(boffs[0], 
					       btend-boffs[0]),
			    m_wordpos - nchars,
			    boffs[0], btend)) {
	    return false;
	}
    }

    m_span.erase();
    m_inNumber = false;
    m_wordStart = m_wordLen = m_prevpos = m_prevlen = 0;
    m_spanpos = m_wordpos;
    *cp = c;
    return true;
}

// Callback class for countWords 
class utSplitterCB : public TextSplitCB {
 public:
    int wcnt;
    utSplitterCB() : wcnt(0) {}
    bool takeword(const string &term, int pos, int bs, int be) {
	wcnt++;
	return true;
    }
};

int TextSplit::countWords(const string& s, TextSplit::Flags flgs)
{
    utSplitterCB cb;
    TextSplit splitter(&cb, flgs);
    splitter.text_to_words(s);
    return cb.wcnt;
}

bool TextSplit::hasVisibleWhite(const string &in)
{
    setcharclasses();
    Utf8Iter it(in);
    for (; !it.eof(); it++) {
	unsigned int c = *it;
	LOGDEB3(("TextSplit::hasVisibleWhite: testing 0x%04x\n", c));
	if (c == (unsigned int)-1) {
	    LOGERR(("hasVisibleWhite: error while scanning UTF-8 string\n"));
	    return false;
	}
	if (visiblewhite.find(c) != visiblewhite.end())
	    return true;
    }
    return false;
}

template <class T> bool u8stringToStrings(const string &s, T &tokens)
{
    setcharclasses();
    Utf8Iter it(s);

    string current;
    tokens.clear();
    enum states {SPACE, TOKEN, INQUOTE, ESCAPE};
    states state = SPACE;
    for (; !it.eof(); it++) {
	unsigned int c = *it;
	if (visiblewhite.find(c) != visiblewhite.end()) 
	    c = ' ';
	LOGDEB3(("TextSplit::stringToStrings: 0x%04x\n", c));
	if (c == (unsigned int)-1) {
	    LOGERR(("TextSplit::stringToStrings: error while "
		    "scanning UTF-8 string\n"));
	    return false;
	}

	switch (c) {
	    case '"': 
	    switch(state) {
	    case SPACE: state = INQUOTE; continue;
	    case TOKEN: goto push_char;
	    case ESCAPE: state = INQUOTE; goto push_char;
	    case INQUOTE: tokens.push_back(current);current.clear();
		state = SPACE; continue;
	    }
	    break;
	    case '\\': 
	    switch(state) {
	    case SPACE: 
	    case TOKEN: state=TOKEN; goto push_char;
	    case INQUOTE: state = ESCAPE; continue;
	    case ESCAPE: state = INQUOTE; goto push_char;
	    }
	    break;

	    case ' ': 
	    case '\t': 
	    case '\n': 
	    case '\r': 
	    switch(state) {
	      case SPACE: continue;
	      case TOKEN: tokens.push_back(current); current.clear();
		state = SPACE; continue; 
	    case INQUOTE: 
	    case ESCAPE: goto push_char;
	    }
	    break;

	    default:
	    switch(state) {
	      case ESCAPE: state = INQUOTE; break;
	      case SPACE:  state = TOKEN;  break;
	      case TOKEN: 
	      case INQUOTE: break;
	    }
	push_char:
	    it.appendchartostring(current);
	}
    }

    // End of string. Process residue, and possible error (unfinished quote)
    switch(state) {
    case SPACE: break;
    case TOKEN: tokens.push_back(current); break;
    case INQUOTE: 
    case ESCAPE: return false;
    }
    return true;
}

bool TextSplit::stringToStrings(const string &s, list<string> &tokens)
{
    return u8stringToStrings<list<string> >(s, tokens);
}

#else  // TEST driver ->

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include <iostream>

#include "textsplit.h"
#include "readfile.h"
#include "debuglog.h"

using namespace std;

// A small class to hold state while splitting text
class mySplitterCB : public TextSplitCB {
    int first;
    bool nooutput;
 public:
    mySplitterCB() : first(1), nooutput(false) {}
    void setNoOut(bool val) {nooutput = val;}
    bool takeword(const string &term, int pos, int bs, int be) {
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
};

static string teststring = 
	    "Un bout de texte \nnormal. 2eme phrase.3eme;quatrieme.\n"
	    "\"Jean-Francois Dockes\" <jfd@okyz.com>\n"
	    "n@d @net .net t@v@c c# c++ o'brien 'o'brien' l'ami\n"
	    "134 +134 -14 -1.5 +1.5 1.54e10 1,2 1,2e30\n"
	    "@^#$(#$(*)\n"
	    "192.168.4.1 one\n\rtwo\r"
	    "Debut-\ncontinue\n" 
	    "[olala][ululu]  (valeur) (23)\n"
	    "utf-8 ucs-4© \\nodef\n"
	    "','this\n"
	    " ,able,test-domain "
	    " -wl,--export-dynamic "
	    " ~/.xsession-errors "
;
static string teststring1 = " nouvel-an ";

static string thisprog;

static string usage =
    " textsplit [opts] [filename]\n"
    "   -S: no output\n"
    "   -s:  only spans\n"
    "   -w:  only words\n"
    "   -k:  preserve wildcards (?*)\n"
    "   -c: just count words\n"
    " if filename is 'stdin', will read stdin for data (end with ^D)\n"
    "  \n\n"
    ;

static void
Usage(void)
{
    cerr << thisprog  << ": usage:\n" << usage;
    exit(1);
}

static int        op_flags;
#define OPT_s	  0x1 
#define OPT_w	  0x2
#define OPT_S	  0x4
#define OPT_c     0x8
#define OPT_k     0x10
#define OPT_C     0x20

int main(int argc, char **argv)
{
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'c':	op_flags |= OPT_c; break;
	    case 'C':	op_flags |= OPT_C; break;
	    case 'k':	op_flags |= OPT_k; break;
	    case 's':	op_flags |= OPT_s; break;
	    case 'S':	op_flags |= OPT_S; break;
	    case 'w':	op_flags |= OPT_w; break;
	    default: Usage();	break;
	    }
	argc--; argv++;
    }
    DebugLog::getdbl()->setloglevel(DEBDEB1);
    DebugLog::setfilename("stderr");

    mySplitterCB cb;
    TextSplit::Flags flags = TextSplit::TXTS_NONE;

    if (op_flags&OPT_S)
	cb.setNoOut(true);

    if (op_flags&OPT_s)
	flags = TextSplit::TXTS_ONLYSPANS;
    else if (op_flags&OPT_w)
	flags = TextSplit::TXTS_NOSPANS;
    if (op_flags & OPT_k) 
	flags = (TextSplit::Flags)(flags | TextSplit::TXTS_KEEPWILD); 

    string data;
    if (argc == 1) {
	const char *filename = *argv++;	argc--;
	if (!strcmp(filename, "stdin")) {
	    char buf[1024];
	    int nread;
	    while ((nread = read(0, buf, 1024)) > 0) {
		data.append(buf, nread);
	    }
	} else if (!file_to_string(filename, data)) 
	    exit(1);
    } else {
	cout << endl << teststring << endl << endl;  
	data = teststring;
    }
    if (op_flags & OPT_c) {
	int n = TextSplit::countWords(data, flags);
	cout << n << " words" << endl;
    } else {
	TextSplit splitter(&cb,  flags);
	splitter.text_to_words(data);
    }    
}
#endif // TEST
