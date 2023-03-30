// TODO: Align with A4
/*
# -----------------------------------------------------------
# Name: Matthew Braun | Student ID: 1497171
# This code is based off of the sockMsg.cc file from eClass, as well as some code from my assignment 2 submission.
# 
# compile with:  g++ sockMsg.cc -o  sockMsg
#
# Usage:  a4w23 -s
#         a4w23 -c idNumber inputFile serverAddress------------------------------------------------------------
*/

#include <stdio.h>
#include <pthread.h>
#include <iostream>
#include <map>
#include <string>
#include <cstring>

#define _REENTRANT
#define NRES_TYPES 10
#define NTASKS 25
#define STR_LEN 32

using namespace std;

class Task {
  public:
    int task; //a keyword that specifies a task line
    char taskName[STR_LEN];
    int busyTime;
    int idleTime;
    map<string, int> name_value; // specifies the name of a resource type, and the number of units of this
                                 // resource type needed for the task to execute
    void myFunction();
    Task(int task, const char* taskName, int busyTime, 
         int idleTime, const std::map<std::string, int>& name_value)
        : task(task), busyTime(busyTime), idleTime(idleTime), name_value(name_value)
    {
        // Copy the task name to the member variable
        strncpy(this->taskName, taskName, STR_LEN);
    }
};

// TODO: Redo!
void *print_message(void *arg) {
    char *message = (char *)arg;
    printf("%s\n", message);
    pthread_exit(NULL);
}

// TODO: Redo!
int main() {
    pthread_t thread1, thread2;

    char *message1 = "Hello from thread 1!";
    char *message2 = "Hello from thread 2!";

    pthread_create(&thread1, NULL, print_message, (void *)message1);
    pthread_create(&thread2, NULL, print_message, (void *)message2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    return 0;
}