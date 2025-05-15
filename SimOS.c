// Ching Hsiang Chen

#include "SimOS.h"
#include <algorithm>
#include <stdexcept>

SimOS::SimOS(int numberOfDisks, unsigned long long amountOfRAM, unsigned long long sizeOfOS)
    : nextPid_(2), cpuPid_(NO_PROCESS), totalRAM_(amountOfRAM) {
    // Initialize disks
    diskQueues_.resize(numberOfDisks);
    
    // Initialize memory with OS
    MemoryBlock osBlock{0, sizeOfOS, false, 1};
    memoryBlocks_.push_back(osBlock);
    
    // Create OS process
    Process osProcess;
    osProcess.pid = 1;
    osProcess.priority = 0;
    osProcess.memorySize = sizeOfOS;
    osProcess.parentPid = NO_PROCESS;
    osProcess.isZombie = false;
    osProcess.memory = {0, sizeOfOS, 1};
    processes_[1] = osProcess;
    
    // Add remaining memory as free block if available
    if (sizeOfOS < amountOfRAM) {
        MemoryBlock freeBlock{sizeOfOS, amountOfRAM - sizeOfOS, true, NO_PROCESS};
        memoryBlocks_.push_back(freeBlock);
    }
}

bool SimOS::NewProcess(unsigned long long size, int priority) {
    Process newProcess;
    newProcess.pid = nextPid_++;
    newProcess.priority = priority;
    newProcess.memorySize = size;
    newProcess.parentPid = NO_PROCESS;
    newProcess.isZombie = false;
    
    if (!allocateMemory(newProcess)) {
        return false;
    }
    
    processes_[newProcess.pid] = newProcess;
    addToReadyQueue(newProcess.pid);
    scheduleCPU();
    return true;
}

bool SimOS::SimFork() {
    if (cpuPid_ == NO_PROCESS || cpuPid_ == 1) {
        return false; // No process running or OS process
    }
    
    Process& parent = processes_[cpuPid_];
    Process child;
    child.pid = nextPid_++;
    child.priority = parent.priority;
    child.memorySize = parent.memorySize;
    child.parentPid = cpuPid_;
    child.isZombie = false;
    
    if (!allocateMemory(child)) {
        return false;
    }
    
    processes_[child.pid] = child;
    parent.children.push_back(child.pid);
    addToReadyQueue(child.pid);
    scheduleCPU();
    return true;
}

void SimOS::SimExit() {
    if (cpuPid_ == NO_PROCESS || cpuPid_ == 1) {
        return; // No process running or OS process
    }
    
    terminateProcessAndDescendants(cpuPid_);
    scheduleCPU();
}

void SimOS::SimWait() {
    if (cpuPid_ == NO_PROCESS || cpuPid_ == 1) {
        return; // No process running or OS process
    }
    
    Process& process = processes_[cpuPid_];
    
    // Check for zombie children
    auto it = std::find_if(process.children.begin(), process.children.end(),
        [this](int pid) { return processes_[pid].isZombie; });
    
    if (it != process.children.end()) {
        // Found a zombie child - remove it
        processes_.erase(*it);
        process.children.erase(it);
    } else {
        // No zombies - move to waiting state (remove from CPU)
        cpuPid_ = NO_PROCESS;
        scheduleCPU();
    }
}

void SimOS::DiskReadRequest(int diskNumber, std::string fileName) {
    if (cpuPid_ == NO_PROCESS || cpuPid_ == 1 || 
        diskNumber < 0 || diskNumber >= static_cast<int>(diskQueues_.size())) {
        return;
    }
    
    FileReadRequest request;
    request.PID = cpuPid_;
    request.fileName = fileName;
    
    if (activeDiskJobs_.count(diskNumber) == 0) {
        // Disk is idle - start serving immediately
        activeDiskJobs_[diskNumber] = request;
    } else {
        // Add to queue
        diskQueues_[diskNumber].push(request);
    }
    
    // Remove process from CPU
    cpuPid_ = NO_PROCESS;
    scheduleCPU();
}

void SimOS::DiskJobCompleted(int diskNumber) {
    if (diskNumber < 0 || diskNumber >= static_cast<int>(diskQueues_.size())) {
        return;
    }
    
    if (activeDiskJobs_.count(diskNumber) == 0) {
        return; // No active job
    }
    
    int completedPid = activeDiskJobs_[diskNumber].PID;
    activeDiskJobs_.erase(diskNumber);
    
    // Start next job if available
    if (!diskQueues_[diskNumber].empty()) {
        activeDiskJobs_[diskNumber] = diskQueues_[diskNumber].front();
        diskQueues_[diskNumber].pop();
    }
    
    // Add completed process back to ready queue
    if (processes_.count(completedPid) && !processes_[completedPid].isZombie) {
        addToReadyQueue(completedPid);
        scheduleCPU();
    }
}

int SimOS::GetCPU() const {
    return cpuPid_;
}

std::vector<int> SimOS::GetReadyQueue() const {
    return std::vector<int>(readyQueue_.begin(), readyQueue_.end());
}

