#pragma once
#include "vm_declarations.h"
#include "part.h"
#include "Process.h"

#include <list>
#include <mutex>
#include <queue>

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

	PhysicalAddress firstFit(std::list<FreeChunk*>* list, unsigned long bytes);
	PhysicalAddress getFreeFrame(ProcessId pid);

	std::list<FreeChunk*>* getFreePMTChunks();

	unsigned long freePMTEntry(PhysicalAddress pa);
	
	Partition* getPartition();
	ClusterNo getFreeCluster(ProcessId pid);
	bool setFreeCluster(ClusterNo clust);

	unsigned long phyToNum(PhysicalAddress pa);

protected:
	unsigned long replacePage();

private:
	PhysicalAddress processVMSpace;
	PageNum processVMSpaceSize;
	PhysicalAddress pmtSpace;
	PageNum pmtSpaceSize;
	Partition* partition;

	std::list<FreeChunk*> freePMTChunks;

	p_SysPageDescriptor pmtTable;
	p_SysDiskDescriptor diskTable;
	
	std::list<Process*> listOfProcesses;

	std::mutex mutex_freePMTChunks;
	std::mutex mutex_listOfProcesses;
	std::mutex mutex_pmtTable;
	std::mutex mutex_access;

	std::queue<unsigned long> fifoQueue;
};
