CXXFLAGS=-I./rapidjson/include -pthread
LDFLAGS=-lcurl
LD=g++
CC=g++

all: blocking_queue_client sequential_client

blocking_queue_client: blocking_queue_client.o
	$(LD) $< -o $@ $(LDFLAGS)
blocking_queue_client.o: client.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

sequential_client: sequential_client.o
	$(LD) $< -o $@ $(LDFLAGS)
sequential_client.o: sequential_client.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@
clean:
	-rm -f blocking_queue_client blocking_queue_client.o sequential_client sequential_client.o
