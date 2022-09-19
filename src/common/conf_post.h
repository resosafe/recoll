#ifndef _AUTOCONF_CONF_POST_H_INCLUDED
#define _AUTOCONF_CONF_POST_H_INCLUDED

#ifdef _WIN32


#ifdef _MSC_VER
#include "safewindows.h"
// gmtime is supposedly thread-safe on windows
#define gmtime_r(A, B) gmtime(A)
#define localtime_r(A,B) localtime_s(B,A)
typedef int mode_t;
#define fseeko _fseeki64
#define ftello (off_t)_ftelli64
#define ftruncate _chsize_s
#define PATH_MAX MAX_PATH
#define RCL_ICONV_INBUF_CONST 1
#define HAVE_STRUCT_TIMESPEC
#define strdup _strdup
#define timegm _mkgmtime

#else // End _MSC_VER -> Gminw

// Allow use of features specific to Windows 7 or later.
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#define LOGFONTW void
#include "safewindows.h"

#undef RCL_ICONV_INBUF_CONST
#define timegm portable_timegm

#endif // GMinw only

typedef int pid_t;
inline int readlink(const char *a, void *b, int c)
{
    a = a; b = b; c = c;
    return -1;
}

#define MAXPATHLEN PATH_MAX
typedef DWORD32 u_int32_t;
typedef DWORD64 u_int64_t;
typedef unsigned __int8 u_int8_t;
typedef int ssize_t;
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define chdir _chdir

#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef X_OK
#define X_OK 4
#endif

#define S_ISLNK(X) false
#define lstat stat

#endif // _WIN32

#ifndef PRETEND_USE
#  define PRETEND_USE(expr) ((void)(expr))
#endif /* PRETEND_USE */

// It's complicated to really detect gnu gcc because other compilers define __GNUC__
// See stackoverflow questions/38499462/how-to-tell-clang-to-stop-pretending-to-be-other-compilers
#if defined(__GNUC__) && !defined(__llvm__) && !defined(__INTEL_COMPILER)
#define REAL_GCC   __GNUC__ // probably
#endif

#ifdef REAL_GCC
// Older gcc versions pretended to supply std::regex, but the resulting programs mostly crashed.
#include <features.h>
#if ! __GNUC_PREREQ(6,0)
#define NO_STD_REGEX 1
#endif
#endif

#endif /* INCLUDED */
