/* Copyright (C) 2004 J.F.Dockes
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
#ifndef _TEXTSPLIT_H_INCLUDED_
#define _TEXTSPLIT_H_INCLUDED_

#include <string>
#include <list>
#ifndef NO_NAMESPACES
using std::string;
using std::list;
#endif

class Utf8Iter;


/** 
 * Split text into words. 
 * See comments at top of .cpp for more explanations.
 * This uses a callback function. It could be done with an iterator instead,
 * but 'ts much simpler this way...
 */
class TextSplit {
public:
    // Should we activate special processing of Chinese characters ? This
    // needs a little more cpu, so it can be turned off globally. This is set
    // by rclconfig, changing it means reindexing
    static bool o_processCJK;
    static unsigned int  o_CJKNgramLen;
    static const unsigned int o_CJKMaxNgramLen =  5;
    static void cjkProcessing(bool onoff, unsigned int ngramlen = 2) 
    {
	o_processCJK = onoff;
	o_CJKNgramLen = ngramlen <= o_CJKMaxNgramLen ? 
	    ngramlen : o_CJKMaxNgramLen;
    }

    // Are we indexing numbers ? Set by rclconfig. Change needs reindex
    static bool o_noNumbers;
    static void noNumbers()
    {
	o_noNumbers = true;
    }

    enum Flags {TXTS_NONE = 0, 
		TXTS_ONLYSPANS = 1,  // Only return maximum spans (a@b.com) 
		TXTS_NOSPANS = 2,  // Only return atomic words (a, b, com)
		TXTS_KEEPWILD = 4 // Handle wildcards as letters
    };

    
    TextSplit(Flags flags = Flags(TXTS_NONE))
	: m_flags(flags), m_maxWordLength(40), m_prevpos(-1)
    {
    }
    virtual ~TextSplit() {}

    /** Split text, emit words and positions. */
    virtual bool text_to_words(const string &in);

    /** Process one output word: to be implemented by the actual user class */
    virtual bool takeword(const string& term, 
			  int pos,  // term pos
			  int bts,  // byte offset of first char in term
			  int bte   // byte offset of first char after term
			  ) = 0; 


    // Static utility functions:

    /** Count words in string, as the splitter would generate them */
    static int countWords(const string &in, Flags flgs = TXTS_ONLYSPANS);

    /** Check if this is visibly not a single block of text */
    static bool hasVisibleWhite(const string &in);

    /** Split text span into strings, at white space, allowing for substrings
     * quoted with " . Escaping with \ works as usual inside the quoted areas.
     * This has to be kept separate from smallut.cpp's stringsToStrings, which
     * basically works only if whitespace is ascii, and which processes 
     * non-utf-8 input (iso-8859 config files work ok). This hopefully
     * handles all Unicode whitespace, but needs correct utf-8 input
     */
    static bool stringToStrings(const string &s, list<string> &tokens);

    /** Is char CJK ? */
    static bool isCJK(int c);

private:
    Flags         m_flags;
    int           m_maxWordLength;

    // Current span. Might be jf.dockes@wanadoo.f
    string        m_span; 

    // Current word: no punctuation at all in there. Byte offset
    // relative to the current span and byte length
    int           m_wordStart;
    unsigned int  m_wordLen;

    // Currently inside number
    bool          m_inNumber;

    // Term position of current word and span
    int           m_wordpos; 
    int           m_spanpos;

    // It may happen that our cleanup would result in emitting the
    // same term twice. We try to avoid this
    int           m_prevpos;
    unsigned int  m_prevlen;

    // This processes cjk text:
    bool cjk_to_words(Utf8Iter *it, unsigned int *cp);

    bool emitterm(bool isspan, string &term, int pos, int bs, int be);
    bool doemit(bool spanerase, int bp, bool spanemit=false);
    void discardspan();
};

#endif /* _TEXTSPLIT_H_INCLUDED_ */
