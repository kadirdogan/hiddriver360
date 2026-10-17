#pragma once
// Observe native driver matching BEFORE HID/XInput dispatch. Fixed append-only
// slots: no USB requests, allocation, file I/O or retained descriptor pointers.
enum { USB_ENUM_CAPACITY = 64 };
struct UsbEnumEvent {
    volatile LONG seq;
    DWORD tick, caller, valid;
    BYTE device[18], iface[9], reserved;
};
struct UsbEnumTrace {
    DWORD magic1, magic2, version;
    volatile LONG count;
    UsbEnumEvent events[USB_ENUM_CAPACITY];
};
typedef char CheckEnumEventSize[(sizeof(UsbEnumEvent)==44)?1:-1];
typedef char CheckEnumTraceSize[(sizeof(UsbEnumTrace)==2832)?1:-1];

// The caller validates readability while descriptors are owned by enumeration.
// Publish last so readers never consume a partially copied event.
static void RecordUsbEnum(UsbEnumTrace& trace, DWORD tick, DWORD caller,
                          const BYTE* device, const BYTE* iface) {
    LONG seq=InterlockedIncrement(&trace.count);
    if (seq<=0 || seq>USB_ENUM_CAPACITY) return;
    UsbEnumEvent& e=trace.events[seq-1];
    e.tick=tick; e.caller=caller; e.valid=0;
    if (device && device[0]>=18 && device[1]==1) {
        for (unsigned i=0;i<18;++i) e.device[i]=device[i];
        e.valid|=1;
    }
    if (iface && iface[0]>=9 && iface[1]==4) {
        for (unsigned i=0;i<9;++i) e.iface[i]=iface[i];
        e.valid|=2;
    }
    InterlockedExchange(&e.seq,seq);
}
