#include "KernelProcess.h"

#include <iostream>
using namespace std;
#include <windows.h>

#define directoryMask 0xFE0000
#define pageMask 0x01FC00
#define offsetMask 0x0003FF

//VA(7:7:10)
#define MAX_PMT_TABLES 128 
#define MAX_PMT_ENTRIES 128

KernelProcess::KernelProcess(ProcessId pid) : mutex_pageDirectory() 
{
	this->pid = pid;
	
	cout << "Prosao konstruktor procesa: " << pid << endl;
}


KernelProcess::~KernelProcess()
{
}

ProcessId KernelProcess::getProcessId() const
{
	return pid;
}

Status KernelProcess::createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags)
{
	std::lock_guard<std::mutex> lock(mutex_pageDirectory);
	if (segmentSize == 0) {
		cout << "BUKI: NE MOZES KREIRATI SEGMENT SA 0 STRANICA" << endl;
		cin;
		exit(1);
	}

	//Take data from VA
	unsigned dir = (startAddress & directoryMask) >> 17;
	unsigned page = (startAddress & pageMask) >> 10;
	unsigned offset = startAddress & offsetMask;
	if (offset != 0) {
		cout << "BUKI: NIJE PORAVNAT SEGMENT NA POCETAK STRANICE VRACAM TRAP" << endl;
		return Status::TRAP;
	}

	//Find if it has continuous segmentSize free entries inside PMT for segment allocation

	//Flag that tells if we can allocate segment
	bool canAllocate = true;
	//For counting how much more we need to allocate in case i need to go inside other dir
	signed long tempCounter = segmentSize;
	//For going into next dir of PMT
	unsigned tempCurrentDir = dir;
	//For incrementing PMT entries inside PMT
	PageNum tempCurrentPage = page;
	//START SEARCHING segmentSize SEQUENTIAL ENTRIES 
	while (tempCounter > 0) {
		p_PageDescriptor pmt = nullptr;
		if (pageDirectory[tempCurrentDir].pageTableAddress == nullptr) {
			tempCounter -= MAX_PMT_ENTRIES - tempCurrentPage;
			tempCurrentPage = 0;
			tempCurrentDir++;
			if (tempCurrentDir == MAX_PMT_TABLES) {
				canAllocate = false;
				break;
			}
			continue;
		}
		else {
			pmt = pageDirectory[tempCurrentDir].pageTableAddress;
		}
		if(!pmt[tempCurrentPage].init){
			tempCurrentPage++;
			tempCounter--;
			if (tempCurrentPage ==  MAX_PMT_ENTRIES)
			{
				tempCurrentPage = 0;
				tempCurrentDir++;
				if (tempCurrentDir == MAX_PMT_TABLES) {
					canAllocate = false;
					break;
				}
			}	
			continue;
		} else {
			canAllocate = false;
			break;
		}
	}
	if (!canAllocate) {
		cout << "BUKI: NEMA PROSTORA ZA ALOKACIJU SEGMENTA" << endl;
		return Status::TRAP;
	}
	//REINITIALIZE VARIABLES
	//For counting how much more we need to allocate in case i need to go inside other dir
	tempCounter = segmentSize;
	//For going into next dir of PMT
	tempCurrentDir = dir;
	//For incrementing PMT entries inside PMT
	tempCurrentPage = page;
	//For specifying how long the segment was only in first descriptor
	bool segmentSet = false;
	//ALLOCATE SEQUENTIAL ENTRIES
	while (tempCounter > 0) {
		p_PageDescriptor pmt = nullptr;
		if (pageDirectory[tempCurrentDir].pageTableAddress == nullptr) {
			pmt = pageDirectory[tempCurrentDir].pageTableAddress = reinterpret_cast<p_PageDescriptor>( sys->firstFit(sys->getFreePMTChunks(), sizeof(PageDescriptor)*MAX_PMT_ENTRIES));
			for (int i = 0; i < MAX_PMT_ENTRIES; i++) {
				pmt[i].init = 0;
				pmt[i].valid = 0;
				pmt[i].dirty = 0;
				pmt[i].numOfPages = 0;
			}
			for (int i = tempCurrentPage; i < MAX_PMT_ENTRIES; i++) {
				if (!segmentSet) {
					pmt[i].numOfPages = segmentSize;
					segmentSet = true;
				}
				pmt[i].init = 1;
				pmt[i].valid = 1;
				PhysicalAddress addr = sys->getFreeFrame(&pmt[i]);
				if (addr == nullptr) {
					cout << "BUKI: NEMA SLOBODNIH OKVIRA U MEMORIJI RACUNARA" << endl;
					cin;
					exit(1);
				}
				pmt[i].block = addr;
				pmt[i].rwe = flags;
				tempCounter--;
				if (tempCounter == 0)
					break;
			}
			//tempCounter -= MAX_PMT_ENTRIES - tempCurrentPage;
			tempCurrentPage = 0;
			tempCurrentDir++;
			continue;
		}
		else {
			pmt = pageDirectory[tempCurrentDir].pageTableAddress;
		}
		if (!segmentSet) {
			pmt[tempCurrentPage].numOfPages = segmentSize;
			segmentSet = true;
		}
		pmt[tempCurrentPage].init = 1;
		pmt[tempCurrentPage].valid = 1;
		PhysicalAddress addr = sys->getFreeFrame(&pmt[tempCurrentPage]);
		if (addr == nullptr) {
			cout << "BUKI: NEMA SLOBODNIH OKVIRA U MEMORIJI" << endl;
			cin;
			exit(1);
		}
		pmt[tempCurrentPage].block = addr;
		pmt[tempCurrentPage].rwe = flags;
		tempCurrentPage++;
		tempCounter--;
		if (tempCurrentPage ==  MAX_PMT_ENTRIES)
		{
			tempCurrentPage = 0;
			tempCurrentDir++;
		}	
	}
	return Status::OK;
}

