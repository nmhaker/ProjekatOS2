#include <chrono>
#include <cstdint>
#include <memory>
#include <iostream>
#include <thread>
#include "System.h"
#include "part.h"
#include "vm_declarations.h"
#include "ProcessTest.h"

#define VM_SPACE_SIZE (5000)
#define PMT_SPACE_SIZE (3000)
#define N_PROCESS (20)
#define PERIODIC_JOB_COST (1)

PhysicalAddress alignPointer(PhysicalAddress address) {
    uint64_t addr = reinterpret_cast<uint64_t> (address);

    addr += PAGE_SIZE;
    addr = addr / PAGE_SIZE * PAGE_SIZE;

    return reinterpret_cast<PhysicalAddress> (addr);
}

int main() {
    Partition part("p1.ini");

    uint64_t size = (VM_SPACE_SIZE + 2) * PAGE_SIZE;
    PhysicalAddress vmSpace = (PhysicalAddress ) new char[size];
    PhysicalAddress alignedVmSpace = alignPointer(vmSpace);

    size = (PMT_SPACE_SIZE + 2) * PAGE_SIZE;
    PhysicalAddress pmtSpace = (PhysicalAddress ) new char[size];
    PhysicalAddress alignedPmtSpace = alignPointer(pmtSpace);

    System system(alignedVmSpace, VM_SPACE_SIZE, alignedPmtSpace, PMT_SPACE_SIZE, &part);
    SystemTest systemTest(system, alignedVmSpace, VM_SPACE_SIZE);
    ProcessTest* process[N_PROCESS];
    std::thread *threads[N_PROCESS];

    std::mutex globalMutex;

    for (int i = 0; i < N_PROCESS; i++) {
        std::cout << "Create process " << i << std::endl;
        process[i] = new ProcessTest(system, systemTest);
        threads[i] = new std::thread(&ProcessTest::run, process[i]);
    }
	std::cout << "MAIN: KREIRANI PROCESI" << std::endl;

    Time time;
    while ((time = system.periodicJob())) {
        std::this_thread::sleep_for(std::chrono::microseconds(time));

        std::lock_guard<std::mutex> guard(systemTest.getGlobalMutex());

        std::cout << "Doing periodic job\n";

        std::this_thread::sleep_for(std::chrono::microseconds(PERIODIC_JOB_COST));

        bool finished = true;
        for (int i = 0; i < N_PROCESS; i++) {
            finished = finished && process[i]->isFinished();
        }

        if (finished) {
            break;
        }
    }

	std::cout << "MAIN: BRISEM PROCESE I NITI" << std::endl;
    for (int i = 0; i < N_PROCESS; i++) {
        threads[i]->join();
        delete threads[i];
        delete process[i];
    }
	std::cout << "MAIN: OBRISANI PROCESI I NITI" << std::endl;

    delete [] vmSpace;
    delete [] pmtSpace;

    std::cout << "Test finished\n";
}