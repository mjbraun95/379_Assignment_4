#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <pthread.h>
#include <unistd.h>
#include <chrono>
#include <map>
#include <mutex>
#include <algorithm>

#define NRES_TYPES 10
#define NTASKS 25

using namespace std;

struct Resource {
    string name;
    int available;
    int max_avail;
};

struct Task {
    string name;
    int busy_time;
    int idle_time;
    map<string, int> resources_needed;
    int iter;
    pthread_t tid;
    int state;
    int wait_time;
};

enum State {WAIT, RUN, IDLE};

vector<Resource> resources;
vector<Task> tasks;
chrono::steady_clock::time_point start_time;

mutex resources_mtx;
mutex tasks_mtx;

void* task_thread(void* arg) {
    Task* task = (Task*)arg;

    for (int i = 0; i < task->iter; ++i) {
        chrono::steady_clock::time_point wait_start = chrono::steady_clock::now();

        while (true) {
            resources_mtx.lock();

            bool can_acquire = true;
            for (const auto& pair : task->resources_needed) {
                auto it = find_if(resources.begin(), resources.end(), [&](const Resource& r) {
                    return r.name == pair.first;
                });

                if (it->available < pair.second) {
                    can_acquire = false;
                    break;
                }
            }

            if (can_acquire) {
                for (const auto& pair : task->resources_needed) {
                    auto it = find_if(resources.begin(), resources.end(), [&](const Resource& r) {
                        return r.name == pair.first;
                    });

                    it->available -= pair.second;
                }
                resources_mtx.unlock();
                break;
            }

            resources_mtx.unlock();
            usleep(10 * 1000);
        }

        chrono::steady_clock::time_point wait_end = chrono::steady_clock::now();
        task->wait_time += chrono::duration_cast<chrono::milliseconds>(wait_end - wait_start).count();

        task->state = RUN;
        usleep(task->busy_time * 1000);

        resources_mtx.lock();
        for (const auto& pair : task->resources_needed) {
            auto it = find_if(resources.begin(), resources.end(), [&](const Resource& r) {
                return r.name == pair.first;
            });

            it->available += pair.second;
        }
        resources_mtx.unlock();

        task->state = IDLE;
        usleep(task->idle_time * 1000);

        tasks_mtx.lock();
        cout << "task: " << task->name << " (tid= " << hex << task->tid << dec << ", iter= " << i + 1 << ", time= " << chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start_time).count() << " msec)" << endl;
        tasks_mtx.unlock();
    }

    return nullptr;
}

void* monitor_thread(void* arg) {
    int monitor_time = *((int*)arg);

    while (true) {
        tasks_mtx.lock();
        cout << "monitor: [WAIT]" << endl;
        for (const Task& task : tasks) {
            if (task.state == WAIT) {
                cout << task.name << " ";
            }
        }
        cout << endl << "[RUN]";
        for (const Task& task : tasks) {
            if (task.state == RUN) {
                cout << " " << task.name;
            }
        }
        cout << endl << "[IDLE]";
        for (const Task& task : tasks) {
            if (task.state == IDLE) {
                cout << " " << task.name;
            }
        }
        cout << endl << endl;
        tasks_mtx.unlock();
        usleep(monitor_time * 1000);
    }

    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " inputFile monitorTime NITER" << endl;
        return 1;
    }
    string input_file = argv[1];
    int monitor_time = stoi(argv[2]);
    int niter = stoi(argv[3]);

    ifstream infile(input_file);
    if (!infile.is_open()) {
        cerr << "Failed to open input file: " << input_file << endl;
        return 1;
    }

    string line;
    while (getline(infile, line)) {
        if (line.empty() || line[0] == '#') continue;

        stringstream ss(line);
        string word;
        ss >> word;

        if (word == "resources") {
            while (ss >> word) {
                size_t colon_pos = word.find(':');
                string name = word.substr(0, colon_pos);
                int value = stoi(word.substr(colon_pos + 1));

                resources.push_back({name, value, value});
            }
        } else if (word == "task") {
            Task task;
            task.iter = niter;
            task.wait_time = 0;
            task.state = WAIT;

            ss >> task.name >> task.busy_time >> task.idle_time;

            while (ss >> word) {
                size_t colon_pos = word.find(':');
                string name = word.substr(0, colon_pos);
                int value = stoi(word.substr(colon_pos + 1));

                task.resources_needed[name] = value;
            }

            tasks.push_back(task);
        }
    }

    infile.close();

    start_time = chrono::steady_clock::now();
    pthread_t monitor_tid;
    pthread_create(&monitor_tid, nullptr, monitor_thread, &monitor_time);

    for (Task& task : tasks) {
        pthread_create(&task.tid, nullptr, task_thread, &task);
    }

    for (Task& task : tasks) {
        pthread_join(task.tid, nullptr);
    }

    pthread_cancel(monitor_tid);
    pthread_join(monitor_tid, nullptr);

    // Print final output
    // ...

    return 0;
}