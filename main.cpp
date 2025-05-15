// main.cpp
#include "SimOS.h"
#include <iostream>

int main() {
    std::cout << "Program started!\n";  // Add this line
    
    SimOS os(2, 10000, 1000);
    os.NewProcess(2000, 3);
    
    std::cout << "CPU PID: " << os.GetCPU() << "\n";  // And this line
    return 0;
}