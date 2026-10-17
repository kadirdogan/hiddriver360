#pragma once
#include "usb.h"

// Only the observed Hori PC interface. Never generalize this to all FF devices.
static bool IsHoriXInput(unsigned vid,unsigned pid,unsigned cls,unsigned sub,unsigned proto) {
    return vid==0x0F0D && pid==0x00ED && cls==0xFF && sub==0x5D && proto==1;
}
static int16_t HoriSignedLE(const uint8_t* p) {
    unsigned v=p[0]|((unsigned)p[1]<<8);
    return (int16_t)(v>=32768 ? (int)v-65536 : (int)v);
}
// Windows capture: 20-byte XInput input packets, header00/14. Decode explicitly
// because Xenon is big-endian. Never reinterpret the USB buffer as XINPUT_GAMEPAD.
static bool DecodeHoriXInput(const uint8_t* p,unsigned length,ButtonsReport& result) {
    if (!p || length<20 || p[0]!=0 || p[1]!=20) return false;
    ButtonsReport b={};
    unsigned buttons=p[2]|((unsigned)p[3]<<8);
    b.dpad_up=(buttons&0x0001)!=0; b.dpad_down=(buttons&0x0002)!=0;
    b.dpad_left=(buttons&0x0004)!=0; b.dpad_right=(buttons&0x0008)!=0;
    b.start=(buttons&0x0010)!=0; b.back=(buttons&0x0020)!=0;
    b.l3=(buttons&0x0040)!=0; b.r3=(buttons&0x0080)!=0;
    b.l1=(buttons&0x0100)!=0; b.r1=(buttons&0x0200)!=0;
    b.xbox=(buttons&0x0400)!=0;
    b.a_button=(buttons&0x1000)!=0; b.b_button=(buttons&0x2000)!=0;
    b.x_button=(buttons&0x4000)!=0; b.y_button=(buttons&0x8000)!=0;
    b.rx=p[4]; b.ry=p[5]; b.l2=p[4]!=0; b.r2=p[5]!=0;
    b.x=HoriSignedLE(p+6); b.y=HoriSignedLE(p+8);
    b.z=HoriSignedLE(p+10); b.rz=HoriSignedLE(p+12);
    result=b;
    return true;
}

struct HoriXInputTrace {
    DWORD magic1,magic2,version;
    volatile LONG routed,ready,good,bad,errors;
    volatile DWORD stage,status,length;
};
typedef char CheckHoriXInputTraceSize[(sizeof(HoriXInputTrace)==44)?1:-1];
