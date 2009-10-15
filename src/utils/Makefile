depth = ..
include $(depth)/mk/sysconf

PROGS = trmd5 trreadfile trfileudi trconftree wipedir smallut \
      trfstreewalk trpathut \
      transcode trbase64 \
      trmimeparse trexecmd utf8iter idfile

all: $(PROGS) $(BIGLIB)

$(BIGLIB): force
	cd $(depth)/lib;$(MAKE)
force:

FSTREEWALK_OBJS= trfstreewalk.o fstreewalk.o $(BIGLIB)
trfstreewalk : $(FSTREEWALK_OBJS)
	$(CXX) -o trfstreewalk $(FSTREEWALK_OBJS) $(LIBICONV)
trfstreewalk.o : fstreewalk.cpp fstreewalk.h
	$(CXX) -o trfstreewalk.o -c $(ALL_CXXFLAGS) \
	       -DTEST_FSTREEWALK fstreewalk.cpp

READFILE_OBJS= trreadfile.o readfile.o $(BIGLIB)
trreadfile : $(READFILE_OBJS)
	$(CXX) -o trreadfile $(READFILE_OBJS) $(LIBICONV) $(LIBSYS)
trreadfile.o : readfile.cpp readfile.h
	$(CXX) -o trreadfile.o -c $(ALL_CXXFLAGS) \
	       -DTEST_READFILE readfile.cpp

MD5_OBJS= trmd5.o md5.o $(BIGLIB)
trmd5 : $(MD5_OBJS)
	$(CXX) -o trmd5 $(MD5_OBJS) $(LIBICONV) $(LIBSYS)
trmd5.o : md5.cpp md5.h
	$(CXX) -o trmd5.o -c $(ALL_CXXFLAGS) \
	       -DTEST_MD5 md5.cpp

PATHUT_OBJS= trpathut.o  $(BIGLIB)
trpathut : $(PATHUT_OBJS) 
	$(CXX) $(ALL_CXXFLAGS) -o trpathut $(PATHUT_OBJS)  $(LIBICONV)
trpathut.o : pathut.cpp pathut.h
	$(CXX) -o trpathut.o -c $(ALL_CXXFLAGS) -DTEST_PATHUT pathut.cpp

FILEUDI_OBJS= trfileudi.o  $(BIGLIB)
trfileudi : $(FILEUDI_OBJS) 
	$(CXX) $(ALL_CXXFLAGS) -o trfileudi $(FILEUDI_OBJS)
trfileudi.o : fileudi.cpp fileudi.h
	$(CXX) -o trfileudi.o -c $(ALL_CXXFLAGS) -DTEST_FILEUDI fileudi.cpp

EXECMD_OBJS= trexecmd.o $(BIGLIB)
trexecmd : $(EXECMD_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o trexecmd $(EXECMD_OBJS) $(LIBICONV) $(LIBSYS)
trexecmd.o : execmd.cpp execmd.h
	$(CXX) -o trexecmd.o -c $(ALL_CXXFLAGS) -DTEST_EXECMD execmd.cpp

TRANSCODE_OBJS= trtranscode.o  $(BIGLIB) 
transcode : $(TRANSCODE_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o transcode $(TRANSCODE_OBJS) $(LIBICONV)
trtranscode.o : transcode.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_TRANSCODE -c -o trtranscode.o \
	       transcode.cpp

IDFILE_OBJS= tridfile.o  $(BIGLIB) 
idfile : $(IDFILE_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o idfile $(IDFILE_OBJS) $(LIBICONV)
tridfile.o : idfile.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_IDFILE -c -o tridfile.o \
	       idfile.cpp

MIMEPARSE_OBJS= trmimeparse.o  $(BIGLIB) 
trmimeparse : $(MIMEPARSE_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o trmimeparse $(MIMEPARSE_OBJS) $(LIBICONV)
trmimeparse.o : mimeparse.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_MIMEPARSE -c -o trmimeparse.o \
	       mimeparse.cpp

SMALLUT_OBJS= trsmallut.o  $(BIGLIB) 
smallut : $(SMALLUT_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o smallut $(SMALLUT_OBJS) $(LIBICONV)
trsmallut.o : smallut.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_SMALLUT -c -o trsmallut.o \
	       smallut.cpp

WIPEDIR_OBJS= trwipedir.o  $(BIGLIB) 
wipedir : $(WIPEDIR_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o wipedir $(WIPEDIR_OBJS) $(LIBICONV)
trwipedir.o : wipedir.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_WIPEDIR -c -o trwipedir.o \
	       wipedir.cpp

UTF8ITER_OBJS= trutf8iter.o 
utf8iter : $(UTF8ITER_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o utf8iter $(UTF8ITER_OBJS) $(BIGLIB) $(LIBICONV) 
trutf8iter.o : utf8iter.cpp utf8iter.h
	$(CXX) $(ALL_CXXFLAGS) -DTEST_UTF8ITER -c -o trutf8iter.o \
	       utf8iter.cpp


CONFTREE_OBJS= trconftree.o  $(BIGLIB)
trconftree : $(CONFTREE_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o trconftree $(CONFTREE_OBJS) $(LIBICONV) 
trconftree.o : conftree.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_CONFTREE -c -o trconftree.o \
	       conftree.cpp

BASE64_OBJS= trbase64.o  $(BIGLIB)
trbase64 : $(BASE64_OBJS) 
	$(CXX) $(ALL_CXXFLAGS) -o trbase64 $(BASE64_OBJS)
trbase64.o : base64.cpp 
	$(CXX) $(ALL_CXXFLAGS) -DTEST_BASE64 -c -o trbase64.o \
	       base64.cpp

X11MON_OBJS= trx11mon.o x11mon.o
trx11mon : $(X11MON_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o trx11mon $(X11MON_OBJS)  -L/usr/X11R6/lib -lX11
trx11mon.o : x11mon.cpp x11mon.h
	$(CXX) -o trx11mon.o -c $(ALL_CXXFLAGS) -DTEST_X11MON x11mon.cpp
x11mon.o: x11mon.cpp
	$(CXX) -c -I/usr/X11R6/include $(ALL_CXXFLAGS) x11mon.cpp
clean:
	rm -f *.o $(PROGS)

