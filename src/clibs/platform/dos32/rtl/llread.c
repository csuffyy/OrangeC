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
#include <time.h>
#include <stdio.h>
#include <wchar.h>
#include <locale.h>
#include <errno.h>
#include "libp.h"
#include <dpmi.h>
#include "llp.h"

extern int __dtabuflen ;
int __ll_read(int fd, void *buf, size_t size)
{
    DPMI_REGS regs;
    int mod = size %__dtabuflen,i, tsize = 0;
    for (i=0; i < (size & -__dtabuflen); i += __dtabuflen) {
        regs.h.dx = 0;
        regs.h.bx = fd;
        regs.h.cx = __dtabuflen;
        __doscall(0x3f,&regs);
        if (regs.h.flags & 1)
            return -1;
        __dtatobuf((char *)buf+i,__dtabuflen);
        tsize += regs.h.ax;
        if (regs.h.ax < __dtabuflen)
            return tsize;
    }
    regs.h.dx = 0;
    regs.h.bx = fd;
    regs.h.cx = mod;
    __doscall(0x3f,&regs);
    tsize += regs.h.ax;
    if (regs.h.flags & 1)
        return -1;
    __dtatobuf((char *)buf+i,mod);
    return tsize;
}