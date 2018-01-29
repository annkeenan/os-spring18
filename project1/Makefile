# DO NOT EDIT -------------------------------
CPP=/afs/nd.edu/user14/csesoft/new/bin/g++
CPPFLAGS=-Wall
LD=/afs/nd.edu/user14/csesoft/new/bin/g++
LDFLAGS=-L/afs/nd.edu/user14/csesoft/2018-spring/lib -lzmq
INCLUDE=/afs/nd.edu/user14/csesoft/2018-spring/include
# ---------------------------- END DO NOT EDIT


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
