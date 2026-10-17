#include <xtl.h>
#include <xkelib.h>
#include <stdio.h>
#include "../hiddriver/Kernel17511.h"
#include "../hiddriver/PortTrace17511.h"
#include "../hiddriver/BootTakeover17511.h"
#include "../hiddriver/HoriInitTrace.h"
#include "../hiddriver/UsbEnumTrace17511.h"
#include "../hiddriver/HoriNativeTrace.h"
#include "../hiddriver/HoriXInput.h"

// Loader probe: flush each stage before loading the system DLL. No unload.
static HANDLE report = INVALID_HANDLE_VALUE;
static void Log(const char* text) {
    if (report == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(report, text, (DWORD)strlen(text), &written, NULL);
    FlushFileBuffers(report);
}

// Reference: Xenomega/ArchLoader, ArchLauncher.cpp (system loader on CPU 4).
// Worker avoids title file handles, CRT formatting, and logging handshakes.
static volatile LONG loaderStage = 0;
static DWORD loaderStatus = 0;
static HANDLE loadedModule = NULL;
static DWORD WINAPI SystemLoad(LPVOID) {
    InterlockedExchange(&loaderStage, 1);
    loaderStatus = XexLoadImage("game:\\hiddriver-17511-sdk.xex",
        XEX_MODULE_FLAG_DLL, 0, &loadedModule);
    InterlockedExchange(&loaderStage, 2);
    return loaderStatus;
}

static DWORD LoadDriver() {
    Log("Main-thread loader entered. Calling SMC tray query.\r\n");
    BYTE input[16] = {0}, output[16] = {0};
    input[0] = 0x0A;
    HalSendSMCMessage(input, output);
    char check[160];
    sprintf_s(check, sizeof(check), "SMC tray byte=0x%02X (0x60 means bypass)\r\n", output[1]);
    Log(check);
    bool matches = true;
    for (unsigned i=0; i<sizeof(hid17511Checks)/sizeof(hid17511Checks[0]); ++i) {
        const Hid17511Check& c = hid17511Checks[i];
        sprintf_s(check, sizeof(check), "Checking address %08X\r\n", c.address);
        Log(check);
        bool readable = MmIsAddressValid((PVOID)c.address) &&
            MmIsAddressValid((PVOID)(c.address+c.length-1));
        bool same = readable && memcmp((void*)c.address,c.bytes,c.length)==0;
        sprintf_s(check, sizeof(check), "Code %08X readable=%d match=%d\r\n",c.address,readable,same);
        Log(check);
        if (!same) {
            matches = false;
            if (readable) {
                char bytes[160];
                unsigned pos = 0;
                for (unsigned j=0; j<c.length; ++j)
                    pos += sprintf_s(bytes+pos, sizeof(bytes)-pos, "%02X ", ((BYTE*)c.address)[j]);
                Log("Actual bytes at rejected address:\r\n");
                Log(bytes); Log("\r\n");
            }

        }
    }
    if (output[1] == 0x60 || !matches) {
        Log("SKIP: driver would reject tray/code checks. No hooks attempted.\r\n");
        return 1;
    }
    Log("Creating CPU4 system loader; source=game:\\hiddriver-17511-sdk.xex\r\n");
    HANDLE thread = NULL;
    DWORD status = ExCreateThread(&thread, 0, NULL, XapiThreadStartup,
        SystemLoad, NULL, EX_CREATE_FLAG_SYSTEM | EX_CREATE_FLAG_SUSPENDED);
    char result[192];
    sprintf_s(result, sizeof(result), "ExCreateThread=0x%08X\r\n",status); Log(result);
    if (status != 0 || !thread) return status;
    DWORD cpu = XSetThreadProcessor(thread, 4);
    BOOL priority = SetThreadPriority(thread, THREAD_PRIORITY_NORMAL);
    DWORD resumed = ResumeThread(thread);
    sprintf_s(result, sizeof(result), "CPU previous=%u priority_ok=%d ResumeThread=%u\r\n",cpu,priority,resumed); Log(result);
    DWORD start = GetTickCount();
    LONG last = -1;
    bool warned = false;
    for (;;) {
        LONG stage = InterlockedCompareExchange(&loaderStage, 0, 0);
        if (stage != last) {
            sprintf_s(result, sizeof(result), "Loader stage=%ld (0=not entered, 1=inside load, 2=returned)\r\n",stage); Log(result);
            last = stage;
        }
        if (stage == 2) break;
        if (!warned && GetTickCount()-start > 30000) {
            Log("Timeout: stage persisted; keeping application resident. Power off before retrieving log.\r\n");
            warned = true;
        }
        Sleep(10);
    }
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    sprintf_s(result, sizeof(result), "XexLoadImage status=0x%08X module=%p\r\n",loaderStatus,loadedModule); Log(result);
    return loaderStatus;
}

static void ReportHook(const char* name, DWORD address, const BYTE* original) {
    char line[256];
    if (!address || !MmIsAddressValid((PVOID)address) ||
        !MmIsAddressValid((PVOID)(address+15))) {
        sprintf_s(line,sizeof(line),"Hook %s address=%08X unreadable\r\n",name,address); Log(line); return;
    }
    DWORD* words=(DWORD*)address;
    sprintf_s(line,sizeof(line),"Hook %s @%08X: %08X %08X %08X %08X\r\n",
        name,address,words[0],words[1],words[2],words[3]); Log(line);
    if (original) Log(memcmp((void*)address,original,16)==0 ?
        "Original instructions unchanged: hook NOT installed here.\r\n" :
        "Instructions changed: inspect jump target; modification alone does not identify owner.\r\n");
}
static void ReportHooks() {
    Log("Live hook snapshot (read-only):\r\n");
    // Resolve by address: validation entries can be inserted/reordered.
    const DWORD hookAddresses[]={0x800FA878,0x800D7A28,0x800D71C8,0x800D77E8,0x800D76B8,0x800E5DB8,0x800E5D78};
    const char* hookNames[]={"NativeXInputAdd","UsbdAddDeviceComplete","UsbDriverMatch","UsbPortRemove","UsbRemoveDispatch","HidAddDevice","HidRemoveDevice"};
    for (unsigned h=0;h<sizeof(hookAddresses)/sizeof(hookAddresses[0]);++h) {
        const BYTE* original=NULL;
        for (unsigned c=0;c<sizeof(hid17511Checks)/sizeof(hid17511Checks[0]);++c)
            if (hid17511Checks[c].address==hookAddresses[h]) original=hid17511Checks[c].bytes;
        ReportHook(hookNames[h],hookAddresses[h],original);
    }
    const char* modules[]={"xboxkrnl.exe","xam.xex","xam.xex"};
    const DWORD ordinals[]={486,685,402};
    const char* names[]={"XInputdReadState","XamInputGetCapabilitiesEx","XamInputSetState"};
    for (unsigned i=0;i<3;++i) {
        HANDLE mod=NULL; PVOID proc=NULL;
        NTSTATUS status=XexGetModuleHandle((PCHAR)modules[i],&mod);
        if (status==0 && mod) status=XexGetProcedureAddress(mod,ordinals[i],&proc);
        char line[128]; sprintf_s(line,sizeof(line),"Resolve %s status=%08X\r\n",names[i],(DWORD)status); Log(line);
        if (status==0) ReportHook(names[i],(DWORD)proc,NULL);
    }
}

static HidPortTrace* FindPortTrace() {
    for (DWORD a=0x81F00000;a<0x81F50000;a+=4) {
        if (!MmIsAddressValid((PVOID)a) || !MmIsAddressValid((PVOID)(a+sizeof(HidPortTrace)-1))) continue;
        HidPortTrace* p=(HidPortTrace*)a;
        if (p->magic1==0x48505254 && p->magic2==0x31373531 && p->version==1) return p;
    }
    return NULL;
}
static void ReportPortTrace(HidPortTrace* trace, unsigned& next) {
    if (!trace) return;
    while (next<32) {
        HidPortEvent& e=trace->events[next];
        if (InterlockedCompareExchange(&e.seq,0,0)!=(LONG)(next+1)) break;
        char line[200];
        // Raw caller addresses are authoritative. Labels are disassembly interpretations.
        sprintf_s(line,sizeof(line),"PORT event=%u caller=%08X tick=%u parent=%08X port=%u\r\n",
            next+1,e.caller,e.tick,e.parent,e.port); Log(line); ++next;
    }
    if (next==32 && trace->count>32) {
        Log("PORT trace full; later events not captured.\r\n"); ++next;
    }
}
static void PollXInputChanges() {
    static bool seen[4]={false,false,false,false};
    static DWORD statuses[4]={0,0,0,0};
    static WORD buttons[4]={0,0,0,0};
    static BYTE lt[4]={0,0,0,0}, rt[4]={0,0,0,0};
    const WORD masks[]={XINPUT_GAMEPAD_A,XINPUT_GAMEPAD_B,XINPUT_GAMEPAD_X,XINPUT_GAMEPAD_Y,
        XINPUT_GAMEPAD_DPAD_UP,XINPUT_GAMEPAD_DPAD_DOWN,XINPUT_GAMEPAD_DPAD_LEFT,XINPUT_GAMEPAD_DPAD_RIGHT,
        XINPUT_GAMEPAD_START,XINPUT_GAMEPAD_BACK,XINPUT_GAMEPAD_LEFT_SHOULDER,XINPUT_GAMEPAD_RIGHT_SHOULDER,
        XINPUT_GAMEPAD_LEFT_THUMB,XINPUT_GAMEPAD_RIGHT_THUMB};
    const char* names[]={"A","B","X","Y","UP","DOWN","LEFT","RIGHT","START","BACK","LB","RB","L3","R3"};
    for (DWORD user=0;user<4;++user) {
        XINPUT_STATE state={0}; DWORD status=XInputGetState(user,&state); char line[160];
        if (!seen[user] || statuses[user]!=status) {
            sprintf_s(line,sizeof(line),"XINPUT player=%u status=%08X\r\n",user+1,status); Log(line);
        }
        if (status==ERROR_SUCCESS) {
            WORD changed=buttons[user]^state.Gamepad.wButtons;
            for (unsigned i=0;i<sizeof(masks)/sizeof(masks[0]);++i) if (changed&masks[i]) {
                sprintf_s(line,sizeof(line),"XINPUT player=%u %s %s\r\n",user+1,names[i],
                    (state.Gamepad.wButtons&masks[i])?"pressed":"released"); Log(line);
            }
            if (lt[user]!=state.Gamepad.bLeftTrigger || rt[user]!=state.Gamepad.bRightTrigger) {
                sprintf_s(line,sizeof(line),"XINPUT player=%u LT=%u RT=%u\r\n",user+1,
                    state.Gamepad.bLeftTrigger,state.Gamepad.bRightTrigger); Log(line);
            }
            buttons[user]=state.Gamepad.wButtons;
            lt[user]=state.Gamepad.bLeftTrigger; rt[user]=state.Gamepad.bRightTrigger;
        } else { buttons[user]=0; lt[user]=rt[user]=0; }
        statuses[user]=status; seen[user]=true;
    }
}

static HidBootTrace* FindBootTrace() {
    for (DWORD a=0x81F00000;a<0x81F50000;a+=4) {
        if (!MmIsAddressValid((PVOID)a) || !MmIsAddressValid((PVOID)(a+sizeof(HidBootTrace)-1))) continue;
        HidBootTrace* p=(HidBootTrace*)a;
        if (p->magic1==0x48424F54 && p->magic2==0x31373531 && p->version==1) return p;
    }
    return NULL;
}
static void ReportBootTrace(HidBootTrace* trace, LONG& lastStage) {
    if (!trace) return;
    LONG stage=InterlockedCompareExchange(&trace->stage,0,0);
    if (stage==lastStage) return;
    lastStage=stage;
    char line[240];
    sprintf_s(line,sizeof(line),"BOOT stage=%d (0=waiting,1=scanning,2=done,3=guard-failed,4=busy-timeout,5=dpc-failed)\r\n",stage); Log(line);
    if (stage<2) return; // Records are immutable after terminal publication.
    sprintf_s(line,sizeof(line),"BOOT visited=%u resetRequests=%u invalid=%u busyWaits=%u status=%08X\r\n",
        trace->visited,trace->requested,trace->invalid,trace->busyWaits,trace->status); Log(line);
    for (DWORD i=0;i<trace->visited && i<32;++i) {
        const HidBootPort& p=trace->ports[i];
        if (!p.action) continue;
        sprintf_s(line,sizeof(line),"BOOT controller=%u port=%u node=%08X owner=%08X flags=%02X typeFlags=%02X action=%u (1=skip,2=reset-request,3=invalid)\r\n",
            p.controller,p.port,p.node,p.owner,p.flags,p.typeFlags,p.action); Log(line);
    }
}

static HoriInitTrace* FindHoriInitTrace() {
    for (DWORD a=0x81F00000;a<0x81F50000;a+=4) {
        if (!MmIsAddressValid((PVOID)a) || !MmIsAddressValid((PVOID)(a+sizeof(HoriInitTrace)-1))) continue;
        HoriInitTrace* p=(HoriInitTrace*)a;
        if (p->magic1==0x484F5249 && p->magic2==0x49444C31 && p->version==1) return p;
    }
    return NULL;
}
static void ReportHoriInit(HoriInitTrace* p, LONG& previous) {
    if (!p) return;
    LONG seq=InterlockedCompareExchange(&p->seq,0,0);
    if (!seq || (seq&1) || seq==previous) return;
    DWORD vid=p->vid,pid=p->pid,iface=p->interfaceNumber,stage=p->stage,status=p->status;
    DWORD sent=p->requestedTick,done=p->completedTick;
    if (seq!=InterlockedCompareExchange(&p->seq,0,0)) return;
    previous=seq;
    char line[240];
    sprintf_s(line,sizeof(line),"HORI INIT VID=%04X PID=%04X interface=%u SET_IDLE stage=%u (1=requested,2=completed) status=%08X sent=%u done=%u\r\n",
        vid,pid,iface,stage,status,sent,done); Log(line);
}

static UsbEnumTrace* FindUsbEnumTrace() {
    for (DWORD a=0x81F00000;a<0x81F50000;a+=4) {
        if (!MmIsAddressValid((PVOID)a) || !MmIsAddressValid((PVOID)(a+sizeof(UsbEnumTrace)-1))) continue;
        UsbEnumTrace* p=(UsbEnumTrace*)a;
        if (p->magic1==0x55454E4D && p->magic2==0x31373531 && p->version==1) return p;
    }
    return NULL;
}
static void ReportUsbEnum(UsbEnumTrace* trace, unsigned& next) {
    if (!trace) return;
    while (next<USB_ENUM_CAPACITY) {
        UsbEnumEvent& e=trace->events[next];
        if (InterlockedCompareExchange(&e.seq,0,0)!=(LONG)(next+1)) break;
        char line[320];
        unsigned vid=e.device[8]|(e.device[9]<<8), pid=e.device[10]|(e.device[11]<<8);
        sprintf_s(line,sizeof(line),
            "ENUM event=%u tick=%u caller=%08X valid=%u VID=%04X PID=%04X device=%02X/%02X/%02X interface=%u alt=%u class=%02X/%02X/%02X endpoints=%u\r\n",
            next+1,e.tick,e.caller,e.valid,vid,pid,e.device[4],e.device[5],e.device[6],
            e.iface[2],e.iface[3],e.iface[5],e.iface[6],e.iface[7],e.iface[4]);
        Log(line); ++next;
    }
    if (next==USB_ENUM_CAPACITY && InterlockedCompareExchange(&trace->count,0,0)>USB_ENUM_CAPACITY) {
        Log("ENUM trace full; later enumeration events NOT captured. Cold boot for a new capture.\r\n"); ++next;
    }
}

static HoriNativeTrace* FindHoriNativeTrace() {
    for (DWORD a=0x81F00000;a<0x81F50000;a+=4) {
        if (!MmIsAddressValid((PVOID)a) || !MmIsAddressValid((PVOID)(a+sizeof(HoriNativeTrace)-1))) continue;
        HoriNativeTrace* p=(HoriNativeTrace*)a;
        if (p->magic1==0x484E4154 && p->magic2==0x31373531 && p->version==1) return p;
    }
    return NULL;
}
static void ReportHoriNative(HoriNativeTrace* trace,unsigned& next) {
    if (!trace) return;
    while (next<16) {
        HoriNativeEvent& e=trace->events[next];
        if (InterlockedCompareExchange(&e.seq,0,0)!=(LONG)(next+1)) break;
        const char* label=e.stage==1 ? "native-add-enter" : e.stage==2 ? "add-complete-status" :
            e.stage==3 ? "native-add-return-raw" : "removed-before-completion";
        char line[256];
        sprintf_s(line,sizeof(line),"HORI NATIVE event=%u tick=%u stage=%u %s node=%08X value=%08X caller=%08X\r\n",
            next+1,e.tick,e.stage,label,e.node,e.status,e.caller);
        Log(line); ++next;
    }
    if (next==16 && InterlockedCompareExchange(&trace->count,0,0)>16) {
        Log("HORI NATIVE trace full; cold boot for a new capture.\r\n"); ++next;
    }
}

static HoriXInputTrace* FindHoriXInputTrace() {
    for (DWORD a=0x81F00000;a<0x81F50000;a+=4) {
        if (!MmIsAddressValid((PVOID)a) || !MmIsAddressValid((PVOID)(a+sizeof(HoriXInputTrace)-1))) continue;
        HoriXInputTrace* p=(HoriXInputTrace*)a;
        if (p->magic1==0x4858494E && p->magic2==0x31373531 && p->version==1) return p;
    }
    return NULL;
}
static void ReportHoriXInput(HoriXInputTrace* p) {
    if (!p) return;
    char line[240];
    sprintf_s(line,sizeof(line),"HORI XINPUT routed=%ld ready=%ld good=%ld bad=%ld errors=%ld stage=%u status=%08X length=%u\r\n",
        p->routed,p->ready,p->good,p->bad,p->errors,p->stage,p->status,p->length);
    Log(line);
}

static void MonitorUsb() {
    volatile DWORD* found=NULL;
    // Current driver's verified load range is 0x81F00000..0x81F50000.
    for (DWORD page=0x81F00000;page<0x81F50000;page+=0x1000) {
        if (!MmIsAddressValid((PVOID)page) || !MmIsAddressValid((PVOID)(page+0xFFF))) continue;
        for (DWORD a=page;a<page+0x1000;a+=4) {
            volatile DWORD* p=(volatile DWORD*)a;
            if (*p!=0x48313735 || !MmIsAddressValid((PVOID)(a+651))) continue;
            if (p[1]==0x54454C31 && p[2]==6) { found=p; break; }
        }
        if (found) break;
    }
    if (!found) { Log("Telemetry missing: use telemetry6 driver and cold boot to replace resident copy.\r\n"); return; }
    Log("USB monitoring 45 seconds: Hori test = connect once now, keep connected and press buttons.\r\n");
    HidPortTrace* portTrace=FindPortTrace(); unsigned nextPort=0;
    HidBootTrace* bootTrace=FindBootTrace(); LONG lastBootStage=-1;
    HoriInitTrace* horiTrace=FindHoriInitTrace(); LONG lastHoriSeq=0;
    UsbEnumTrace* enumTrace=FindUsbEnumTrace(); unsigned nextEnum=0;
    HoriNativeTrace* nativeTrace=FindHoriNativeTrace(); unsigned nextNative=0;
    HoriXInputTrace* horiInput=FindHoriXInputTrace();
    Log(horiInput ? "HORI XINPUT handler present (horixinput1).\r\n" : "HORI XINPUT missing: use new driver and cold boot.\r\n");
    Log(nativeTrace ? "HORI NATIVE observer present.\r\n" : "HORI NATIVE trace missing: use nativeprobe1 driver and cold boot.\r\n");
    if (!enumTrace) Log("ENUM trace missing: use enumtrace1 driver and cold boot.\r\n");
    else Log("ENUM observer present: captures driver-match descriptors, including non-HID. valid=3 means both descriptors valid. No event does not prove no electrical reconnect.\r\n");
    if (!horiTrace) Log("HORI init trace missing: use horiidle1 driver and cold boot.\r\n");
    if (!bootTrace) Log("BOOT trace missing: use bootclaim1 driver and cold boot.\r\n");
    if (!portTrace) Log("PORT trace missing: use porttrace1 driver and cold boot.\r\n");
    DWORD lastDescriptor=0;
    for (unsigned tick=0;tick<45;++tick) {
        char line[256];
        sprintf_s(line,sizeof(line),"USB t=%u adds=%u manager=%u accepted=%u callbacks=%u VID=%04X PID=%04X class/sub/proto=%06X transfer=%08X parse=%u\r\n",
            tick,found[3],found[4],found[5],found[6],found[7],found[8],found[9],found[10],found[11]);
        Log(line);
        sprintf_s(line,sizeof(line),"USB detail endpoint=%08X queue=%08X user=%u reports=%u managerLoops=%u mappingStarts=%u\r\n",
            found[12],found[13],found[14],found[15],found[16],found[17]);
        Log(line);
        sprintf_s(line,sizeof(line),"USB teardown removals=%u closed=%u reaped=%u closeError=%08X\r\n",found[18],found[19],found[20],found[21]);
        Log(line);
        sprintf_s(line,sizeof(line),"USB timing addTick=%u removeTick=%u removeCaller=%08X lastInputStatus=%08X firstInputError=%08X inputNonzero=%u\r\n",
            found[153],found[154],found[155],found[156],found[157],found[158]); Log(line);
        const char* origin = found[159]==0x800D7954 ? "port-change" :
            found[159]==0x800D7A08 ? "enumeration-cleanup" : "unknown-or-none";
        sprintf_s(line,sizeof(line),"USB origin caller=%08X tick=%u nodeState=%08X count=%u path=%s\r\n",
            found[159],found[160],found[161],found[162],origin); Log(line);
        DWORD seq=found[22];
        if (seq && !(seq&1) && seq!=lastDescriptor) {
            DWORD size=found[23], captured=found[24]; BYTE snapshot[512];
            if (captured<=sizeof(snapshot)) {
                for (DWORD j=0;j<captured;++j) snapshot[j]=((volatile BYTE*)(found+25))[j];
                if ((DWORD)InterlockedCompareExchange((volatile LONG*)(found+22),0,0)==seq) {
                    sprintf_s(line,sizeof(line),"Descriptor sequence=%u reported=%u captured=%u VID=%04X PID=%04X\r\n",seq,size,captured,found[7],found[8]); Log(line);
                    for (DWORD j=0;j<captured;j+=16) {
                        unsigned pos=sprintf_s(line,sizeof(line),"DESC %04X: ",j);
                        for (DWORD k=j;k<captured && k<j+16;++k) pos+=sprintf_s(line+pos,sizeof(line)-pos,"%02X ",snapshot[k]);
                        Log(line); Log("\r\n");
                    }
                    lastDescriptor=seq;
                }
            }
        }
        ReportPortTrace(portTrace,nextPort);
        ReportBootTrace(bootTrace,lastBootStage);
        ReportHoriInit(horiTrace,lastHoriSeq);
        ReportUsbEnum(enumTrace,nextEnum);
        ReportHoriNative(nativeTrace,nextNative);
        ReportHoriXInput(horiInput);
        for (unsigned poll=0;poll<50;++poll) { PollXInputChanges(); Sleep(20); }
    }
}

int main() {
    report = CreateFileA("game:\\hid17511-diagnostic.log", GENERIC_WRITE,
        FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (report == INVALID_HANDLE_VALUE) {
        report = CreateFileA("Hdd:\\hid17511-diagnostic.log", GENERIC_WRITE,
            FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    Log("HID17511 diagnostic v22 - USB driver-match observer\r\n");
    Log("Application entry reached. Preparing loader test.\r\n");
    char version[128];
    sprintf_s(version, sizeof(version), "Kernel %u.%u.%u.%u\r\n",
        XboxKrnlVersion->Major, XboxKrnlVersion->Minor,
        XboxKrnlVersion->Build, XboxKrnlVersion->Qfe);
    Log(version);
    bool loaded = false;
    const char* names[] = {"hiddriver.xex", "hiddriver-17511-prototype.xex", "hiddriver-17511-sdk.xex"};
    for (unsigned i=0; i<sizeof(names)/sizeof(names[0]); ++i) {
        HANDLE module = NULL;
        NTSTATUS status = XexGetModuleHandle((PCHAR)names[i], &module);
        char line[200];
        sprintf_s(line, sizeof(line), "Existing %s: status=0x%08X module=%p\r\n", names[i], (DWORD)status, module);
        Log(line);
        if (status == 0 && module) loaded = true;
    }
    HANDLE file = CreateFileA("game:\\hiddriver-17511-sdk.xex", GENERIC_READ,
        FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    bool present = file != INVALID_HANDLE_VALUE;
    if (present) CloseHandle(file);
    Log(present ? "Driver file beside probe: YES\r\n" : "Driver file beside probe: NO\r\n");
    if (loaded) Log("SKIP: driver already loaded; no second load.\r\n");
    else if (!present) Log("SKIP: missing driver file.\r\n");
    else if (XboxKrnlVersion->Build != 17511) Log("SKIP: expected kernel 17511.\r\n");
    else {
        Log("Checking on main thread; loading beside diagnostic on CPU4.\r\n");
        DWORD status = LoadDriver();
        char line[128];
        sprintf_s(line, sizeof(line), "Loader probe returned=0x%08X\r\n", status);
        Log(line);
    }
    Log("Starting monitor immediately; driver manager may still be starting.\r\n");
    ReportHooks();
    MonitorUsb();
    Log("Probe finished.\r\n");
    if (report != INVALID_HANDLE_VALUE) CloseHandle(report);
    XLaunchNewImage(NULL, NULL);
    Sleep(INFINITE);
    return 0;
}
