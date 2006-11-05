 /* -*- mode:c++;c-basic-offset:2 -*- */
/*  --------------------------------------------------------------------
 *  Filename:
 *    mime-parsefull.cc
 *  
 *  Description:
 *    Implementation of main mime parser components
 *  --------------------------------------------------------------------
 *  Copyright 2002-2005 Andreas Aardal Hanssen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *  --------------------------------------------------------------------
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mime.h"
#include "mime-utils.h"
#include "mime-inputsource.h"
#include "convert.h"
#include <string>
#include <vector>
#include <map>
#include <exception>
#include <iostream>

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

Binc::MimeInputSource *mimeSource = 0;

#ifndef NO_NAMESPACES
using namespace ::std;
#endif /* NO_NAMESPACES */

//------------------------------------------------------------------------
void Binc::MimeDocument::parseFull(int fd) const
{
  if (allIsParsed)
    return;

  allIsParsed = true;

  if (!mimeSource || mimeSource->getFileDescriptor() != fd) {
    delete mimeSource;
    mimeSource = new MimeInputSource(fd);
  } else {
    mimeSource->reset();
  }

  headerstartoffsetcrlf = 0;
  headerlength = 0;
  bodystartoffsetcrlf = 0;
  bodylength = 0;
  size = 0;
  messagerfc822 = false;
  multipart = false;

  int bsize = 0;
  string bound;
  MimePart::parseFull(bound, bsize);

  // eat any trailing junk to get the correct size
  char c;
  while (mimeSource->getChar(&c));

  size = mimeSource->getOffset();
}

void Binc::MimeDocument::parseFull(istream& s) const
{
  if (allIsParsed)
    return;

  allIsParsed = true;

  delete mimeSource;
  mimeSource = new MimeInputSourceStream(s);

  headerstartoffsetcrlf = 0;
  headerlength = 0;
  bodystartoffsetcrlf = 0;
  bodylength = 0;
  size = 0;
  messagerfc822 = false;
  multipart = false;

  int bsize = 0;
  string bound;
  MimePart::parseFull(bound, bsize);

  // eat any trailing junk to get the correct size
  char c;
  while (mimeSource->getChar(&c));

  size = mimeSource->getOffset();
}

//------------------------------------------------------------------------
static bool parseOneHeaderLine(Binc::Header *header, unsigned int *nlines)
{
  using namespace ::Binc;
  char c;
  bool eof = false;
  char cqueue[4];
  string name;
  string content;

  while (mimeSource->getChar(&c)) {
    // If we encounter a \r before we got to the first ':', then
    // rewind back to the start of the line and assume we're at the
    // start of the body.
    if (c == '\r') {
      for (int i = 0; i < (int) name.length() + 1; ++i)
	mimeSource->ungetChar();
      return false;
    }

    // A colon marks the end of the header name
    if (c == ':') break;

    // Otherwise add to the header name
    name += c;
  }

  cqueue[0] = '\0';
  cqueue[1] = '\0';
  cqueue[2] = '\0';
  cqueue[3] = '\0';

  // Read until the end of the header.
  bool endOfHeaders = false;
  while (!endOfHeaders) {
    if (!mimeSource->getChar(&c)) {
      eof = true;
      break;
    }

    if (c == '\n') ++*nlines;

    for (int i = 0; i < 3; ++i)
      cqueue[i] = cqueue[i + 1];
    cqueue[3] = c;

    if (strncmp(cqueue, "\r\n\r\n", 4) == 0) {
      endOfHeaders = true;
      break;
    }

    // If the last character was a newline, and the first now is not
    // whitespace, then rewind one character and store the current
    // key,value pair.
    if (cqueue[2] == '\n' && c != ' ' && c != '\t') {
      if (content.length() > 2)
	content.resize(content.length() - 2);

      trim(content);
      header->add(name, content);

      if (c != '\r') {
	mimeSource->ungetChar();
	if (c == '\n') --*nlines;
	return true;
      }
	
      mimeSource->getChar(&c);
      return false;
    }

    content += c;
  }

  if (name != "") {
    if (content.length() > 2)
      content.resize(content.length() - 2);
    header->add(name, content);
  }

  return !(eof || endOfHeaders);
}

//------------------------------------------------------------------------
static void parseHeader(Binc::Header *header, unsigned int *nlines)
{
  while (parseOneHeaderLine(header, nlines))
  { }
}

