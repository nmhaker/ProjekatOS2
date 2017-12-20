System
	-System(PhysicalAddress processVMSpace, 
			PageNum processVMSpaceSize,
			PhysicalAddress pmtSpace,
			PageNum pmtSpaceSize,
			Partition* partition)
	-Process* createProcess();
	-Time periodicJob();
	-Status access(ProcessId pid,
				   VirtualAddress address,
				   AccessType type)
	
Process
	-Process(ProcessId pid)
	-ProcessId getProcessId() const;
	-Status createSegment(VirtualAddress startAddress,
						  PageNum segmentSize,
						  AccessRight flags)
	-Status loadSegment(VirtualAddress startAddress,
						PageNum segmentSize,
						AccessRight flags,
						void* content)
	-Status deleteSegment(VirtualAddress startAddress)
	-Status pageFault(VirtualAddress address)
	-PhysicalAddress getPhysicalAddress(VirtualAddress address)
	