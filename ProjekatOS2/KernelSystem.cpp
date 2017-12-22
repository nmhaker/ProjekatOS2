#include "KernelSystem.h"

#include <iostream>
using namespace std;

#define directoryMask 0xFE0000
#define pageMask 0x01FC00
#define offsetMask 0x0003FF

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
		pmtTable[i].pageDescr = nullptr;
	}

	diskTable = reinterpret_cast<p_SysDiskDescriptor>(firstFit(&freePMTChunks, sizeof(SysDiskDescriptor)*partition->getNumOfClusters()));
	if (diskTable == nullptr) {
		cout << "BUKI: NO SPACE FOR SYSTEM DISK TABLE" << endl;
		cin;
		exit(1);
	}
	for (unsigned int i = 0; i < partition->getNumOfClusters(); i++) {
		diskTable[i].free = true;
		//diskTable[i].pid = 0;
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

	//Find chunks of memory that are segmented that is near each other but not grouped in 1 chunk and group them
	for (auto it1 = freePMTChunks.begin(); it1 != freePMTChunks.end(); it1++) {
		for (auto it2 = freePMTChunks.begin(); it2 != freePMTChunks.end(); it2++) {
			if ((unsigned long)(*it1)->startingAddress + (*it1)->size == (unsigned long)(*it2)->startingAddress) {
				(*it1)->size += (*it2)->size;
				delete *it2;
				freePMTChunks.erase(it2);
			}
		}
	}

	return 1000 * 1000;
}

