#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#define fplog 0
#include <math.h>
#define HAVE_STDARG_H
#include "../86box.h"
#include "cpu.h"
#include "../mem.h"
#include "../pic.h"
#include "x86.h"
#include "x86_flags.h"
#include "x86_ops.h"
#include "x87.h"
#include "386_common.h"


#ifdef ENABLE_FPU_LOG
int fpu_do_log = ENABLE_FPU_LOG;


static void
fpu_log(const char *fmt, ...)
{
    va_list ap;

    if (fpu_log) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
}
#else
#define fpu_log(fmt, ...)
#endif


uint16_t x87_gettag()
{
        uint16_t ret = 0;
        int c;
        
        for (c = 0; c < 8; c++)
        {
                if (cpu_state.tag[c] & TAG_UINT64)
                        ret |= 2 << (c*2);
                else
                        ret |= (cpu_state.tag[c] << (c*2));
        }

        return ret;
}

void x87_settag(uint16_t new_tag)
{
        cpu_state.tag[0] = new_tag & 3;
        cpu_state.tag[1] = (new_tag >> 2) & 3;
        cpu_state.tag[2] = (new_tag >> 4) & 3;
        cpu_state.tag[3] = (new_tag >> 6) & 3;
        cpu_state.tag[4] = (new_tag >> 8) & 3;
        cpu_state.tag[5] = (new_tag >> 10) & 3;
        cpu_state.tag[6] = (new_tag >> 12) & 3;
        cpu_state.tag[7] = (new_tag >> 14) & 3;
}


#ifdef ENABLE_808X_LOG
void x87_dumpregs()
{
        if (cpu_state.ismmx)
        {
                fpu_log("MM0=%016llX\tMM1=%016llX\tMM2=%016llX\tMM3=%016llX\n", cpu_state.MM[0].q, cpu_state.MM[1].q, cpu_state.MM[2].q, cpu_state.MM[3].q);
                fpu_log("MM4=%016llX\tMM5=%016llX\tMM6=%016llX\tMM7=%016llX\n", cpu_state.MM[4].q, cpu_state.MM[5].q, cpu_state.MM[6].q, cpu_state.MM[7].q);
        }
        else
        {
                fpu_log("ST(0)=%f\tST(1)=%f\tST(2)=%f\tST(3)=%f\t\n",cpu_state.ST[cpu_state.TOP],cpu_state.ST[(cpu_state.TOP+1)&7],cpu_state.ST[(cpu_state.TOP+2)&7],cpu_state.ST[(cpu_state.TOP+3)&7]);
                fpu_log("ST(4)=%f\tST(5)=%f\tST(6)=%f\tST(7)=%f\t\n",cpu_state.ST[(cpu_state.TOP+4)&7],cpu_state.ST[(cpu_state.TOP+5)&7],cpu_state.ST[(cpu_state.TOP+6)&7],cpu_state.ST[(cpu_state.TOP+7)&7]);
        }
        fpu_log("Status = %04X  Control = %04X  Tag = %04X\n", cpu_state.npxs, cpu_state.npxc, x87_gettag());
}
#endif
