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

//All of my implementation will be protected
protected:
	//Process needs to have system that it belongs to
	void setSystem(KernelSystem* sys);
	
	p_PageDirectory getPageDirectory();
private:
	KernelProcess *pProcess;
	friend class System;
	friend class KernelSystem;
};

