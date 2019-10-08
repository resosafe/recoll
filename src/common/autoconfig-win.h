/* Manually edited version of autoconfig.h for windows. Many things are
overriden in the c++ code by ifdefs _WIN32 anyway  */
#ifndef _AUTOCONFIG_H_INCLUDED
#define _AUTOCONFIG_H_INCLUDED
/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Path to the aspell api include file */
/* #undef ASPELL_INCLUDE "aspell-local.h" */

/* Path to the aspell program */
/* #define ASPELL_PROG "/usr/bin/aspell" */

/* No X11 session monitoring support */
#define DISABLE_X11MON

/* Path to the fam api include file */
/* #undef FAM_INCLUDE */

/* Path to the file program */
#define FILE_PROG "/usr/bin/file"

/* "Have C++0x" */
#define HAVE_CXX0X_UNORDERED 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `dl' library (-ldl). */
#define HAVE_LIBDL 1

/* Define to 1 if you have the `pthread' library (-lpthread). */
#define HAVE_LIBPTHREAD 1

/* Define to 1 if you have the `z' library (-lz). */
#define HAVE_LIBZ 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mkdtemp' function. */
/* #undef HAVE_MKDTEMP */

/* Define to 1 if you have the `posix_spawn,' function. */
/* #undef HAVE_POSIX_SPAWN_ */

/* Define to 1 if you have the `setrlimit' function. */
#define HAVE_SETRLIMIT 1

/* Has std::shared_ptr */
#define HAVE_SHARED_PTR_STD

/* Has std::tr1::shared_ptr */
/* #undef HAVE_SHARED_PTR_TR1 */

/* Define to 1 if you have the <spawn.h> header file. */
#define HAVE_SPAWN_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/mount.h> header file. */
/* #undef HAVE_SYS_MOUNT_H */

/* Define to 1 if you have the <sys/param.h,> header file. */
/* #undef HAVE_SYS_PARAM_H_ */

/* Define to 1 if you have the <sys/statfs.h> header file. */
/* #undef HAVE_SYS_STATFS_H */

/* Define to 1 if you have the <sys/statvfs.h> header file. */
/* #undef HAVE_SYS_STATVFS_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/vfs.h> header file. */
/* #undef HAVE_SYS_VFS_H */

/* "Have tr1" */
/* #undef HAVE_TR1_UNORDERED */

/* Define to 1 if you have the <unistd.h> header file. */
/* #undef HAVE_UNISTD_H */

/* Use multiple threads for indexing */
#define IDX_THREADS 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "Recoll"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "Recoll 1.25.23"

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.25.23"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "recoll"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* putenv parameter is const */
/* #undef PUTENV_ARG_CONST */

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST

/* Real time monitoring option */
#undef RCL_MONITOR

/* Split camelCase words */
/* #undef RCL_SPLIT_CAMELCASE */

/* Compile the aspell interface */
/* #undef RCL_USE_ASPELL */

/* Compile the fam interface */
/* #undef RCL_USE_FAM */

/* Compile the inotify interface */
#define RCL_USE_INOTIFY 1

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Use posix_spawn() */
/* #undef USE_POSIX_SPAWN */

/* Enable using the system's 'file' command to id mime if we fail internally
   */
/* #undef USE_SYSTEM_FILE_COMMAND */

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

#define DISABLE_WEB_INDEXER

#include "conf_post.h"
#endif // already included
