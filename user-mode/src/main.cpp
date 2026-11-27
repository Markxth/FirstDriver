
/*
Comments and useful info for this code.

Notions: uintptr_t = unsigned integer type. This is capable of storing a pointer without losing any info.
module = executable image loaded within a process' address space.
*/

#include <Windows.h>
#include <Tlhelp32.h>
#include <cstdint>
#include <iostream>

//take a snapshot of all processes -> enum. processes -> find pid matching name.
static DWORD get_proc(const wchar_t* process_name) {
    DWORD pid = 0;

    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS,
        0
    );

    if (snapshot == INVALID_HANDLE_VALUE) {
        std::cout << "Failed to get a snapshot.\n";
        return pid;
    }
    else {
        std::cout << "Snapshot taken successfully.\n";
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(decltype(entry));

    BOOL procF = Process32FirstW(
        snapshot,
        &entry
    );

    if (procF == TRUE) {
        if (_wcsicmp(process_name, entry.szExeFile) == 0) {
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

//enumerate modules within pid
static std::uintptr_t get_mod(const DWORD pid, const wchar_t* module_name) {

    std::uintptr_t module_base = 0;

    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        pid
    );

    if (snapshot == INVALID_HANDLE_VALUE) {
        std::cout << "Failed to get the handle.\n";
        return module_base;
    }
    else {
        std::cout << "Got handle to a module.\n";
    }

    MODULEENTRY32W entry = {};
    entry.dwSize = sizeof(decltype(entry));

    BOOL modF = Module32FirstW(snapshot, &entry);

    if (modF == TRUE) {
        do {
            if (_wcsicmp(module_name, entry.szModule) == 0) {
                module_base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snapshot, &entry) == TRUE);
    }

    CloseHandle(snapshot);
    return module_base;
}


namespace driver {
    namespace code {
        //for attaching to a process. This comes before r/w
        constexpr ULONG attach = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
        //constexpr because CTL_CODE itself returns a const

        //note : CTL_CODE is used for defining codes which are 32-bit integers that are passed by a umode app to the driver via DeviceIoControl
        //CTL_CODE(DeviceType, function (0x800 and above. the rest are microsoft's), Method, Access)
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
}


//now we will setup the logic for the attach, read and write. The driver has the handler logic.
DWORD receiver; //for DeviceIoControl, as MSDN states the last 2 params can't be null at the same time.

bool attachP(HANDLE driver_handle, const DWORD pid) {
    driver::Request r; //no initialization to reduce cpu cycles
    r.process_id = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(pid));

    return DeviceIoControl(
        driver_handle,
        driver::code::attach,
        &r,
        sizeof(r),
        &r,
        sizeof(r),
        &receiver,
        nullptr
    );
}


//template to ensure efficiency and writing different code for different data types.
template <class T>
T read_memory(HANDLE driver_handle, const std::uintptr_t address) {
    T temp = {};
    driver::Request r;

    // IOCTL buffer handling must be designed consistently with the driver.
    // Not implemented here.
    (void)driver_handle;
    (void)address;
    (void)r;

    return temp;
}

template <class T>
bool write_memory(HANDLE driver_handle, const std::uintptr_t addr, const T& value) {
    driver::Request r;

    // IOCTL buffer handling must be designed consistently with the driver.
    // Not implemented here.
    (void)driver_handle;
    (void)addr;
    (void)value;
    (void)r;

    return false;
}


int main() {

    const DWORD pid = get_proc(L"notepad.exe");

    if (pid == 0) {
        std::cout << "No process found.\n";
        std::cin.get(); //to make the console pause, we do \n at the end of the previous file, and then we call cin.get() which takes in 1 char, or the \n if present.
        return 1;
    }

    const HANDLE driver_handle = CreateFile(
        L"\\\\.\\KMDF Driver1",
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (driver_handle == INVALID_HANDLE_VALUE) {
        std::cout << "Invalid handle to given pid.\n";
        std::cin.get();
        return 1;
    }

    if (attachP(driver_handle, pid) == TRUE) {
        std::cout << "Successfully attached.\n";
    }

    CloseHandle(driver_handle); //prevent leaking
    return 0;
}
