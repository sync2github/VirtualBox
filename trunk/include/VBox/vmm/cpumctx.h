/** @file
 * CPUM - CPU Monitor(/ Manager), Context Structures.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_vmm_cpumctx_h
#define ___VBox_vmm_cpumctx_h

#ifndef VBOX_FOR_DTRACE_LIB
# include <iprt/x86.h>
# include <VBox/types.h>
#else
# pragma D depends_on library x86.d
#endif


RT_C_DECLS_BEGIN

/** @defgroup grp_cpum_ctx  The CPUM Context Structures
 * @ingroup grp_cpum
 * @{
 */

/**
 * Selector hidden registers.
 */
typedef struct CPUMSELREG
{
    /** The selector register. */
    RTSEL       Sel;
    /** Padding, don't use. */
    RTSEL       PaddingSel;
    /** The selector which info resides in u64Base, u32Limit and Attr, provided
     * that CPUMSELREG_FLAGS_VALID is set. */
    RTSEL       ValidSel;
    /** Flags, see CPUMSELREG_FLAGS_XXX. */
    uint16_t    fFlags;

    /** Base register.
     *
     * Long mode remarks:
     *  - Unused in long mode for CS, DS, ES, SS
     *  - 32 bits for FS & GS; FS(GS)_BASE msr used for the base address
     *  - 64 bits for TR & LDTR
     */
    uint64_t    u64Base;
    /** Limit (expanded). */
    uint32_t    u32Limit;
    /** Flags.
     * This is the high 32-bit word of the descriptor entry.
     * Only the flags, dpl and type are used. */
    X86DESCATTR Attr;
} CPUMSELREG;
#ifdef VBOX_FOR_DTRACE_LIB
AssertCompileSize(CPUMSELREG, 24)
#endif

/** @name CPUMSELREG_FLAGS_XXX - CPUMSELREG::fFlags values.
 * @{ */
#define CPUMSELREG_FLAGS_VALID      UINT16_C(0x0001)
#define CPUMSELREG_FLAGS_STALE      UINT16_C(0x0002)
#define CPUMSELREG_FLAGS_VALID_MASK UINT16_C(0x0003)
/** @} */

/** Checks if the hidden parts of the selector register are valid. */
#ifdef VBOX_WITH_RAW_MODE_NOT_R0
# define CPUMSELREG_ARE_HIDDEN_PARTS_VALID(a_pVCpu, a_pSelReg) \
    (   ((a_pSelReg)->fFlags & CPUMSELREG_FLAGS_VALID) \
     && (   (a_pSelReg)->ValidSel == (a_pSelReg)->Sel \
         || (   (a_pVCpu) /*!= NULL*/ \
             && (a_pSelReg)->ValidSel == ((a_pSelReg)->Sel & X86_SEL_MASK_OFF_RPL) \
             && ((a_pSelReg)->Sel      & X86_SEL_RPL) == 1 \
             && ((a_pSelReg)->ValidSel & X86_SEL_RPL) == 0 \
             && CPUMIsGuestInRawMode(a_pVCpu) \
            ) \
        ) \
    )
#else
# define CPUMSELREG_ARE_HIDDEN_PARTS_VALID(a_pVCpu, a_pSelReg) \
    (   ((a_pSelReg)->fFlags & CPUMSELREG_FLAGS_VALID) \
     && (a_pSelReg)->ValidSel == (a_pSelReg)->Sel  )
#endif

/** Old type used for the hidden register part.
 * @deprecated  */
typedef CPUMSELREG CPUMSELREGHID;

/**
 * The sysenter register set.
 */
typedef struct CPUMSYSENTER
{
    /** Ring 0 cs.
     * This value +  8 is the Ring 0 ss.
     * This value + 16 is the Ring 3 cs.
     * This value + 24 is the Ring 3 ss.
     */
    uint64_t    cs;
    /** Ring 0 eip. */
    uint64_t    eip;
    /** Ring 0 esp. */
    uint64_t    esp;
} CPUMSYSENTER;

/** @def CPUM_UNION_NAME
 * For compilers (like DTrace) that does not grok nameless unions, we have a
 * little hack to make them palatable.
 */
#ifdef VBOX_FOR_DTRACE_LIB
# define CPUM_UNION_NAME(a_Nm)  a_Nm
#elif defined(VBOX_WITHOUT_UNNAMED_UNIONS)
# define CPUM_UNION_NAME(a_Nm)  a_Nm
#else
# define CPUM_UNION_NAME(a_Nm)
#endif

