# DO NOT EDIT -------------------------------
CPP=/afs/nd.edu/user14/csesoft/new/bin/g++
CPPFLAGS=-Wall
LD=/afs/nd.edu/user14/csesoft/new/bin/g++
LDFLAGS=-L/afs/nd.edu/user14/csesoft/2018-spring/lib -lzmq
INCLUDE=/afs/nd.edu/user14/csesoft/2018-spring/include
# ---------------------------- END DO NOT EDIT

# set environment variable if shared libraries not linked:
# setenv LD_LIBRARY_PATH /opt/und/local/lib:/opt/und/intel//compiler2016/lib:/opt/und/pgi/8.0/linux86-64/8.0/lib:/afs/nd.edu/i386_linux24/opt/und/matlab/9.1/sys/opengl/lib/glnxa64:/afs/nd.edu/user14/csesoft/2018-spring/lib

CPPFLAGS += -std=c++11 -g
LDFLAGS +=
OBJECTS=Server.o

project1: $(OBJECTS)
	$(LD) $^ $(LDFLAGS) -o $@

%.o: %.cpp
	$(CPP) -I$(INCLUDE) $(CPPFLAGS) -c $<

.PHONY: clean
clean:
	rm -f *.o project1
