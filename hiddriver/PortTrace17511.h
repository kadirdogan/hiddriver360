#pragma once
// Append-only trace: each callback reserves one slot and publishes seq last.
// No allocation, waiting, device changes or filesystem access in USB context.
struct HidPortEvent { volatile LONG seq; volatile DWORD caller, tick, parent, port; };
struct HidPortTrace {
    DWORD magic1, magic2, version;
    volatile LONG count;
    HidPortEvent events[32];
};
typedef char CheckPortEventSize[(sizeof(HidPortEvent)==20)?1:-1];
typedef char CheckPortTraceSize[(sizeof(HidPortTrace)==656)?1:-1];
