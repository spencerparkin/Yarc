# Makefile for YarcTester

SRCS = App.cpp \
		ClusterTestCase.cpp \
		Frame.cpp \
		SimpleTestCase.cpp \
		TestCase.cpp
OBJS = $(SRCS:.cpp=.o)
CC = `wx-config --cxx`
CCFLAGS = `wx-config --cxxflags` -g -Wall -I../../Source -std=c++2a -D__LINUX__
LD = `wx-config --cxx`
LDFLAGS = `wx-config --libs` -L../../Source -lyarc
PRGM = YarcTester

all: $(PRGM)

$(PRGM): $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) -c -o $@ $< $(CCFLAGS)

clean:
	rm -rf $(OBJS) $(PRGM)

dep:
	makedepend $(SRCS)

