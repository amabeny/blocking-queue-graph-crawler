CXXFLAGS=-I./rapidjson/include -pthread
LDFLAGS=-lcurl
LD=g++
CC=g++

all: blocking_queue_client

blocking_queue_client: blocking_queue_client.o
    $(LD) $< -o $@ $(LDFLAGS)

blocking_queue_client.o: client.cpp # Changed to client.cpp
    $(CC) $(CXXFLAGS) -c $< -o $@

clean:
    -rm -f blocking_queue_client blocking_queue_client.o