//------------------------------------------------------------------------
static void analyzeHeader(Binc::Header *header, bool *multipart,
			  bool *messagerfc822, string *subtype,
			  string *boundary)
{
  using namespace ::Binc;

  // Do simple parsing of headers to determine the
  // type of message (multipart,messagerfc822 etc)
  HeaderItem ctype;
  if (header->getFirstHeader("content-type", ctype)) {
    vector<string> types;
    split(ctype.getValue(), ";", types);

    if (types.size() > 0) {
      // first element should describe content type
      string tmp = types[0];
      trim(tmp);
      vector<string> v;
      split(tmp, "/", v);
      string key, value;

      key = (v.size() > 0) ? v[0] : "text";
      value = (v.size() > 1) ? v[1] : "plain";
      lowercase(key);

      if (key == "multipart") {
	*multipart = true;
	lowercase(value);
	*subtype = value;
      } else if (key == "message") {
	lowercase(value);
	if (value == "rfc822")
	  *messagerfc822 = true;
      }
    }

    for (vector<string>::const_iterator i = types.begin();
	 i != types.end(); ++i) {
      string element = *i;
      trim(element);

      if (element.find("=") != string::npos) {
	string::size_type pos = element.find('=');
	string key = element.substr(0, pos);
	string value = element.substr(pos + 1);
	
	lowercase(key);
	trim(key);

	if (key == "boundary") {
	  trim(value, " \"");
	  *boundary = value;
	}
      }
    }
  }
}

static void parseMessageRFC822(vector<Binc::MimePart> *members,
			       bool *foundendofpart,
			       unsigned int *bodylength,
			       unsigned int *nbodylines,
			       const string &toboundary)
{
  using namespace ::Binc;

  // message rfc822 means a completely enclosed mime document. we
  // call the parser recursively, and pass on the boundary string
  // that we got. when parse() finds this boundary, it returns 0. if
  // it finds the end boundary (boundary + "--"), it returns != 0.
  MimePart m;

  unsigned int bodystartoffsetcrlf = mimeSource->getOffset();
    
  // parsefull returns the number of bytes that need to be removed
  // from the body because of the terminating boundary string.
  int bsize = 0;
  if (m.parseFull(toboundary, bsize))
    *foundendofpart = true;

  // make sure bodylength doesn't overflow    
  *bodylength = mimeSource->getOffset();
  if (*bodylength >= bodystartoffsetcrlf) {
    *bodylength -= bodystartoffsetcrlf;
    if (*bodylength >= (unsigned int) bsize) {
      *bodylength -= (unsigned int) bsize;
    } else {
      *bodylength = 0;
    }
  } else {
    *bodylength = 0;
  }

  *nbodylines += m.getNofLines();

  members->push_back(m);
}

static bool skipUntilBoundary(const string &delimiter,
			      unsigned int *nlines, bool *eof)
{
  int endpos = delimiter.length();
  char *delimiterqueue = 0;
  int delimiterpos = 0;
  const char *delimiterStr = delimiter.c_str();
  if (delimiter != "") {
    delimiterqueue = new char[endpos];
    memset(delimiterqueue, 0, endpos);
  }

  // first, skip to the first delimiter string. Anything between the
  // header and the first delimiter string is simply ignored (it's
  // usually a text message intended for non-mime clients)
  char c;

  bool foundBoundary = false;
  for (;;) {    
    if (!mimeSource->getChar(&c)) {
      *eof = true;
      break;
    }

    if (c == '\n')
      ++*nlines;

    // if there is no delimiter, we just read until the end of the
    // file.
    if (!delimiterqueue)
      continue;

    delimiterqueue[delimiterpos++ % endpos] = c;

    if (compareStringToQueue(delimiterStr, delimiterqueue,
			     delimiterpos, endpos)) {
      foundBoundary = true;
      break;
    }
  }

  delete [] delimiterqueue;
  delimiterqueue = 0;

  return foundBoundary;
}


