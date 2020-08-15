# Makefile for TestClient

SRCS = TestClient.cpp
OBJS = $(SRCS:.cpp=.o)
CC = g++-8
CCFLAGS = -I../Source -g -std=c++2a -D__LINUX__
LD = g++-8
LDFLAGS = -L../Source -lyarc -lpthread
PRGM = TestClient

all: $(PRGM)

$(PRGM): $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) -c -o $@ $< $(CCFLAGS)

clean:
	rm -rf $(PRGM) $(OBJS)

dep:
	makedepend -- $(CCFLAGS) -- $(SRCS)

# DO NOT DELETE

TestClient.o: ../Source/yarc_simple_client.h /usr/include/stdint.h
TestClient.o: ../Source/yarc_api.h ../Source/yarc_thread.h
TestClient.o: /usr/include/pthread.h /usr/include/features.h
TestClient.o: /usr/include/stdc-predef.h /usr/include/sched.h
TestClient.o: /usr/include/time.h ../Source/yarc_client_iface.h
TestClient.o: ../Source/yarc_dynamic_array.h /usr/include/assert.h
TestClient.o: /usr/include/memory.h /usr/include/string.h
TestClient.o: /usr/include/strings.h /usr/include/stdlib.h
TestClient.o: /usr/include/alloca.h ../Source/yarc_socket_stream.h
TestClient.o: ../Source/yarc_byte_stream.h /usr/include/netdb.h
TestClient.o: /usr/include/netinet/in.h /usr/include/endian.h
TestClient.o: /usr/include/rpc/netdb.h /usr/include/arpa/inet.h
TestClient.o: /usr/include/unistd.h ../Source/yarc_linked_list.h
TestClient.o: ../Source/yarc_thread_safe_list.h ../Source/yarc_mutex.h
TestClient.o: ../Source/yarc_protocol_data.h /usr/include/stdio.h
