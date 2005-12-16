#ifndef lint
static char rcsid[] = "@(#$Id: mimeparse.cpp,v 1.8.2.1 2005-12-16 10:06:13 dockes Exp $ (C) 2004 J.F.Dockes";
#endif

#ifndef TEST_MIMEPARSE

#include <string>
#include <ctype.h>
#include <stdio.h>
#include <ctype.h>

#include "mimeparse.h"
#include "base64.h"

#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

// Parsing a header value. Only content-type has parameters, but
// others are compatible with content-type syntax, only, parameters
// are not used. So we can parse all like content-type:
//    headertype: value [; paramname=paramvalue] ...
// Value and paramvalues can be quoted strings, and there can be
// comments in there



// The lexical token returned by find_next_token
class Lexical {
 public:
    enum kind {none, token, separator};
    kind   what;
    string value;
    string error;
    char quote;
    Lexical() : what(none), quote(0) {}
    void reset() {what = none; value.erase(); error.erase();quote = 0;}
};

// Skip mime comment. This must be called with in[start] == '('
string::size_type skip_comment(const string &in, string::size_type start, 
			       Lexical &lex)
{
    int commentlevel = 0;
    for (; start < in.size(); start++) {
	if (in[start] == '\\') {
	    // Skip escaped char. 
	    if (start+1 < in.size()) {
		start++;
		continue;
	    } else {
		lex.error.append("\\ at end of string ");
		return string::npos;
	    }
	}
	if (in[start] == '(')
	    commentlevel++;
	if (in[start] == ')') {
	    if (--commentlevel == 0)
		break;
	}
    }
    if (start == in.size()) {
	lex.error.append("Unclosed comment ");
	return string::npos;
    }
    return start;
}

// Skip initial whitespace and (possibly nested) comments. 
string::size_type skip_whitespace_and_comment(const string &in, 
					      string::size_type start, 
					      Lexical &lex)
{
    while (1) {
	if ((start = in.find_first_not_of(" \t\r\n", start)) == string::npos)
	    return in.size();
	if (in[start] == '(') {
	    if ((start = skip_comment(in, start, lex)) == string::npos)
		return string::npos;
	} else {
	    break;
	}
    }
    return start;
}

/// Find next token in mime header value string. 
/// @return the next starting position in string, string::npos for error 
///   (ie unbalanced quoting)
/// @param in the input string
/// @param start the starting position
/// @param lex  the returned token and its description
/// @param delims separators we should look for
string::size_type find_next_token(const string &in, string::size_type start, 
				  Lexical &lex, string delims = ";=")
{
    char oquot, cquot;

    start = skip_whitespace_and_comment(in, start, lex);
    if (start == string::npos || start == in.size())
	return start;

    // Begins with separator ? return it.
    string::size_type delimi = delims.find_first_of(in[start]);
    if (delimi != string::npos) {
	lex.what = Lexical::separator;
	lex.value = delims[delimi];
	return start+1;
    }

    // Check for start of quoted string
    oquot = in[start];
    switch (oquot) {
    case '<': cquot = '>';break;
    case '"': cquot = '"';break;
    default: cquot = 0; break;
    }

    if (cquot != 0) {
	// Quoted string parsing
	string::size_type end;
	start++; // Skip quote character
	for (end = start;end < in.size() && in[end] != cquot; end++) {
	    if (in[end] == '\\') {
		// Skip escaped char. 
		if (end+1 < in.size()) {
		    end++;
		} else {
		    // backslash at end of string: error
		    lex.error.append("\\ at end of string ");
		    return string::npos;
		}
	    }
	}
	if (end == in.size()) {
	    // Found end of string before closing quote character: error
	    lex.error.append("Unclosed quoted string ");
	    return string::npos;
	}
	lex.what = Lexical::token;
	lex.value = in.substr(start, end-start);
	lex.quote = oquot;
	return ++end;
    } else {
	string::size_type end = in.find_first_of(delims + " \t(", start);
	lex.what = Lexical::token;
	lex.quote = 0;
	if (end == string::npos) {
	    end = in.size();
	    lex.value = in.substr(start);
	} else {
	    lex.value = in.substr(start, end-start);
	}
	return end;
    }
}

void stringtolower(string &out, const string& in)
{
    for (string::size_type i = 0; i < in.size(); i++)
	out.append(1, char(tolower(in[i])));
}