static void parseMultipart(const string &boundary,
			   const string &toboundary,
			   bool *eof,
			   unsigned int *nlines,
			   int *boundarysize,
			   bool *foundendofpart,
			   unsigned int *bodylength,
			   vector<Binc::MimePart> *members)
{
  using namespace ::Binc;
  unsigned int bodystartoffsetcrlf = mimeSource->getOffset();

  // multipart parsing starts with skipping to the first
  // boundary. then we call parse() for all parts. the last parse()
  // command will return a code indicating that it found the last
  // boundary of this multipart. Note that the first boundary does
  // not have to start with CRLF.
  string delimiter = "--" + boundary;

  skipUntilBoundary(delimiter, nlines, eof);

  if (!eof)
    *boundarysize = delimiter.size();

  // Read two more characters. This may be CRLF, it may be "--" and
  // it may be any other two characters.
  char a;
  if (!mimeSource->getChar(&a))
    *eof = true;

  if (a == '\n')
    ++*nlines; 

  char b;
  if (!mimeSource->getChar(&b))
    *eof = true;
    
  if (b == '\n')
    ++*nlines;
    
  // If we find two dashes after the boundary, then this is the end
  // of boundary marker.
  if (!*eof) {
    if (a == '-' && b == '-') {
      *foundendofpart = true;
      *boundarysize += 2;
	
      if (!mimeSource->getChar(&a))
	*eof = true;
	
      if (a == '\n')
	++*nlines; 
	
      if (!mimeSource->getChar(&b))
	*eof = true;
	
      if (b == '\n')
	++*nlines;
    }

    if (a == '\r' && b == '\n') {
      // This exception is to handle a special case where the
      // delimiter of one part is not followed by CRLF, but
      // immediately followed by a CRLF prefixed delimiter.
      if (!mimeSource->getChar(&a) || !mimeSource->getChar(&b))
	*eof = true; 
      else if (a == '-' && b == '-') {
	mimeSource->ungetChar();
	mimeSource->ungetChar();
	mimeSource->ungetChar();
	mimeSource->ungetChar();
      } else {
	mimeSource->ungetChar();
	mimeSource->ungetChar();
      }

      *boundarysize += 2;
    } else {
      mimeSource->ungetChar();
      mimeSource->ungetChar();
    }
  }

  // read all mime parts.
  if (!*foundendofpart && !*eof) {
    bool quit = false;
    do {
      MimePart m;

      // If parseFull returns != 0, then it encountered the multipart's
      // final boundary.
      int bsize = 0;
      if (m.parseFull(boundary, bsize)) {
	quit = true;
	*boundarysize = bsize;
      }

      members->push_back(m);

    } while (!quit);
  }

  if (!*foundendofpart && !*eof) {
    // multipart parsing starts with skipping to the first
    // boundary. then we call parse() for all parts. the last parse()
    // command will return a code indicating that it found the last
    // boundary of this multipart. Note that the first boundary does
    // not have to start with CRLF.
    string delimiter = "\r\n--" + toboundary;

    skipUntilBoundary(delimiter, nlines, eof);

    if (!*eof)
      *boundarysize = delimiter.size();

    // Read two more characters. This may be CRLF, it may be "--" and
    // it may be any other two characters.
    char a = '\0';
    if (!mimeSource->getChar(&a))
      *eof = true;

    if (a == '\n')
      ++*nlines; 

    char b = '\0';
    if (!mimeSource->getChar(&b))
      *eof = true;
    
    if (b == '\n')
      ++*nlines;
    
    // If we find two dashes after the boundary, then this is the end
    // of boundary marker.
    if (!*eof) {
      if (a == '-' && b == '-') {
	*foundendofpart = true;
	*boundarysize += 2;
	
	if (!mimeSource->getChar(&a))
	  *eof = true;
	
	if (a == '\n')
	  ++*nlines; 
	
	if (!mimeSource->getChar(&b))
	  *eof = true;
	
	if (b == '\n')
	  ++*nlines;
      }

      if (a == '\r' && b == '\n') {
	// This exception is to handle a special case where the
	// delimiter of one part is not followed by CRLF, but
	// immediately followed by a CRLF prefixed delimiter.
	if (!mimeSource->getChar(&a) || !mimeSource->getChar(&b))
	  *eof = true; 
	else if (a == '-' && b == '-') {
	  mimeSource->ungetChar();
	  mimeSource->ungetChar();
	  mimeSource->ungetChar();
	  mimeSource->ungetChar();
	} else {
	  mimeSource->ungetChar();
	  mimeSource->ungetChar();
	}

	*boundarysize += 2;
      } else {
	mimeSource->ungetChar();
	mimeSource->ungetChar();
      }
    }
  }

  // make sure bodylength doesn't overflow    
  *bodylength = mimeSource->getOffset();
  if (*bodylength >= bodystartoffsetcrlf) {
    *bodylength -= bodystartoffsetcrlf;
    if (*bodylength >= (unsigned int) *boundarysize) {
      *bodylength -= (unsigned int) *boundarysize;
    } else {
      *bodylength = 0;
    }
  } else {
    *bodylength = 0;
  }
}

