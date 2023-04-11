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

struct Resource {
    string resouce_name;
    int max_available;
    int held;
};

struct Task {
    string task_name;
    int busy_time;
    int idle_time;
    int iterations_completed;
    int total_wait_time;
    map<string, int> resources_needed;
    pthread_t tid;
    string state; // (WAIT, RUN, IDLE)
};

// Global vars
// TODO: Redo
map<string, Resource> global_resources_map;
vector<Task> global_task_vector;
int NITER;
string input_file;
int monitor_time;
mutex mtx;
high_resolution_clock::time_point start_time;

// TODO: Redo
// Attempts to acquire a resource
//  Returns false if unsuccessful
//  Returns true if successful
bool resource_acquired(Task& task) {
    unique_lock<mutex> lock(mtx);          // Lock the mutex

    // Check if the resources needed are available
    for (auto& res : task.resources_needed) {
        Resource& resource = global_resources_map[res.first];
        if (resource.held + res.second > resource.max_available) {
            return false;
        }
    }

    // Acquire the resources needed
    for (auto& res : task.resources_needed) {
        Resource& resource = global_resources_map[res.first];
        resource.held += res.second;
    }

    return true;
}

// TODO: Redo
// Function to release_resource resources held by a task
void release_resource(Task& task) {
    unique_lock<mutex> lock(mtx);          // Lock the mutex

    // Release resources held by the task
    for (auto& res : task.resources_needed) {
        Resource& resource = global_resources_map[res.first];
        resource.held -= res.second;
    }
}

// TODO: Redo
// Function executed by task threads
void* task_thread(void* arg) {
    Task* task_ptr = (Task*)arg;
    Task& task = *task_ptr;

    // Loop for NITER iterations
    for (int i = 0; i < NITER; ++i) {
        high_resolution_clock::time_point wait_start = high_resolution_clock::now();

        // Try acquiring resources needed for the task
        while (resource_acquired(task) == false) {
            // Wait for 10ms if no resources available
            usleep(10000);
        }

        // Update total_wait_time of the task
        task.total_wait_time += duration_cast<milliseconds>(high_resolution_clock::now() - wait_start).count();

        // Set task state to "RUN"
        {
            unique_lock<mutex> lock(mtx);
            task.state = "RUN";
        }

        // Simulate task's busy time
        usleep(task.busy_time * 1000);

        // Increment task iteration, print task info, and set task state to "IDLE"
        {
            unique_lock<mutex> lock(mtx);
            task.iterations_completed++;
            int time_elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - start_time).count();

            // Convert thread ID to hexadecimal string
            stringstream hex_tid;
            hex_tid << std::hex << task.tid;

            cout << "task: " << task.task_name << " (tid= 0x" << hex_tid.str() << ", iter= " << task.iterations_completed << ", time= " << time_elapsed << " msec)" << endl;
            task.state = "IDLE";
        }

        // Release resources held by the task
        release_resource(task);

        // Simulate task's idle time
        usleep(task.idle_time * 1000);
    }

    return NULL;
}

// TODO: Redo
// Function executed by monitor thread
void* monitor_thread(void* arg) {
    while (true) {
        // Sleep for the monitor_time interval
        usleep(monitor_time * 1000);
        unique_lock<mutex> lock(mtx);

        // Check if all tasks have completed their iterations
        bool all_done = true;
        for (const Task& task : global_task_vector) {
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
            for (const Task& task : global_task_vector) {
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
// Main function
int main(int argc, char* argv[]) {
    // Check args
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " input_file monitor_time NITER" << endl;
        return 1;
    }
    input_file = argv[1];
    monitor_time = stoi(argv[2]);
    NITER = stoi(argv[3]);


    // Open input file
    ifstream fin(input_file);
    if (!fin) {
        cerr << "Error opening input file: " << input_file << endl;
        return 1;
    }

    // Read input file and create tasks and resources
    string line;
    while (getline(fin, line)) {
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }

        stringstream ss(line);
        string keyword;
        ss >> keyword;

        if (keyword == "resources") {
            string resource_str;
            while (ss >> resource_str) {
                size_t pos = resource_str.find(':');
                string name = resource_str.substr(0, pos);
                int value = stoi(resource_str.substr(pos + 1));

                Resource resource;
                resource.resouce_name = name;
                resource.max_available = value;
                resource.held = 0;
                global_resources_map[name] = resource;
            }
        } else if (keyword == "task") {
            Task task;
            ss >> task.task_name >> task.busy_time >> task.idle_time;

            string resource_str;
            while (ss >> resource_str) {
                size_t pos = resource_str.find(':');
                string name = resource_str.substr(0, pos);
                int value = stoi(resource_str.substr(pos + 1));

                task.resources_needed[name] = value;
            }

            task.iterations_completed = 0;
            task.state = "WAIT";
            task.total_wait_time = 0;
            global_task_vector.push_back(task);
        }
    }
    // Close input file
    fin.close();

    // Record the start time of the program
    start_time = high_resolution_clock::now();

    // Create and start monitor thread
    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, monitor_thread, NULL);

    // Create and start task threads
    for (Task& task : global_task_vector) {
        pthread_create(&task.tid, NULL, task_thread, &task);
    }

    // Wait for all task threads to finish
    for (Task& task : global_task_vector) {
        pthread_join(task.tid, NULL);
    }

    // Cancel and join the monitor thread
    pthread_cancel(monitor_tid);
    pthread_join(monitor_tid, NULL);

    // Print information on resource types
    cout << "System Resources:" << endl;
    for (const auto& res : global_resources_map) {
        cout << res.first << ": (max_available= " << res.second.max_available << ", held= " << res.second.held << ")" << endl;
    }
    cout << endl;

    // Print information on tasks
    cout << "System Tasks:" << endl;
    for (size_t i = 0; i < global_task_vector.size(); ++i) {
        const Task& task = global_task_vector[i];

        // Convert thread ID to hexadecimal string
        stringstream hex_tid;
        hex_tid << std::hex << task.tid;

        cout << "[" << i << "] " << task.task_name << " (IDLE, runTime= " << task.busy_time << " msec, idle_time= " << task.idle_time << " msec):" << endl;
        cout << "       (tid= 0x" << hex_tid.str() << ")" << endl;

        for (const auto& res : task.resources_needed) {
            cout << "       " << res.first << ": (needed= " << res.second << ", held= 0)" << endl;
        }

        cout  << "       " << "(RUN: " << task.iterations_completed << " times, WAIT: " << task.total_wait_time << " msec)" << endl;
        cout << endl;
    }

    // Calculate and print total running time
    int total_time = duration_cast<milliseconds>(high_resolution_clock::now() - start_time).count();
    cout << "Running time= " << total_time << " msec" << endl;

    return 0;
}