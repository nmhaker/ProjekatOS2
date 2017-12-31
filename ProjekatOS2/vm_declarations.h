#pragma once

//#include "part.h"
#include <mutex>

class KernelProcess;

typedef unsigned long PageNum;
typedef unsigned long VirtualAddress;
typedef void* PhysicalAddress;
typedef unsigned long Time;

enum Status { OK, PAGE_FAULT, TRAP };

enum AccessType { READ, WRITE, READ_WRITE, EXECUTE };

typedef unsigned ProcessId;

#define PAGE_SIZE 1024

//MY DECLARATIONS

struct FreeChunk {
	PhysicalAddress startingAddress;
	unsigned long size;
};

struct PageDescriptor{
	bool init;
	bool valid;
	bool dirty;
	AccessType rwe;
	PageNum numOfPages;
	PhysicalAddress block;
	unsigned long disk; //ClusterNo = unsigned long
	PhysicalAddress sourceAddress;
};
typedef PageDescriptor* p_PageDescriptor;


struct PageDirectory {
	bool valid;
	p_PageDescriptor pageTableAddress;
	unsigned long diskAddress; //ClusterNo = unsigned long
};
typedef PageDirectory* p_PageDirectory;

struct SysPageDescriptor {
	bool free;
	p_PageDescriptor pageDescr;
	KernelProcess* proc;
};
typedef SysPageDescriptor* p_SysPageDescriptor;
	
struct SysDiskDescriptor {
	bool free;
};
typedef SysDiskDescriptor* p_SysDiskDescriptor;

struct SharedSegment {
	VirtualAddress startAddress;
	PageNum segmentSize;
	const char* name;
	AccessType flags;
	ProcessId pid;
	unsigned long counter;
};
typedef SharedSegment* p_SharedSegment;

