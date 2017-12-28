#include "KernelSystem.h"
#include "KernelProcess.h"

#include <iostream>
using namespace std;

#define directoryMask 0xFE0000
#define pageMask 0x01FC00
#define offsetMask 0x0003FF

unsigned KernelSystem::PID = 0;

KernelSystem::KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition * partition) : freePMTChunks(), listOfProcesses(), mutex_freePMTChunks(), mutex_listOfProcesses(), mutex_pmtTable(), mutex_diskTable()
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

	outputFile.open("output.txt");

	cout << "Kreiran system" << endl;
	outputFile << "Kreiran system" << endl;
}

KernelSystem::~KernelSystem()
{
	freePMTChunks.clear();
	listOfProcesses.clear();
	outputFile.close();
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

	return 5 * 1000 * 1000;
}

Status KernelSystem::access(ProcessId pid, VirtualAddress address, AccessType type)
{
	std::lock_guard<std::mutex> lock(mutex_pmtTable);
	std::lock_guard<std::mutex> lock2(mutex_listOfProcesses);
	for (Process* p : listOfProcesses) {
		if (p->getProcessId() == pid) {
			//Take data from VA
			unsigned dir = (address & directoryMask) >> 17;
			unsigned page = (address & pageMask) >> 10;
			unsigned offset = address & offsetMask;
			p_PageDirectory pageDir = p->getPageDirectory();
			if(pageDir[dir].pageTableAddress[page].init){
				if (pageDir[dir].pageTableAddress[page].rwe == type || (pageDir[dir].pageTableAddress[page].rwe==READ_WRITE && (type == READ || type == WRITE))) {
					if (pageDir[dir].pageTableAddress[page].valid) {
						
						if (type == WRITE) {
							pageDir[dir].pageTableAddress[page].dirty = true;
							outputFile << "WRITE INSTRUCTION" << endl;
							cout << "WRITE INSTRUCTION " << endl;
						} else {
							if (type == READ){
								outputFile << "READ" << " INSTRUCTION" << endl;
								cout << "READ INSTRUCTION" << endl;
							}
							else if (type == READ_WRITE) {
								outputFile << "READ_WRITE" << " INSTRUCTION" << endl;
								cout << "READ_WRITE INSTRUCTION" << endl;
							}
							else if (type == EXECUTE) {
								outputFile << "EXECUTE" << " INSTRUCTION" << endl;
								cout << "EXECUTE INSTRUCTION" << endl;
							}
						}
						cout << "PID: " << pid << "   VA: " << hex << address << " ->   PA: " << hex << (unsigned long)pageDir[dir].pageTableAddress[page].block + offset << endl;
						outputFile << "PID: " << pid << "   VA: " << hex << address << " ->   PA: " << hex << (unsigned long)pageDir[dir].pageTableAddress[page].block + offset << endl;
						return Status::OK;
					} else {
						cout << "STRANICA: "<< address << " JE PAGED OUT, PAGE FAULT" << endl;
						outputFile << "STRANICA: "<< address << " JE PAGED OUT, PAGE FAULT" << endl;
						//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						return Status::PAGE_FAULT;
					}
				}
				else {
						cout << "ACCESS : NEUSPESAN PRISTUP STRANICI POGRESNA PRAVA PRISTUPA" << endl;
						exit(1);
						return Status::TRAP;	
				}
			}
			else {
				cout << "ACCESS: POKUSAVA SE PRISTUPITI NE POSTOJECOJ ADRESI , VRACAM TRAP" << endl;
				exit(1);
				return Status::TRAP;
			}
		}
	}
	cout << "NIJE PRONADJEN KORISNIK GRESKA" << endl;
	exit(1);
}

