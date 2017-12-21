#pragma once

//#include "part.h"

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

struct SysPageDescriptor {
	bool free;
	ProcessId pid;
};
typedef SysPageDescriptor* p_SysPageDescriptor;
	
struct SysDiskDescriptor {
	bool free;
	ProcessId pid;
};
typedef SysDiskDescriptor* p_SysDiskDescriptor;

struct PageDescriptor{
	bool init;
	bool valid;
	bool dirty;
	AccessType rwe;
	PageNum numOfPages;
	PhysicalAddress block;
	unsigned long disk; //ClusterNo = unsigned long
};
typedef PageDescriptor* p_PageDescriptor;


struct PageDirectory {
	bool valid;
	p_PageDescriptor pageTableAddress;
	unsigned long diskAddress; //ClusterNo = unsigned long
};
typedef PageDirectory* p_PageDirectory;