MemoryUse SimOS::GetMemory() const {
    MemoryUse memoryUsage;
    for (const auto& block : memoryBlocks_) {
        if (!block.isFree) {
            memoryUsage.push_back({block.start, block.size, block.pid});
        }
    }
    return memoryUsage;
}

FileReadRequest SimOS::GetDisk(int diskNumber) const {
    if (diskNumber < 0 || diskNumber >= static_cast<int>(diskQueues_.size())) {
        return FileReadRequest();
    }
    
    if (activeDiskJobs_.count(diskNumber)) {
        return activeDiskJobs_.at(diskNumber);
    }
    return FileReadRequest();
}

std::queue<FileReadRequest> SimOS::GetDiskQueue(int diskNumber) const {
    if (diskNumber < 0 || diskNumber >= static_cast<int>(diskQueues_.size())) {
        return std::queue<FileReadRequest>();
    }
    return diskQueues_[diskNumber];
}

// Private helper methods

void SimOS::scheduleCPU() {
    if (readyQueue_.empty()) {
        cpuPid_ = NO_PROCESS;
        return;
    }
    
    // Find highest priority process
    auto maxIt = std::max_element(readyQueue_.begin(), readyQueue_.end(),
        [this](int a, int b) {
            return processes_.at(a).priority < processes_.at(b).priority;
        });
    
    int highestPid = *maxIt;
    
    if (cpuPid_ == NO_PROCESS || 
        processes_[highestPid].priority > processes_[cpuPid_].priority) {
        // Preempt current process if needed
        if (cpuPid_ != NO_PROCESS) {
            addToReadyQueue(cpuPid_);
        }
        
        cpuPid_ = highestPid;
        removeFromReadyQueue(highestPid);
    }
}

bool SimOS::allocateMemory(Process& process) {
    // Find worst fit block
    auto worstFitIt = memoryBlocks_.end();
    unsigned long long maxSize = 0;
    
    for (auto it = memoryBlocks_.begin(); it != memoryBlocks_.end(); ++it) {
        if (it->isFree && it->size >= process.memorySize && it->size > maxSize) {
            worstFitIt = it;
            maxSize = it->size;
        }
    }
    
    if (worstFitIt == memoryBlocks_.end()) {
        return false; // No suitable block found
    }
    
    // Split the block if there's remaining space
    if (worstFitIt->size > process.memorySize) {
        MemoryBlock newFreeBlock{
            worstFitIt->start + process.memorySize,
            worstFitIt->size - process.memorySize,
            true,
            NO_PROCESS
        };
        
        memoryBlocks_.insert(worstFitIt + 1, newFreeBlock);
    }
    
    // Allocate the block
    worstFitIt->isFree = false;
    worstFitIt->pid = process.pid;
    worstFitIt->size = process.memorySize;
    
    process.memory = {worstFitIt->start, worstFitIt->size, process.pid};
    return true;
}

void SimOS::deallocateMemory(int pid) {
    for (auto it = memoryBlocks_.begin(); it != memoryBlocks_.end(); ++it) {
        if (!it->isFree && it->pid == pid) {
            it->isFree = true;
            it->pid = NO_PROCESS;
            
            // Merge with adjacent free blocks
            while (it != memoryBlocks_.begin() && (it-1)->isFree) {
                (it-1)->size += it->size;
                memoryBlocks_.erase(it);
                it = memoryBlocks_.begin() + (it - memoryBlocks_.begin() - 1);
            }
            
            while (it + 1 != memoryBlocks_.end() && (it+1)->isFree) {
                it->size += (it+1)->size;
                memoryBlocks_.erase(it + 1);
            }
            
            break;
        }
    }
}

void SimOS::terminateProcessAndDescendants(int pid) {
    Process& process = processes_[pid];
    
    // Terminate all children first (cascading termination)
    for (int childPid : process.children) {
        if (processes_.count(childPid)) {
            terminateProcessAndDescendants(childPid);
        }
    }
    
    // Handle parent relationship
    if (process.parentPid != NO_PROCESS && processes_.count(process.parentPid)) {
        Process& parent = processes_[process.parentPid];
        auto childIt = std::find(parent.children.begin(), parent.children.end(), pid);
        if (childIt != parent.children.end()) {
            parent.children.erase(childIt);
        }
    }
    
    // Mark as zombie or terminate immediately
    if (process.parentPid != NO_PROCESS && processes_.count(process.parentPid)) {
        process.isZombie = true;
    } else {
        // No parent or parent doesn't exist - terminate immediately
        deallocateMemory(pid);
        processes_.erase(pid);
    }
    
    // Remove from CPU or ready queue
    if (cpuPid_ == pid) {
        cpuPid_ = NO_PROCESS;
    } else {
        removeFromReadyQueue(pid);
    }
}

void SimOS::addToReadyQueue(int pid) {
    if (std::find(readyQueue_.begin(), readyQueue_.end(), pid) == readyQueue_.end()) {
        readyQueue_.push_back(pid);
    }
}

void SimOS::removeFromReadyQueue(int pid) {
    readyQueue_.remove(pid);
}