/** A general register (union). */
typedef union CPUMCTXGREG
{
    /** Natural unsigned integer view. */
    uint64_t            u;
    /** 64-bit view. */
    uint64_t            u64;
    /** 32-bit view. */
    uint32_t            u32;
    /** 16-bit view. */
    uint16_t            u16;
    /** 8-bit view. */
    uint8_t             u8;
    /** 8-bit low/high view.    */
    struct
    {
        /** Low byte (al, cl, dl, bl, ++). */
        uint8_t         bLo;
        /** High byte in the first word - ah, ch, dh, bh. */
        uint8_t         bHi;
    } CPUM_UNION_NAME(s);
} CPUMCTXGREG;
#ifdef VBOX_FOR_DTRACE_LIB
AssertCompileSize(CPUMCTXGREG, 8);
AssertCompileMemberOffset(CPUMCTXGREG, CPUM_UNION_NAME(s.) bLo, 0);
AssertCompileMemberOffset(CPUMCTXGREG, CPUM_UNION_NAME(s.) bHi, 1);
#endif



/**
 * CPU context core.
 *
 * @todo        Eliminate this structure!
 * @deprecated  We don't push any context cores any more in TRPM.
 */
#pragma pack(1)
typedef struct CPUMCTXCORE
{
    /** @name General Register.
     * @note  These follow the encoding order (X86_GREG_XXX) and can be accessed as
     *        an array starting a rax.
     * @{ */
    union
    {
        uint8_t         al;
        uint16_t        ax;
        uint32_t        eax;
        uint64_t        rax;
    } CPUM_UNION_NAME(rax);
    union
    {
        uint8_t         cl;
        uint16_t        cx;
        uint32_t        ecx;
        uint64_t        rcx;
    } CPUM_UNION_NAME(rcx);
    union
    {
        uint8_t         dl;
        uint16_t        dx;
        uint32_t        edx;
        uint64_t        rdx;
    } CPUM_UNION_NAME(rdx);
    union
    {
        uint8_t         bl;
        uint16_t        bx;
        uint32_t        ebx;
        uint64_t        rbx;
    } CPUM_UNION_NAME(rbx);
    union
    {
        uint16_t        sp;
        uint32_t        esp;
        uint64_t        rsp;
    } CPUM_UNION_NAME(rsp);
    union
    {
        uint16_t        bp;
        uint32_t        ebp;
        uint64_t        rbp;
    } CPUM_UNION_NAME(rbp);
    union
    {
        uint8_t         sil;
        uint16_t        si;
        uint32_t        esi;
        uint64_t        rsi;
    } CPUM_UNION_NAME(rsi);
    union
    {
        uint8_t         dil;
        uint16_t        di;
        uint32_t        edi;
        uint64_t        rdi;
    } CPUM_UNION_NAME(rdi);
    uint64_t            r8;
    uint64_t            r9;
    uint64_t            r10;
    uint64_t            r11;
    uint64_t            r12;
    uint64_t            r13;
    uint64_t            r14;
    uint64_t            r15;
    /** @} */

    /** @name Segment registers.
     * @note These follow the encoding order (X86_SREG_XXX) and can be accessed as
     *       an array starting a es.
     * @{  */
    CPUMSELREG          es;
    CPUMSELREG          cs;
    CPUMSELREG          ss;
    CPUMSELREG          ds;
    CPUMSELREG          fs;
    CPUMSELREG          gs;
    /** @} */

    /** The program counter. */
    union
    {
        uint16_t        ip;
        uint32_t        eip;
        uint64_t        rip;
    } CPUM_UNION_NAME(rip);

    /** The flags register. */
    union
    {
        X86EFLAGS       eflags;
        X86RFLAGS       rflags;
    } CPUM_UNION_NAME(rflags);

} CPUMCTXCORE;
#pragma pack()


/**
 * CPU context.
 */
