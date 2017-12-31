#include "KernelProcess.h"

#include <iostream>
using namespace std;

#define directoryMask 0xFE0000
#define pageMask 0x01FC00
#define offsetMask 0x0003FF

//VA(7:7:10)
#define MAX_PMT_TABLES 128 
#define MAX_PMT_ENTRIES 128

KernelProcess::KernelProcess(ProcessId pid) : mutex_pageDirectory(), listOfSharedSegments()
{
	this->pid = pid;
	
	cout << "Prosao konstruktor procesa: " << pid << endl;
}


KernelProcess::~KernelProcess()
{
	sys->removeProcess(this->pid);
}

ProcessId KernelProcess::getProcessId() const
{
	return pid;
}

Status KernelProcess::createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags)
{
	std::lock_guard<std::mutex> lock(mutex_pageDirectory);
	if (segmentSize == 0) {
		cout << "NE MOZES KREIRATI SEGMENT SA 0 STRANICA" << endl;
		exit(1);
	}

	//Take data from VA
	unsigned dir = (startAddress & directoryMask) >> 17;
	unsigned page = (startAddress & pageMask) >> 10;
	unsigned offset = startAddress & offsetMask;
	if (offset != 0) {
		cout << "NIJE PORAVNAT SEGMENT NA POCETAK STRANICE VRACAM TRAP" << endl;
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
		cout << "NEMA PROSTORA ZA ALOKACIJU SEGMENTA" << endl;
		exit(1);
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
				pmt[i].disk = 0;
			}
			for (int i = tempCurrentPage; i < MAX_PMT_ENTRIES; i++) {
				if (!segmentSet) {
					pmt[i].numOfPages = segmentSize;
					segmentSet = true;
				}
				pmt[i].init = 1;
				pmt[i].valid = 1;
				PhysicalAddress addr = sys->getFreeFrame(&pmt[i], this);
				if (addr == nullptr) {
					cout << " NEMA SLOBODNIH OKVIRA U MEMORIJI RACUNARA" << endl;
					cin;
					exit(1);
				}
				pmt[i].block = addr;
				pmt[i].rwe = flags;
				pmt[i].sourceAddress = nullptr;
				tempCounter--;
				if (tempCounter == 0)
					break;
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
		pmt[tempCurrentPage].dirty = 0;
		PhysicalAddress addr = sys->getFreeFrame(&pmt[tempCurrentPage], this);
		if (addr == nullptr) {
			cout << "NEMA SLOBODNIH OKVIRA U MEMORIJI" << endl;
			cin;
			exit(1);
		}
		pmt[tempCurrentPage].block = addr;
		pmt[tempCurrentPage].rwe = flags;
		pmt[tempCurrentPage].sourceAddress = nullptr;
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
		cout << " NE MOZES KREIRATI SEGMENT SA 0 STRANICA" << endl;
		exit(1);
	}

	//Take data from VA
	unsigned dir = (startAddress & directoryMask) >> 17;
	unsigned page = (startAddress & pageMask) >> 10;
	unsigned offset = startAddress & offsetMask;
	if (offset != 0) {
		cout << " NIJE PORAVNAT SEGMENT NA POCETAK STRANICE VRACAM TRAP" << endl;
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
		cout << "NEMA PROSTORA ZA ALOKACIJU SEGMENTA" << endl;
		exit(1);
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
				pmt[i].disk = 0;
			}
			for (int i = tempCurrentPage; i < MAX_PMT_ENTRIES; i++) {
				if (!segmentSet) {
					pmt[tempCurrentPage].numOfPages = segmentSize;
					segmentSet = true;
				}
				pmt[i].init = 1;
				pmt[i].valid = 1;
				PhysicalAddress addr = sys->getFreeFrame(&pmt[i],this);
				if (addr == nullptr) {
					cout << " NEMA SLOBODNIH OKVIRA U MEMORIJI RACUNARA" << endl;
					exit(1);
				}
				pmt[i].block = addr;
				pmt[i].rwe = flags;
				
				//INITIALIZE BLOCK WITH DATA
				char* memoryFrame = reinterpret_cast<char*>(addr);
				char* byteContent = reinterpret_cast<char*>(content);
				pmt[i].sourceAddress = reinterpret_cast<PhysicalAddress>(byteContent + bytesTransfered);
				for (unsigned j = 0; j < PAGE_SIZE; j++) {
					memoryFrame[j] = byteContent[bytesTransfered++];
					if (bytesTransfered > segmentSize*PAGE_SIZE) {
						cout << "prelazim granicu bajtova" << endl;
						exit(1);
					}
				}
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
		pmt[tempCurrentPage].dirty = 0;
		PhysicalAddress addr = sys->getFreeFrame(&pmt[tempCurrentPage], this);
		if (addr == nullptr) {
			cout << "NEMA SLOBODNIH OKVIRA U MEMORIJI" << endl;
			exit(1);
		}
		pmt[tempCurrentPage].block = addr;
		pmt[tempCurrentPage].rwe = flags; 
		tempCurrentPage++;
		tempCounter--;

		//INITIALIZE BLOCK WITH DATA
		char* memoryFrame = reinterpret_cast<char*>(addr);
		char* byteContent = reinterpret_cast<char*>(content);
		pmt[tempCurrentPage].sourceAddress = reinterpret_cast<PhysicalAddress>(byteContent + bytesTransfered);	
		for(unsigned i=0; i < PAGE_SIZE; i++)
			memoryFrame[i] = byteContent[bytesTransfered++];
		//-------

		if (tempCurrentPage ==  MAX_PMT_ENTRIES)
		{
			tempCurrentPage = 0;
			tempCurrentDir++;
		}	
	}


	return Status::OK;


}

