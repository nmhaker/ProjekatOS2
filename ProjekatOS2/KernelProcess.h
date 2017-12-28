#pragma once
#include "vm_declarations.h"
#include "KernelSystem.h"

#include <list>
#include <mutex>

class KernelProcess
{
public:
	KernelProcess(ProcessId pid);
	~KernelProcess();
	
	ProcessId getProcessId() const;

	Status createSegment(VirtualAddress startAddress,
						 PageNum segmentSize,
						 AccessType flags);
	Status loadSegment(VirtualAddress startAddress,
					   PageNum segmentSize,
					   AccessType flags,
					   void* content);
	Status deleteSegment(VirtualAddress startAddress);

	Status pageFault(VirtualAddress address);
	PhysicalAddress getPhysicalAddress(VirtualAddress address);

	//My implementation, can be public because it is called by the kernel
	void setSystem(KernelSystem* sys); //Process needs to have system that owns it
	p_PageDirectory getPageDirectory();

	void acquireMutex();
	void releaseMutex();

	bool checkDirty(VirtualAddress address);
	void setDirty(VirtualAddress address);
	
private:
	ProcessId pid;
	KernelSystem* sys;

	p_PageDirectory pageDirectory;
	std::mutex mutex_pageDirectory;
};

