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
LIB = libyarc.so
CC = g++-8
CCFLAGS = -g -Wall -D__LINUX__ -std=c++2a -fPIC
LD = ld
LDFLAGS = -shared

all: $(LIB)

$(LIB): $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) -c -o $@ $< $(CCFLAGS)

clean:
	rm -rf $(OBJS) $(LIB)

dep:
	makedepend -- $(CCFLAGS) -- $(SRCS)
	rm -f Makefile.bak
	
# DO NOT DELETE

yarc_byte_stream.o: yarc_byte_stream.h yarc_api.h /usr/include/stdint.h
yarc_byte_stream.o: /usr/include/string.h /usr/include/stdlib.h
yarc_byte_stream.o: /usr/include/stdio.h
yarc_client_iface.o: yarc_client_iface.h yarc_api.h yarc_dynamic_array.h
yarc_client_iface.o: /usr/include/assert.h /usr/include/features.h
yarc_client_iface.o: /usr/include/stdc-predef.h /usr/include/memory.h
yarc_client_iface.o: /usr/include/string.h /usr/include/stdlib.h
yarc_client_iface.o: yarc_socket_stream.h yarc_byte_stream.h
yarc_client_iface.o: /usr/include/stdint.h /usr/include/netdb.h
yarc_client_iface.o: /usr/include/netinet/in.h /usr/include/endian.h
yarc_client_iface.o: /usr/include/rpc/netdb.h /usr/include/arpa/inet.h
yarc_client_iface.o: /usr/include/unistd.h /usr/include/time.h
yarc_client_iface.o: yarc_protocol_data.h yarc_linked_list.h
yarc_cluster.o: yarc_cluster.h yarc_api.h yarc_dynamic_array.h
yarc_cluster.o: /usr/include/assert.h /usr/include/features.h
yarc_cluster.o: /usr/include/stdc-predef.h /usr/include/memory.h
yarc_cluster.o: /usr/include/string.h /usr/include/stdlib.h
yarc_cluster.o: yarc_linked_list.h yarc_reducer.h yarc_process.h
yarc_cluster.o: /usr/include/stdint.h yarc_simple_client.h yarc_thread.h
yarc_cluster.o: /usr/include/pthread.h /usr/include/sched.h
yarc_cluster.o: /usr/include/time.h yarc_client_iface.h yarc_socket_stream.h
yarc_cluster.o: yarc_byte_stream.h /usr/include/netdb.h
yarc_cluster.o: /usr/include/netinet/in.h /usr/include/endian.h
yarc_cluster.o: /usr/include/rpc/netdb.h /usr/include/arpa/inet.h
yarc_cluster.o: /usr/include/unistd.h yarc_thread_safe_list.h yarc_mutex.h
yarc_cluster.o: yarc_protocol_data.h yarc_misc.h /usr/include/stdio.h
yarc_cluster_client.o: yarc_cluster_client.h yarc_client_iface.h yarc_api.h
yarc_cluster_client.o: yarc_dynamic_array.h /usr/include/assert.h
yarc_cluster_client.o: /usr/include/features.h /usr/include/stdc-predef.h
yarc_cluster_client.o: /usr/include/memory.h /usr/include/string.h
yarc_cluster_client.o: /usr/include/stdlib.h yarc_socket_stream.h
yarc_cluster_client.o: yarc_byte_stream.h /usr/include/stdint.h
yarc_cluster_client.o: /usr/include/netdb.h /usr/include/netinet/in.h
yarc_cluster_client.o: /usr/include/endian.h /usr/include/rpc/netdb.h
yarc_cluster_client.o: /usr/include/arpa/inet.h /usr/include/unistd.h
yarc_cluster_client.o: /usr/include/time.h yarc_simple_client.h yarc_thread.h
yarc_cluster_client.o: /usr/include/pthread.h /usr/include/sched.h
yarc_cluster_client.o: yarc_linked_list.h yarc_thread_safe_list.h
yarc_cluster_client.o: yarc_mutex.h yarc_protocol_data.h yarc_reducer.h
yarc_connection_pool.o: yarc_connection_pool.h yarc_socket_stream.h
yarc_connection_pool.o: yarc_byte_stream.h yarc_api.h /usr/include/stdint.h
yarc_connection_pool.o: /usr/include/netdb.h /usr/include/features.h
yarc_connection_pool.o: /usr/include/stdc-predef.h /usr/include/netinet/in.h
yarc_connection_pool.o: /usr/include/endian.h /usr/include/rpc/netdb.h
yarc_connection_pool.o: /usr/include/arpa/inet.h /usr/include/unistd.h
yarc_connection_pool.o: /usr/include/time.h /usr/include/assert.h
yarc_crc16.o: yarc_crc16.h /usr/include/stdint.h
yarc_misc.o: yarc_misc.h yarc_api.h /usr/include/stdint.h
yarc_misc.o: /usr/include/stdlib.h
yarc_process.o: yarc_process.h /usr/include/string.h /usr/include/unistd.h
yarc_process.o: /usr/include/features.h /usr/include/stdc-predef.h
yarc_protocol_data.o: yarc_protocol_data.h yarc_dynamic_array.h
yarc_protocol_data.o: /usr/include/assert.h /usr/include/features.h
yarc_protocol_data.o: /usr/include/stdc-predef.h /usr/include/memory.h
yarc_protocol_data.o: /usr/include/string.h /usr/include/stdlib.h
yarc_protocol_data.o: yarc_byte_stream.h yarc_api.h /usr/include/stdint.h
yarc_protocol_data.o: yarc_linked_list.h yarc_misc.h yarc_crc16.h
yarc_protocol_data.o: /usr/include/ctype.h
yarc_pubsub.o: yarc_pubsub.h yarc_simple_client.h /usr/include/stdint.h
yarc_pubsub.o: yarc_api.h yarc_thread.h /usr/include/pthread.h
yarc_pubsub.o: /usr/include/features.h /usr/include/stdc-predef.h
yarc_pubsub.o: /usr/include/sched.h /usr/include/time.h yarc_client_iface.h
yarc_pubsub.o: yarc_dynamic_array.h /usr/include/assert.h
yarc_pubsub.o: /usr/include/memory.h /usr/include/string.h
yarc_pubsub.o: /usr/include/stdlib.h yarc_socket_stream.h yarc_byte_stream.h
yarc_pubsub.o: /usr/include/netdb.h /usr/include/netinet/in.h
yarc_pubsub.o: /usr/include/endian.h /usr/include/rpc/netdb.h
yarc_pubsub.o: /usr/include/arpa/inet.h /usr/include/unistd.h
yarc_pubsub.o: yarc_linked_list.h yarc_thread_safe_list.h yarc_mutex.h
yarc_pubsub.o: yarc_protocol_data.h
yarc_reducer.o: yarc_reducer.h yarc_linked_list.h
yarc_simple_client.o: yarc_simple_client.h /usr/include/stdint.h yarc_api.h
yarc_simple_client.o: yarc_thread.h /usr/include/pthread.h
yarc_simple_client.o: /usr/include/features.h /usr/include/stdc-predef.h
yarc_simple_client.o: /usr/include/sched.h /usr/include/time.h
yarc_simple_client.o: yarc_client_iface.h yarc_dynamic_array.h
yarc_simple_client.o: /usr/include/assert.h /usr/include/memory.h
yarc_simple_client.o: /usr/include/string.h /usr/include/stdlib.h
yarc_simple_client.o: yarc_socket_stream.h yarc_byte_stream.h
yarc_simple_client.o: /usr/include/netdb.h /usr/include/netinet/in.h
yarc_simple_client.o: /usr/include/endian.h /usr/include/rpc/netdb.h
yarc_simple_client.o: /usr/include/arpa/inet.h /usr/include/unistd.h
yarc_simple_client.o: yarc_linked_list.h yarc_thread_safe_list.h yarc_mutex.h
yarc_simple_client.o: yarc_protocol_data.h yarc_connection_pool.h yarc_misc.h
yarc_socket_stream.o: yarc_socket_stream.h yarc_byte_stream.h yarc_api.h
yarc_socket_stream.o: /usr/include/stdint.h /usr/include/netdb.h
yarc_socket_stream.o: /usr/include/features.h /usr/include/stdc-predef.h
yarc_socket_stream.o: /usr/include/netinet/in.h /usr/include/endian.h
yarc_socket_stream.o: /usr/include/rpc/netdb.h /usr/include/arpa/inet.h
yarc_socket_stream.o: /usr/include/unistd.h /usr/include/time.h yarc_misc.h
yarc_socket_stream.o: /usr/include/string.h
yarc_thread.o: yarc_thread.h /usr/include/pthread.h /usr/include/features.h
yarc_thread.o: /usr/include/stdc-predef.h /usr/include/sched.h
yarc_thread.o: /usr/include/time.h /usr/include/string.h