#pragma pack(1) /* for VBOXIDTR / VBOXGDTR. */
typedef struct CPUMCTX
{
    /** CPUMCTXCORE Part.
     * @{ */

    /** General purpose registers. */
    union /* no tag! */
    {
        /** The general purpose register array view, indexed by X86_GREG_XXX. */
        CPUMCTXGREG     aGRegs[16];

        /** 64-bit general purpose register view. */
        struct /* no tag! */
        {
            uint64_t    rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15;
        } CPUM_UNION_NAME(qw);
        /** 64-bit general purpose register view. */
        struct /* no tag! */
        {
            uint64_t    r0, r1, r2, r3, r4, r5, r6, r7;
        } CPUM_UNION_NAME(qw2);
        /** 32-bit general purpose register view. */
        struct /* no tag! */
        {
            uint32_t     eax, u32Pad00,      ecx, u32Pad01,      edx, u32Pad02,      ebx, u32Pad03,
                         esp, u32Pad04,      ebp, u32Pad05,      esi, u32Pad06,      edi, u32Pad07,
                         r8d, u32Pad08,      r9d, u32Pad09,     r10d, u32Pad10,     r11d, u32Pad11,
                        r12d, u32Pad12,     r13d, u32Pad13,     r14d, u32Pad14,     r15d, u32Pad15;
        } CPUM_UNION_NAME(dw);
        /** 16-bit general purpose register view. */
        struct /* no tag! */
        {
            uint16_t      ax, au16Pad00[3],   cx, au16Pad01[3],   dx, au16Pad02[3],   bx, au16Pad03[3],
                          sp, au16Pad04[3],   bp, au16Pad05[3],   si, au16Pad06[3],   di, au16Pad07[3],
                         r8w, au16Pad08[3],  r9w, au16Pad09[3], r10w, au16Pad10[3], r11w, au16Pad11[3],
                        r12w, au16Pad12[3], r13w, au16Pad13[3], r14w, au16Pad14[3], r15w, au16Pad15[3];
        } CPUM_UNION_NAME(w);
        struct /* no tag! */
        {
            uint8_t   al, ah, abPad00[6], cl, ch, abPad01[6], dl, dh, abPad02[6], bl, bh, abPad03[6],
                         spl, abPad04[7],    bpl, abPad05[7],    sil, abPad06[7],    dil, abPad07[7],
                         r8l, abPad08[7],    r9l, abPad09[7],   r10l, abPad10[7],   r11l, abPad11[7],
                        r12l, abPad12[7],   r13l, abPad13[7],   r14l, abPad14[7],   r15l, abPad15[7];
        } CPUM_UNION_NAME(b);
    } CPUM_UNION_NAME(g);

    /** Segment registers. */
    union /* no tag! */
    {
        /** The segment register array view, indexed by X86_SREG_XXX. */
        CPUMSELREG      aSRegs[6];
        /** The named segment register view. */
        struct /* no tag! */
        {
            CPUMSELREG  es, cs, ss, ds, fs, gs;
        } CPUM_UNION_NAME(n);
    } CPUM_UNION_NAME(s);

    /** The program counter. */
    union
    {
        uint16_t        ip;
        uint32_t        eip;
        uint64_t        rip;
    } CPUM_UNION_NAME(rip);

    /** The flags register. */
    union
    {
        X86EFLAGS       eflags;
        X86RFLAGS       rflags;
    } CPUM_UNION_NAME(rflags);

    /** @} */ /*(CPUMCTXCORE)*/


    /** @name Control registers.
     * @{ */
    uint64_t            cr0;
    uint64_t            cr2;
    uint64_t            cr3;
    uint64_t            cr4;
    /** @} */

    /** Debug registers.
     * @remarks DR4 and DR5 should not be used since they are aliases for
     *          DR6 and DR7 respectively on both AMD and Intel CPUs.
     * @remarks DR8-15 are currently not supported by AMD or Intel, so
     *          neither do we.
     */
    uint64_t        dr[8];

    /** Padding before the structure so the 64-bit member is correctly aligned.
     * @todo fix this structure!  */
    uint16_t        gdtrPadding[3];
    /** Global Descriptor Table register. */
    VBOXGDTR        gdtr;

    /** Padding before the structure so the 64-bit member is correctly aligned.
     * @todo fix this structure!  */
    uint16_t        idtrPadding[3];
    /** Interrupt Descriptor Table register. */
    VBOXIDTR        idtr;

    /** The task register.
     * Only the guest context uses all the members. */
    CPUMSELREG      ldtr;
    /** The task register.
     * Only the guest context uses all the members. */
    CPUMSELREG      tr;

    /** The sysenter msr registers.
     * This member is not used by the hypervisor context. */
    CPUMSYSENTER    SysEnter;

    /** @name System MSRs.
     * @{ */
    uint64_t        msrEFER;
    uint64_t        msrSTAR;            /**< Legacy syscall eip, cs & ss. */
    uint64_t        msrPAT;             /**< Page attribute table. */
    uint64_t        msrLSTAR;           /**< 64 bits mode syscall rip. */
    uint64_t        msrCSTAR;           /**< Compatibility mode syscall rip. */
    uint64_t        msrSFMASK;          /**< syscall flag mask. */
    uint64_t        msrKERNELGSBASE;    /**< swapgs exchange value. */
    uint64_t        msrApicBase;        /**< The local APIC base (IA32_APIC_BASE MSR). */
    /** @} */

    /** The XCR0..XCR1 registers. */
    uint64_t                    aXcr[2];
    /** The mask to pass to XSAVE/XRSTOR in EDX:EAX.  If zero we use
     *  FXSAVE/FXRSTOR (since bit 0 will always be set, we only need to test it). */
    uint64_t                    fXStateMask;

    /** Pointer to the FPU/SSE/AVX/XXXX state ring-0 mapping. */
    R0PTRTYPE(PX86XSAVEAREA)    pXStateR0;
    /** Pointer to the FPU/SSE/AVX/XXXX state ring-3 mapping. */
    R3PTRTYPE(PX86XSAVEAREA)    pXStateR3;
    /** Pointer to the FPU/SSE/AVX/XXXX state raw-mode mapping. */
    RCPTRTYPE(PX86XSAVEAREA)    pXStateRC;
    /** State component offsets into pXState, UINT16_MAX if not present. */
    uint16_t                    aoffXState[64];

    /** Size padding. */
    uint32_t        au32SizePadding[HC_ARCH_BITS == 32 ? 13 : 11];
} CPUMCTX;
#pragma pack()

