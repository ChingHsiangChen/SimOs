//Ching Hsiang Chen
#ifndef SIMOS_H
#define SIMOS_H

#include <vector>
#include <queue>
#include <string>
#include <unordered_map>
#include <list>

struct FileReadRequest
{
    int PID{0};
    std::string fileName{""};
};

struct MemoryItem
{
    unsigned long long itemAddress;
    unsigned long long itemSize;
    int PID; // PID of the process using this chunk of memory
};

using MemoryUse = std::vector<MemoryItem>;

constexpr int NO_PROCESS{-1};

class SimOS {
public:
    SimOS(int numberOfDisks, unsigned long long amountOfRAM, unsigned long long sizeOfOS);
    bool NewProcess(unsigned long long size, int priority);
    bool SimFork();
    void SimExit();
    void SimWait();
    void DiskReadRequest(int diskNumber, std::string fileName);
    void DiskJobCompleted(int diskNumber);
    
    int GetCPU();
    std::vector<int> GetReadyQueue();
    MemoryUse GetMemory();
    FileReadRequest GetDisk(int diskNumber);
    std::queue<FileReadRequest> GetDiskQueue(int diskNumber);

private:
    struct Process {
        int pid;
        int priority;
        unsigned long long memorySize;
        int parentPid;
        std::vector<int> children;
        bool isZombie;
        MemoryItem memory;
    };

    struct MemoryBlock {
        unsigned long long start;
        unsigned long long size;
        bool isFree;
        int pid; // Only relevant if isFree is false
    };

    int nextPid_;
    int cpuPid_;
    unsigned long long totalRAM_;
    std::vector<MemoryBlock> memoryBlocks_;
    std::unordered_map<int, Process> processes_;
    
    // Ready queue stores PIDs ordered by priority
    std::list<int> readyQueue_;
    
    // Disk management
    std::vector<std::queue<FileReadRequest>> diskQueues_;
    std::unordered_map<int, FileReadRequest> activeDiskJobs_;
    
    void scheduleCPU();
    bool allocateMemory(Process& process);
    void deallocateMemory(int pid);
    void terminateProcessAndDescendants(int pid);
    void addToReadyQueue(int pid);
    void removeFromReadyQueue(int pid);
};

#endif // SIMOS_H