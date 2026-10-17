#pragma once
#include "usb.h"
// Apply to the outgoing snapshot, never stored input. Preserve LB+RB save/load.
inline void ApplyControllerShortcuts(uint16_t vid, uint16_t pid, ButtonsReport& b) {
    const bool supported = (vid==0x054C && pid==0x0CDA) ||
                           (vid==0x0810 && pid==0x0003);
    if (supported && b.start && b.r1 && b.r2) {
        b.start=0; b.r1=0; b.r2=0;
        b.r3=1;
    }
}