Status KernelProcess::loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void * content)
{
	std::lock_guard<std::mutex> lock(mutex_pageDirectory);
	if (segmentSize == 0) {
		cout << "BUKI: NE MOZES KREIRATI SEGMENT SA 0 STRANICA" << endl;
		cin;
		exit(1);
	}

	//Take data from VA
	unsigned dir = (startAddress & directoryMask) >> 17;
	unsigned page = (startAddress & pageMask) >> 10;
	unsigned offset = startAddress & offsetMask;
	if (offset != 0) {
		cout << "BUKI: NIJE PORAVNAT SEGMENT NA POCETAK STRANICE VRACAM TRAP" << endl;
		return Status::TRAP;
	}

	//Find if it has continuous segmentSize free entries inside PMT for segment allocation

	//Flag that tells if we can allocate segment
	bool canAllocate = true;
	//For counting how much more we need to allocate in case i need to go inside other dir
	signed long tempCounter = segmentSize;
	//For going into next dir of PMT
	unsigned tempCurrentDir = dir;
	//For incrementing PMT entries inside PMT
	PageNum tempCurrentPage = page;
	//START SEARCHING segmentSize SEQUENTIAL ENTRIES 
	while (tempCounter > 0) {
		p_PageDescriptor pmt = nullptr;
		if (pageDirectory[tempCurrentDir].pageTableAddress == nullptr) {
			tempCounter -= MAX_PMT_ENTRIES - tempCurrentPage;
			tempCurrentPage = 0;
			tempCurrentDir++;
			if (tempCurrentDir == MAX_PMT_TABLES) {
				canAllocate = false;
				break;
			}
			continue;
		}
		else {
			pmt = pageDirectory[tempCurrentDir].pageTableAddress;
		}
		if(!pmt[tempCurrentPage].init){
			tempCurrentPage++;
			tempCounter--;
			if (tempCurrentPage ==  MAX_PMT_ENTRIES)
			{
				tempCurrentPage = 0;
				tempCurrentDir++;
				if (tempCurrentDir == MAX_PMT_TABLES) {
					canAllocate = false;
					break;
				}
			}	
			continue;
		} else {
			canAllocate = false;
			break;
		}
	}
	if (!canAllocate) {
		cout << "BUKI: NEMA PROSTORA ZA ALOKACIJU SEGMENTA" << endl;
		return Status::TRAP;
	}
	//REINITIALIZE VARIABLES
	//For counting how much more we need to allocate in case i need to go inside other dir
	tempCounter = segmentSize;
	//For going into next dir of PMT
	tempCurrentDir = dir;
	//For incrementing PMT entries inside PMT
	tempCurrentPage = page;

	//ALLOCATE AND INITIALIZE SEQUENTIAL ENTRIES
	
	//Pointer to current byte to transfer
	unsigned long bytesTransfered = 0;

	bool segmentSet = false;

	while (tempCounter > 0) {
		p_PageDescriptor pmt = nullptr;
		if (pageDirectory[tempCurrentDir].pageTableAddress == nullptr) {
			pmt = pageDirectory[tempCurrentDir].pageTableAddress = reinterpret_cast<p_PageDescriptor>( sys->firstFit(sys->getFreePMTChunks(), sizeof(PageDescriptor)*MAX_PMT_ENTRIES));
			for (int i = 0; i < MAX_PMT_ENTRIES; i++) {
				pmt[i].init = 0;
				pmt[i].valid = 0;
				pmt[i].dirty = 0;
				pmt[i].numOfPages = 0;
			}
			for (int i = tempCurrentPage; i < MAX_PMT_ENTRIES; i++) {
				if (!segmentSet) {
					pmt[tempCurrentPage].numOfPages = segmentSize;
					segmentSet = true;
				}
				pmt[i].init = 1;
				pmt[i].valid = 1;
				PhysicalAddress addr = sys->getFreeFrame(&pmt[i]);
				if (addr == nullptr) {
					cout << "BUKI: NEMA SLOBODNIH OKVIRA U MEMORIJI RACUNARA" << endl;
					cin;
					exit(1);
				}
				pmt[i].block = addr;
				pmt[i].rwe = flags;

				//INITIALIZE BLOCK WITH DATA
				char* memoryFrame = reinterpret_cast<char*>(addr);
				char* byteContent = reinterpret_cast<char*>(content);
				for (unsigned j = 0; j < PAGE_SIZE; j++) {
					memoryFrame[j] = byteContent[bytesTransfered++];
					if (bytesTransfered > segmentSize*PAGE_SIZE) {
						cout << "prelazim granicu bajtova" << endl;
						cin;
						exit(1);
					}
				}
				// INSERT INTO PAGE FIFO QUEUE, used for page replacement algorithm

				//If all pages loaded break
				tempCounter--;
				if (tempCounter == 0) {
					break;
				}
			}
			tempCurrentPage = 0;
			tempCurrentDir++;
			continue;
		}
		else {
			pmt = pageDirectory[tempCurrentDir].pageTableAddress;
		}
		if (!segmentSet) {
			pmt[tempCurrentPage].numOfPages = segmentSize;
			segmentSet = true;
		}
		pmt[tempCurrentPage].init = 1;
		pmt[tempCurrentPage].valid = 1;
		PhysicalAddress addr = sys->getFreeFrame(&pmt[tempCurrentPage]);
		if (addr == nullptr) {
			cout << "BUKI: NEMA SLOBODNIH OKVIRA U MEMORIJI" << endl;
			cin;
			exit(1);
		}
		pmt[tempCurrentPage].block = addr;
		pmt[tempCurrentPage].rwe = flags;
		tempCurrentPage++;
		tempCounter--;

		//INITIALIZE BLOCK WITH DATA
		char* memoryFrame = reinterpret_cast<char*>(addr);
		char* byteContent = reinterpret_cast<char*>(content);
		for(unsigned i=0; i < PAGE_SIZE; i++)
			memoryFrame[i] = byteContent[bytesTransfered++];
		//-------

		if (tempCurrentPage ==  MAX_PMT_ENTRIES)
		{
			tempCurrentPage = 0;
			tempCurrentDir++;
		}	
	}

	cout << "BUKI: Kreiran segment i inicijalizovani okviri u memoriji " << endl;

	return Status::OK;

	//cout << "NISAM IMPLEMENTIRAO loadSegment()" << endl;
	//cin;
	//exit(1);
	//return Status();
}

