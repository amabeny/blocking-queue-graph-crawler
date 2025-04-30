#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <cstdio>
#include <cstdlib>
#include <curl/curl.h>
#include <stdexcept>
#include "rapidjson/document.h"
#include "rapidjson/error/error.h"
#include "rapidjson/reader.h"
#include <chrono>
#include <atomic>

using namespace std;
using namespace rapidjson;

bool debug = false; // Set to true for detailed debugging output

// Updated service URL
const string SERVICE_URL = "http://hollywood-graph-crawler.bridgesuncc.org/neighbors/";

// 1. Blocking Queue Implementation
template <typename T>
class BlockingQueue {
private:
    queue<T> q;
    mutex m;
    condition_variable cv;
    bool done = false; // Flag to signal no more items

public:
    // Add an item to the queue
    void push(T item) {
        {
            unique_lock<mutex> lock(m);
            q.push(item);
        }
        cv.notify_one(); // Notify one waiting thread
    }

    // Remove an item from the queue
    T pop() {
        unique_lock<mutex> lock(m);
        // Block until there's an item or the queue is done
        cv.wait(lock, [this] { return !q.empty() || done; });
        if (!q.empty()) {
            T item = q.front();
            q.pop();
            return item;
        }
        // If the queue is empty and done is true, throw exception to stop thread.
        throw runtime_error("Queue is empty and done");
    }

    // Check if the queue is empty
    bool empty() const {
        unique_lock<mutex> lock(m);
        return q.empty();
    }

    // Signal that no more items will be added
    void setDone() {
        {
            unique_lock<mutex> lock(m);
            done = true;
        }
        cv.notify_all(); // Notify all waiting threads
    }

    //check if queue is done.
    bool isDone() const{
        unique_lock<mutex> lock(m);
        return done;
    }
};

struct ParseException : std::runtime_error, rapidjson::ParseResult {
    ParseException(rapidjson::ParseErrorCode code, const char* msg, size_t offset) :
        std::runtime_error(msg),
        rapidjson::ParseResult(code, offset) {}
};

#define RAPIDJSON_PARSE_ERROR_NORETURN(code, offset) \
    throw ParseException(code, #code, offset)

// Function to HTTP encode parts of URLs
string url_encode(CURL* curl, string input) {
    char* out = curl_easy_escape(curl, input.c_str(), input.size());
    string s = out;
    curl_free(out);
    return s;
}

// Callback function for writing response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// Function to fetch neighbors using libcurl
string fetch_neighbors(CURL* curl, const string& node) {
    string url = SERVICE_URL + url_encode(curl, node);
    string response;

    if (debug)
        cout << "Sending request to: " << url << endl;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Set a User-Agent header
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: Parallel-GraphCrawler/1.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        cerr << "CURL error for " << node << ": " << curl_easy_strerror(res) << endl;
        return "{}"; // Return empty JSON string on error
    } else {
        if (debug)
            cout << "CURL request successful for " << node << "!" << endl;
    }

    if (debug)
        cout << "Response received for " << node << ": " << response << endl;

    return response;
}

// Function to parse JSON and extract neighbors
vector<string> get_neighbors(const string& json_str) {
    vector<string> neighbors;
    try {
        Document doc;
        doc.Parse(json_str.c_str());

        if (doc.HasMember("neighbors") && doc["neighbors"].IsArray()) {
            for (const auto& neighbor : doc["neighbors"].GetArray())
                neighbors.push_back(neighbor.GetString());
        }
    } catch (const ParseException& e) {
        std::cerr << "Error while parsing JSON: " << json_str << std::endl;
        throw e;
    }
    return neighbors;
}

// 2. Parallel BFS Algorithm with Blocking Queue
void parallel_bfs(CURL* curl, const string& start_node, int depth, vector<string>& result, int num_threads) {
    BlockingQueue<pair<string, int>> q;
    unordered_set<string> visited;
    mutex visited_mutex;
    atomic<int> working_threads(0); // Atomic counter for working threads

    q.push({start_node, 0});
    visited.insert(start_node);

    // Lambda function for worker threads
    auto worker_thread_func = [&]() {
        working_threads++; // Increment when thread starts working
        try {
            while (true) { // Loop until queue is done and empty
                pair<string, int> current_node_info = q.pop(); // Blocks when queue is empty
                string current_node = current_node_info.first;
                int current_depth = current_node_info.second;

                if (current_depth <= depth) {
                    {
                        lock_guard<mutex> lock(visited_mutex);
                        result.push_back(current_node);
                    }
                }

                if (current_depth < depth) {
                    try{
                        for (const auto& neighbor : get_neighbors(fetch_neighbors(curl, current_node))) {
                            lock_guard<mutex> lock(visited_mutex);
                            if (visited.find(neighbor) == visited.end()) {
                                visited.insert(neighbor);
                                q.push({neighbor, current_depth + 1});
                            }
                        }
                    } catch (const ParseException& e){
                        cerr << "Error processing node" << current_node << endl;
                    }
                }
            }
        } catch (const runtime_error& e) {
            // catch the exception.
            if (debug){
                cerr << "Thread exiting: " << e.what() << endl; // Expected "Queue is empty and done"
            }
        }
        working_threads--; // Decrement when thread finishes
    };

    // 2. Create a pool of worker threads
    vector<thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker_thread_func);
    }

    // Wait until all work is added to the queue
    while (!q.empty() || working_threads > 0) {
        this_thread::sleep_for(chrono::milliseconds(10)); //sleep
    }

    q.setDone(); // Signal no more items will be added

    // Join all worker threads
    for (auto& thread : threads) {
        thread.join();
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <node_name> <depth>\n";
        return 1;
    }

    string start_node = argv[1];
    int depth;
    try {
        depth = stoi(argv[2]);
    } catch (const exception& e) {
        cerr << "Error: Depth must be an integer.\n";
        return 1;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        cerr << "Failed to initialize CURL" << endl;
        return -1;
    }

    vector<string> result;
    const auto start_time = chrono::steady_clock::now();
    parallel_bfs(curl, start_node, depth, result, 8); // Use 8 worker threads
    const auto end_time = chrono::steady_clock::now();
    const chrono::duration<double> elapsed_seconds = end_time - start_time;

    cout << "BFS Traversal Results:\n";
    for (const auto& node : result) {
        cout << "- " << node << "\n";
    }
    cout << "Time to crawl: " << elapsed_seconds.count() << "s\n";

    curl_easy_cleanup(curl);
    return 0;
}
