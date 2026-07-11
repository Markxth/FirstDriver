#include <iostream> 
#include <Windows.h> 
#include<TlHelp32.h>
#include <cstdint>

static DWORD get_process_id(const wchar_t* process_name) {

	DWORD pid = 0;

	HANDLE snapshot = CreateToolhelp32Snapshot(
		TH32CS_SNAPPROCESS,
		NULL
	);


	if (snapshot == INVALID_HANDLE_VALUE) {
		std::cout << "Could not create a snapshot";
		return pid;
	}
	else {
		std::cout << "Snapshot taken.";
	}
		
	PROCESSENTRY32W entry = {};  //zero-initialise to clean up struct in case of garbage
	entry.dwSize = sizeof(decltype(entry)); //decltype so that if we change procentry32w to something else, we only need to do that once and not for everything

	bool procW = Process32FirstW(snapshot, &entry);

	if (procW == TRUE) {
		if (_wcsicmp(process_name, entry.szExeFile) == 0) {
			//where szExeFile holds the executable of the process; i.e LSASS in task mng. is actually lsass.exe 
			pid = entry.th32ProcessID;
		}
		else {
			while (Process32NextW(snapshot, &entry) == TRUE) {
				if (_wcsicmp(process_name, entry.szExeFile) == 0) {
					pid = entry.th32ProcessID;
					break;
				}
			}
		}

	}	
	CloseHandle(snapshot); 
	return pid; 
}

static std::uintptr_t module_base(const DWORD pid, const wchar_t* module_name) {
	std::uintptr_t module_base = 0;

	HANDLE snapshot = CreateToolhelp32Snapshot(
		TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
		pid
	); 

	if (snapshot == INVALID_HANDLE_VALUE) {
		std::cout << "Snapshot for handle failed";    
		return module_base; 
	}

	MODULEENTRY32W entry = {};
	entry.dwSize = sizeof(decltype(entry)); 

	BOOL modW = Module32FirstW(snapshot, &entry);

	if (modW == TRUE) {
		if (wcsstr(entry.szModule, module_name) != nullptr ) {
			module_base =  reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
			CloseHandle(snapshot);
			return module_base;
		}

		while (Module32NextW(snapshot, &entry) == TRUE) {
			if (wcsstr(entry.szModule, module_name) != nullptr) {
				module_base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
				break;
			}
		}
	}

	CloseHandle(snapshot); 
	return module_base; 
}


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
		PVOID target_id; //set buffer or r/w proc memo. Has to do with MmCopy...  

		SIZE_T size;
		SIZE_T return_size;

	};
	DWORD bytesReturned; //the device io control needs a  place to outpute the bytes if lpOverlapped is NULL

	bool attach_to_process(HANDLE driver_handle, DWORD pid) {
		Request p; 
		p.process_id = reinterpret_cast<HANDLE>(static_cast<DWORD_PTR>(pid)) ; //because it needs t

		return DeviceIoControl(driver_handle, code::attach, &p, sizeof(p), &p, sizeof(p), &bytesReturned, NULL);
	}

	template<class T>
	T readMemory(HANDLE driver_handle, std::uintptr_t addr) {
		T temp = {}; //the driver will put the result in this because we pass the address to temp
		Request r; 

		r.target_id = reinterpret_cast<PVOID>(addr); 
		r.buffer = &temp; 
		r.size = sizeof(T); 

		DeviceIoControl(driver_handle, code::read, &r, sizeof(r), &r, sizeof(r), &bytesReturned, NULL);

		return temp; 
	}

	template<class T>
	void writeMemory(HANDLE hProcess, std::uintptr_t addr, const T& value) {
		Request r;
		r.target_id = reinterpret_cast<PVOID>(addr);
		r.buffer = (PVOID)&value;
		r.size = sizeof(T);
		DeviceIoControl(hProcess, code::write, &r, sizeof(r), &r, sizeof(r), &bytesReturned, NULL);
	}
	}
	

int main() {
	const DWORD pid = get_process_id(L"notepad.exe"); 

	if (pid == 0) {
		std::cout << "You failure.";
	}
	else {
		std::cout << "Got the notepad pid - yes, hardcoded, are you judging?- at : " << pid << " "; 
	}


	const HANDLE driver = CreateFile(L"\\\\.\\KMDF Driver1", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	
	if (driver == INVALID_HANDLE_VALUE) {
		std::cout << "Noob, invalid handle";
		std::cin.get();
		return 1;
	} 

	if (driver::attach_to_process(driver, pid) == TRUE) {
		std::cout << "Attched to process. Good to go!"; 
	}

	CloseHandle(driver); 

	std::cin.get(); 

	return 0; 
} 

