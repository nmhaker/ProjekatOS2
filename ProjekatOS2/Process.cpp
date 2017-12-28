#include "Process.h"
#include "KernelProcess.h"

Process::Process(ProcessId pid)
{
	pProcess = new KernelProcess(pid);
}

Process::~Process()
{
	delete pProcess;
}

ProcessId Process::getProcessId() const
{
	return pProcess->getProcessId();
}

Status Process::createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags)
{
	return pProcess->createSegment(startAddress, segmentSize, flags);
}

Status Process::loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void * content)
{
	return pProcess->loadSegment(startAddress, segmentSize, flags, content);
}

Status Process::deleteSegment(VirtualAddress startAddress)
{
	return pProcess->deleteSegment(startAddress);
}

Status Process::pageFault(VirtualAddress address)
{
	return pProcess->pageFault(address);
}

PhysicalAddress Process::getPhysicalAddress(VirtualAddress address)
{
	return pProcess->getPhysicalAddress(address);
}

bool Process::checkDirty(VirtualAddress address)
{
	return pProcess->checkDirty(address);
}

void Process::setDirty(VirtualAddress address)
{
	return pProcess->setDirty(address);
}

void Process::setSystem(KernelSystem * sys)
{
	pProcess->setSystem(sys);
}

p_PageDirectory Process::getPageDirectory()
{
	return pProcess->getPageDirectory();
}
