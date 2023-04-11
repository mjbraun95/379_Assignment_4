#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <pthread.h>
#include <semaphore.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#define NRES_TYPES 10
#define NTASKS 25

using namespace std;
using namespace chrono;

class Resource;
class Task;

// Global vars
map<string, Resource> global_resource_pool;

vector<Task> global_tasks;

// mutex global_resource_pool_mtx;
// mutex global_tasks_mtx;
mutex global_mtx;

high_resolution_clock::time_point global_start_time = high_resolution_clock::now();

int NITER;
string input_file;
int monitor_time;

// TODO?: Implement semaphores
class Resource {
public:
    string resource_name; //TODO? Remove if redundant
    int current_quantity_held;
    int max_quantity_available;
    // sem_t semaphore;

    Resource() {} // Default constructor

    Resource(string resource_name, int max_quantity_available)
    {
        // sem_init(&semaphore, 0, max_quantity_available);
        current_quantity_held = 0;
        max_quantity_available = max_quantity_available;
    }
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
    {
        this->task_name = task_name;
        this->busy_time = busy_time;
        this->idle_time = idle_time;
        this->resources_needed = resources_needed;
        iterations_completed = 0;
        total_wait_time = 0;
        state = "WAIT";
    }

    // Attempts to acquire a resource
    //      - Returns false if unsuccessful
    //      - Returns true if successful
    bool attempt_to_get_needed_resources() {
        // Occupy resource pool
        // unique_lock<mutex> lock(global_resource_pool_mtx); //TODO?: Redo
        unique_lock<mutex> lock(global_mtx); //TODO?: Redo

        // Check if the resources needed are available
        for (auto name_value_pair = this->resources_needed.begin(); name_value_pair != this->resources_needed.end(); name_value_pair++) {
            const string& resource_name = name_value_pair->first;
            const int quantity_needed = name_value_pair->second;
            Resource& resource = global_resource_pool[resource_name];
            if (resource.current_quantity_held + quantity_needed > resource.max_quantity_available) {
                return false;
            }
        }

        // Resources are available. Acquire the resources needed
        for (auto name_value_pair = this->resources_needed.begin(); name_value_pair != this->resources_needed.end(); name_value_pair++) {
            const string& resource_name = name_value_pair->first;
            const int quantity_needed = name_value_pair->second;
            Resource& resource = global_resource_pool[resource_name];
            resource.current_quantity_held += quantity_needed;
        }

        return true;
    }

    void release_resources() {
        // unique_lock<mutex> lock(global_resource_pool_mtx); //TODO?: Redo
        unique_lock<mutex> lock(global_mtx); //TODO?: Redo

        // Release resources held by the task
        for (auto name_value_pair = this->resources_needed.begin(); name_value_pair != this->resources_needed.end(); name_value_pair++) {
            const string& resource_name = name_value_pair->first;
            const int quantity_unoccupied = name_value_pair->second;
            Resource& resource = global_resource_pool[resource_name];
            resource.current_quantity_held -= quantity_unoccupied;
        }
    }
};

// TODO: Redo
// Function executed by monitor thread
void* monitor_thread(void* arg) {
    while (true) {
        // Sleep for the monitorTime interval
        usleep(monitor_time * 1000);
        unique_lock<mutex> lock(global_mtx);

        // Check if all tasks have completed their iterations
        bool all_done = true;
        for (const Task& task : global_tasks) {
            if (task.iterations_completed < NITER) {
                all_done = false;
                break;
            }
        }

        // If all tasks are done, break the loop
        if (all_done) {
            break;
        }

        // Print the monitor information
        cout << "monitor: ";
        for (const string& state : {"WAIT", "RUN", "IDLE"}) {
            cout << "[" << state << "] ";
            for (const Task& task : global_tasks) {
                if (task.state == state) {
                    cout << task.task_name << " ";
                }
            }
            cout << endl << "         ";
        }
        cout << endl;
    }

    return NULL;
}

// TODO: Redo
void *task_thread(void *arg) {
    Task *task = (Task *)arg;
    // cout << "debug: task_thread" << endl;
    for (int i = 0; i < NITER; ++i) {
        high_resolution_clock::time_point wait_start_time = high_resolution_clock::now();

        // Wait until resources acquired
        while (task->attempt_to_get_needed_resources() == false) {
            usleep(10000);
        }

        // Update total_wait_time of the task
        task->total_wait_time += duration_cast<milliseconds>(high_resolution_clock::now() - wait_start_time).count();

        // TODO: Redo
        // Set task state to "RUN"
        {
            // unique_lock<mutex> lock(global_tasks_mtx);
            unique_lock<mutex> lock(global_mtx);
            task->state = "RUN";
        }

        // TODO: Redo
        // Simulate task's busy time
        usleep(task->busy_time * 1000);

        // TODO: Redo
        // Increment task iteration, print task info, and set task state to "IDLE"
        {
            // unique_lock<mutex> lock(global_tasks_mtx);
            unique_lock<mutex> lock(global_mtx);
            task->iterations_completed++;
            int time_elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - global_start_time).count();

            // Convert thread ID to hexadecimal string
            stringstream hex_tid;
            hex_tid << hex << task->tid;

            cout << "task: " << task->task_name << " (tid= 0x" << hex_tid.str() << ", iter= " << task->iterations_completed << ", time= " << time_elapsed << " msec)" << endl;
            task->state = "IDLE";
        }

        // Release resources held by the task
        task->release_resources();

        // Simulate task's idle time
        usleep(task->idle_time * 1000);
    }

    return NULL;
}



int main(int argc, char* argv[]) {

    // Check arguments match
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " input_file monitor_time number_of_iterations" << endl;
        return 1;
    }
    input_file = argv[1];
    monitor_time = stoi(argv[2]);
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
                int max_quantity_available = stoi(max_resources_available_str);

                // Load new Resource
                Resource resource_type = Resource(resource_name, max_quantity_available);
                global_resource_pool.insert(pair<string, Resource>(resource_name, resource_type));
            }
        } else if (first_word == "task") {
            // Load attributes
            string task_name, busy_time_str, idle_time_str;
            string_reader >> task_name >> busy_time_str >> idle_time_str;
            // cout << "debug 283: task_name: " << task_name << endl;
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
            global_tasks.push_back(task);

        } else {
            cout << "Invalid line in input file. Skipping" << endl;
            continue;
        }
    }
    // Close the input file
    input_file_reader.close();
    cout << "debug: Input file closed. Spawning tasks" << endl;

    // Create and start monitor thread
    // TODO?: Redo
    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, monitor_thread, NULL);

    // Spawn all task threads, then wait for each thread to close
    for (Task& task : global_tasks) {
        pthread_create(&task.tid, NULL, task_thread, &task);
    }
    for (Task& task : global_tasks) {
        pthread_join(task.tid, NULL);
    }

    // TODO?: Redo
    // Cancel and join the monitor thread
    pthread_cancel(monitor_tid);
    pthread_join(monitor_tid, NULL);

    // TODO: Implement final printing

    return 0;
}