Status KernelSystem::access(ProcessId pid, VirtualAddress address, AccessType type)
{
	std::lock_guard<std::mutex> lock(mutex_pmtTable);
	for (Process* p : listOfProcesses) {
		if (p->getProcessId() == pid) {
			//Take data from VA
			unsigned dir = (address & directoryMask) >> 17;
			unsigned page = (address & pageMask) >> 10;
			unsigned offset = address & offsetMask;
			p_PageDirectory pageDir = p->getPageDirectory();
			if(pageDir[dir].pageTableAddress[page].init){
				if (pageDir[dir].pageTableAddress[page].rwe == type) {
					if (pageDir[dir].pageTableAddress[page].valid) {
						cout << "PID: " << pid << "   VA: " << hex << address << " ->   PA: " << hex << (unsigned long)pageDir[dir].pageTableAddress[page].block + offset << endl;
						return Status::OK;
					} else {
						cout << "STRANICA: "<< address << " JE NA DISKU, PAGE FAULT" << endl;
						//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						return Status::PAGE_FAULT;
					}
				}
				else {
						cout << "ACCESS : NEUSPESAN PRISTUP STRANICI POGRESNA PRAVA PRISTUPA" << endl;
						cin;
						return Status::TRAP;				}
			}
			else {
				cout << "ACCESS: POKUSAVA SE PRISTUPITI NE POSTOJECOJ ADRESI , VRACAM TRAP" << endl;
				cin;
				return Status::TRAP;
			}
		}
	}
	cout << "NIJE PRONADJEN KORISNIK GRESKA" << endl;
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

PhysicalAddress KernelSystem::getFreeFrame(p_PageDescriptor pd)
{
	std::lock_guard<std::mutex> guard(mutex_pmtTable);
	for (unsigned long i = 0; i < processVMSpaceSize; i++) {
		if (pmtTable[i].free) {
			pmtTable[i].free = false;
			pmtTable[i].pageDescr = pd;
			//Isert into FIFO QUEUE for page replacement algorithm, to keep track of oldest page loaded into memory
			fifoQueue.push(i);
			//Return address of frame
			return reinterpret_cast<PhysicalAddress>((reinterpret_cast<char*>(processVMSpace)) + i*PAGE_SIZE);
		}
	}
	unsigned long freeFrame = replacePage(pd);
	return reinterpret_cast<PhysicalAddress>((reinterpret_cast<char*>(processVMSpace)) + freeFrame*PAGE_SIZE);
	//cout << "BUKI: MEMORIJA JE PUNA NEMA SLOBODNIH OKVIRA, TREBA DA SE POKRENE PAGING NA DISK NEKOG PROCESA/STRANICE I DA SE OSLOBODE OKVIRI ZA SAD FAIL I EXIT" << endl;
	//cin;
	//exit(1);
	// ZA SADA VRACAM NULLPTR ALI
	// OVDE TREBA DA SE NEKA STRANICA VRATI NA DISK DA BI SE OSLOBODIO OKVIR DA SE ISKORISTI ZA OVAJ PROCES KOJI TRAZI, RADICU GLOBALNU POLITIKU ZAMENE STRANICA STO ZNACI DA NEKI DRUGI PROCES MOZE DA ZAMENI STRANICU NEKOM DRUGOM PROCESU
	return nullptr;
}

std::list<FreeChunk*>* KernelSystem::getFreePMTChunks()
{
	return &freePMTChunks;
}

unsigned long KernelSystem::freePMTEntry(PhysicalAddress pa)
{
	std::lock_guard<std::mutex> lock(mutex_pmtTable);
	unsigned long PA_integer = phyToNum(pa);
	pmtTable[PA_integer].free = true;
	pmtTable[PA_integer].pageDescr = nullptr;

	return PA_integer;
}

Partition * KernelSystem::getPartition()
{
	return partition;
}

ClusterNo KernelSystem::getFreeCluster()
{
	for (unsigned long i = 0; i < partition->getNumOfClusters(); i++) {
		if (diskTable[i].free) {
			diskTable[i].free = false;
			//diskTable[i].pid = pid;
			return i;
		}
	}
	cout << "BUKI: DISK MEMORIJA JE PUNA NEMA SLOBODNIH OKVIRA, NE MOGU DA PAGE-ujem OKVIR NA DISK ERROR" << endl;
	cin;
	exit(1);
}

bool KernelSystem::setFreeCluster(ClusterNo clust)
{
	diskTable[clust].free = true;
	//diskTable[clust].pid = 0;
	return true;
}

unsigned long KernelSystem::phyToNum(PhysicalAddress pa)
{
	return (reinterpret_cast<unsigned long>(pa) - reinterpret_cast<unsigned long>(processVMSpace))/PAGE_SIZE;
}

PhysicalAddress KernelSystem::numToPhy(unsigned long num)
{
	return reinterpret_cast<PhysicalAddress>((reinterpret_cast<char*>(processVMSpace)) + num*PAGE_SIZE);
}

unsigned long KernelSystem::replacePage(p_PageDescriptor pd)
{
	if (fifoQueue.empty()) {
		cout << "BUKI:  ERROR Trying to access empty fifoQueue structure for page replacement" << endl;
		cin;
		exit(1);
	}
	//Get num of victim page
	unsigned long victimPage = fifoQueue.front();
	//Remove from queue victim page
	fifoQueue.pop();
	//And insert it on the end because it will be accessed now, that is loaded
	fifoQueue.push(victimPage);
	//Get PA from victimPage
	PhysicalAddress vpPA = numToPhy(victimPage);
	//Alocate one free cluster on disk
	ClusterNo disk = getFreeCluster();
	//Convert fram to byte writable
	char* byteFrame = reinterpret_cast<char*> (vpPA);
	//Write frame to cluster
	partition->writeCluster(disk, byteFrame);
	//Update pageDescriptor in pmt table of process that owned this frame so that its address now points to disk because his frame is not in memory anymore
	pmtTable[victimPage].pageDescr->valid = false;
	pmtTable[victimPage].pageDescr->disk = disk;
	//Change the descriptor that it belongs to to new process
	pmtTable[victimPage].pageDescr = pd;
	//Return new victimPage which is not zeroed but it will be overwritten 
	//cout << "SENDING FRAME TO DISK" << endl;
	//std::this_thread::sleep_for(std::chrono::milliseconds(400));
	return victimPage;
}