Status KernelProcess::deleteSegment(VirtualAddress startAddress)
{
	std::lock_guard<std::mutex> lock(mutex_pageDirectory);
	//Take data from VA
	unsigned dir = (startAddress & directoryMask) >> 17;
	unsigned page = (startAddress & pageMask) >> 10;
	unsigned offset = startAddress & offsetMask;
	if (offset != 0) {
		cout << "NIJE PORAVNAT SEGMENT NA POCETAK STRANICE VRACAM TRAP" << endl;
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
			cout << "Hoces da obrises segment koji ne postoji!" << endl;
			exit(1);
		}
		else {
			pmt = pageDirectory[tempCurrentDir].pageTableAddress;
		}
		if (!segmentSet) {
			if (pmt[tempCurrentPage].numOfPages == 0) {
				cout << "GRESKA POKUSAVAS DA OBRISES STRANICU SEGMENTA, NE KRECES OD GLAVE" << endl;
				exit(1);
			}
			segmentSet = true;
		}

		sys->freePMTEntry(pmt[tempCurrentPage].block);

		pmt[tempCurrentPage].init = 0;
		pmt[tempCurrentPage].valid = 0;
		pmt[tempCurrentPage].dirty = 0;
		pmt[tempCurrentPage].block = 0;
		pmt[tempCurrentPage].disk = 0;
		pmt[tempCurrentPage].numOfPages = 0;
		pmt[tempCurrentPage].sourceAddress = 0;
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

Status KernelProcess::pageFault(VirtualAddress address)
{
	std::lock_guard<std::mutex> lock(mutex_pageDirectory);
	unsigned dir = (address & directoryMask) >> 17;
	unsigned page = (address & pageMask) >> 10;
	unsigned offset = address & offsetMask;
	
	//Check for access violation
	if (pageDirectory[dir].pageTableAddress[page].init == 0) {
		cout << "PAGE_FAULT EXCEPTION: ACCESS VIOLATION, INIT = 0" << endl;
		exit(1);
	}
	//Get new frame from kernel where to load paged out page
	//Here system pages out some other frame to make room for this page getFreeFrame -> replacePage () :)  , if there are no free frames
	PhysicalAddress freeFrame = sys->getFreeFrame(&pageDirectory[dir].pageTableAddress[page],this);

	//Buffer for loading page from disk or source address
	char* buffer = new char[PAGE_SIZE];

	if(pageDirectory[dir].pageTableAddress[page].disk == 0){
		//Just allocate new frame and return it if it was empty page created by createSegment(...)
		if (pageDirectory[dir].pageTableAddress[page].sourceAddress == nullptr) {
			//Validate PMT entry
			pageDirectory[dir].pageTableAddress[page].block = freeFrame;
			pageDirectory[dir].pageTableAddress[page].valid = true;	
			pageDirectory[dir].pageTableAddress[page].dirty = false;	
			static unsigned long numOfEmptyPageFaults = 0;
			numOfEmptyPageFaults++;
			cout << "               Proslo je: " << numOfEmptyPageFaults << " EMPTY page faultova do sad" << endl;
			return Status::OK;
		}
		//Load page into buffer from source address
		buffer = reinterpret_cast<char*>(pageDirectory[dir].pageTableAddress[page].sourceAddress);
		//Convert to byte pointer for iteration
		char* byteFrame = reinterpret_cast<char*>(freeFrame);
		//Write buffer into newly allocated frame
		for (unsigned i = 0; i < ClusterSize; i++) {
			byteFrame[i] = buffer[i];
		}

		pageDirectory[dir].pageTableAddress[page].dirty = false;	

		static unsigned long numOfSourcePageFaults = 0;
		numOfSourcePageFaults += 1;	
		cout << "               Proslo je: " << numOfSourcePageFaults << " SOURCE page faultova do sad" << endl;

	//cout << "				SOURCE ADDRESS PAGE FAULT! " << endl;
	}
	else {
		//Load page into buffer from disk
		Partition* partition = sys->getPartition();
		partition->readCluster(pageDirectory[dir].pageTableAddress[page].disk, buffer);
		
		//Convert to byte pointer for iteration
		char* byteFrame = reinterpret_cast<char*>(freeFrame);
		//Write buffer into newly allocated frame
		for (unsigned i = 0; i < ClusterSize; i++) {
			byteFrame[i] = buffer[i];
		}
		//Set free loaded cluster from disk for others to use
		sys->setFreeCluster(pageDirectory[dir].pageTableAddress[page].disk);
		
		pageDirectory[dir].pageTableAddress[page].dirty = true;	

		static unsigned long numOfDiskPageFaults = 0;
		numOfDiskPageFaults += 1;	
		cout << "               Proslo je: " << numOfDiskPageFaults << " DISK page faultova do sad" << endl;
	}
	//Validate PMT entry
	pageDirectory[dir].pageTableAddress[page].block = freeFrame;
	pageDirectory[dir].pageTableAddress[page].valid = true;	
	pageDirectory[dir].pageTableAddress[page].disk = 0;	

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
		cout << " ACCESS VIOLATION" << endl;
		exit(1);
	}
	if (!pmt[page].init) {
		cout << "ACCESS VIOLATION" << endl;
		exit(1);
	}
	if (!pmt[page].valid) {
		cout << "TRYING TO GET PA FROM VA WHERE valid=0 PAGE FAULT" << endl;
		//pageFault(address);
		exit(1);
	}
	return reinterpret_cast<PhysicalAddress>(reinterpret_cast<char*>(pmt[page].block) + offset);
}

