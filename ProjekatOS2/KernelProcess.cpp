#include "KernelProcess.h"

#include <iostream>
using namespace std;

#define directoryMask 0xFE0000
#define pageMask 0x01FC00
#define offsetMask 0x0003FF

//VA(7:7:10)
#define MAX_PMT_TABLES 128 
#define MAX_PMT_ENTRIES 128

KernelProcess::KernelProcess(ProcessId pid)
{
	this->pid = pid;
	pageDirectory = reinterpret_cast<p_PageDirectory>(sys->firstFit(sys->getFreePMTChunks(), sizeof(PageDirectory)*MAX_PMT_TABLES));
	if (pageDirectory == nullptr) {
		cout << "BUKI: NE MOGU DA ALOCIRAM PROSTOR ZA pageDirectory za proces" << endl;
		exit(1);
	}
	for (int i = 0; i < MAX_PMT_TABLES; i++) {
		pageDirectory[i].pageTableAddress = nullptr;
		pageDirectory[i].valid = false;
	}
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
	if (segmentSize == 0) {
		cout << "BUKI: NE MOZES KREIRATI SEGMENT SA 0 STRANICA" << endl;
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
	PageNum tempCounter = segmentSize;
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
	//ALLOCATE SEQUENTIAL ENTRIES
	while (tempCounter > 0) {
		p_PageDescriptor pmt = nullptr;
		if (pageDirectory[tempCurrentDir].pageTableAddress == nullptr) {
			pmt = pageDirectory[tempCurrentDir].pageTableAddress = reinterpret_cast<p_PageDescriptor>( sys->firstFit(sys->getFreePMTChunks(), sizeof(PageDescriptor)*MAX_PMT_ENTRIES));
			for (int i = 0; i < MAX_PMT_ENTRIES; i++) {
				pmt[i].init = 0;
				pmt[i].valid = 0;
				pmt[i].dirty = 0;
			}
			for (int i = tempCurrentPage; i < MAX_PMT_ENTRIES; i++) {
				pmt[i].init = 1;
				pmt[i].valid = 1;
				PhysicalAddress addr = sys->getFreeFrame(this->pid);
				if (addr == nullptr) {
					cout << "BUKI: NEMA SLOBODNIH OKVIRA U MEMORIJI RACUNARA" << endl;
					exit(1);
				}
				pmt[i].block = addr;
				pmt[i].rwe = flags;
			}
			tempCounter -= MAX_PMT_ENTRIES - tempCurrentPage;
			tempCurrentPage = 0;
			tempCurrentDir++;
			continue;
		}
		else {
			pmt = pageDirectory[tempCurrentDir].pageTableAddress;
		}
		pmt[tempCurrentPage].init = 1;
		pmt[tempCurrentPage].valid = 1;
		PhysicalAddress addr = sys->getFreeFrame(this->pid);
		if (addr == nullptr) {
			cout << "BUKI: NEMA SLOBODNIH OKVIRA U MEMORIJI" << endl;
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
	return Status();
}

Status KernelProcess::deleteSegment(VirtualAddress startAddress)
{
	return Status();
}

Status KernelProcess::pageFault(VirtualAddress address)
{
	return Status();
}

PhysicalAddress KernelProcess::getPhysicalAddress(VirtualAddress address)
{
	//Take data from VA
	unsigned dir = (address & directoryMask) >> 17;
	unsigned page = (address & pageMask) >> 10;
	unsigned offset = address & offsetMask;
	p_PageDescriptor pmt = pageDirectory[dir].pageTableAddress;
	if (pmt == nullptr) {
		cout << "BUKI: ACCESS VIOLATION" << endl;
		exit(1);
	}
	if (!pmt[page].init) {
		cout << "BUKI: ACCESS VIOLATION" << endl;
		exit(1);
	}
	if (!pmt[page].valid) {
		cout << "BUKI: PAGE FAULT" << endl;
		return nullptr;
	}
	return pmt[page].block;
}

void KernelProcess::setSystem(KernelSystem * sys)
{
	this->sys = sys;
}
