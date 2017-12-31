#pragma once
#include "vm_declarations.h"

class KernelProcess;
class System;
class KernelSystem;

class Process
{
public:

	Process(ProcessId pid);
	~Process();

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

	//bool checkDirty(VirtualAddress address);
	//void setDirty(VirtualAddress address);

	//Second part
	Process* clone(ProcessId pid);
	Status createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char* name, AccessType flags);
	Status disconnectSharedSegment(const char* name);
	Status deleteSharedSegment(const char* name);
	Status deleteSharedSegmentSystem(const char* name, bool systemCall);
	//---------

	//Process needs to have system that it belongs to
	void setSystem(KernelSystem* sys);
	
	p_PageDirectory getPageDirectory();

private:
	KernelProcess *pProcess;
	friend class System;
	friend class KernelSystem;
	friend class KernelProcess;
};

