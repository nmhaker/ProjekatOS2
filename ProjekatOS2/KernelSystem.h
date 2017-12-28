#pragma once
#include "vm_declarations.h"
#include "part.h"
#include "Process.h"

#include <list>
#include <mutex>
#include <queue>
#include <fstream>

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
	PhysicalAddress getFreeFrame(p_PageDescriptor pd, KernelProcess* proc);

	std::list<FreeChunk*>* getFreePMTChunks();

	unsigned long freePMTEntry(PhysicalAddress pa);
	
	Partition* getPartition();
	ClusterNo getFreeCluster();
	bool setFreeCluster(ClusterNo clust);

	unsigned long phyToNum(PhysicalAddress pa);
	PhysicalAddress numToPhy(unsigned long num);

	void removeProcess(ProcessId pid);

	std::ofstream outputFile;
protected:
	unsigned long replacePage(p_PageDescriptor pd, KernelProcess*p);

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
	std::mutex mutex_diskTable;
	//std::mutex mutex_access;

	std::queue<unsigned long> fifoQueue;

};

