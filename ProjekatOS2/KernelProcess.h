#pragma once
#include "vm_declarations.h"
#include "KernelSystem.h"

#include <list>
#include <mutex>
#include <fstream>

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

	//Second part
	Process* clone(ProcessId pid);
	Status createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char* name, AccessType flags);
	Status disconnectSharedSegment(const char* name);
	Status deleteSharedSegment(const char* name, bool systemCall);
	//---------

private:
	ProcessId pid;
	KernelSystem* sys;

	p_PageDirectory pageDirectory;
	std::mutex mutex_pageDirectory;
	
	std::list<p_SharedSegment> listOfSharedSegments;
	std::mutex mutex_listOfSharedSegments;

};