bool parseMimeHeaderValue(const string& value, MimeHeaderValue& parsed)
{
    parsed.value.erase();
    parsed.params.clear();

    Lexical lex;
    string::size_type start = 0;
    start = find_next_token(value, start, lex);
    if (start == string::npos || lex.what != Lexical::token) 
	return false;

    parsed.value = lex.value;

    for (;;) {
	string paramname, paramvalue;
	lex.reset();
	start = find_next_token(value, start, lex);
	if (start == value.size())
	    return true;
	if (start == string::npos)
	    return false;
	if (lex.what == Lexical::separator && lex.value[0] == ';')
	    continue;
	if (lex.what != Lexical::token) 
	    return false;
	stringtolower(paramname, lex.value);

	start = find_next_token(value, start, lex);
	if (start == string::npos || lex.what != Lexical::separator || 
	    lex.value[0] != '=') 
	    return false;

	start = find_next_token(value, start, lex);
	if (start == string::npos || lex.what != Lexical::token)
	    return false;
	paramvalue = lex.value;
	parsed.params[paramname] = paramvalue;
    }
    return true;
}

// Decode a string encoded with quoted-printable encoding. 
bool qp_decode(const string& in, string &out) 
{
    out.reserve(in.length());
    string::size_type ii;
    for (ii = 0; ii < in.length(); ii++) {
	if (in[ii] == '=') {
	    ii++; // Skip '='
	    if(ii >= in.length() - 1) { // Need at least 2 more chars
		break;
	    } else if (in[ii] == '\r' && in[ii+1] == '\n') { // Soft nl, skip
		ii++;
	    } else if (in[ii] != '\n' && in[ii] != '\r') { // decode
		char c = in[ii];
		char co;
		if(c >= 'A' && c <= 'F') {
		    co = char((c - 'A' + 10) * 16);
		} else if (c >= 'a' && c <= 'f') {
		    co = char((c - 'a' + 10) * 16);
		} else if (c >= '0' && c <= '9') {
		    co = char((c - '0') * 16);
		} else {
		    return false;
		}
		if(++ii >= in.length()) 
		    break;
		c = in[ii];
		if (c >= 'A' && c <= 'F') {
		    co += char(c - 'A' + 10);
		} else if (c >= 'a' && c <= 'f') {
		    co += char(c - 'a' + 10);
		} else if (c >= '0' && c <= '9') {
		    co += char(c - '0');
		} else {
		    return false;
		}
		out += co;
	    }
	} else {
	    out += in[ii];
	}
    }
    return true;
}


#include "transcode.h"
#include "smallut.h"

// Decode a parsed encoded word
static bool rfc2047_decodeParsed(const std::string& charset, 
				 const std::string& encoding, 
				 const std::string& value, 
				 std::string &utf8)
{
    //    fprintf(stderr, "DecodeParsed: charset [%s] enc [%s] val [%s]\n",
    //	    charset.c_str(), encoding.c_str(), value.c_str());
    utf8 = "";

    string decoded;
    if (!stringlowercmp("b", encoding)) {
	if (!base64_decode(value, decoded))
	    return false;
	//	fprintf(stderr, "FromB64: [%s]\n", decoded.c_str());
    } else if (!stringlowercmp("q", encoding)) {
	if (!qp_decode(value, decoded))
	    return false;
	// Need to translate _ to ' ' here
	string temp;
	for (string::size_type pos = 0; pos < decoded.length(); pos++)
	    if (decoded[pos] == '_')
		temp += ' ';
	    else 
		temp += decoded[pos];
	decoded = temp;
	//	fprintf(stderr, "FromQP: [%s]\n", decoded.c_str());
    } else {
	//	fprintf(stderr, "Bad encoding [%s]\n", encoding.c_str());
	return false;
    }

    if (!transcode(decoded, utf8, charset, "UTF-8")) {
	//	fprintf(stderr, "Transcode failed\n");
	return false;
    }
    return true;
}

// Parse a mail header encoded value
typedef enum  {rfc2047base, rfc2047open_eq, rfc2047charset, rfc2047encoding, 
	       rfc2047value, rfc2047close_q} Rfc2047States;

