#pragma once
struct HoriNativeEvent {
    volatile LONG seq;
    DWORD tick, stage, node, status, caller;
};
struct HoriNativeTrace {
    DWORD magic1, magic2, version;
    volatile LONG count;
    volatile LONG pendingNode;
    HoriNativeEvent events[16];
};
typedef char CheckHoriNativeSize[(sizeof(HoriNativeTrace)==404)?1:-1];
// Stage1=enter native add; 2=UsbdAddDeviceComplete; 3=native add returned.
// Stage3 return value is raw register data, NOT the device acceptance status.
static void RecordHoriNative(HoriNativeTrace& trace,DWORD tick,DWORD stage,
                             DWORD node,DWORD status,DWORD caller) {
    LONG seq=InterlockedIncrement(&trace.count);
    if (seq<=0 || seq>16) return;
    HoriNativeEvent& e=trace.events[seq-1];
    e.tick=tick; e.stage=stage; e.node=node; e.status=status; e.caller=caller;
    InterlockedExchange(&e.seq,seq);
}
