/*
Some notes for the following code, to not fill the code with comments.

Vocab:
DPFLTR = Debug Print Filter. A filter that allows you to filter debug messages
based on the component and level of the message.
PCSTR = const char*.

Good to know:
For debug_print, I use #ifdef DEBUG so that debug output is compiled only
when DEBUG is defined in the project settings.

CTL codes are constexpr.
An EPROCESS is a Windows process object in the kernel.

KdMapper-specifics:
DriverEntry has no DriverObject or registry path. See notes.txt for explanations.
*/

#include <ntifs.h>

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

// Forward declarations

extern "C" {

    NTKERNELAPI NTSTATUS IoCreateDriver(
        PUNICODE_STRING driver_name,
        PDRIVER_INITIALIZE InitFunc
    );

    NTKERNELAPI NTSTATUS MmCopyVirtualMemory(
        PEPROCESS SourceProcess,
        PVOID SourceAddress,
        PEPROCESS TargetProcess,
        PVOID TargetAddress,
        SIZE_T BufferSize,
        KPROCESSOR_MODE PreviousMode,
        PSIZE_T ReturnSize
    );
}

void debug_print(PCSTR text);

// Driver namespace
namespace driver {

    namespace code {

        constexpr ULONG attach = CTL_CODE(
            FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_ANY_ACCESS
        );

        constexpr ULONG read = CTL_CODE(
            FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS
        );

        constexpr ULONG write = CTL_CODE(
            FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS
        );
    }

    // Shared user-mode and kernel-mode request structure
    struct Request {
        HANDLE process_id;
        PVOID target;
        PVOID buffer;
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

        debug_print("Device control called, device object is working.\n");

        NTSTATUS status = STATUS_UNSUCCESSFUL;

        PIO_STACK_LOCATION stack_irp =
            IoGetCurrentIrpStackLocation(irp);

        auto request = reinterpret_cast<Request*>(
            irp->AssociatedIrp.SystemBuffer
            );

        // Against BSOD
        if (stack_irp == nullptr || request == nullptr) {

            irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            irp->IoStatus.Information = 0;

            IoCompleteRequest(irp, IO_NO_INCREMENT);

            return status;
        }

        static PEPROCESS target_process = nullptr;

        const ULONG control_code =
            stack_irp->Parameters.DeviceIoControl.IoControlCode;

        switch (control_code) {

        case code::attach:

            status = PsLookupProcessByProcessId(
                request->process_id,
                &target_process
            );

            break;

        case code::read:

            if (target_process != nullptr) {

                status = MmCopyVirtualMemory(
                    target_process,
                    request->target,
                    PsGetCurrentProcess(),
                    request->buffer,
                    request->size,
                    KernelMode,
                    &request->return_size
                );
            }

            break;

        case code::write:

            if (target_process != nullptr) {

                status = MmCopyVirtualMemory(
                    PsGetCurrentProcess(),
                    request->buffer,
                    target_process,
                    request->target,
                    request->size,
                    KernelMode,
                    &request->return_size
                );
            }

            break;

        default:

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

// Debug print implementation
void debug_print(PCSTR text) {

#ifdef DEBUG

    KdPrintEx((
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_INFO_LEVEL,
        "%s",
        text
        ));

#else

    UNREFERENCED_PARAMETER(text);

#endif
}

NTSTATUS driver_main(
    PDRIVER_OBJECT driver_object,
    PUNICODE_STRING registry_path
) {

    UNREFERENCED_PARAMETER(registry_path);

    UNICODE_STRING device_name = {};

    RtlInitUnicodeString(
        &device_name,
        L"\\Device\\KMDF Driver1"
    );

    // Create the device object
    PDEVICE_OBJECT device_object = {};

    NTSTATUS status = IoCreateDevice(
        driver_object,
        0,
        &device_name,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &device_object
    );

    if (status != STATUS_SUCCESS) {

        debug_print("Could not create a driver device.\n");

        return status;
    }

    debug_print("Created a driver object!\n");

    // Create the symbolic link
    UNICODE_STRING symbolic_link = {};

    RtlInitUnicodeString(
        &symbolic_link,
        L"\\DosDevices\\KMDF Driver1"
    );

    status = IoCreateSymbolicLink(
        &symbolic_link,
        &device_name
    );

    if (status != STATUS_SUCCESS) {

        debug_print("Failed to create a symbolic link!\n");
    }
    else {

        debug_print("Created a symbolic link!\n");
    }

    // Set up IO communications
    SetFlag(device_object->Flags, DO_BUFFERED_IO);

    // Register dispatch handlers
    driver_object->MajorFunction[IRP_MJ_CREATE] =
        driver::create;

    driver_object->MajorFunction[IRP_MJ_CLOSE] =
        driver::close;

    driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
        driver::device_control;

    ClearFlag(
        device_object->Flags,
        DO_DEVICE_INITIALIZING
    );

    debug_print("The driver is ready to receive communications.\n");

    return status;
}

NTSTATUS DriverEntry() {

    debug_print("Driver entry test check.\n");

    UNICODE_STRING driver_name = {};

    RtlInitUnicodeString(
        &driver_name,
        L"\\Driver\\KMDF Driver1"
    );

    return IoCreateDriver(
        &driver_name,
        &driver_main
    );
}