PhysicalAddress KernelSystem::firstFit(std::list<FreeChunk*>* list, unsigned long bytes) {
	std::lock_guard<std::mutex> lock(mutex_freePMTChunks);
	if (list == nullptr) {
		cout << "list ptr == nullptr !" << endl;
		exit(1);
	}
	if (list->empty()) {
		cout << "List is empty" << endl;
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

PhysicalAddress KernelSystem::getFreeFrame(p_PageDescriptor pd, KernelProcess* proc)
{
	std::lock_guard<std::mutex> guard(mutex_pmtTable);

	//Try to allocate free frame from memory if it fails replace some page by fifo policy
	for (unsigned long i = 0; i < processVMSpaceSize; i++) {
		if (pmtTable[i].free) {
			pmtTable[i].free = false;
			pmtTable[i].pageDescr = pd;
			pmtTable[i].proc = proc;
			//Isert into FIFO QUEUE for page replacement algorithm, to keep track of oldest page loaded into memory
			fifoQueue.push(i);
			//Return address of frame
			return reinterpret_cast<PhysicalAddress>((reinterpret_cast<char*>(processVMSpace)) + i*PAGE_SIZE);
		}
	}

	//Go and replace page that is first to be replaced by fifo replacement algorithm and return it
	unsigned long freeFrame = replacePage(pd, proc);

	//Convert to physical address and return to the user
	return reinterpret_cast<PhysicalAddress>((reinterpret_cast<char*>(processVMSpace)) + freeFrame*PAGE_SIZE);
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
	std::lock_guard<std::mutex> lock(mutex_diskTable);
	//clusterNo = 0 is reserved for check in descriptor to know if it should get page that is paged out from the disk or it should get if from source address because it was overwritten and not paged out because its dirty bit was 0; Its better to waste 1KB that is 1 cluster for infinite pages than to waste 1B inside every descriptor for boolean flag, because 10000 Pages result in 10KB wasted memory for flag while this way it results in 1KB wasted disk space, for 1M pages it is 1MB wasted memory while this uses only 1KB of disk
	for (unsigned long i = 1; i < partition->getNumOfClusters(); i++) {
		if (diskTable[i].free) {
			diskTable[i].free = false;
			return i;
		}
	}
	cout << "BUKI: DISK MEMORIJA JE PUNA NEMA SLOBODNIH OKVIRA, NE MOGU DA PAGE-ujem OKVIR NA DISK ERROR" << endl;
	exit(1);
}

bool KernelSystem::setFreeCluster(ClusterNo clust)
{
	std::lock_guard<std::mutex> lock(mutex_diskTable);
	if (diskTable == nullptr) {
		cout << "DISKTABLE=nullptr" << endl;
		exit(1);
	}
	diskTable[clust].free = true;
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

void KernelSystem::removeProcess(ProcessId pid)
{
	std::lock_guard<std::mutex> lock(mutex_listOfProcesses);

	for (Process* p : listOfProcesses) {
		if (p->getProcessId() == pid) {
			listOfProcesses.remove(p);
			return;
		}
	}
	cout << "Greska nisam nasao proces da izbrisem iz liste procesa u sistemu" << endl;
	exit(1);
}

unsigned long KernelSystem::replacePage(p_PageDescriptor pd, KernelProcess* p)
{
	//std::lock_guard<std::mutex> lock(p->getMutex());
	//p->acquireMutex();

	if (fifoQueue.empty()) {
		cout << "BUKI:  ERROR Trying to access empty fifoQueue structure for page replacement" << endl;
		exit(1);
	}
	//Get num of victim page
	unsigned long victimPage = fifoQueue.front();
	//Remove from queue victim page
	fifoQueue.pop();
	//And insert it on the end because it will be accessed now, that is loaded
	fifoQueue.push(victimPage);
	//If this page is dirty then we need to page it out on disk, otherwise overwrite over it cause it is in memory
	if (pmtTable[victimPage].pageDescr->dirty) {
		cout << "ZAPRLJANA, SNIMAM" << endl;
		//exit(1);
		//Get PA from victimPage
		PhysicalAddress vpPA = numToPhy(victimPage);
		//Alocate one free cluster on disk
		ClusterNo disk = getFreeCluster();
		//Convert fram to byte writable
		char* byteFrame = reinterpret_cast<char*> (vpPA);
		//Write frame to cluster
		partition->writeCluster(disk, byteFrame);
		//Update pageDescriptor in pmt table of process that owned this frame so that its address now points to disk because his frame is not in memory anymore
		pmtTable[victimPage].pageDescr->disk = disk;
		//Reset dirty bit ???
		//pmtTable[victimPage].pageDescr->dirty = false;
	} else {
		//Update pageDescriptor in pmt table of process that owned this frame so that its address now points to clusterNo=0 because his frame is not in memory anymore but must be returned from source address
		pmtTable[victimPage].pageDescr->disk = 0;

		//Pages created with createSegment dont have sourceAddress because they are empty with content but can be used => if i set valid to false and disk to 0 it would try to load from sourceAddress which is nullptr, so i need to set valid to false only those pages which can be brought from sourceAddress and those created with createSegment because dirty is zero i just overwrite them so valid is true
			//if (pmtTable[victimPage].pageDescr->sourceAddress != nullptr)
	}
	//Set flag to invalid to know it is not in memory
	pmtTable[victimPage].pageDescr->valid = false;
	//pmtTable[victimPage].pageDescr->dirty = false;
	//Change the descriptor that it belongs to to new process
	pmtTable[victimPage].pageDescr = pd;
	pmtTable[victimPage].proc = p;

	//Return new victimPage which is not zeroed but it will be overwritten 

	//p->releaseMutex();

	return victimPage;
}