#ifndef VBOX_FOR_DTRACE_LIB
AssertCompileSizeAlignment(CPUMCTX, 64);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rax,   0);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rcx,   8);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rdx,  16);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rbx,  24);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rsp,  32);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rbp,  40);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rsi,  48);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rdi,  56);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.)  r8,  64);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.)  r9,  72);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r10,  80);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r11,  88);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r12,  96);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r13, 104);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r14, 112);
AssertCompileMemberOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r15, 120);
AssertCompileMemberOffset(CPUMCTX,   CPUM_UNION_NAME(s.n.) es, 128);
AssertCompileMemberOffset(CPUMCTX,   CPUM_UNION_NAME(s.n.) cs, 152);
AssertCompileMemberOffset(CPUMCTX,   CPUM_UNION_NAME(s.n.) ss, 176);
AssertCompileMemberOffset(CPUMCTX,   CPUM_UNION_NAME(s.n.) ds, 200);
AssertCompileMemberOffset(CPUMCTX,   CPUM_UNION_NAME(s.n.) fs, 224);
AssertCompileMemberOffset(CPUMCTX,   CPUM_UNION_NAME(s.n.) gs, 248);
AssertCompileMemberOffset(CPUMCTX,                        rip, 272);
AssertCompileMemberOffset(CPUMCTX,                     rflags, 280);
AssertCompileMemberOffset(CPUMCTX,                        cr0, 288);
AssertCompileMemberOffset(CPUMCTX,                        cr2, 296);
AssertCompileMemberOffset(CPUMCTX,                        cr3, 304);
AssertCompileMemberOffset(CPUMCTX,                        cr4, 312);
AssertCompileMemberOffset(CPUMCTX,                         dr, 320);
AssertCompileMemberOffset(CPUMCTX,                       gdtr, 384+6);
AssertCompileMemberOffset(CPUMCTX,                       idtr, 400+6);
AssertCompileMemberOffset(CPUMCTX,                       ldtr, 416);
AssertCompileMemberOffset(CPUMCTX,                         tr, 440);
AssertCompileMemberOffset(CPUMCTX,                   SysEnter, 464);
AssertCompileMemberOffset(CPUMCTX,                    msrEFER, 488);
AssertCompileMemberOffset(CPUMCTX,                    msrSTAR, 496);
AssertCompileMemberOffset(CPUMCTX,                     msrPAT, 504);
AssertCompileMemberOffset(CPUMCTX,                   msrLSTAR, 512);
AssertCompileMemberOffset(CPUMCTX,                   msrCSTAR, 520);
AssertCompileMemberOffset(CPUMCTX,                  msrSFMASK, 528);
AssertCompileMemberOffset(CPUMCTX,            msrKERNELGSBASE, 536);
AssertCompileMemberOffset(CPUMCTX,                msrApicBase, 544);
AssertCompileMemberOffset(CPUMCTX,                       aXcr, 552);
AssertCompileMemberOffset(CPUMCTX,                fXStateMask, 568);
AssertCompileMemberOffset(CPUMCTX,                  pXStateR0, 576);
AssertCompileMemberOffset(CPUMCTX,                  pXStateR3, HC_ARCH_BITS == 64 ? 584 : 580);
AssertCompileMemberOffset(CPUMCTX,                  pXStateRC, HC_ARCH_BITS == 64 ? 592 : 584);
AssertCompileMemberOffset(CPUMCTX,                 aoffXState, HC_ARCH_BITS == 64 ? 596 : 588);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rax, CPUMCTX, CPUM_UNION_NAME(g.)  aGRegs);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rax, CPUMCTX, CPUM_UNION_NAME(g.qw2.)  r0);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rcx, CPUMCTX, CPUM_UNION_NAME(g.qw2.)  r1);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rdx, CPUMCTX, CPUM_UNION_NAME(g.qw2.)  r2);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rbx, CPUMCTX, CPUM_UNION_NAME(g.qw2.)  r3);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rsp, CPUMCTX, CPUM_UNION_NAME(g.qw2.)  r4);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rbp, CPUMCTX, CPUM_UNION_NAME(g.qw2.)  r5);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rsi, CPUMCTX, CPUM_UNION_NAME(g.qw2.)  r6);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rdi, CPUMCTX, CPUM_UNION_NAME(g.qw2.)  r7);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rax, CPUMCTX, CPUM_UNION_NAME(g.dw.)  eax);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rcx, CPUMCTX, CPUM_UNION_NAME(g.dw.)  ecx);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rdx, CPUMCTX, CPUM_UNION_NAME(g.dw.)  edx);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rbx, CPUMCTX, CPUM_UNION_NAME(g.dw.)  ebx);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rsp, CPUMCTX, CPUM_UNION_NAME(g.dw.)  esp);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rbp, CPUMCTX, CPUM_UNION_NAME(g.dw.)  ebp);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rsi, CPUMCTX, CPUM_UNION_NAME(g.dw.)  esi);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rdi, CPUMCTX, CPUM_UNION_NAME(g.dw.)  edi);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.)  r8, CPUMCTX, CPUM_UNION_NAME(g.dw.)  r8d);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.)  r9, CPUMCTX, CPUM_UNION_NAME(g.dw.)  r9d);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r10, CPUMCTX, CPUM_UNION_NAME(g.dw.) r10d);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r11, CPUMCTX, CPUM_UNION_NAME(g.dw.) r11d);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r12, CPUMCTX, CPUM_UNION_NAME(g.dw.) r12d);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r13, CPUMCTX, CPUM_UNION_NAME(g.dw.) r13d);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r14, CPUMCTX, CPUM_UNION_NAME(g.dw.) r14d);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r15, CPUMCTX, CPUM_UNION_NAME(g.dw.) r15d);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rax, CPUMCTX, CPUM_UNION_NAME(g.w.)    ax);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rcx, CPUMCTX, CPUM_UNION_NAME(g.w.)    cx);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rdx, CPUMCTX, CPUM_UNION_NAME(g.w.)    dx);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rbx, CPUMCTX, CPUM_UNION_NAME(g.w.)    bx);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rsp, CPUMCTX, CPUM_UNION_NAME(g.w.)    sp);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rbp, CPUMCTX, CPUM_UNION_NAME(g.w.)    bp);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rsi, CPUMCTX, CPUM_UNION_NAME(g.w.)    si);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rdi, CPUMCTX, CPUM_UNION_NAME(g.w.)    di);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.)  r8, CPUMCTX, CPUM_UNION_NAME(g.w.)   r8w);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.)  r9, CPUMCTX, CPUM_UNION_NAME(g.w.)   r9w);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r10, CPUMCTX, CPUM_UNION_NAME(g.w.)  r10w);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r11, CPUMCTX, CPUM_UNION_NAME(g.w.)  r11w);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r12, CPUMCTX, CPUM_UNION_NAME(g.w.)  r12w);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r13, CPUMCTX, CPUM_UNION_NAME(g.w.)  r13w);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r14, CPUMCTX, CPUM_UNION_NAME(g.w.)  r14w);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r15, CPUMCTX, CPUM_UNION_NAME(g.w.)  r15w);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rax, CPUMCTX, CPUM_UNION_NAME(g.b.)    al);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rcx, CPUMCTX, CPUM_UNION_NAME(g.b.)    cl);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rdx, CPUMCTX, CPUM_UNION_NAME(g.b.)    dl);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rbx, CPUMCTX, CPUM_UNION_NAME(g.b.)    bl);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rsp, CPUMCTX, CPUM_UNION_NAME(g.b.)   spl);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rbp, CPUMCTX, CPUM_UNION_NAME(g.b.)   bpl);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rsi, CPUMCTX, CPUM_UNION_NAME(g.b.)   sil);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rdi, CPUMCTX, CPUM_UNION_NAME(g.b.)   dil);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.)  r8, CPUMCTX, CPUM_UNION_NAME(g.b.)   r8l);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.)  r9, CPUMCTX, CPUM_UNION_NAME(g.b.)   r9l);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r10, CPUMCTX, CPUM_UNION_NAME(g.b.)  r10l);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r11, CPUMCTX, CPUM_UNION_NAME(g.b.)  r11l);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r12, CPUMCTX, CPUM_UNION_NAME(g.b.)  r12l);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r13, CPUMCTX, CPUM_UNION_NAME(g.b.)  r13l);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r14, CPUMCTX, CPUM_UNION_NAME(g.b.)  r14l);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r15, CPUMCTX, CPUM_UNION_NAME(g.b.)  r15l);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(s.n.) es,   CPUMCTX, CPUM_UNION_NAME(s.)  aSRegs);
# ifndef _MSC_VER
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rax, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_xAX]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rcx, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_xCX]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rdx, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_xDX]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rbx, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_xBX]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rsp, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_xSP]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rbp, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_xBP]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rsi, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_xSI]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) rdi, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_xDI]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.)  r8, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_x8]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.)  r9, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_x9]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r10, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_x10]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r11, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_x11]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r12, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_x12]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r13, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_x13]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r14, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_x14]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(g.qw.) r15, CPUMCTX, CPUM_UNION_NAME(g.) aGRegs[X86_GREG_x15]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(s.n.) es,   CPUMCTX, CPUM_UNION_NAME(s.) aSRegs[X86_SREG_ES]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(s.n.) cs,   CPUMCTX, CPUM_UNION_NAME(s.) aSRegs[X86_SREG_CS]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(s.n.) ss,   CPUMCTX, CPUM_UNION_NAME(s.) aSRegs[X86_SREG_SS]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(s.n.) ds,   CPUMCTX, CPUM_UNION_NAME(s.) aSRegs[X86_SREG_DS]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(s.n.) fs,   CPUMCTX, CPUM_UNION_NAME(s.) aSRegs[X86_SREG_FS]);
AssertCompileMembersAtSameOffset(CPUMCTX, CPUM_UNION_NAME(s.n.) gs,   CPUMCTX, CPUM_UNION_NAME(s.) aSRegs[X86_SREG_GS]);
# endif

