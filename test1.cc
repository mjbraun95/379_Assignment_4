// Include necessary libraries and headers
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <pthread.h>
#include <unistd.h>

// Define constants
#define NRES_TYPES 10
#define NTASKS 25

// Use standard namespace and chrono namespace
using namespace std;
using namespace chrono;

// Define ResourceType structure
struct ResourceType {
    string name;           // Name of the resource
    int max_avail;         // Maximum available units of the resource
    int held;              // Units of the resource currently held by tasks
};

// Define Task structure
struct Task {
    string name;                            // Name of the task
    int busyTime;                          // Time spent on the task when it is busy
    int idleTime;                          // Time spent on the task when it is idle
    map<string, int> resources_needed;      // Map of resources required by the task
    int iteration;                          // Current iteration of the task
    pthread_t tid;                          // Task thread ID
    string state;                           // Task state (WAIT, RUN, IDLE)
    int wait_time;                          // Time spent waiting for resources
};

// Global variables
map<string, ResourceType> resources;       // Vector to store ResourceType objects
vector<Task> tasks;                        // Vector to store Task objects
int NITER;                                 // Number of iterations for each task
int monitorTime;                          // Time interval for the monitor thread
mutex mtx;                                 // Mutex for protecting shared data
high_resolution_clock::time_point start_time; // Start time of the program

// Function to try acquiring resources for a task
bool try_acquire(Task& task) {
    unique_lock<mutex> lock(mtx);          // Lock the mutex

    // Check if the resources needed are available
    for (auto& res : task.resources_needed) {
        ResourceType& resource = resources[res.first];
        if (resource.held + res.second > resource.max_avail) {
            return false;
        }
    }

    // Acquire the resources needed
    for (auto& res : task.resources_needed) {
        ResourceType& resource = resources[res.first];
        resource.held += res.second;
    }

    return true;
}

// Function to release resources held by a task
void release(Task& task) {
    unique_lock<mutex> lock(mtx);          // Lock the mutex

    // Release resources held by the task
    for (auto& res : task.resources_needed) {
        ResourceType& resource = resources[res.first];
        resource.held -= res.second;
    }
}

// Function executed by task threads
void* task_thread(void* arg) {
    Task* task_ptr = (Task*)arg;
    Task& task = *task_ptr;

    // Loop for NITER iterations
    for (int i = 0; i < NITER; ++i) {
        high_resolution_clock::time_point wait_start = high_resolution_clock::now();

        // Try acquiring resources needed for the task
        while (!try_acquire(task)) {
            usleep(10000);
        }

        // Update wait_time of the task
        task.wait_time += duration_cast<milliseconds>(high_resolution_clock::now() - wait_start).count();

        // Set task state to "RUN"
        {
            unique_lock<mutex> lock(mtx);
            task.state = "RUN";
        }

        // Simulate task's busy time
        usleep(task.busyTime * 1000);

        // Increment task iteration, print task info, and set task state to "IDLE"
        {
            unique_lock<mutex> lock(mtx);
            task.iteration++;
            int time_elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - start_time).count();

            // Convert thread ID to hexadecimal string
            stringstream hex_tid;
            hex_tid << std::hex << task.tid;

            cout << "task: " << task.name << " (tid= 0x" << hex_tid.str() << ", iter= " << task.iteration << ", time= " << time_elapsed << " msec)" << endl;
            task.state = "IDLE";
        }

        // Release resources held by the task
        release(task);

        // Simulate task's idle time
        usleep(task.idleTime * 1000);
    }

    return NULL;
}

// Function executed by monitor thread
void* monitor_thread(void* arg) {
    while (true) {
        // Sleep for the monitorTime interval
        usleep(monitorTime * 1000);
        unique_lock<mutex> lock(mtx);

        // Check if all tasks have completed their iterations
        bool all_done = true;
        for (const Task& task : tasks) {
            if (task.iteration < NITER) {
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
            for (const Task& task : tasks) {
                if (task.state == state) {
                    cout << task.name << " ";
                }
            }
            cout << endl << "         ";
        }
        cout << endl;
    }

    return NULL;
}

// Main function
int main(int argc, char* argv[]) {
    // Check for correct number of command line arguments
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <inputFile> <monitorTime> <NITER>" << endl;
        return 1;
    }
    string inputFile = argv[1];
    monitorTime = stoi(argv[2]);
    NITER = stoi(argv[3]);
    // Open input file
    ifstream fin(inputFile);
    if (!fin) {
        cerr << "Error opening input file: " << inputFile << endl;
        return 1;
    }

    // Read input file and create tasks and resources
    string line;
    while (getline(fin, line)) {
        if (line.empty() || line[0] == '#') {
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

                ResourceType resource;
                resource.name = name;
                resource.max_avail = value;
                resource.held = 0;
                resources[name] = resource;
            }
        } else if (keyword == "task") {
            Task task;
            ss >> task.name >> task.busyTime >> task.idleTime;

            string resource_str;
            while (ss >> resource_str) {
                size_t pos = resource_str.find(':');
                string name = resource_str.substr(0, pos);
                int value = stoi(resource_str.substr(pos + 1));

                task.resources_needed[name] = value;
            }

            task.iteration = 0;
            task.state = "WAIT";
            task.wait_time = 0;
            tasks.push_back(task);
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
    for (Task& task : tasks) {
        pthread_create(&task.tid, NULL, task_thread, &task);
    }

    // Wait for all task threads to finish
    for (Task& task : tasks) {
        pthread_join(task.tid, NULL);
    }

    // Cancel and join the monitor thread
    pthread_cancel(monitor_tid);
    pthread_join(monitor_tid, NULL);

    // Print information on resource types
    cout << "System Resources:" << endl;
    for (const auto& res : resources) {
        cout << res.first << ": (maxAvail= " << res.second.max_avail << ", held= " << res.second.held << ")" << endl;
    }
    cout << endl;

    // Print information on tasks
    cout << "System Tasks:" << endl;
    for (size_t i = 0; i < tasks.size(); ++i) {
        const Task& task = tasks[i];

        // Convert thread ID to hexadecimal string
        stringstream hex_tid;
        hex_tid << std::hex << task.tid;

        cout << "[" << i << "] " << task.name << " (IDLE, runTime= " << task.busyTime << " msec, idleTime= " << task.idleTime << " msec):" << endl;
        cout << "       (tid= 0x" << hex_tid.str() << ")" << endl;

        for (const auto& res : task.resources_needed) {
            cout << "       " << res.first << ": (needed= " << res.second << ", held= 0)" << endl;
        }

        cout  << "       " << "(RUN: " << task.iteration << " times, WAIT: " << task.wait_time << " msec)" << endl;
        cout << endl;
    }

    // Calculate and print total running time
    int total_time = duration_cast<milliseconds>(high_resolution_clock::now() - start_time).count();
    cout << "Running time= " << total_time << " msec" << endl;

    return 0;
}