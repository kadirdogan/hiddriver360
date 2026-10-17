#pragma once
inline bool NeedsHoriIdle(unsigned vid, unsigned pid) {
    return vid==0x0F0D && (pid==0x00EB || pid==0x00EC);
}
// Seqlock snapshot: odd seq while updating, even seq when published.
struct HoriInitTrace {
    DWORD magic1, magic2, version;
    volatile LONG seq;
    volatile DWORD vid, pid, interfaceNumber, stage, status, requestedTick, completedTick;
};
typedef char CheckHoriInitTraceSize[(sizeof(HoriInitTrace)==44)?1:-1];
