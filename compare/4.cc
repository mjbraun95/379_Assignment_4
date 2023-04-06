#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <pthread.h>
#include <chrono>
#include <thread>
#include <mutex>

constexpr int NRES_TYPES = 10;
constexpr int NTASKS = 25;
constexpr int MAX_NAME_LENGTH = 32;

struct Resource {
    std::string name;
    int max_available;
    int held;
};

struct Task {
    std::string name;
    int busy_time;
    int idle_time;
    std::unordered_map<std::string, int> resources_needed;
    int iterations_completed;
    int wait_time;
    pthread_t tid;
    std::string state;
};

std::vector<Resource> resources;
std::vector<Task> tasks;
std::mutex resource_mutex;
std::mutex monitor_mutex;
bool monitor_active = false;
int monitor_time;
int niter;

void load_input_file(const std::string &input_file) {
    std::ifstream file(input_file);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::stringstream ss(line);
        std::string keyword;
        ss >> keyword;
        if (keyword == "resources") {
            std::string resource_pair;
            while (ss >> resource_pair) {
                size_t colon_pos = resource_pair.find(':');
                std::string name = resource_pair.substr(0, colon_pos);
                int value = std::stoi(resource_pair.substr(colon_pos + 1));
                resources.push_back({name, value, 0});
            }
        } else if (keyword == "task") {
            Task task;
            ss >> task.name >> task.busy_time >> task.idle_time;
            std::string resource_pair;
            while (ss >> resource_pair) {
                size_t colon_pos = resource_pair.find(':');
                std::string name = resource_pair.substr(0, colon_pos);
                int value = std::stoi(resource_pair.substr(colon_pos + 1));
                task.resources_needed[name] = value;
            }
            task.iterations_completed = 0;
            task.wait_time = 0;
            task.state = "WAIT";
            tasks.push_back(task);
        }
    }
}

bool acquire_resources(Task &task) {
    std::unique_lock<std::mutex> lock(resource_mutex, std::defer_lock);
    while (true) {
        lock.lock();
        bool all_resources_acquired = true;
        for (const auto &entry : task.resources_needed) {
            const std::string &resource_name = entry.first;
            int units_needed = entry.second;
            Resource *resource = nullptr;
            for (Resource &res : resources) {
                if (res.name == resource_name) {
                    resource = &res;
                    break;
                }
            }
            if (resource->held + units_needed <= resource->max_available) {
                resource->held += units_needed;
            } else {
                all_resources_acquired = false;
                break;
            }
        }
        if (all_resources_acquired) {
            lock.unlock();
            return true;
        } else {
            for (const auto &entry : task.resources_needed) {
                const std::string &resource_name = entry.first;
                int units_needed = entry.second;
                Resource *resource = nullptr;
                for (Resource &res : resources) {
                    if (res.name == resource_name) {
                        resource = &res;
                        break;
                    }
                }
                resource->held -= units_needed;
            }
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            task.wait_time += 10;
        }
    }
}

void release_resources(const Task &task) {
    std::unique_lock<std::mutex> lock(resource_mutex);
    for (const auto &entry : task.resources_needed) {
        const std::string &resource_name = entry.first;
        int units_needed = entry.second;
        Resource *resource = nullptr;
        for (Resource &res : resources) {
            if (res.name == resource_name) {
                resource = &res;
                break;
            }
        }
        resource->held -= units_needed;
    }
}

void *task_thread(void *arg) {
    Task *task = static_cast<Task *>(arg);
    while (task->iterations_completed < niter) {
        if (acquire_resources(*task)) {
            {
                std::unique_lock<std::mutex> monitor_lock(monitor_mutex);
                task->state = "RUN";
            }
            auto start_time = std::chrono::high_resolution_clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(task->busy_time));
            auto end_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> iteration_time = end_time - start_time;
            release_resources(*task);
            {
                std::unique_lock<std::mutex> monitor_lock(monitor_mutex);
                task->state = "IDLE";
            }
            std::cout << "task: " << task->name << " (tid= " << std::hex << task->tid << ", iter= "
                      << task->iterations_completed + 1 << ", time= " << std::dec
                      << std::chrono::duration_cast<std::chrono::milliseconds>(iteration_time).count()
                      << " msec)" << std::endl;
            task->iterations_completed++;
            std::this_thread::sleep_for(std::chrono::milliseconds(task->idle_time));
            {
                std::unique_lock<std::mutex> monitor_lock(monitor_mutex);
                task->state = "WAIT";
            }
        }
    }
    return nullptr;
}

void *monitor_thread(void *arg) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(monitor_time));
        std::unique_lock<std::mutex> monitor_lock(monitor_mutex);
        if (monitor_active) {
            break;
        }
        std::cout << "monitor: [WAIT]" << std::endl;
        for (const Task &task : tasks) {
            if (task.state == "WAIT") {
                std::cout << task.name << " ";
            }
        }
        std::cout << std::endl;
        std::cout << "[RUN]";
        for (const Task &task : tasks) {
            if (task.state == "RUN") {
                std::cout << " " << task.name;
            }
        }
        std::cout << std::endl;
        std::cout << "[IDLE]";
        for (const Task &task : tasks) {
            if (task.state == "IDLE") {
                std::cout << " " << task.name;
            }
        }
        std::cout << std::endl << std::endl;
    }
    return nullptr;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " inputFile monitorTime NITER" << std::endl;
        return 1;
    }

    std::string input_file(argv[1]);
    monitor_time = std::stoi(argv[2]);
    niter = std::stoi(argv[3]);

    load_input_file(input_file);

    pthread_t monitor_tid;
    pthread_create(&monitor_tid, nullptr, monitor_thread, nullptr);

    for (Task &task : tasks) {
        pthread_create(&task.tid, nullptr, task_thread, &task);
    }

    for (Task &task : tasks) {
        pthread_join(task.tid, nullptr);
    }

    {
        std::unique_lock<std::mutex> monitor_lock(monitor_mutex);
        monitor_active = true;
    }

    pthread_join(monitor_tid, nullptr);

    // print_resources_info();
    // print_tasks_info();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> running_time = end_time - start_time;
    std::cout << "Running time= " << std::chrono::duration_cast<std::chrono::milliseconds>(running_time).count()
              << " msec" << std::endl;

    return 0;
}

