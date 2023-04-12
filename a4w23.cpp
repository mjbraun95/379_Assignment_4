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

using namespace std;
using namespace chrono;

class Resource;
class Task;

// Global vars
high_resolution_clock::time_point global_start_time;
map<string, Resource> global_resource_pool;
vector<Task*> global_tasks;
mutex global_mutex;

string input_file;
int monitor_time;
int NITER;

const string STATES[] = {"WAIT","RUN","IDLE"};

class Resource
{
public:
    string resource_name;
    int current_quantity_held;
    int max_quantity_available;

    Resource() {}

    Resource(string resource_name, int max_quantity_available)
    {
        this->current_quantity_held = 0;
        this->max_quantity_available = max_quantity_available;
    }
};

class Task
{
public:
    pthread_t tid;

    string task_name;
    int busy_time;
    int idle_time;
    map<string,int> resources_needed;

    int total_wait_time;
    int total_runs_completed;
    map<string,int> resources_held;
    string state;

    Task(string task_name, int busy_time, int idle_time, map<string, int> resources_needed)
    {
        this->task_name = task_name;
        this->busy_time = busy_time;
        this->idle_time = idle_time;
        this->resources_needed = resources_needed;

        this->total_wait_time = 0;
        this->total_runs_completed = 0;
        for (auto &resource_name_value_pair: resources_needed)
        {
            string resouce_name = resource_name_value_pair.first;
            resources_held[resouce_name] = 0;
        }
    }

    bool attempt_to_get_needed_resources()
    {
        // Occupy the resource pool
        unique_lock<mutex> lock(global_mutex);

        // Check if the resources needed are available
        bool all_resources_available = true;
        for (auto resource_name_value_pair = this->resources_needed.begin(); resource_name_value_pair != this->resources_needed.end(); resource_name_value_pair++)
        {
            const string& resource_name = resource_name_value_pair->first;
            const int quantity_needed = resource_name_value_pair->second;
            Resource& requested_resource = global_resource_pool[resource_name];
            // If any of the task's needed resources aren't available, don't take any
            if ((requested_resource.max_quantity_available - requested_resource.current_quantity_held) < quantity_needed)
            {
                all_resources_available = false;
                break;
            }
        }
        if (all_resources_available)
        {
            // Resources are available. Get the resources needed
            for (auto resource_name_value_pair = this->resources_needed.begin(); resource_name_value_pair != this->resources_needed.end(); resource_name_value_pair++)
            {
                // Update the relevant Resource instance
                const string& resource_name = resource_name_value_pair->first;
                const int quantity_needed = resource_name_value_pair->second;
                Resource& resource = global_resource_pool[resource_name];
                resource.current_quantity_held += quantity_needed;
                this->resources_held[resource_name] += quantity_needed;
            }
            return true;
        }
        else
        {
            // Resources not all available
            return false;
        }
    }

    // Release resources held by the task
    void free_resources()
    {
        unique_lock<mutex> lock(global_mutex);

        for (auto resource_name_value_pair = this->resources_needed.begin(); resource_name_value_pair != this->resources_needed.end(); resource_name_value_pair++)
        {
            const string& resource_name = resource_name_value_pair->first;
            const int quantity_unoccupied = resource_name_value_pair->second;
            Resource& resource = global_resource_pool[resource_name];
            // Free up space in resource object
            resource.current_quantity_held -= quantity_unoccupied;
            this->resources_held[resource_name] -= quantity_unoccupied;
        }
    }
};

