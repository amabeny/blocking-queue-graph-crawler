# blocking-queue-graph-crawler

## Description

This C++ program implements a parallel breadth-first search (BFS) algorithm to traverse a graph using a blocking queue for efficient thread management.  It interacts with a web API to fetch graph data.

## Features

* Parallel BFS traversal
* Uses a thread-safe blocking queue
* Fetches graph data from a web API
* Handles graph traversal up to a specified depth

## Dependencies

* C++ compiler (g++)
* libcurl
* RapidJSON
* pthread (for threading)

## Building

1.  Clone the repository.
2.  Ensure you have the dependencies installed.
3.  Compile the program using the provided `Makefile`:

    ```bash
    make
    ```

## Running

```bash
./blocking_queue_client <node_name> <depth>
