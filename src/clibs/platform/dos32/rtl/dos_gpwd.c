/*
    Software License Agreement (BSD License)
    
    Copyright (c) 1997-2008, David Lindauer, (LADSoft).
    All rights reserved.
    
    Redistribution and use of this software in source and binary forms, with or without modification, are
    permitted provided that the following conditions are met:
    
    * Redistributions of source code must retain the above
      copyright notice, this list of conditions and the
      following disclaimer.
    
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the
      following disclaimer in the documentation and/or other
      materials provided with the distribution.
    
    * Neither the name of LADSoft nor the names of its
      contributors may be used to endorse or promote products
      derived from this software without specific prior
      written permission of LADSoft.
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
    WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <errno.h>
#include <dos.h>
#include <dpmi.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <wchar.h>
#include <locale.h>
#include "libp.h"

int _RTL_FUNC _dos_getpwd(char *buf, int drive)
{
    DPMI_REGS regs;
    SELECTOR sel,para;
    if (dpmi_alloc_real_memory(&sel,&para,0x5))
        return 8;
    regs.b.ah = 0x47;
    regs.h.flags = 0x72;
    regs.h.ss = 0;
    regs.h.sp = 0;
    regs.h.ds = para;
    regs.h.dx = drive;
    regs.h.si = 0;
    dpmi_simulate_real_interrupt(0x21,&regs);
    dpmi_copy_to_ds(buf,sel,0,65);
    dpmi_dealloc_real_memory(sel);
    return strlen(buf);
}
