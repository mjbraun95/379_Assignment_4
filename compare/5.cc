#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <pthread.h>
#include <unistd.h>

#define NRES_TYPES 10
#define NTASKS 25

using namespace std;
using namespace std::chrono;

struct Resource {
    int maxAvail;
    int held;
    pthread_mutex_t mutex;
};

struct Task {
    string name;
    int busyTime;
    int idleTime;
    map<string, int> resourcesNeeded;
    pthread_t tid;
    int iter;
    int64_t totalTimeWaited;
    int state; // 0: WAIT, 1: RUN, 2: IDLE
};

vector<Resource> resources;
map<string, int> resourceIndex;
vector<Task> tasks;

pthread_mutex_t monitorMutex = PTHREAD_MUTEX_INITIALIZER;

int64_t getCurrentMillis() {
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

bool tryAcquireResources(const map<string, int>& resourcesNeeded) {
    bool acquired = true;

    for (const auto& kv : resourcesNeeded) {
        int idx = resourceIndex[kv.first];
        pthread_mutex_lock(&resources[idx].mutex);

        if (resources[idx].held + kv.second <= resources[idx].maxAvail) {
            resources[idx].held += kv.second;
        } else {
            acquired = false;
        }

        pthread_mutex_unlock(&resources[idx].mutex);

        if (!acquired) {
            break;
        }
    }

    if (!acquired) {
        for (const auto& kv : resourcesNeeded) {
            int idx = resourceIndex[kv.first];
            pthread_mutex_lock(&resources[idx].mutex);
            resources[idx].held -= kv.second;
            pthread_mutex_unlock(&resources[idx].mutex);
        }
    }

    return acquired;
}

void releaseResources(const map<string, int>& resourcesNeeded) {
    for (const auto& kv : resourcesNeeded) {
        int idx = resourceIndex[kv.first];
        pthread_mutex_lock(&resources[idx].mutex);
        resources[idx].held -= kv.second;
        pthread_mutex_unlock(&resources[idx].mutex);
    }
}

void* taskThread(void* arg) {
    Task* task = (Task*)arg;

    for (int i = 0; i < task->iter; i++) {
        int64_t startTimeWait = getCurrentMillis();
        while (!tryAcquireResources(task->resourcesNeeded)) {
            usleep(10 * 1000); // Wait for 10 milliseconds
        }
        int64_t endTimeWait = getCurrentMillis();

        task->totalTimeWaited += endTimeWait - startTimeWait;

        pthread_mutex_lock(&monitorMutex);
        task->state = 1; // RUN
        pthread_mutex_unlock(&monitorMutex);

        usleep(task->busyTime * 1000);

        releaseResources(task->resourcesNeeded);

        pthread_mutex_lock(&monitorMutex);
        task->state = 2; // IDLE
        pthread_mutex_unlock(&monitorMutex);

        usleep(task->idleTime * 1000);

        cout << "task: " << task->name << " (tid= 0x" << hex << task->tid << dec << ", iter= " << (i + 1)
             << ", time= " << (endTimeWait - startTimeWait) << " msec)" << endl;
    }

    return NULL;
}

void* monitorThread(void* arg) {
    int monitorTime = *((int*)arg);

    while (true) {
        usleep(monitorTime * 1000);

        pthread_mutex_lock(&monitorMutex);
        vector<string> tasksInState[3];
        for (const Task& task : tasks) {
            tasksInState[task.state].push_back(task.name);
        }

        cout << "monitor: [WAIT]" << endl;
        for (const string& taskName : tasksInState[0]) {
            cout << taskName << " ";
        }
        cout << endl;

        cout << "[RUN]" << endl;
        for (const string& taskName : tasksInState[1]) {
            cout << taskName << " ";
        }
        cout << endl;

        cout << "[IDLE]" << endl;
        for (const string& taskName : tasksInState[2]) {
            cout << taskName << " ";
        }
        cout << endl;

        pthread_mutex_unlock(&monitorMutex);

        bool allTaskThreadsFinished = true;
        for (const Task& task : tasks) {
            if (task.state != 2) {
                allTaskThreadsFinished = false;
                break;
            }
        }

        if (allTaskThreadsFinished) {
            break;
        }
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " inputFile monitorTime NITER" << endl;
        return 1;
    }
    ifstream inputFile(argv[1]);
    int monitorTime = stoi(argv[2]);
    int niter = stoi(argv[3]);

    string line;
    while (getline(inputFile, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        istringstream iss(line);
        string keyword;
        iss >> keyword;

        if (keyword == "resources") {
            string resourceName;
            int resourceValue;

            while (iss >> resourceName >> resourceValue) {
                resourceName.pop_back(); // Remove the ':' at the end
                resourceIndex[resourceName] = resources.size();
                Resource resource;
                resource.maxAvail = resourceValue;
                resource.held = 0;
                pthread_mutex_init(&resource.mutex, NULL);
                resources.push_back(resource);
            }
        } else if (keyword == "task") {
            Task task;
            iss >> task.name >> task.busyTime >> task.idleTime;

            string resourceName;
            int resourceValue;

            while (iss >> resourceName >> resourceValue) {
                resourceName.pop_back(); // Remove the ':' at the end
                task.resourcesNeeded[resourceName] = resourceValue;
            }

            task.iter = niter;
            task.totalTimeWaited = 0;
            task.state = 0; // WAIT
            tasks.push_back(task);
        }
    }

    pthread_t monitorTid;
    pthread_create(&monitorTid, NULL, monitorThread, &monitorTime);

    int64_t startTime = getCurrentMillis();

    for (Task& task : tasks) {
        pthread_create(&task.tid, NULL, taskThread, &task);
    }

    for (Task& task : tasks) {
        pthread_join(task.tid, NULL);
    }

    pthread_cancel(monitorTid);
    pthread_join(monitorTid, NULL);

    int64_t endTime = getCurrentMillis();

    // Print the final output
    // ...

    return 0;
}