/*
Comments and useful info for this code.

Notions: uintptr_t = unsigned integer type. This is capable of storing a pointer without losing any info.
module = executable image loaded within a process' address space.
*/

#include <ntifs.h> 
#include <Windows.h> 
#include<Tlhelp32.h> 
#include<cstdint.h> 

//take a snapshot of all processes -> enum. processes -> find pid matching name.
static DWORD get_proc(cons wchar_t* process_name) { 
    DWORD pid = nullptr; 
    HANDLE snapshopt = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS,
        0
    ) ;

    if ( snapshot == INVALID_HANDLE_VALUE){
        std::cout<<"Failed to get a snapshot. \n" ; 
    }
    else{
        std::cout<<"Snapshot taken successfully.\n" ; 
    }

    PROCESSENTRY32W entry = {} ; 
    size.dwSize = sizeof(decltype(entry))

    bool procF = Process32First(
        snapshot,
        &entry
    ) ; 

    if(procF == TRUE) { 
        if( _wcsicmp(process_name, entry.szExeFile) == 0 ){
            pid = entry.th32ProcessID ;
        }
    else{ 
            while(Process32NextW(snapshot, &entry) == TRUE ){
                if(_wcsicmp(process_name, entry.szExeFile) == 0 ) { 
                    pid = entry.th32ProcessID; 
                    break; 
                }
            }
    }
    }
    CloseHandle(snapshot) ; 
    return 0 ; 
}

//enumerate modules within pid
static DWORD get_mod(const char pid, const wchar_t* module_name ){ 

    std::uintptr_t module_base = 0  ; 
    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        pid
    ) ; 

    if ( snapshot == INVALID_HANDLE_VALUE) { 
        std::cout<<"Failed to get the handle" ; 
    }
    else{ 
        std::cout<<"Got handle to a module"
    }  

    MODULEENTRY32W entry = {} ; 
    entry.dwSize = sizeof(decltype(entry))  ; 

    BOOL modF = Module32FirstW(snapshot, &entry) ; 

    if (modF == TRUE) {
        if(_wcsicmp(module_base, entry.szModuleNmae) == 0 ){ 
            module_base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddress) ; 
            CloseHandle(snapshot) ; 
            break ; 
        }
        else{ 
            while(Handl32NextW(snapshot, &entry) == TRUE) { 
                if ( _wcsicmp(module_base, entry.szModuleName) == 0 ) { 
                    module_base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddress) ; 
                    CloseHandle(snapshot) ; 
                    break;
                }
            }
        }
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

    return 0 ; 
}

namespace driver {
	namespace code {
		constexpr ULONG attach = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
		constexpr ULONG read = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
		constexpr ULONG write = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

	}

	struct Request {
		HANDLE process_id; 
		PVOID buffer;
		PVOID target_id;
		SIZE_T size;
		SIZE_T return_size;

	};
} 


//now we will setup the logic for the attach, read and write. The driver has the handler logic.
DWORD receiver ; //for deviocontrol, as msdn states the last 2 params cant be null at the same time.
bool attachP(HANDLE driver_handle, const DWORD pid) { 
    Request r ; //no initialization to reduce cpu cycles
    r.process_id = reinterpret_cast<HANDLE>(pid) ;

    return DeviceIoControl(driver_handle, code::attach, &r, sizeof(r), &r, sizeof(r) , &receiver , nullptr) ; 

}   
//template to ensure efficiency and writing different code for different data types. 
template < class T > 
T read_memory(HANDLE driver_handle, const std::uintptr_t addres ) { 
T temp= {} ;
Request r ; 
r.target_id = reintepret_cast<PVOID>(addr) ; 
r.buffer = &temp ; //the read result goes into temp 
r.size = sizeof(T) ;  

DeviceIoControl(driver_handle, code::read, &r , sizeof(r) , &r, sizeof(r), &receiver, nullptr ) ; 
return temp ; 
}

template < class T > 
T write_memory(HANDLE driver_handle, const std::uintptr_t addr, const T& value){ 
T temp = {} 
Request r ; 
r.target_id = reinterpret_cast<PVOID>(addr) ; 
r.buffer = (PVOID)&value; 
r.size = sizeof(T); 
DeviceIoControl(driver_handler, case::write, &r , sizeof(r), &r, sizeof(r) , &receiver, nullptr) ; 
}

int main() { 

    const pid = get_proc(L"notepad.exe") ; 
    if(pid == 0 ) {
        std::cout<<"No process found.\n" ;
        std::cin.get() ; //to make the console pause, we do \n at the end of the previous file, and then we call cin.get() which takes in 1 char, or the \n if present. 
        return 1 ; 
    }
    
    const HANDLE driver_handle = CreateFile(L"\\\\.\\KMDF Driver1" , GENERIC_READ, 0 , nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr); 
    if(driver == INVALID_HANDLE_VALUE) { 
        std::cout<<"Invalid handle to given pid. \m " ; 
        std::cin.get() ;
        return 1 ; 
    }

    if ( driver::attach_to_process(driver_handler, pid) == TRUE){ 
        std::cout<<"Succesfully attached." ; 
    }

    CloseHandle(driver); //prevent leaking    
    return 0 ; 
}