int parse_input_and_spawn_objects(const string& input_file)
{
    // Source: https://www.bogotobogo.com/cplusplus/fstream_input_output.php
    string line;
    ifstream input_file_reader(input_file);
    if (!input_file_reader)
    {
        cerr << "Error opening input file." << endl;
        return 1;
    }

    // Load input file data
    // Iterate through each input file line
    while (!input_file_reader.eof())
    {
        // Load next input file line into line
        getline(input_file_reader, line);
        // Skip lines starting with #
        if (line[0] == '#')
        {
            continue;
        }

        // Source: https://www.softwaretestinghelp.com/stringstream-class-in-cpp/
        stringstream string_reader(line);
        string line_type;

        // Load first line word into stringstream
        string_reader >> line_type;

        string resource_name_quantity_pair_str;
        if (line_type.compare("resources") == 0)
        {
            // Iterate through each resource specified in the resources line
            string resource_name_quantity_pair_str;
            while (getline(string_reader, resource_name_quantity_pair_str, ' '))
            {
                istringstream resource_name_quantity_pair_stream(resource_name_quantity_pair_str);
                string resource_name;
                int max_quantity_available;

                // Parse resource string
                if (getline(resource_name_quantity_pair_stream, resource_name, ':') && resource_name_quantity_pair_stream >> max_quantity_available)
                {
                    // Create + Add new Resource instance to global resource pool
                    Resource resource_type = Resource(resource_name, max_quantity_available);
                    global_resource_pool.insert(pair<string, Resource>(resource_name, resource_type));
                }
            }

        }
        else if (line_type.compare("task") == 0)
        {
            // Load Task attributes
            string task_name, busy_time_str, idle_time_str;
            int busy_time, idle_time;
            map<string, int> resources_needed;

            string_reader >> task_name;

            string_reader >> busy_time_str;
            busy_time = stoi(busy_time_str);

            string_reader >> idle_time_str;
            idle_time = stoi(idle_time_str);
            // Create a map of needed resources for the task
            // Iterate through each required resource specified in the task line
            string resource_name_quantity_pair_str;
            while (getline(string_reader, resource_name_quantity_pair_str, ' '))
            {
                istringstream resource_name_quantity_pair_stream(resource_name_quantity_pair_str);
                string resource_name;
                int quantity_needed;

                // Parse resource string
                if (getline(resource_name_quantity_pair_stream, resource_name, ':') && resource_name_quantity_pair_stream >> quantity_needed)
                {
                    // Add pair to resources needed map
                    resources_needed.insert(make_pair(resource_name, quantity_needed));
                }
            }
            // Create + Add new Task pointer to global tasks vector
            Task* task = new Task(task_name, busy_time, idle_time, resources_needed);
            global_tasks.push_back(task);
        }
        else
        {
            cout << "Invalid line in input file. Skipping" << endl;
            continue;
        }
    }
    // Close the input file
    input_file_reader.close();
    return 0;
}

void *task_thread(void *arg);
void *monitor_thread(void *arg);

void spawn_threads()
{
    // Create and start monitor thread
    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, monitor_thread, NULL);

    // Source: https://stackoverflow.com/questions/11624545/how-to-make-main-thread-wait-for-all-child-threads-finish
    // Spawn all task threads
    for (Task* task : global_tasks)
    {
        pthread_create(&task->tid, NULL, task_thread, task);
    }
    // Wait for each thread to finish
    for (Task* task : global_tasks)
    {
        pthread_join(task->tid, NULL);
    }

    // Source: https://www.ibm.com/docs/en/zos/2.2.0?topic=functions-pthread-cancel-cancel-thread
    // Cancel and join the monitor thread
    if (pthread_cancel(monitor_tid) == -1)
    {
        perror("Error cancelling monitor thread");
        exit(1);
    }
    pthread_join(monitor_tid, NULL);
}

void *task_thread(void *arg)
{
    Task *task = (Task *)arg;
    for (int i = 0; i < NITER; i++)
    {
        task->state = "WAIT";
        // Start recording wait time
        high_resolution_clock::time_point wait_start_time = high_resolution_clock::now();

        // Wait until resources acquired
        while (true)
        {
            if (task->attempt_to_get_needed_resources() == true)
            {
                break;
            }
            usleep(10000);
        }

        // Update total_wait_time of the task
        // Source: https://stackoverflow.com/questions/31657511/chrono-the-difference-between-two-points-in-time-in-milliseconds
        auto ms1 = duration_cast<milliseconds>(high_resolution_clock::now() - wait_start_time);
        int new_wait_time_elapsed_ms = ms1.count();
        task->total_wait_time += new_wait_time_elapsed_ms;

        // Set task to RUN
        {
            unique_lock<mutex> lock(global_mutex);
            task->state = "RUN";
        }
        // Run for busy_time ms
        usleep(task->busy_time * 1000);

        // Locked scope to update total_runs_completed
        {
            unique_lock<mutex> lock(global_mutex);
            // Increment runs completed
            task->total_runs_completed++;
        }
        // Source: https://stackoverflow.com/questions/31657511/chrono-the-difference-between-two-points-in-time-in-milliseconds
        auto ms2 = duration_cast<milliseconds>(high_resolution_clock::now() - global_start_time);
        int total_time_elapsed_ms = ms2.count();

        // Convert thread ID to hexadecimal string
        // Source: https://stackoverflow.com/questions/5100718/integer-to-hex-string-in-c
        stringstream hex_tid;
        hex_tid << hex << task->tid;
        string hex_tid_str = hex_tid.str();

        // Print task info
        cout << "task: " << task->task_name << " (tid= 0x" << hex_tid_str << ", iter= " << task->total_runs_completed << ", time= " << total_time_elapsed_ms << " msec)" << endl;

        // Set task to IDLE
        {
            unique_lock<mutex> lock(global_mutex);
            // Set task state to IDLE
            task->state = "IDLE";
        }

        // Release resources held by the task
        task->free_resources();

        // Idle for idle_time ms
        usleep(task->idle_time * 1000);
    }

    return NULL;
}

