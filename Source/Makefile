# Makefile for Yarc library.

SRCS = yarc_byte_stream.cpp \
		yarc_client_iface.cpp \
		yarc_cluster.cpp \
		yarc_cluster_client.cpp \
		yarc_connection_pool.cpp \
		yarc_crc16.cpp \
		yarc_dllmain.cpp \
		yarc_misc.cpp \
		yarc_process.cpp \
		yarc_protocol_data.cpp \
		yarc_pubsub.cpp \
		yarc_reducer.cpp \
		yarc_simple_client.cpp \
		yarc_socket_stream.cpp \
		yarc_thread.cpp

OBJS = $(SRCS:.cpp=.o)
LIB = yarc.so
CC = g++-8
CCFLAGS = -g -Wall -D__LINUX__ -std=c++2a -fPIC
LD = ld
LDFLAGS = -shared -lc

all: $(LIB)

$(LIB): $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) -c -o $@ $< $(CCFLAGS)

clean:
	rm -rf $(OBJS) $(LIB)

dep:
	makedepend -- $(CCFLAGS) -- $(SRCS)
	
