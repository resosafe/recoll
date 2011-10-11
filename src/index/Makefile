depth = ..
include $(depth)/mk/sysconf

PROGS = recollindex 
SRCS = recollindex.cpp rclmonrcv.cpp rclmonprc.cpp

all: depend $(PROGS) $(BIGLIB)

RECOLLINDEX_OBJS= recollindex.o rclmonrcv.o rclmonprc.o $(BIGLIB) 
recollindex : $(RECOLLINDEX_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o recollindex $(RECOLLINDEX_OBJS) \
	       $(BSTATIC) $(LIBXAPIAN) $(LIBXAPIANSTATICEXTRA) \
	       $(LIBICONV) $(BDYNAMIC) \
	       $(LIBFAM) \
	       $(X_LIBS) $(X_PRE_LIBS) $(X_LIBX11) $(X_EXTRA_LIBS) \
	       $(LIBSYS)
recollindex.o : recollindex.cpp
	$(CXX) $(ALL_CXXFLAGS) -c -o recollindex.o $<
rclmonrcv.o : rclmonrcv.cpp 
	$(CXX) $(ALL_CXXFLAGS) -c -o rclmonrcv.o $<
rclmonprc.o : rclmonprc.cpp 
	$(CXX) $(ALL_CXXFLAGS) -c -o rclmonprc.o $<

CSGUESS_OBJS= trcsguess.o  $(BIGLIB) 
csguess : $(CSGUESS_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o csguess $(CSGUESS_OBJS) \
	       $(LIBICONV)
trcsguess.o : csguess.cpp
	$(CXX) $(ALL_CXXFLAGS) -DTEST_CSGUESS -c -o trcsguess.o \
	       csguess.cpp

MIMETYPE_OBJS= trmimetype.o  $(BIGLIB) 
mimetype : $(MIMETYPE_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o mimetype $(MIMETYPE_OBJS) \
	       $(LIBICONV) $(LIBSYS)
trmimetype.o : mimetype.cpp
	$(CXX) $(ALL_CXXFLAGS) -DTEST_MIMETYPE -c -o trmimetype.o \
	       mimetype.cpp

$(BIGLIB): force
	cd $(depth)/lib;$(MAKE)
force:

depend: alldeps.stamp
alldeps.stamp : $(SRCS)
	$(CXX) -M $(ALL_CXXFLAGS) $(SRCS) > alldeps
	touch alldeps.stamp

clean:
	cp /dev/null alldeps
	rm -f alldeps.stamp
	rm -f *.o $(PROGS)

include alldeps