static void parseSinglePart(const string &toboundary,
			    int *boundarysize,
			    unsigned int *nbodylines,
			    unsigned int *nlines,
			    bool *eof, bool *foundendofpart,
			    unsigned int *bodylength)
{
  using namespace ::Binc;
  unsigned int bodystartoffsetcrlf = mimeSource->getOffset();

  // If toboundary is empty, then we read until the end of the
  // file. Otherwise we will read until we encounter toboundary.
  string _toboundary; 
  if (toboundary != "") {
    _toboundary = "\r\n--";
    _toboundary += toboundary;
  }

  //  if (skipUntilBoundary(_toboundary, nlines, eof))
  //    *boundarysize = _toboundary.length();

  char *boundaryqueue = 0;
  int endpos = _toboundary.length();
  if (toboundary != "") {
    boundaryqueue = new char[endpos];
    memset(boundaryqueue, 0, endpos);
  }
  int boundarypos = 0;

  *boundarysize = 0;

  const char *_toboundaryStr = _toboundary.c_str();
  string line;
  bool toboundaryIsEmpty = (toboundary == "");
  char c;
  while (mimeSource->getChar(&c)) {
    if (c == '\n') { ++*nbodylines; ++*nlines; }

    if (toboundaryIsEmpty)
      continue;

    // find boundary
    boundaryqueue[boundarypos++ % endpos] = c;
      
    if (compareStringToQueue(_toboundaryStr, boundaryqueue,
			     boundarypos, endpos)) {
      *boundarysize = _toboundary.length();
      break;
    }
  }

  delete [] boundaryqueue;

  if (toboundary != "") {
    char a;
    if (!mimeSource->getChar(&a))
      *eof = true;

    if (a == '\n')
      ++*nlines;
    char b;
    if (!mimeSource->getChar(&b))
      *eof = true;

    if (b == '\n') 
      ++*nlines;

    if (a == '-' && b == '-') {
      *boundarysize += 2;
      *foundendofpart = true;
      if (!mimeSource->getChar(&a))
	*eof = true;

      if (a == '\n')
	++*nlines;

      if (!mimeSource->getChar(&b))
	*eof = true;
	  
      if (b == '\n')
	++*nlines;
    }

    if (a == '\r' && b == '\n') {
      // This exception is to handle a special case where the
      // delimiter of one part is not followed by CRLF, but
      // immediately followed by a CRLF prefixed delimiter.
      if (!mimeSource->getChar(&a) || !mimeSource->getChar(&b))
	*eof = true; 
      else if (a == '-' && b == '-') {
	mimeSource->ungetChar();
	mimeSource->ungetChar();
	mimeSource->ungetChar();
	mimeSource->ungetChar();
      } else {
	mimeSource->ungetChar();
	mimeSource->ungetChar();
      }

      *boundarysize += 2;
    } else {
      mimeSource->ungetChar();
      mimeSource->ungetChar();
    }
  } else {
    // Recoll: in the case of a null boundary (probably illegal but wtf), eof
    // was not set and multipart went into a loop until bad alloc.
    *eof = true;
  }

  // make sure bodylength doesn't overflow    
  *bodylength = mimeSource->getOffset();
  if (*bodylength >= bodystartoffsetcrlf) {
    *bodylength -= bodystartoffsetcrlf;
    if (*bodylength >= (unsigned int) *boundarysize) {
      *bodylength -= (unsigned int) *boundarysize;
    } else {
      *bodylength = 0;
    }
  } else {
    *bodylength = 0;
  }

}

//------------------------------------------------------------------------
int Binc::MimePart::parseFull(const string &toboundary,
			      int &boundarysize) const
{
  headerstartoffsetcrlf = mimeSource->getOffset();

  // Parse the header of this mime part.
  parseHeader(&h, &nlines);

  // Headerlength includes the seperating CRLF. Body starts after the
  // CRLF.
  headerlength = mimeSource->getOffset() - headerstartoffsetcrlf;
  bodystartoffsetcrlf = mimeSource->getOffset();
  bodylength = 0;

  // Determine the type of mime part by looking at fields in the
  // header.
  analyzeHeader(&h, &multipart, &messagerfc822, &subtype, &boundary);

  bool eof = false;
  bool foundendofpart = false;

  if (messagerfc822) {
    parseMessageRFC822(&members, &foundendofpart, &bodylength,
		       &nbodylines, toboundary);

  } else if (multipart) {
    parseMultipart(boundary, toboundary, &eof, &nlines, &boundarysize,
		   &foundendofpart, &bodylength,
		   &members);
  } else {
    parseSinglePart(toboundary, &boundarysize, &nbodylines, &nlines,
		    &eof, &foundendofpart, &bodylength);
  }

  return (eof || foundendofpart) ? 1 : 0;
}