// Prints task information
void* monitor_thread(void* arg)
{
    while (true)
    {
        // Locked scope to prevent states from changing during printing
        {
            unique_lock<mutex> lock(global_mutex);

            // Print the monitor information
            cout << endl << "monitor: ";
            for (const string& state : STATES)
            {
                cout << "[" << state << "] ";
                // Iterate through each Task and print ordered by state
                for (const Task* task : global_tasks)
                {
                    if (task->state == state)
                    {
                        cout << task->task_name << " ";
                    }
                }
                cout << endl << "         ";
            }
            cout << endl;
        }

        // Sleep for the monitor_time interval
        usleep(monitor_time * 1000);
    }

    return NULL;
}

void print_system_info() {
    cout << "System Resources:" << endl;
    for (auto resource_name_object_pair : global_resource_pool)
    {
        string resource_name = resource_name_object_pair.first;
        Resource resource = resource_name_object_pair.second;

        // Print info
        cout << "        " << resource_name << ": (maxAvail= " << resource.max_quantity_available << ", held= " << resource.current_quantity_held << ')' << endl;
    }
    cout << endl;
    cout << "System Tasks:" << endl;
    int i = 0;
    for (Task* task : global_tasks)
    {
        // Convert thread ID to hexadecimal string
        // Source: https://stackoverflow.com/questions/5100718/integer-to-hex-string-in-c
        stringstream hex_tid;
        hex_tid << hex << task->tid;
        string hex_tid_str = hex_tid.str();

        // Print info
        cout << '[' << i << "] (" << task->state << ", runTime= " << task->busy_time << " msec, idleTime= " << task->idle_time << " msec):" << endl;
        cout << "       (tid= 0x" << hex_tid_str << ')' << endl;
        for (auto resources_name_quantity_pair : task->resources_needed)
        {
            string resource_name = resources_name_quantity_pair.first;
            int quantity_needed = resources_name_quantity_pair.second;
            int quantity_held = task->resources_held[resource_name];
        cout << "       " << resource_name << ": (needed= " << quantity_needed << ", held= " << quantity_held << ')' << endl;
        }
        cout << "       (RUN: " << task->total_runs_completed << " times, WAIT: " << task->total_wait_time << " msec)" << endl;
        i++;
    }
    // Print running time
    // Source: https://stackoverflow.com/questions/31657511/chrono-the-difference-between-two-points-in-time-in-milliseconds
    auto ms = duration_cast<milliseconds>(high_resolution_clock::now() - global_start_time);
    int running_time = ms.count();
    cout << "Running time= " << running_time << " msec" << endl;
}

int main(int argc, char* argv[])
{
    // Start recording execution time
    global_start_time = high_resolution_clock::now();

    // Check arguments match
    if (argc != 4)
    {
        cerr << "Usage: ./a4w23 input_filepath monitor_time NITER" << endl;
        exit(1);
    }
    else
    {
        // Load arguments
        input_file = argv[1];
        monitor_time = stoi(argv[2]);
        NITER = stoi(argv[3]);

        parse_input_and_spawn_objects(input_file);
        spawn_threads();
        print_system_info();

        // Delete Task pointers
        for (Task* task : global_tasks) delete task;

        return 0;
    }
}