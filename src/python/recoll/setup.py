from distutils.core import setup, Extension
import os

sys = os.uname()[0]
if sys == 'Linux':
    libs = ['xapian']
else:
    libs = ['xapian', 'iconv']

if 'RECOLL_DATADIR' in os.environ:
    datadirs = [os.environ['RECOLL_DATADIR']]
else:
    datadirs = []
datadirs = datadirs + ['/usr/share/recoll', '/usr/local/share/recoll', '/']
for datadir in datadirs:
    if os.path.exists(datadir):
        break
if datadir == '/':
    print 'You need to install Recoll first'
    os.exit(1)


top = os.path.join('..', '..')

module1 = Extension('recoll',
                    define_macros = [('MAJOR_VERSION', '1'),
                                     ('MINOR_VERSION', '0'),
                                     ('UNAC_VERSION', '"1.0.7"'),
                                     ('RECOLL_DATADIR', '"' + datadir + '"')
                                     ],
                    include_dirs = ['/usr/local/include',
                                    os.path.join(top, 'utils'), 
                                    os.path.join(top, 'common'), 
                                    os.path.join(top, 'rcldb'), 
                                    os.path.join(top, 'query'), 
                                    os.path.join(top, 'unac')
                                    ],
                    libraries = libs,
                    library_dirs = ['/usr/local/lib'],
                    sources = ['pyrecoll.cpp',
                               os.path.join(top, 'common/rclconfig.cpp'),
                               os.path.join(top, 'common/rclinit.cpp'),
                               os.path.join(top, 'common/textsplit.cpp'),
                               os.path.join(top, 'common/unacpp.cpp'),
                               os.path.join(top, 'query/wasastringtoquery.cpp'),
                               os.path.join(top, 'query/wasatorcl.cpp'),
                               os.path.join(top, 'utils/fileudi.cpp'),
                               os.path.join(top, 'rcldb/rcldb.cpp'),
                               os.path.join(top, 'rcldb/rcldoc.cpp'),
                               os.path.join(top, 'rcldb/rclquery.cpp'),
                               os.path.join(top, 'rcldb/searchdata.cpp'),
                               os.path.join(top, 'rcldb/stemdb.cpp'),
                               os.path.join(top, 'rcldb/stoplist.cpp'),
                               os.path.join(top, 'unac/unac.c'),
                               os.path.join(top, 'utils/base64.cpp'),
                               os.path.join(top, 'utils/conftree.cpp'),
                               os.path.join(top, 'utils/debuglog.cpp'),
                               os.path.join(top, 'utils/md5.cpp'),
                               os.path.join(top, 'utils/pathut.cpp'),
                               os.path.join(top, 'utils/readfile.cpp'),
                               os.path.join(top, 'utils/smallut.cpp'),
                               os.path.join(top, 'utils/transcode.cpp'),
                               os.path.join(top, 'utils/wipedir.cpp')
                               ])

setup (name = 'Recoll',
       version = '1.0',
       description = 'Query/Augment a Recoll full text index',
       author = 'J.F. Dockes',
       author_email = 'jean-francois.dockes@wanadoo.fr',
       long_description = '''
''',
       ext_modules = [module1])