Status KernelProcess::deleteSegment(VirtualAddress startAddress)
{
	std::lock_guard<std::mutex> lock(mutex_pageDirectory);
	//Take data from VA
	unsigned dir = (startAddress & directoryMask) >> 17;
	unsigned page = (startAddress & pageMask) >> 10;
	unsigned offset = startAddress & offsetMask;
	if (offset != 0) {
		cout << "BUKI: NIJE PORAVNAT SEGMENT NA POCETAK STRANICE VRACAM TRAP" << endl;
		return Status::TRAP;
	}//For counting how much more we need to allocate in case i need to go inside other dir
	signed long tempCounter = 1; //Initialized to 1 to be able to enter the loop. It is initialized inside loop from descriptor->numOfPages
	//For going into next dir of PMT
	unsigned tempCurrentDir = dir;
	//For incrementing PMT entries inside PMT
	unsigned tempCurrentPage = page;

	//ALLOCATE AND INITIALIZE SEQUENTIAL ENTRIES
	
	//Pointer to current byte to transfer
	unsigned long bytesTransfered = 0;

	bool segmentSet = false;

	while (tempCounter > 0) {
		p_PageDescriptor pmt = nullptr;
		if (pageDirectory[tempCurrentDir].pageTableAddress == nullptr) {
			cout << "BUKI: Hoces da obrises segment koji ne postoji!" << endl;
			cin;
			exit(1);
		}
		else {
			pmt = pageDirectory[tempCurrentDir].pageTableAddress;
		}
		if (!segmentSet) {
			if (pmt[tempCurrentPage].numOfPages == 0) {
				cout << "BUKI: GRESKA POKUSAVAS DA OBRISES STRANICU SEGMENTA, NE KRECES OD GLAVE" << endl;
				cin;
				exit(1);
			}
			segmentSet = true;
		}

		sys->freePMTEntry(pmt[tempCurrentPage].block);

		pmt[tempCurrentPage].init = 0;
		pmt[tempCurrentPage].valid = 0;
		tempCurrentPage++;
		tempCounter--;

		if (tempCurrentPage ==  MAX_PMT_ENTRIES)
		{
			tempCurrentPage = 0;
			tempCurrentDir++;
		}	
	}
	//cout << "Nisam implementirao deleteSegment()" << endl;
	//cin;
	//exit(1);
	return Status::OK;
}

