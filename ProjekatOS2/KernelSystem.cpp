#include "KernelSystem.h"

#include <iostream>
using namespace std;

unsigned KernelSystem::PID = 0;

KernelSystem::KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition * partition)
{
	this->processVMSpace = processVMSpace;
	this->processVMSpaceSize = processVMSpaceSize;
	this->pmtSpace = pmtSpace;
	this->pmtSpaceSize = pmtSpaceSize;
	this->partition = partition;

	freePMTChunks.push_back(new FreeChunk{pmtSpace, pmtSpaceSize*PAGE_SIZE});
			
	pmtTable = reinterpret_cast<p_SysPageDescriptor>(firstFit(freePMTChunks,sizeof(SysPageDescriptor)*processVMSpaceSize));
	
	if (pmtTable == nullptr) {
		cout << "BUKI : NO SPACE FOR SYSTEM PMT TABLE" << endl;
		exit(1);
	}
	
	for (unsigned int i = 0; i < processVMSpaceSize; i++) {
		pmtTable[i].free = true;
		pmtTable[i].pid = 0;
	}
}

KernelSystem::~KernelSystem()
{
	freePMTChunks.clear();
	listOfProcesses.clear();
}

Process * KernelSystem::createProcess()
{
	Process *newProc = new Process(KernelSystem::PID++);
	newProc->setSystem(this);
	listOfProcesses.push_back(newProc);
	return newProc;
}

Time KernelSystem::periodicJob()
{
	return Time();
}

Status KernelSystem::access(ProcessId pid, VirtualAddress address, AccessType type)
{
	return Status();
}

PhysicalAddress KernelSystem::firstFit(std::list<FreeChunk*> list, unsigned long bytes) {
	for (std::list<FreeChunk*>::iterator it = list.begin(); it != list.end(); it++) {
		if ((*it)->size >= bytes) {
			if ((*it)->size == bytes) {
				PhysicalAddress sAddr = (*it)->startingAddress;
				list.remove(*it);
				return sAddr;
			}
			else {
				FreeChunk* newFreeChunk = new FreeChunk{
					reinterpret_cast<PhysicalAddress>(reinterpret_cast<char*>((*it)->startingAddress) + bytes),
					(*it)->size - bytes
				};
				PhysicalAddress sAddr = (*it)->startingAddress;
				list.remove(*it);
				list.push_back(newFreeChunk);
				return sAddr;
			}
		}
	}
	return nullptr;
}

PhysicalAddress KernelSystem::getFreeFrame(ProcessId pid)
{
	for (unsigned long i = 0; i < processVMSpaceSize; i++) {
		if (pmtTable[i].free) {
			pmtTable[i].free = false;
			pmtTable[i].pid = pid;
			return reinterpret_cast<PhysicalAddress>((reinterpret_cast<char*>(processVMSpace)) + i*PAGE_SIZE);
		}
	}
	// ZA SADA VRACAM NULLPTR ALI
	// OVDE TREBA DA SE NEKA STRANICA VRATI NA DISK DA BI SE OSLOBODIO OKVIR DA SE ISKORISTI ZA OVAJ PROCES KOJI TRAZI, RADICU GLOBALNU POLITIKU ZAMENE STRANICA STO ZNACI DA NEKI DRUGI PROCES MOZE DA ZAMENI STRANICU NEKOM DRUGOM PROCESU
	return nullptr;
}

std::list<FreeChunk*> KernelSystem::getFreePMTChunks()
{
	return freePMTChunks;
}
