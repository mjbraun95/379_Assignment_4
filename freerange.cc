#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <pthread.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#define NRES_TYPES 10
#define NTASKS 25

using namespace std;
using namespace chrono;

class Resource_Type;
class Task;

// Global vars
// TODO: Redo
map<string, Resource_Type> resource_types_map;
vector<Task> tasks_vector;
int NITER;
string input_file;
int monitorTime;
mutex mtx;
high_resolution_clock::time_point start_time;

class Resource_Type {
public:
    string resource_name;
    int max_resources_available;
    int held;

    Resource_Type() {} // Default constructor

    Resource_Type(string resource_name, int max_resources_available)
      : resource_name(resource_name),
        max_resources_available(max_resources_available),
        held(0) {}

    // void print_attributes() {
    //     cout << "resource_name: " << resource_name << "\nmax_resources_available: " << max_resources_available << "\nheld: " << held << endl;
    // }
};

class Task {
public:
    string task_name;
    int busy_time;
    int idle_time;
    map<string, int> resources_needed;
    int iterations_completed;
    int total_wait_time;
    pthread_t tid;
    string state; // (WAIT, RUN, IDLE)

    Task(string task_name, int busy_time, int idle_time, map<string, int> resources_needed)
      : task_name(task_name),
        busy_time(busy_time),
        idle_time(idle_time),
        resources_needed(resources_needed),
        iterations_completed(0),
        total_wait_time(0),
        state("WAIT") {}

    // TODO: Redo
    // Attempts to acquire a resource
    //  Returns false if unsuccessful
    //  Returns true if successful
    bool resource_acquired() {
        unique_lock<mutex> lock(mtx);          // Lock the mutex

        // Check if the resources needed are available
        for (auto& pair : this.resources_needed) {
            Resource_Type& resource = resource_types_map[pair.first];
            if (resource.held + pair.second > resource.maxAvail) {
                return false;
            }
        }

        // Acquire the resources needed
        for (auto& pair : this.resources_needed) {
            Resource_Type& resource = resource_types_map[pair.first];
            resource.held += pair.second;
        }

        return true;
    }

    // TODO: Redo
    // Function to release_resource resources held by a task
    void release_resource() {
        unique_lock<mutex> lock(mtx);          // Lock the mutex

        // Release resources held by the task
        for (auto& pair : task.resources_needed) {
            Resource_Type& resource = resource_types_map[pair.first];
            resource.held -= pair.second;
        }
    }

    static void *task_thread(void *arg) {
        Task *task = static_cast<Task *>(arg);
        cout << "task_thread" << endl;
        return NULL;
    }
    // void print_attributes() {
    //     cout << "task_name: " << task_name << "\nbusy_time: " << busy_time << "\nidle_time: " << idle_time << "\nresources_needed: ";
    //     for (const auto& kv_pair : resources_needed) {
    //         cout << kv_pair.first << ": " << kv_pair.second << ", ";
    //     }
    //     cout << endl;
    // }
};

// void parse_

int main(int argc, char* argv[]) {

    // Check arguments match
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <input file> <monitor time> <number of iterations>" << endl;
        return 1;
    }
    input_file = argv[1];
    monitorTime = stoi(argv[2]);
    NITER = stoi(argv[3]);

    // Source: https://www.bogotobogo.com/cplusplus/fstream_input_output.php
    string line;

    ifstream input_file_reader(input_file);
    if (!input_file_reader) {
        cerr << "Error opening input file." << endl;
        return 1;
    }

    // Load input file data
    // Iterate through each input file line
    while (!input_file_reader.eof()) {
        // Load next input file line into line
        getline(input_file_reader, line);
        // Skip lines starting with #
        if (line[0] == '#') {
            continue;
        }
        // Skip blank lines
        if (line.empty()) {
            continue;
        }

        // Source: https://www.softwaretestinghelp.com/stringstream-class-in-cpp/
        stringstream string_reader(line);
        string first_word;

        // Load first line word into stringstream
        string_reader >> first_word;

        string resource_name_quantity_str;
        size_t delimiter;
        // cout << "debug: " << "first_word: " << first_word << endl;
        if (first_word == "resources") {
            // Iterate through each resource specified in resources line
            while (string_reader >> resource_name_quantity_str) {
                // Parse string
                delimiter = resource_name_quantity_str.find(':');
                string resource_name = resource_name_quantity_str.substr(0, delimiter);
                string max_resources_available_str = resource_name_quantity_str.substr(delimiter+1);
                int max_resources_available = stoi(max_resources_available_str);

                // Load new Resource_Type
                Resource_Type resource_type = Resource_Type(resource_name, max_resources_available);
                resource_types_map.insert(pair<string, Resource_Type>(resource_name, resource_type));
            }
        } else if (first_word == "task") {
            // Load attributes
            string task_name, busy_time_str, idle_time_str;
            string_reader >> task_name >> busy_time_str >> idle_time_str;
            int busy_time = stoi(busy_time_str);
            int idle_time = stoi(idle_time_str);
            map<string, int> resources_needed;
            while (string_reader >> resource_name_quantity_str) {
                // Parse string
                delimiter = resource_name_quantity_str.find(':');
                string resource_name = resource_name_quantity_str.substr(0, delimiter);
                string quantity_needed_str = resource_name_quantity_str.substr(delimiter+1);
                int quantity_needed = stoi(quantity_needed_str);
                resources_needed.insert(make_pair(resource_name, quantity_needed));

            }

            // Load new task
            Task task = Task(task_name, busy_time, idle_time, resources_needed);
            tasks_vector.push_back(task);

        } else {
            cout << "Invalid line in input file. Skipping" << endl;
            continue;
        }
    }
    // Close the input file
    input_file_reader.close();

    // Initialize + run task threads
    for (Task& task : tasks_vector) {
        pthread_create(&task.tid, NULL, &Task::task_thread, &task);
    }

    // Wait for each task thread to complete
    for (Task& task : tasks_vector) {
        pthread_join(task.tid, NULL);
    }

    return 0;
}