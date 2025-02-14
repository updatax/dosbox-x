/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <math.h>

#define LoadMbs(off) (int8_t)(LoadMb(off))
#define LoadMws(off) (int16_t)(LoadMw(off))
#define LoadMds(off) (int32_t)(LoadMd(off))

#define LoadRb(reg) reg
#define LoadRw(reg) reg
#define LoadRd(reg) reg

#define SaveRb(reg,val)	reg=((uint8_t)(val))
#define SaveRw(reg,val)	reg=((uint16_t)(val))
#define SaveRd(reg,val)	reg=((uint32_t)(val))

static INLINE int8_t Fetchbs() {
	return (int8_t)Fetchb();
}
static INLINE int16_t Fetchws() {
	return (int16_t)Fetchw();
}

static INLINE int32_t Fetchds() {
	return (int32_t)Fetchd();
}


#define RUNEXCEPTION() {										\
	PRE_EXCEPTION \
	CPU_Exception(cpu.exception.which,cpu.exception.error);		\
	continue;													\
}

#define EXCEPTION(blah)										\
	{														\
		PRE_EXCEPTION \
		CPU_Exception(blah);								\
		continue;											\
	}

/* NTS: At first glance, this looks like code that will only fetch the delta for conditional jumps
 *      if the condition is true. Further examination shows that DOSBox's core has two different
 *      CS:IP variables, reg_ip and core.cseip which Fetchb() modifies. */
//TODO Could probably make all byte operands fast?
#define JumpCond16_b(COND) {						\
	const uint32_t adj=(uint32_t)Fetchbs();						\
	SAVEIP;								\
	if (COND) reg_ip+=adj;						\
	continue;							\
}

#define JumpCond16_w(COND) {						\
	const uint32_t adj=(uint32_t)Fetchws();						\
	SAVEIP;								\
	if (COND) reg_ip+=adj;						\
	continue;							\
}

#define JumpCond32_b(COND) {						\
	const uint32_t adj=(uint32_t)Fetchbs();						\
	SAVEIP;								\
	if (COND) reg_eip+=adj;						\
	continue;							\
}

#define JumpCond32_d(COND) {						\
	const uint32_t adj=(uint32_t)Fetchds();						\
	SAVEIP;								\
	if (COND) reg_eip+=adj;						\
	continue;							\
}

#define MoveCond16(COND) {							\
	GetRMrw;										\
	if (rm >= 0xc0 ) {GetEArw; if (COND) *rmrw=*earw;}\
	else {GetEAa; if (COND) *rmrw=LoadMw(eaa);}		\
}

#define MoveCond32(COND) {							\
	GetRMrd;										\
	if (rm >= 0xc0 ) {GetEArd; if (COND) *rmrd=*eard;}\
	else {GetEAa; if (COND) *rmrd=LoadMd(eaa);}		\
}

#if CPU_CORE >= CPU_ARCHTYPE_80386

/* Most SSE instructions require memory access aligned to the SSE data type, else
 * an exception occurs. */
static INLINE bool SSE_REQUIRE_ALIGNMENT(const PhysPt v) {
	return ((unsigned int)v & 15u) == 0; /* 16 bytes * 8 bits = 128 bits */
}

/* Throw GPF on SSE misalignment [https://c9x.me/x86/html/file_module_x86_id_180.html] */
/* NTS: This macro intended for use in normal core */
#define SSE_ALIGN_EXCEPTION() EXCEPTION(EXCEPTION_GP)

#define STEP(i) SSE_MULPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_MULPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v *= s.v;
}

static INLINE void SSE_MULPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_MULSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_ANDPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_ANDPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw &= s.raw;
}

static INLINE void SSE_ANDPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_XORPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_XORPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw ^= s.raw;
}

static INLINE void SSE_XORPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_SQRTPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_SQRTPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v = sqrtf(s.v);
}

static INLINE void SSE_SQRTPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_SQRTSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_MOVAPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_MOVAPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw = s.raw;
}

static INLINE void SSE_MOVAPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_MOVUPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_MOVUPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw = s.raw;
}

static INLINE void SSE_MOVUPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_MOVSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

static INLINE void SSE_MOVHLPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[0] = s.u64[1];
}

static INLINE void SSE_MOVLPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[0] = s.u64[0];
}
#undef STEP

#endif // 386+

#define SETcc(cc)							\
	{								\
		GetRM;							\
		if (rm >= 0xc0 ) {GetEArb;*earb=(cc) ? 1 : 0;}		\
		else {GetEAa;SaveMb(eaa,(cc) ? 1 : 0);}			\
	}

void CPU_FXSAVE(PhysPt eaa);
void CPU_FXRSTOR(PhysPt eaa);
bool CPU_LDMXCSR(PhysPt eaa);
bool CPU_STMXCSR(PhysPt eaa);

#include "helpers.h"
#if CPU_CORE <= CPU_ARCHTYPE_8086
# include "table_ea_8086.h"
#else
# include "table_ea.h"
#endif
#include "../modrm.h"