bool rfc2047_decode(const std::string& in, std::string &out) 
{
    Rfc2047States state = rfc2047base;
    string encoding, charset, value, utf8;

    out = "";

    for (string::size_type ii = 0; ii < in.length(); ii++) {
	char ch = in[ii];
	switch (state) {
	case rfc2047base: 
	    {
		switch (ch) {
		case '=': state = rfc2047open_eq; break;
		default: value += ch;
		}
	    }
	    break;
	case rfc2047open_eq: 
	    {
		switch (ch) {
		case '?': 
		    {
			// Transcode current (unencoded part) value:
			// we sometimes find 8-bit chars in
			// there. Interpret as Iso8859.
			if (value.length() > 0) {
			    transcode(value, utf8, "ISO-8859-1", "UTF-8");
			    out += utf8;
			    value = "";
			}
			state = rfc2047charset; 
		    }
		    break;
		default: state = rfc2047base; out += '='; out += ch;break;
		}
	    } 
	    break;
	case rfc2047charset: 
	    {
		switch (ch) {
		case '?': state = rfc2047encoding; break;
		default: charset += ch; break;
		}
	    } 
	    break;
	case rfc2047encoding: 
	    {
		switch (ch) {
		case '?': state = rfc2047value; break;
		default: encoding += ch; break;
		}
	    }
	    break;
	case rfc2047value: 
	    {
		switch (ch) {
		case '?': state = rfc2047close_q; break;
		default: value += ch;break;
		}
	    }
	    break;
	case rfc2047close_q: 
	    {
		switch (ch) {
		case '=': 
		    {
			string utf8;
			state = rfc2047base; 
			if (!rfc2047_decodeParsed(charset, encoding, value, 
						  utf8)) {
			    return false;
			}
			out += utf8;
			charset = encoding = value = "";
		    }
		    break;
		default: state = rfc2047value; value += '?';value += ch;break;
		}
	    }
	    break;
	default: // ??
	    return false;
	}
    }

    if (value.length() > 0) {
	transcode(value, utf8, "ISO-8859-1", "UTF-8");
	out += utf8;
	value = "";
    }
    if (state != rfc2047base) 
	return false;
    return true;
}

#else 

#include <string>
#include "mimeparse.h"
#include "readfile.h"


using namespace std;
int
main(int argc, const char **argv)
{
#if 0
    //    const char *tr = "text/html; charset=utf-8; otherparam=garb";
    const char *tr = "text/html;charset = UTF-8 ; otherparam=garb; \n"
	"QUOTEDPARAM=\"quoted value\"";

    MimeHeaderValue parsed;

    if (!parseMimeHeaderValue(tr, parsed)) {
	fprintf(stderr, "PARSE ERROR\n");
    }
    
    printf("'%s' \n", parsed.value.c_str());
    map<string, string>::iterator it;
    for (it = parsed.params.begin();it != parsed.params.end();it++) {
	printf("  '%s' = '%s'\n", it->first.c_str(), it->second.c_str());
    }
#elif 0
    const char *qp = "=41=68 =e0 boire=\r\n continue 1ere\ndeuxieme\n\r3eme "
	"agrave is: '=E0' probable skipped decode error: =\n"
	"Actual decode error =xx this wont show";

    string out;
    if (!qp_decode(string(qp), out)) {
	fprintf(stderr, "qp_decode returned error\n");
    }
    printf("Decoded: '%s'\n", out.c_str());
#elif 0
    //'C'est à boire qu'il nous faut éviter l'excès.'
    //'Deuxième ligne'
    //'Troisième ligne'
    //'Et la fin (pas de nl). '
    const char *b64 = 
 "Qydlc3Qg4CBib2lyZSBxdSdpbCBub3VzIGZhdXQg6XZpdGVyIGwnZXhj6HMuCkRldXhp6G1l\r\n"
	"IGxpZ25lClRyb2lzaehtZSBsaWduZQpFdCBsYSBmaW4gKHBhcyBkZSBubCkuIA==\r\n";

    string out;
    if (!base64_decode(string(b64), out)) {
	fprintf(stderr, "base64_decode returned error\n");
	exit(1);
    }
    printf("Decoded: '%s'\n", out.c_str());
#elif 0
    char line [1024];
    string out;
    while (fgets(line, 1023, stdin)) {
	int l = strlen(line);
	if (l == 0)
	    continue;
	line[l-1] = 0;
	fprintf(stderr, "Line: [%s]\n", line);
	rfc2047_decode(line, out);
	fprintf(stderr, "Out:  [%s]\n", out.c_str());
    }
#elif 1
    string coded, decoded;
    const char *fname = "/tmp/recoll_decodefail";
    if (!file_to_string(fname, coded)) {
	fprintf(stderr, "Cant read %s\n", fname);
	exit(1);
    }
    
    if (!base64_decode(coded, decoded)) {
	fprintf(stderr, "base64_decode returned error\n");
	exit(1);
    }
    printf("Decoded: [%s]\n", decoded.c_str());
    
#endif
}

#endif // TEST_MIMEPARSE
