#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#define NRES_TYPES 10
#define NTASKS 25
#define STR_SIZE 32

typedef struct {
    char name[STR_SIZE];
    int units;
    pthread_mutex_t mutex;
} Resource;

typedef enum {
    WAIT,
    RUN,
    IDLE
} TaskState;

typedef struct {
    char name[STR_SIZE];
    int busyTime;
    int idleTime;
    int resourceCounts[NRES_TYPES];
    TaskState state;
    int iterations;
    long long waitTime;
    pthread_t tid;
} Task;

Resource resources[NRES_TYPES];
Task tasks[NTASKS];
int numResources = 0;
int numTasks = 0;
int NITER;

long long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int find_resource_index(const char *name) {
    for (int i = 0; i < numResources; i++) {
        if (strcmp(resources[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void *task_thread(void *arg) {
    Task *task = (Task *) arg;
    long long startWaitTime;
    long long startRunTime;

    for (int iter = 0; iter < NITER; iter++) {
        task->state = WAIT;
        startWaitTime = get_current_time_ms();

        for (int i = 0; i < numResources; i++) {
            if (task->resourceCounts[i] > 0) {
                pthread_mutex_lock(&resources[i].mutex);
            }
        }

        task->waitTime += get_current_time_ms() - startWaitTime;
        task->state = RUN;
        startRunTime = get_current_time_ms();
        usleep(task->busyTime * 1000);

        for (int i = 0; i < numResources; i++) {
            if (task->resourceCounts[i] > 0) {
                pthread_mutex_unlock(&resources[i].mutex);
            }
        }

        task->iterations++;
        printf("task: %s (tid= %p, iter= %d, time= %lld msec)\n",
               task->name, (void *) task->tid, task->iterations,
               get_current_time_ms() - startRunTime);

        task->state = IDLE;
        usleep(task->idleTime * 1000);
    }

    return NULL;
}

void *monitor_thread(void *arg) {
    int monitorTime = *((int *) arg);

    while (1) {
        usleep(monitorTime * 1000);

        printf("monitor: [WAIT]\n");
        for (int i = 0; i < numTasks; i++) {
            if (tasks[i].state == WAIT) {
                printf("%s ", tasks[i].name);
            }
        }
        printf("\n[RUN]");
        for (int i = 0; i < numTasks; i++) {
            if (tasks[i].state == RUN) {
                printf(" %s", tasks[i].name);
            }
        }
        printf("\n[IDLE]");
        for (int i = 0; i < numTasks; i++) {
            if (tasks[i].state == IDLE) {
                printf(" %s", tasks[i].name);
            }
        }
        printf("\n\n");
    }
}

void read_input_file(const char *inputFile) {
    FILE *file = fopen(inputFile, "r");
    if (file == NULL) {
        perror("Error opening input file");
        exit(1);
    }

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        if (strncmp(line, "resources", 9) == 0) {
            char *token = strtok(line + 9, " ");
            while (token) {
                char name[STR_SIZE];
                int value;
                sscanf(token, "%[^:]:%d", name, &value);
                strcpy(resources[numResources].name, name);
                resources[numResources].units = value;
                pthread_mutex_init(&resources[numResources].mutex, NULL);
                numResources++;
                token = strtok(NULL, " ");
            }
        } else if (strncmp(line, "task", 4) == 0) {
            char *token = strtok(line + 4, " ");
            strcpy(tasks[numTasks].name, token);
            token = strtok(NULL, " ");
            tasks[numTasks].busyTime = atoi(token);
            token = strtok(NULL, " ");
            tasks[numTasks].idleTime = atoi(token);
            for (int i = 0; i < NRES_TYPES; i++) {
                tasks[numTasks].resourceCounts[i] = 0;
            }
            token = strtok(NULL, " ");
            while (token) {
                char name[STR_SIZE];
                int value;
                sscanf(token, "%[^:]:%d", name, &value);
                int index = find_resource_index(name);
                if (index != -1) {
                    tasks[numTasks].resourceCounts[index] = value;
                }
                token = strtok(NULL, " ");
            }
            tasks[numTasks].state = IDLE;
            tasks[numTasks].iterations = 0;
            tasks[numTasks].waitTime = 0;
            numTasks++;
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s inputFile monitorTime NITER\n", argv[0]);
        exit(1);
    }

    read_input_file(argv[1]);
    int monitorTime = atoi(argv[2]);
    NITER = atoi(argv[3]);

    pthread_t monitorThreadId;
    pthread_create(&monitorThreadId, NULL, monitor_thread, &monitorTime);

    for (int i = 0; i < numTasks; i++) {
        pthread_create(&tasks[i].tid, NULL, task_thread, &tasks[i]);
    }

    for (int i = 0; i < numTasks; i++) {
        pthread_join(tasks[i].tid, NULL);
    }

    pthread_cancel(monitorThreadId);
    pthread_join(monitorThreadId, NULL);

    // Print termination information

    return 0;
}
