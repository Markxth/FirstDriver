/*
Some notes for the following code, to not fill the code with comments.

Vocab: 
DPFLTR = Debug Print Filter. It is a filter that allows you to filter out debug messages based on the component and level of the message
CSPTR = const string pointer. A macro for const char* 


Concepts : For debug_print, since I don't want it to be compiled, I did #ifndef DEBUG, where DEBUG is a preprocessor symbol that would be in the project settings.

*/

#include<ntifs.h> 

#define NT_SUCCESS(Status)  (((NTSTATUS)(Status)) >= 0)

//forward declarations

extern "C" { //extern for dealing with name-mangling

//notes : NTKERNELAPI is a macro that lets the compiler know that the function is a Kernel API import/export, as in, provided by Windows. NTSTATUS will be the return type of the function.
	NTKERNELAPI NTSTATUS IoCreateDriver( //make IOCTL driver compatible with kdmapper
		PUNICODE_STRING driver_name,
		PDRIVER_INITIALIZE InitFunc);

	NTKERNELAPI NTSTATUS MmCopyVirtualMemory( //implement RW process memory in our driver ; 
		PEPROCESS SourceProc,		
		PVOID SourceAddy,
		PEPROCESS TargetProc,
		PVOID TargetAddy,
		SIZE_T bufferSize,
		KPROCESSOR_MODE ProcessMode,
		PSIZE_T RetSize
	);
}
void debug_print(PCSTR text); { //where PCSTR is equivalent to const char* .
	//because KdPrintEx is lengthy, we will make a wrapper function for it.
#ifndef DEBUG
	UNREFERENCED_PARAMETER(text); 
#endif 

	KdPrintEx((
		DPFLTR_IHDRIVER_ID,
		DPFLTR_INFO_LEVEL,
		text 
		) //it has to be called with 2 () 
	); 
}
//note : only kernel mode drivers can call this as per NtDocs' remarks


//driver namespace. Since the Windows Kernel is enormous, we need to def our own namespaces, to not risk having a var with the same name as another var
namespace driver {

	namespace code {
		//for attaching to a process. This comes before r/w
		constexpr ULONG attach = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS); 
			//constexpr because CTL_CODE itself  returns a const

		//note : CTL_CODE is used for defining codes which are 32-bit integers that are passed by  a umode app to the driver via DeviceIoControl 
		//CTL_CODE(DeviceType, function ( 0x800 and above. the rest are microsoft's), Method, Access)
		constexpr ULONG read = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		constexpr ULONG write = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

	}

	//structs for user mode and kernel mode - shared
	struct Request {
		HANDLE process_id; //proc we try hack

		PVOID buffer;
		PVOID target_id; //set buffer ro r/w proc memo. Has to do with MmCopy...  

		SIZE_T size; 
		SIZE_T return_size; 

	};

	NTSTATUS create(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object); 
		IoCompleteRequest(irp, IO_NO_INCREMENT); 

		return irp->IoStatus.Status; 
	}

	NTSTATUS close(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);
		IoCompleteRequest(irp, IO_NO_INCREMENT);

		return irp->IoStatus.Status; 
	}

	NTSTATUS device_control(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);
		debug_print("Device control called as an unreferenced parameter. \n "); 
		
		NTSTATUS status = STATUS_UNSUCCESSFUL;
		PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp); //aka get the pointer to the driver's specific frame. IRP = IO Request Packet

		auto request = reinterpret_cast<Request*>(irp->AssociatedIrp.SystemBuffer); //access request obj. sent from user-mode

		// against the blue screens
		if (stack_irp == nullptr || request == nullptr) {
			irp->IoStatus.Status = STATUS_INVALID_PARAMETER; 
			irp->IoStatus.Information = 0; 
			IoCompleteRequest(irp, IO_NO_INCREMENT);
			return status; 
		}

		static PEPROCESS target_process = nullptr; 

		const ULONG control_code = stack_irp->Parameters.DeviceIoControl.IoControlCode; 

		switch (control_code) {

		case code::attach : 
			status = PsLookupProcessByProcessId(request->process_id, &target_process); 
			break;

		case code::read :
			if (target_process != nullptr) {
				status = MmCopyVirtualMemory(
					target_process,
					request->target_id,
					PsGetCurrentProcess(), //current process
					request->buffer,
					request->size,
					KernelMode,
					&request-> return_size
				); 
			}
			break;

		case code::write : 
			if (target_process != nullptr) {
				status = MmCopyVirtualMemory(
					PsGetCurrentProcess(),
					request->buffer,
					target_process, //current process
					request->target_id,
					request->size,
					KernelMode,
					&request->return_size
				);
			}
			break; 
		default :
			break; 
		}

		irp->IoStatus.Status = status; 

		if (NT_SUCCESS(status)) {
			irp->IoStatus.Information = sizeof(Request); 
		}
		else {
			irp->IoStatus.Information = 0; 
		}

		IoCompleteRequest(irp, IO_NO_INCREMENT); 

		return irp->IoStatus.Status;
	}
}

void debug_print(PCSTR text) {
#ifdef DEBUG 
	UNREFERENCED_PARAMETER(text); 
#endif //

	KdPrintEx((
		DPFLTR_IHVDRIVER_ID, 
		DPFLTR_INFO_LEVEL,
		text
	)); //note the double () , that is because this is a macro, and it works only with debug tools, and not with a typical console.
}

NTSTATUS DriverMain(PDRIVER_OBJECT driverObject, PUNICODE_STRING registre_path) {
	UNREFERENCED_PARAMETER(registre_path); //if we do not use something and do not explicitly say it is unreferenced, it does not comppile.
	
	//driver Iocode

	UNICODE_STRING drivername = {}; 
	RtlInitUnicodeString(&drivername, L"\\Driver\\KMDF Driver1"); 

	//device object

	PDEVICE_OBJECT dev_obj = nullptr;
	NTSTATUS status = IoCreateDevice(
		driverObject,
		0,
		&drivername, //pass the PUNICODE_STRING 
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&dev_obj
	); 

	if (status != STATUS_SUCCESS) {
		debug_print("Could not a driver device\n");
		return status;
	} 
	
	debug_print("Created a driver object!\n"); 

	//symbolic link time
	PUNICODE_STRING symbolic_link = nullptr; 
	RtlInitUnicodeString(symbolic_link, L"\\DosDevices\\KMDF Driver1"); 

	status = IoCreateSymbolicLink(symbolic_link, &drivername); 
		if (status != STATUS_SUCCESS) {
			debug_print("Failed to create a symbolic link!"); 
	}

	debug_print("Created a symbolic link!"); 


	//IOCTL Setup. 
	//1. Buffered IO
	
	SetFlag(dev_obj->Flags, DO_BUFFERED_IO); 

	//handlers for driver object
	driverObject->MajorFunction[IRP_MJ_CREATE] = driver::create; 
	driverObject->MajorFunction[IRP_MJ_CLOSE] = driver::close;
	driverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = driver::device_control; 
	
	ClearFlag(dev_obj->Flags, DO_DEVICE_INITIALIZING); 

	debug_print("Symbolic link set!"); 

	return status ; 
}

NTSTATUS DriverEntry() {
	debug_print("a"); 

	UNICODE_STRING driver = {}; 
	RtlInitUnicodeString(&driver, L"\\Driver\\KMDF Driver1"); // kernels do not like const char. 

	return IoCreateDriver(&driver, &DriverMain); 

}