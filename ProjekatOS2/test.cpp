//TEST PROGRAM
// PAGE = 1KB , ARCH = x86 => WORD = 1B (NOT FOR SURE BECAUSE DEPENDS ON CPU ARCH, THAT IS THE SIZE OF THE CPU REGS)
// => WORDS PER PAGE = PAGE / WORD = 2^10/2^0 = 2^10 words per page
// VA : PAGE(14)_WORD(10) 
//		=> PAGES = 2^14 = 16384
//		=> WORDS IN PAGE = 2^10 = 1024 
// PHYSICAL MEMORY CAN BE UP TO 2^64 ADDRESSES CAUSE OF x64 ARCHITECTURE OR UP TO 2^32 ADDRESSES CAUSE OF X86 ARCHITECTURE
// => POINTERS ARE 4 BYTES => WE NEED TO COMPILE IN x86 MODE IN VISUAL STUDIO BECAUSE OF part.lib IS x86 library

#include <cstdlib>
#include <math.h>

#include "System.h"
#include "Process.h"
#include "part.h"
#include "vm_declarations.h"

#include <iostream>

using namespace std;

System *kernelSystem; //Global system reference only one instance of system, otherwise memory overlapping

int main(int argc, char** argv){
	
	PageNum VMSpaceSize = pow(2,10); // 1K PAGES
	PhysicalAddress VMSpace = (PhysicalAddress)malloc(VMSpaceSize*PAGE_SIZE); //ALLOCATING EMPTY MEMORY FOR 1K PAGES OF SIZE 1KB
	
	PageNum pmtSpaceSize = 100;
	PhysicalAddress pmtSpace = (PhysicalAddress)malloc(pmtSpaceSize*PAGE_SIZE); //ALLOCATING 100 PAGES OF SIZE 1KB FOR PMT TABLES 
	
	Partition* partition = new Partition("p1.ini");
	
	kernelSystem = new System(VMSpace,
							  VMSpaceSize,
							  pmtSpace,
							  pmtSpaceSize,
							  partition);
	
	Process *p1 = kernelSystem->createProcess();
	Process *p2 = kernelSystem->createProcess();
	Process *p3 = kernelSystem->createProcess();
	
	VirtualAddress VA1 = 0x000003;	//Page(0)Word(3) => Frame(?)Word(3)
	VirtualAddress VA2 = 0x000405;	//Page(1)Word(5) => Frame(?)Word(5)
	VirtualAddress VA3 = 0x000406;	//Page(1)Word(6) => Frame(?)Word(6)
	VirtualAddress VA4 = 0x000007;	//Page(0)Word(7) => Frame(?)Word(7)
	VirtualAddress VA5 = 0x000808;	//Page(2)Word(8) => Frame(?)Word(8)
	
	VirtualAddress SA1 = 0x000000; //Size of one page till SA2
	VirtualAddress SA2 = 0x000080; //Size of one page till SA3
	VirtualAddress SA3 = 0x000100; //Size of one page till SA4
	VirtualAddress SA4 = 0x000180; 
	
	p1->createSegment(SA1,3,READ);	//Allocates 1 Segment of 3 pages
						      //and READ, WRITE, EXECUTE privilegies
							  //that means that SA2 and SA3 addresses
							  //will not be initialized after SA1 if possible
							  //because they will be taken so they will map
							  //somewhere else in physicall memory
	p1->createSegment(SA2,4,WRITE);
	
	p2->createSegment(SA1,2,READ);
	p2->createSegment(SA2,4,WRITE);

	delete partition;
	delete kernelSystem;
	delete VMSpace;
	delete pmtSpace;
	
	cout << VMSpace << endl;
	cout << pmtSpace << endl;
	long* pa = (long*)pmtSpace;
	cout << ++pa << endl;
	
	return 0;
}
