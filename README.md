# Project Report

Name: Matthew Braun
ID: 1497171

## Objectives

The objectives of this assignment were to gain experience with multithreading, process synchronization, and preventing deadlocks when using shared variables and pthreads.

## Design Overview

This project was designed in a single using C++
Important features of this project:
1. Task class:
    Each task is stored in this class to keep track of busy time, wait time, running time, etc. It contains a function to check if all the resources the task needs are available before occupying them.
2. task_thread function:
    This is a thread function for each task specified in the input file
3. monitor_thread function:
    This is another thread function that prints info about the task states every monitor_time seconds.

## Project Status

The project is currently in the following state:

- Program functionality: The program is working correctly as far as I can tell.
- Known issues: When a task finishes its iteration, each task is not completing iterations in fixed increments of time like it shows in the assignment descriptions (600 msec, 620 msec, 660 msec). I'm not sure if this is an issue or not.
- Difficulties: I had some difficulties with knowing when to lock shared resources using mutexes for the dining philospher's problem. I also had issues parsing the input file using C so I used C++ stringstreams instead.

## Testing and Results

The following commands were executed:

1. ./a4w23 input_file1.txt 100 3
    All tasks idle. No resources held. Running time= 591 msec
2. ./a4w23 input_file1.txt 1000 30
    All tasks idle. No resources held. Running time= 4935 msec
3. ./a4w23 input_file2.txt 1000 5
    All tasks idle. No resources held. Running time= 7952 msec

## Acknowledgments

I used a few code snippets from stack overflow as well as some other sites. Source links are provided near relevant code.
I also received some help during office hours.
I also watched this video on the Dining Philosopher's Problem:
    https://www.youtube.com/watch?v=NbwbQQB7xNQ