/**
 * Calculates the pointer to the given extended state component.
 *
 * @returns Pointer of type @a a_PtrType
 * @param   a_pCtx          Pointer to the context.
 * @param   a_iCompBit      The extended state component bit number.  This bit
 *                          must be set in CPUMCTX::fXStateMask.
 * @param   a_PtrType       The pointer type of the extended state component.
 *
 */
#if defined(VBOX_STRICT) && defined(RT_COMPILER_SUPPORTS_LAMBDA)
# define CPUMCTX_XSAVE_C_PTR(a_pCtx, a_iCompBit, a_PtrType) \
    ([](PCCPUMCTX a_pLambdaCtx) -> a_PtrType \
    { \
        AssertCompile((a_iCompBit) < 64U); \
        AssertMsg(a_pLambdaCtx->fXStateMask & RT_BIT_64(a_iCompBit), (#a_iCompBit "\n")); \
        AssertMsg(a_pLambdaCtx->aoffXState[(a_iCompBit)] != UINT16_MAX, (#a_iCompBit "\n")); \
        return (a_PtrType)((uint8_t *)a_pLambdaCtx->CTX_SUFF(pXState) + a_pLambdaCtx->aoffXState[(a_iCompBit)]); \
    }(a_pCtx))
#elif defined(VBOX_STRICT) && defined(__GNUC__)
# define CPUMCTX_XSAVE_C_PTR(a_pCtx, a_iCompBit, a_PtrType) \
    __extension__ (\
    { \
        AssertCompile((a_iCompBit) < 64U); \
        AssertMsg((a_pCtx)->fXStateMask & RT_BIT_64(a_iCompBit), (#a_iCompBit "\n")); \
        AssertMsg((a_pCtx)->aoffXState[(a_iCompBit)] != UINT16_MAX, (#a_iCompBit "\n")); \
        (a_PtrType)((uint8_t *)(a_pCtx)->CTX_SUFF(pXState) + (a_pCtx)->aoffXState[(a_iCompBit)]); \
    })
#else
# define CPUMCTX_XSAVE_C_PTR(a_pCtx, a_iCompBit, a_PtrType) \
    ((a_PtrType)((uint8_t *)(a_pCtx)->CTX_SUFF(pXState) + (a_pCtx)->aoffXState[(a_iCompBit)]))
#endif

/**
 * Gets the CPUMCTXCORE part of a CPUMCTX.
 */
# define CPUMCTX2CORE(pCtx) ((PCPUMCTXCORE)(void *)&(pCtx)->rax)

/**
 * Gets the CPUMCTX part from a CPUMCTXCORE.
 */
# define CPUMCTX_FROM_CORE(a_pCtxCore) RT_FROM_MEMBER(a_pCtxCore, CPUMCTX, rax)

/**
 * Gets the first selector register of a CPUMCTX.
 *
 * Use this with X86_SREG_COUNT to loop thru the selector registers.
 */
# define CPUMCTX_FIRST_SREG(a_pCtx) (&(a_pCtx)->es)

#endif /* !VBOX_FOR_DTRACE_LIB */

/**
 * Additional guest MSRs (i.e. not part of the CPU context structure).
 *
 * @remarks Never change the order here because of the saved stated!  The size
 *          can in theory be changed, but keep older VBox versions in mind.
 */
typedef union CPUMCTXMSRS
{
    struct
    {
        uint64_t    TscAux;             /**< MSR_K8_TSC_AUX */
        uint64_t    MiscEnable;         /**< MSR_IA32_MISC_ENABLE */
        uint64_t    MtrrDefType;        /**< IA32_MTRR_DEF_TYPE */
        uint64_t    MtrrFix64K_00000;   /**< IA32_MTRR_FIX16K_80000 */
        uint64_t    MtrrFix16K_80000;   /**< IA32_MTRR_FIX16K_80000 */
        uint64_t    MtrrFix16K_A0000;   /**< IA32_MTRR_FIX16K_A0000 */
        uint64_t    MtrrFix4K_C0000;    /**< IA32_MTRR_FIX4K_C0000 */
        uint64_t    MtrrFix4K_C8000;    /**< IA32_MTRR_FIX4K_C8000 */
        uint64_t    MtrrFix4K_D0000;    /**< IA32_MTRR_FIX4K_D0000 */
        uint64_t    MtrrFix4K_D8000;    /**< IA32_MTRR_FIX4K_D8000 */
        uint64_t    MtrrFix4K_E0000;    /**< IA32_MTRR_FIX4K_E0000 */
        uint64_t    MtrrFix4K_E8000;    /**< IA32_MTRR_FIX4K_E8000 */
        uint64_t    MtrrFix4K_F0000;    /**< IA32_MTRR_FIX4K_F0000 */
        uint64_t    MtrrFix4K_F8000;    /**< IA32_MTRR_FIX4K_F8000 */
        uint64_t    PkgCStateCfgCtrl;   /**< MSR_PKG_CST_CONFIG_CONTROL */
    } msr;
    uint64_t    au64[64];
} CPUMCTXMSRS;
/** Pointer to the guest MSR state. */
typedef CPUMCTXMSRS *PCPUMCTXMSRS;
/** Pointer to the const guest MSR state. */
typedef const CPUMCTXMSRS *PCCPUMCTXMSRS;

/**
 * The register set returned by a CPUID operation.
 */
typedef struct CPUMCPUID
{
    uint32_t uEax;
    uint32_t uEbx;
    uint32_t uEcx;
    uint32_t uEdx;
} CPUMCPUID;
/** Pointer to a CPUID leaf. */
typedef CPUMCPUID *PCPUMCPUID;
/** Pointer to a const CPUID leaf. */
typedef const CPUMCPUID *PCCPUMCPUID;

/** @}  */

RT_C_DECLS_END

#endif

