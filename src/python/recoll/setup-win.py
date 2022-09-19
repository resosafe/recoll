from setuptools import setup, Extension
import os
import sys

srcdir = "c:/users/bill/documents/recoll/src/python/recoll"
recolldeps = "c:/users/bill/documents/recolldeps/msvc"

# The compiled-in values are not used under Windows
prefix = "c:/install/recoll"
RECOLL_DATADIR = "c:/install/recoll/Share/"

# For shadow builds: references to the source tree
top = os.path.join(srcdir, '..', '..')
pytop = srcdir
localtop = top
extra_compile_args = ''

VERSION = open(os.path.join(top, "RECOLL-VERSION.txt")).read().strip()

include_dirs = [
        os.path.join(top, 'common'),
        os.path.join(top, 'rcldb'), 
        os.path.join(top, 'xaposix'), 
        os.path.join(top, 'utils'), 
        os.path.join(top, 'query'), 
        os.path.join(top, 'internfile'), 
]

define_macros = [
        ('RECOLL_DATADIR', RECOLL_DATADIR),
        ('__WIN32__', '1')
]

library_dirs = [
        os.path.join(top, "windows", "build-librecoll-Desktop_Qt_5_14_2_MSVC2017_32bit-Release/release"),
        os.path.join(recolldeps, "libxml2/libxml2-2.9.4+dfsg1/win32/bin.msvc"),
        os.path.join(recolldeps, "libxslt/libxslt-1.1.29/win32/bin.msvc"),
        os.path.join(top, "windows", "build-libxapian-Desktop_Qt_5_14_2_MSVC2017_32bit-Release/release"),
        os.path.join(recolldeps, "zlib-1.2.11"),
        os.path.join(recolldeps, "build-libiconv-Desktop_Qt_5_14_2_MSVC2017_32bit-Release/release")
        ]

libraries =  ["librecoll", "libxml2_a", "libxslt_a",
              "libxapian", "libiconv", "zlib",
              "rpcrt4", "ws2_32", "shlwapi", "shell32",
              "psapi", "user32", "kernel32"
]

extra_compile_args = ['-std=c++11']

module1 = Extension('_recoll',
                    define_macros = define_macros,
                    include_dirs = include_dirs,
                    libraries = libraries,
                    library_dirs = library_dirs,
                    sources = [os.path.join(pytop, 'pyrecoll.cpp'),
                               os.path.join(pytop, 'pyresultstore.cpp'),
                               os.path.join(pytop, 'pyrclextract.cpp')])

setup (name = 'Recoll',
       version = VERSION,
       description = 'Query/Augment a Recoll full text index',
       author = 'J.F. Dockes',
       author_email = 'jfd@recoll.org',
       url = 'https://www.recoll.org',
       license = 'GPL',
#       package_dir = {'' : os.path.join(top, 'python', 'recoll')},
       long_description = '''
''',
    packages = ['recoll'],
    ext_package = 'recoll',
       ext_modules = [module1])