void KernelProcess::setSystem(KernelSystem * sys)
{
	this->sys = sys;
	pageDirectory = reinterpret_cast<p_PageDirectory>(sys->firstFit(sys->getFreePMTChunks(), sizeof(PageDirectory)*MAX_PMT_TABLES));
	if (pageDirectory == nullptr) {
		cout << " NE MOGU DA ALOCIRAM PROSTOR ZA pageDirectory za proces" << endl;
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

Process * KernelProcess::clone(ProcessId pid)
{
	std::lock_guard<std::mutex> lock(mutex_pageDirectory);

	Process *clone = new Process(pid);
	clone->setSystem(sys);
	
	KernelProcess *kProc = clone->pProcess;
	//Copy of fields
	kProc->sys = sys;
	//Clone the virtual address space (tables)
	for (p_SharedSegment ss : listOfSharedSegments) {
		kProc->listOfSharedSegments.emplace_back(ss);
	}
	//For going into next dir of PMT
	unsigned long tempCurrentDir = 0;

	//COPY SEQUENTIAL ENTRIES
	while (true) {

		p_PageDescriptor pmt = nullptr;
		p_PageDescriptor pmt_clone = nullptr;

		if (pageDirectory[tempCurrentDir].pageTableAddress == nullptr) {
			if (++tempCurrentDir == MAX_PMT_TABLES)
				break;
			continue;
		} else {
			pmt = pageDirectory[tempCurrentDir].pageTableAddress;
			pmt_clone = kProc->pageDirectory[tempCurrentDir].pageTableAddress = reinterpret_cast<p_PageDescriptor>(sys->firstFit(sys->getFreePMTChunks(), sizeof(PageDescriptor)*MAX_PMT_ENTRIES));
			for (int i = 0; i < MAX_PMT_ENTRIES; i++) {
				pmt_clone[i].init = pmt[i].init;
				if (!pmt_clone[i].init) {
					if (++tempCurrentDir == MAX_PMT_TABLES)
						break; 
					continue;
				}
				pmt_clone[i].valid = pmt[i].valid; 
				pmt_clone[i].dirty = pmt[i].dirty;
				pmt_clone[i].rwe = pmt[i].rwe;	
				pmt_clone[i].block = pmt[i].block;
				pmt_clone[i].disk = pmt[i].disk; 
				pmt_clone[i].numOfPages = pmt[i].numOfPages;
				pmt_clone[i].sourceAddress = pmt[i].sourceAddress;
				if (pmt_clone[i].valid) {
					pmt_clone[i].block = sys->getFreeFrame(&pmt[i], this);
					//Copy page
					char* clone_block = reinterpret_cast<char*>(pmt_clone[i].block);
					char* block = reinterpret_cast<char*>(pmt[i].block);
					for (int i = 0; i < PAGE_SIZE; i++)
						clone_block[i] = block[i];
				}
				if (!pmt_clone[i].valid && pmt_clone[i].disk != 0) {
					ClusterNo newCluster = sys->getFreeCluster();
					char* buffer = new char[PAGE_SIZE];
					sys->getPartition()->readCluster(pmt_clone[i].disk, buffer);
					sys->getPartition()->writeCluster(newCluster, buffer);
					pmt_clone[i].disk = newCluster;
					delete buffer;
				}
			}	
			if (++tempCurrentDir == MAX_PMT_TABLES)
				break;
		}
	}
	return clone;
}

Status KernelProcess::createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char * name, AccessType flags)
{
	std::lock_guard<std::mutex> lock(mutex_pageDirectory);

	//Try to find the existing shared segment if it exists and use it, otherwise create it
	p_SharedSegment ss = sys->getSharedSegment(name);

	//Allocating virtual address space from existing shared segment but starting from my virtual address	
	if (ss != nullptr) {
		
		
		if (segmentSize == 0) {
			cout << "NE MOZES KREIRATI SEGMENT SA 0 STRANICA" << endl;
			exit(1);
		}
		if (flags != ss->flags) {
			cout << "ZAHTEVAM DELJENI SEGMENT SA PRAVIMA PRISTUPA DRUGACIJIM OD SVOJIH" << endl;
			exit(1);
		}

		PageNum maxSizeSegment = 0;
		//Determine the max sizeSegment
		if (segmentSize > ss->segmentSize) {
			maxSizeSegment = ss->segmentSize;
		}
		else
			maxSizeSegment = segmentSize;

		//Take data from SS VA
		unsigned dir_ss = (ss->startAddress & directoryMask) >> 17;
		unsigned page_ss = (ss->startAddress & pageMask) >> 10;
		unsigned offset_ss = ss->startAddress & offsetMask;
		if (offset_ss != 0) {
			cout << "NIJE PORAVNAT SEGMENT NA POCETAK STRANICE VRACAM TRAP" << endl;
			return Status::TRAP;
		}

		//Take data from VA
		unsigned dir = (startAddress & directoryMask) >> 17;
		unsigned page = (startAddress & pageMask) >> 10;
		unsigned offset = startAddress & offsetMask;
		if (offset != 0) {
			cout << "NIJE PORAVNAT SEGMENT NA POCETAK STRANICE VRACAM TRAP" << endl;
			return Status::TRAP;
		}

		//Find if it has continuous segmentSize free entries inside PMT for segment allocation

		//Flag that tells if we can allocate segment
		bool canAllocate = true;
		
		//For going into next dir of PMT
		unsigned tempCurrentDir = dir;
		//For incrementing PMT entries inside PMT
		PageNum tempCurrentPage = page;

		//For going into next dir of PMT
		unsigned tempCurrentDir_ss = dir_ss;
		//For incrementing PMT entries inside PMT
		PageNum tempCurrentPage_ss = page_ss;


		//Iterate through lower number of pages
		signed long tempCounter = maxSizeSegment;

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
			cout << "NEMA PROSTORA ZA ALOKACIJU SEGMENTA" << endl;
			exit(1);
			return Status::TRAP;
		}

		//REINITIALIZE VARIABLES
		//For counting how much more we need to allocate in case i need to go inside other dir
		tempCounter = maxSizeSegment;
		//For going into next dir of PMT
		tempCurrentDir = dir;
		//For incrementing PMT entries inside PMT
		tempCurrentPage = page;
		//For specifying how long the segment was only in first descriptor
		bool segmentSet = false;

		//ALLOCATE SEQUENTIAL ENTRIES

		//Get process from which virtual addresses will be copied
		Process* ss_p = sys->getProcess(ss->pid);
		if (ss_p == nullptr) {
			cout << "Greska ss_p je nullptr" << endl;
			exit(1);
		}
		//Get pageDirectory from which virtual addresses will be copied
		p_PageDirectory pageDirectory_ss = ss_p->getPageDirectory();
		
		while (tempCounter > 0) {

			p_PageDescriptor pmt = nullptr;
			p_PageDescriptor pmt_ss = pageDirectory_ss[tempCurrentDir_ss].pageTableAddress;

			if (pageDirectory[tempCurrentDir].pageTableAddress == nullptr) {

				pmt = pageDirectory[tempCurrentDir].pageTableAddress = reinterpret_cast<p_PageDescriptor>( sys->firstFit(sys->getFreePMTChunks(), sizeof(PageDescriptor)*MAX_PMT_ENTRIES));
				
				for (int i = 0; i < MAX_PMT_ENTRIES; i++) {
					pmt[i].init = 0;
					pmt[i].valid = 0;
					pmt[i].dirty = 0;
					pmt[i].numOfPages = 0;
					pmt[i].disk = 0;
				}

				for (int i = tempCurrentPage; i < MAX_PMT_ENTRIES; i++) {

					if (!segmentSet) {
						pmt[i].numOfPages = segmentSize;
						segmentSet = true;
					}

					pmt[i].init = pmt_ss[tempCurrentPage_ss].init;
					pmt[i].valid = pmt_ss[tempCurrentPage_ss].valid;
					
					pmt[i].block = pmt_ss[tempCurrentPage_ss].block;
					pmt[i].rwe = pmt_ss[tempCurrentPage_ss].rwe;
					pmt[i].sourceAddress = pmt_ss[tempCurrentPage_ss].sourceAddress;
					cout << "COPIED SHARED SEGMENT MAP : Process[" << pid << "] FRAME: " << pmt[i].block << endl;
					if (tempCurrentPage_ss++ == MAX_PMT_ENTRIES) {
						tempCurrentPage_ss = 0;
						tempCurrentDir_ss++;
					}

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
			pmt[tempCurrentPage].init = pmt_ss[tempCurrentPage_ss].init;
			pmt[tempCurrentPage].valid = pmt_ss[tempCurrentPage_ss].valid;
			pmt[tempCurrentPage].dirty = pmt_ss[tempCurrentPage_ss].dirty;
			pmt[tempCurrentPage].block = pmt_ss[tempCurrentPage_ss].block;
			cout << "COPIED SHARED SEGMENT MAP : Process[" << pid << "] FRAME: " << pmt[tempCurrentPage].block << endl;
			pmt[tempCurrentPage].rwe = pmt_ss[tempCurrentPage_ss].rwe;
			pmt[tempCurrentPage].sourceAddress = pmt_ss[tempCurrentPage_ss].sourceAddress;
			tempCurrentPage++;
			tempCurrentPage_ss++;
			tempCounter--;
			if (tempCurrentPage ==  MAX_PMT_ENTRIES)
			{
				tempCurrentPage = 0;
				tempCurrentDir++;
			}	
			if (tempCurrentPage_ss ==  MAX_PMT_ENTRIES)
			{
				tempCurrentPage_ss = 0;
				tempCurrentDir_ss++;
			}	
		}

		SharedSegment *sharedSeg = reinterpret_cast<p_SharedSegment>(sys->firstFit(sys->getFreePMTChunks(), sizeof(SharedSegment)));
		sharedSeg->flags = flags;
		sharedSeg->name = name;
		sharedSeg->segmentSize = maxSizeSegment;
		sharedSeg->startAddress = ss->startAddress;
		sharedSeg->pid = ss->pid;
		sharedSeg->counter++;

		std::lock_guard<std::mutex> lock(mutex_listOfSharedSegments);
		listOfSharedSegments.emplace_back(std::move(sharedSeg));

		cout << "Created New Shared Segment from existing: " << name << endl;

	} else {   //Creating new shared segment into the system

		
		if (segmentSize == 0) {
			cout << " NE MOZES KREIRATI SEGMENT SA 0 STRANICA" << endl;
			exit(1);
		}

		//Take data from VA
		unsigned dir = (startAddress & directoryMask) >> 17;
		unsigned page = (startAddress & pageMask) >> 10;
		unsigned offset = startAddress & offsetMask;
		if (offset != 0) {
			cout << " NIJE PORAVNAT SEGMENT NA POCETAK STRANICE VRACAM TRAP" << endl;
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
			if (!pmt[tempCurrentPage].init) {
				tempCurrentPage++;
				tempCounter--;
				if (tempCurrentPage == MAX_PMT_ENTRIES)
				{
					tempCurrentPage = 0;
					tempCurrentDir++;
					if (tempCurrentDir == MAX_PMT_TABLES) {
						canAllocate = false;
						break;
					}
				}
				continue;
			}
			else {
				canAllocate = false;
				break;
			}
		}

		if (!canAllocate) {
			cout << "NEMA PROSTORA ZA ALOKACIJU SEGMENTA" << endl;
			exit(1);
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
				pmt = pageDirectory[tempCurrentDir].pageTableAddress = reinterpret_cast<p_PageDescriptor>(sys->firstFit(sys->getFreePMTChunks(), sizeof(PageDescriptor)*MAX_PMT_ENTRIES));
				for (int i = 0; i < MAX_PMT_ENTRIES; i++) {
					pmt[i].init = 0;
					pmt[i].valid = 0;
					pmt[i].dirty = 0;
					pmt[i].numOfPages = 0;
					pmt[i].disk = 0;
				}
				for (int i = tempCurrentPage; i < MAX_PMT_ENTRIES; i++) {
					if (!segmentSet) {
						pmt[i].numOfPages = segmentSize;
						segmentSet = true;
					}
					pmt[i].init = 1;
					pmt[i].valid = 1;
					PhysicalAddress addr = sys->getFreeFrame(&pmt[i], this);
					if (addr == nullptr) {
						cout << "NEMA SLOBODNIH OKVIRA U MEMORIJI RACUNARA" << endl;
						cin;
						exit(1);
					}
					cout << "SOURCE SHARED SEGMENT MAP : Process[" << pid << "] FRAME: " << addr << endl;
					pmt[i].block = addr;
					pmt[i].rwe = flags;
					pmt[i].sourceAddress = nullptr;
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
			pmt[tempCurrentPage].dirty = 0;
			PhysicalAddress addr = sys->getFreeFrame(&pmt[tempCurrentPage], this);
			if (addr == nullptr) {
				cout << " NEMA SLOBODNIH OKVIRA U MEMORIJI" << endl;
				cin;
				exit(1);
			}
			pmt[tempCurrentPage].block = addr;
			cout << "SOURCE SHARED SEGMENT MAP : Process[" << pid << "] FRAME: " << addr << endl;
			pmt[tempCurrentPage].rwe = flags;
			pmt[tempCurrentPage].sourceAddress = nullptr;
			tempCurrentPage++;
			tempCounter--;
			if (tempCurrentPage == MAX_PMT_ENTRIES)
			{
				tempCurrentPage = 0;
				tempCurrentDir++;
			}
		}

		SharedSegment *sharedSeg = reinterpret_cast<p_SharedSegment>(sys->firstFit(sys->getFreePMTChunks(), sizeof(SharedSegment)));
		sharedSeg->name = name;
		sharedSeg->flags = flags;
		sharedSeg->startAddress = startAddress;
		sharedSeg->segmentSize = segmentSize;
		sharedSeg->pid = pid;
		sharedSeg->counter = 1;

		std::lock_guard<std::mutex> lock(mutex_listOfSharedSegments);
		listOfSharedSegments.emplace_back(std::move(sharedSeg));
		sys->insertSharedSegment(sharedSeg);

		cout << "Created Shared Segment: " << name << endl;

	}

	return Status::OK;
}

Status KernelProcess::disconnectSharedSegment(const char * name)
{
	std::lock_guard<std::mutex> lock(mutex_listOfSharedSegments);
	for(p_SharedSegment ss : listOfSharedSegments)
		if (ss->name == name) {
			p_SharedSegment gss = sys->getSharedSegment(name);
			if (gss != nullptr) {
				//If i was the creator of this segment remove it globaly
				if (gss->pid == pid) {
					sys->deleteSharedSegment(gss, pid);
					//Delete segment from descriptors in table
					deleteSegment(ss->startAddress);
					//Remove segment from list 
					listOfSharedSegments.remove(ss); 
					return Status::OK;
				}
				//Otherwise just disconnect
				gss->counter--;
				//Delete segment from descriptors in table
				deleteSegment(ss->startAddress);
				//Remove segment from list 
				listOfSharedSegments.remove(ss); 
				return Status::OK;
			}
			else {
				cout << "Greska: pokusavam da obrisem deljeniSegment koji ne postoji u globalnoj listi sistema " << endl;
				exit(1);
			}
			
		}
	return Status::TRAP;
}

Status KernelProcess::deleteSharedSegment(const char * name, bool systemCall)
{
	std::lock_guard<std::mutex> lock(mutex_listOfSharedSegments);
	for (p_SharedSegment ss : listOfSharedSegments)
		if (ss->name == name) {
			p_SharedSegment gss = sys->getSharedSegment(name);
			if (gss != nullptr) {
				//First remove it from global list to stop recursive calls from deleteSharedSegment and disconnectSharedSegment, Go through all processes and delete shared segment
				if (!systemCall) {
					//exit(1);
					sys->deleteSharedSegment(gss, pid);
				}
				
			}
			//Delete segment from descriptors in table
			deleteSegment(ss->startAddress);
			//Remove segment from list and delete it from memory
			listOfSharedSegments.remove(ss);
			return Status::OK;
		}
	return Status::TRAP;
}