Status KernelProcess::pageFault(VirtualAddress address)
{
	std::lock_guard<std::mutex> lock(mutex_pageDirectory);
	//Take data from VA
	unsigned dir = (address & directoryMask) >> 17;
	unsigned page = (address & pageMask) >> 10;
	unsigned offset = address & offsetMask;
	
	//Buffer for loading page from disk
	char* buffer = new char[PAGE_SIZE];
	//Load page into buffer from disk
	Partition* partition = sys->getPartition();
	p_PageDirectory pDir = &pageDirectory[dir];
	p_PageDescriptor pDesc = &pDir->pageTableAddress[page];
	ClusterNo disk = pDesc->disk;
	partition->readCluster(disk, buffer);
	//Get new frame from kernel where to load paged out page
	PhysicalAddress freeFrame = sys->getFreeFrame(&pageDirectory[dir].pageTableAddress[page]);
	//Convert to byte pointer for iteration
	char* byteFrame = reinterpret_cast<char*>(freeFrame);
	//Write buffer into newly allocated frame
	for (unsigned i = 0; i < ClusterSize; i++) {
		byteFrame[i] = buffer[i];
	}
	//Set free loaded cluster from disk for others to use
	sys->setFreeCluster(pageDirectory[dir].pageTableAddress[page].disk);
	//Validate PMT entry
	pageDirectory[dir].pageTableAddress[page].block = freeFrame;
	pageDirectory[dir].pageTableAddress[page].valid = true;
	//cout << "Nisam implementirao pageFault()" << endl;
	//cin;
	//exit(1);
	static unsigned long numOfPageFaults = 0;
	//if (numOfPageFaults++ % 100 == 0) {
		//char msgbuf[100];
		//sprintf_s(msgbuf, "Proslo je: %d\n  page faultova do sad", numOfPageFaults);
		//OutputDebugStringA(msgbuf);
		numOfPageFaults += 1;	
		cout << "Proslo je: " << numOfPageFaults << " page faultova do sad" << endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	//}
	return Status::OK;
}

PhysicalAddress KernelProcess::getPhysicalAddress(VirtualAddress address)
{
	std::lock_guard<std::mutex> lock(mutex_pageDirectory);
	//Take data from VA
	unsigned dir = (address & directoryMask) >> 17;
	unsigned page = (address & pageMask) >> 10;
	unsigned offset = address & offsetMask;
	p_PageDescriptor pmt = pageDirectory[dir].pageTableAddress;
	if (pmt == nullptr) {
		cout << "BUKI: ACCESS VIOLATION" << endl;
		cin;
		exit(1);
	}
	if (!pmt[page].init) {
		cout << "BUKI: ACCESS VIOLATION" << endl;
		cin;
		exit(1);
	}
	if (!pmt[page].valid) {
		cout << "BUKI: PAGE FAULT" << endl;
		return nullptr;
	}
	return reinterpret_cast<PhysicalAddress>(reinterpret_cast<char*>(pmt[page].block) + offset);
}

void KernelProcess::setSystem(KernelSystem * sys)
{
	this->sys = sys;
	pageDirectory = reinterpret_cast<p_PageDirectory>(sys->firstFit(sys->getFreePMTChunks(), sizeof(PageDirectory)*MAX_PMT_TABLES));
	if (pageDirectory == nullptr) {
		cout << "BUKI: NE MOGU DA ALOCIRAM PROSTOR ZA pageDirectory za proces" << endl;
		cin;
		exit(1);
	}
	for (int i = 0; i < MAX_PMT_TABLES; i++) {
		pageDirectory[i].pageTableAddress = nullptr;
		pageDirectory[i].valid = false;
	}
}

p_PageDirectory KernelProcess::getPageDirectory()
{
	return this->pageDirectory;
}
