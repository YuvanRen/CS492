/*  */
/*******************************************************************************
* Filename : edf.c
* Author : Yuvan Rengifo
* Date : March 7, 2024
* Description : Earliest Deadline First Scheduling Algorithm
* Pledge : I pledge my honor that I have abided by the Stevens Honor System.
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int id;              // Process ID
    int cpuTime;         // CPU time required by the process
    int period;          // Period of the process
    int deadline;        // Deadline of the process
    int remainingTime;   // Remaining CPU time for the process to finish
    int waitingTime;     // Accumulated waiting time
} Process;

int gcd(int a, int b) {
    return b == 0 ? a : gcd(b, a % b);
}

int lcm(int a, int b) {
    return (a / gcd(a, b)) * b;
}

// Function to calculate the maximum time to simulate based on all process periods
int calculateMaxTime(Process *processes, int numProcesses) {
    int result = processes[0].period;
    for (int i = 1; i < numProcesses; ++i) {
        result = lcm(result, processes[i].period);
    }
    return result;
}
// Function to find the next process to run based on the earliest deadline
int findNextProcess(Process *processes, int numProcesses, int currentTime) {
    int earliestDeadline = 1000000; // Large number for initial comparison
    int processIndex = -1;

    for (int i = 0; i < numProcesses; ++i) {
        if (processes[i].remainingTime > 0 && processes[i].deadline < earliestDeadline) {
            earliestDeadline = processes[i].deadline;
            processIndex = i;
        }
    }

    return processIndex;
}

void printProcessList(Process *processes, int numProcesses, int currentTime) {
    printf("%d: processes (oldest first):", currentTime);
    for (int i = 0; i < numProcesses; ++i) {
        if (processes[i].remainingTime > 0) {
            printf(" %d (%d ms)", processes[i].id, processes[i].remainingTime);
        }
    }
    printf("\n");
}

// Function for EDF
void simulateEDF(Process *processes, int numProcesses) {
    int currentTime = 0;
    int maxTime = calculateMaxTime(processes, numProcesses);
    int totalWaitingTime = 0;
    int processCount = 0;
    int lastProcessEnded = -1;

    printProcessList(processes, numProcesses, currentTime);

    while (currentTime < maxTime) {
        int processIndex = findNextProcess(processes, numProcesses, currentTime);

        if (processIndex != -1) {
            Process *proc = &processes[processIndex];
            if (proc->remainingTime == proc->cpuTime) { // Process is starting
                printf("%d: process %d starts\n", currentTime, proc->id);
            }

            proc->remainingTime--; // Process is running

            if (proc->remainingTime == 0) { // Process ends
                printf("%d: process %d ends\n", currentTime + 1, proc->id);
                proc->deadline = currentTime + proc->period; // Set next deadline
                proc->waitingTime += currentTime - lastProcessEnded;
                lastProcessEnded = currentTime + 1;
                processCount++;
            }
        }

        // Check and update processes for new periods and print process list
        for (int i = 0; i < numProcesses; ++i) {
            if ((currentTime > 0) && (currentTime % processes[i].period == 0)) {
                if (processes[i].remainingTime > 0) {
                    printf("%d: process %d missed deadline by %d ms\n",
                           currentTime, processes[i].id, processes[i].remainingTime);
                    totalWaitingTime += processes[i].remainingTime;
                }
                processes[i].remainingTime = processes[i].cpuTime;
                processes[i].deadline = currentTime + processes[i].period;
                printProcessList(processes, numProcesses, currentTime);
            }
        }

        currentTime++;
    }

    printf("%d: Max Time reached\n", maxTime);
    printf("Sum of all waiting times: %d\n", totalWaitingTime);
    printf("Number of processes created: %d\n", processCount);
    printf("Average Waiting Time: %.2f\n", (double)totalWaitingTime / processCount);
}

int main() {
    int numProcesses;
    printf("Enter the number of processes to schedule: ");
    scanf("%d", &numProcesses);

    Process *processes = malloc(numProcesses * sizeof(Process));

    for (int i = 0; i < numProcesses; ++i) {
        processes[i].id = i + 1;
        printf("Enter the CPU time of process %d: ", i + 1);
        scanf("%d", &processes[i].cpuTime);
        printf("Enter the period of process %d: ", i + 1);
        scanf("%d", &processes[i].period);

        processes[i].remainingTime = processes[i].cpuTime;
        processes[i].deadline = processes[i].period;
        processes[i].waitingTime = 0;
    }

    simulateEDF(processes, numProcesses);

    free(processes);
    return 0;
}

