#include "KernelSystem.h"

#include <iostream>
using namespace std;

unsigned KernelSystem::PID = 0;

KernelSystem::KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition * partition) : freePMTChunks(), listOfProcesses(), mutex_freePMTChunks(), mutex_listOfProcesses(), mutex_pmtTable()
{
	this->processVMSpace = processVMSpace;
	this->processVMSpaceSize = processVMSpaceSize;
	this->pmtSpace = pmtSpace;
	this->pmtSpaceSize = pmtSpaceSize;
	this->partition = partition;

	freePMTChunks.emplace_back(std::move(new FreeChunk{ pmtSpace, pmtSpaceSize*PAGE_SIZE }));
			
	pmtTable = reinterpret_cast<p_SysPageDescriptor>(firstFit(&freePMTChunks,sizeof(SysPageDescriptor)*processVMSpaceSize));
	
	if (pmtTable == nullptr) {
		cout << "BUKI : NO SPACE FOR SYSTEM PMT TABLE" << endl;
		cin;
		exit(1);
	}
	
	for (unsigned int i = 0; i < processVMSpaceSize; i++) {
		pmtTable[i].free = true;
		pmtTable[i].pid = 0;
	}

	cout << "Kreiran system" << endl;
}

KernelSystem::~KernelSystem()
{
	freePMTChunks.clear();
	listOfProcesses.clear();
}

Process * KernelSystem::createProcess()
{
	std::lock_guard<std::mutex> guard(mutex_listOfProcesses);
	Process *newProc = new Process(KernelSystem::PID++);
	newProc->setSystem(this);
	listOfProcesses.emplace_back(std::move(newProc));
	return newProc;
}

Time KernelSystem::periodicJob()
{
	//NOT IMPLEMENTED YET, RETURNING 0
	return 0;
}

Status KernelSystem::access(ProcessId pid, VirtualAddress address, AccessType type)
{
	cout << "POKUSAVA DA POZOVE access !" << endl << "Nisam jojs implementirao xdd" << endl;
	cin;
	exit(1);
}

PhysicalAddress KernelSystem::firstFit(std::list<FreeChunk*>* list, unsigned long bytes) {
	std::lock_guard<std::mutex> lock(mutex_freePMTChunks);
	if (list == nullptr) {
		cout << "list ptr == nullptr !" << endl;
		cin;
		exit(1);
	}
	if (list->empty()) {
		cout << "List is empty" << endl;
		cin;
		exit(1);
	}
	//std::list<FreeChunk*>::iterator it, end;
	auto it = list->begin();
	auto end = list->end();
	for (it; it != end; ++it) {
		if ((*it)->size >= bytes) {
			if ((*it)->size == bytes) {
				PhysicalAddress sAddr = (*it)->startingAddress;
				delete *it;
				list->erase(it);
				return sAddr;
			}
			else {
				FreeChunk* newFreeChunk = new FreeChunk{
					reinterpret_cast<PhysicalAddress>(reinterpret_cast<char*>((*it)->startingAddress) + bytes),
					(*it)->size - bytes
				};
				PhysicalAddress sAddr = (*it)->startingAddress;
				delete *it;
				list->erase(it);
				list->emplace_back(std::move(newFreeChunk));
				return sAddr;
			}
		}
	}
	return nullptr;
}

PhysicalAddress KernelSystem::getFreeFrame(ProcessId pid)
{
	std::lock_guard<std::mutex> guard(mutex_pmtTable);
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

std::list<FreeChunk*>* KernelSystem::getFreePMTChunks()
{
	return &freePMTChunks;
}
