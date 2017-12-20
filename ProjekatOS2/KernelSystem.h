#pragma once
#include "vm_declarations.h"
#include "part.h"
#include "Process.h"

#include <list>

class KernelSystem
{
public:
	KernelSystem(PhysicalAddress processVMSpace,
		   PageNum processVMSpaceSize,
		   PhysicalAddress pmtSpace,
		   PageNum pmtSpaceSize,
		   Partition* partition);
	~KernelSystem();

	Process* createProcess();

	Time periodicJob();

	//Hardware job
	Status access(ProcessId pid, VirtualAddress address, AccessType type);

	//MY IMPLEMENTATION
	static unsigned PID;

	PhysicalAddress firstFit(std::list<FreeChunk*> list, unsigned long bytes);
	PhysicalAddress getFreeFrame(ProcessId pid);

	std::list<FreeChunk*> getFreePMTChunks();

private:
	PhysicalAddress processVMSpace;
	PageNum processVMSpaceSize;
	PhysicalAddress pmtSpace;
	PageNum pmtSpaceSize;
	Partition* partition;

	std::list<FreeChunk*> freePMTChunks;

	p_SysPageDescriptor pmtTable;
	
	std::list<Process*> listOfProcesses;
};
