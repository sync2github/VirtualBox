/* $Id$ */
/** @file
 * IPRT - Debug Module Interpreter.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/dbg.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/avl.h>
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/once.h>
#include <iprt/param.h>
#include <iprt/semaphore.h>
#include <iprt/strcache.h>
#include <iprt/string.h>
#include "internal/dbgmod.h"
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Debug info interpreter regisration record. */
typedef struct RTDBGMODREGDBG
{
    /** Pointer to the next record. */
    struct RTDBGMODREGDBG  *pNext;
    /** Pointer to the virtual function table for the interpreter.  */
    PCRTDBGMODVTDBG         pVt;
    /** Usage counter.  */
    uint32_t volatile       cUsers;
} RTDBGMODREGDBG;
typedef RTDBGMODREGDBG *PRTDBGMODREGDBG;

/** Image interpreter regisration record. */
typedef struct RTDBGMODREGIMG
{
    /** Pointer to the next record. */
    struct RTDBGMODREGIMG  *pNext;
    /** Pointer to the virtual function table for the interpreter.  */
    PCRTDBGMODVTIMG         pVt;
    /** Usage counter.  */
    uint32_t volatile       cUsers;
} RTDBGMODREGIMG;
typedef RTDBGMODREGIMG *PRTDBGMODREGIMG;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validates a debug module handle and returns rc if not valid. */
#define RTDBGMOD_VALID_RETURN_RC(pDbgMod, rc) \
    do { \
        AssertPtrReturn((pDbgMod), (rc)); \
        AssertReturn((pDbgMod)->u32Magic == RTDBGMOD_MAGIC, (rc)); \
        AssertReturn((pDbgMod)->cRefs > 0, (rc)); \
    } while (0)

/** Locks the debug module. */
#define RTDBGMOD_LOCK(pDbgMod) \
    do { \
        int rcLock = RTCritSectEnter(&(pDbgMod)->CritSect); \
        AssertRC(rcLock); \
    } while (0)

/** Unlocks the debug module. */
#define RTDBGMOD_UNLOCK(pDbgMod) \
    do { \
        int rcLock = RTCritSectLeave(&(pDbgMod)->CritSect); \
        AssertRC(rcLock); \
    } while (0)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Init once object for lazy registration of the built-in image and debug
 * info interpreters. */
static RTONCE           g_rtDbgModOnce = RTONCE_INITIALIZER;
/** Read/Write semaphore protecting the list of registered interpreters.  */
static RTSEMRW          g_hDbgModRWSem = NIL_RTSEMRW;
/** List of registered image interpreters.  */
static RTDBGMODREGIMG   g_pImgHead;
/** List of registered debug infor interpreters.  */
static RTDBGMODREGDBG   g_pDbgHead;
/** String cache for the debug info interpreters.
 * RTSTRCACHE is thread safe. */
DECLHIDDEN(RTSTRCACHE)  g_hDbgModStrCache = NIL_RTSTRCACHE;


/**
 * Cleanup debug info interpreter globals.
 *
 * @param   enmReason           The cause of the termination.
 * @param   iStatus             The meaning of this depends on enmReason.
 * @param   pvUser              User argument, unused.
 */
static DECLCALLBACK(void) rtDbgModTermCallback(RTTERMREASON enmReason, int32_t iStatus, void *pvUser)
{
    if (enmReason == RTTERMREASON_UNLOAD)
    {
        RTSemRWDestroy(g_hDbgModRWSem);
        g_hDbgModRWSem = NIL_RTSEMRW;

        RTStrCacheDestroy(g_hDbgModStrCache);
        g_hDbgModStrCache = NIL_RTSTRCACHE;

        /** @todo deregister interpreters. */
    }
}


/**
 * Do-once callback that initializes the read/write semaphore and registers
 * the built-in interpreters.
 *
 * @returns IPRT status code.
 * @param   pvUser1     NULL.
 * @param   pvUser2     NULL.
 */
static DECLCALLBACK(int) rtDbgModInitOnce(void *pvUser1, void *pvUser2)
{
    /*
     * Create the semaphore and string cache.
     */
    int rc = RTSemRWCreate(&g_hDbgModRWSem);
    AssertRCReturn(rc, rc);

    rc = RTStrCacheCreate(&g_hDbgModStrCache, "RTDBGMOD");
    if (RT_SUCCESS(rc))
    {
        /*
         * Register the interpreters.
         */
        /** @todo */

        if (RT_SUCCESS(rc))
        {
            /*
             * Finally, register the IPRT cleanup callback.
             */
            rc = RTTermRegisterCallback(rtDbgModTermCallback, NULL);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
        }

        RTStrCacheDestroy(g_hDbgModStrCache);
        g_hDbgModStrCache = NIL_RTSTRCACHE;
    }

    RTSemRWDestroy(g_hDbgModRWSem);
    g_hDbgModRWSem = NIL_RTSEMRW;

    return rc;
}


DECLINLINE(int) rtDbgModLazyInit(void)
{
    return RTOnce(&g_rtDbgModOnce, rtDbgModInitOnce, NULL, NULL);
}


/**
 * Creates a module based on the default debug info container.
 *
 * This can be used to manually load a module and its symbol.
 *
 * @returns IPRT status code.
 *
 * @param   phDbgMod        Where to return the module handle.
 * @param   pszName         The name of the module (mandatory).
 * @param   cb              The size of the module. Must be greater than zero.
 * @param   fFlags          Flags reserved for future extensions, MBZ for now.
 */
RTDECL(int)  RTDbgModCreate(PRTDBGMOD phDbgMod, const char *pszName, RTUINTPTR cb, uint32_t fFlags)
{
    /*
     * Input validation and lazy initialization.
     */
    AssertPtrReturn(phDbgMod, VERR_INVALID_POINTER);
    *phDbgMod = NIL_RTDBGMOD;
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertReturn(*pszName, VERR_INVALID_PARAMETER);
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER);

    int rc = rtDbgModLazyInit();
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Allocate a new module instance.
     */
    PRTDBGMODINT pDbgMod = (PRTDBGMODINT)RTMemAllocZ(sizeof(*pDbgMod));
    if (!pDbgMod)
        return VERR_NO_MEMORY;
    pDbgMod->u32Magic = RTDBGMOD_MAGIC;
    pDbgMod->cRefs = 1;
    rc = RTCritSectInit(&pDbgMod->CritSect);
    if (RT_SUCCESS(rc))
    {
        pDbgMod->pszName = RTStrDup(pszName);
        if (pDbgMod->pszName)
        {
            rc = rtDbgModContainerCreate(pDbgMod, cb);
            if (RT_SUCCESS(rc))
            {
                *phDbgMod = pDbgMod;
                return rc;
            }
            RTStrFree(pDbgMod->pszName);
        }
        RTCritSectDelete(&pDbgMod->CritSect);
    }

    RTMemFree(pDbgMod);
    return rc;
}


RTDECL(int)         RTDbgModCreateDeferred(PRTDBGMOD phDbgMod, const char *pszFilename, const char *pszName, RTUINTPTR cb, uint32_t fFlags)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int)         RTDbgModCreateFromImage(PRTDBGMOD phDbgMod, const char *pszFilename, const char *pszName, uint32_t fFlags)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModCreateFromMap(PRTDBGMOD phDbgMod, const char *pszFilename, const char *pszName, RTUINTPTR uSubtrahend, uint32_t fFlags)
{
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Destroys an module after the reference count has reached zero.
 *
 * @param   pDbgMod     The module instance.
 */
static void  rtDbgModDestroy(PRTDBGMODINT pDbgMod)
{
    /*
     * Close the debug info interpreter first, then the image interpret.
     */
    RTCritSectEnter(&pDbgMod->CritSect); /* paranoia  */

    if (pDbgMod->pDbgVt)
    {
        pDbgMod->pDbgVt->pfnClose(pDbgMod);
        pDbgMod->pDbgVt = NULL;
        pDbgMod->pvDbgPriv = NULL;
    }

    if (pDbgMod->pImgVt)
    {
        pDbgMod->pImgVt->pfnClose(pDbgMod);
        pDbgMod->pImgVt = NULL;
        pDbgMod->pvImgPriv = NULL;
    }

    /*
     * Free the resources.
     */
    ASMAtomicWriteU32(&pDbgMod->u32Magic, ~RTDBGMOD_MAGIC);
    RTStrFree(pDbgMod->pszName);
    RTStrFree(pDbgMod->pszImgFile);
    RTStrFree(pDbgMod->pszDbgFile);
    RTCritSectLeave(&pDbgMod->CritSect); /* paranoia  */
    RTCritSectDelete(&pDbgMod->CritSect);
    RTMemFree(pDbgMod);
}


/**
 * Retains another reference to the module.
 *
 * @returns New reference count, UINT32_MAX on invalid handle (asserted).
 *
 * @param   hDbgMod         The module handle.
 *
 * @remarks Will not take any locks.
 */
RTDECL(uint32_t) RTDbgModRetain(RTDBGMOD hDbgMod)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, UINT32_MAX);
    return ASMAtomicIncU32(&pDbgMod->cRefs);
}


/**
 * Release a reference to the module.
 *
 * When the reference count reaches zero, the module is destroyed.
 *
 * @returns New reference count, UINT32_MAX on invalid handle (asserted).
 *
 * @param   hDbgMod         The module handle. The NIL handle is quietly ignored
 *                          and 0 is returned.
 *
 * @remarks Will not take any locks.
 */
RTDECL(uint32_t) RTDbgModRelease(RTDBGMOD hDbgMod)
{
    if (hDbgMod == NIL_RTDBGMOD)
        return 0;
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pDbgMod->cRefs);
    if (!cRefs)
        rtDbgModDestroy(pDbgMod);
    return cRefs;
}


/**
 * Gets the module name.
 *
 * @returns Pointer to a read only string containing the name.
 *
 * @param   hDbgMod             The module handle.
 */
RTDECL(const char *) RTDbgModName(RTDBGMOD hDbgMod)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, NULL);
    return pDbgMod->pszName;
}


RTDECL(RTUINTPTR)   RTDbgModImageSize(RTDBGMOD hDbgMod)
{
    return 1;
}

RTDECL(RTUINTPTR)   RTDbgModSegmentSize(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg)
{
    return 1;
}

RTDECL(RTUINTPTR)   RTDbgModSegmentRva(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg)
{
    return 0;
}

RTDECL(RTDBGSEGIDX) RTDbgModSegmentCount(RTDBGMOD hDbgMod)
{
    return 1;
}


/**
 * Adds a line number to the module.
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 * @retval  VERR_NOT_SUPPORTED if the module interpret doesn't support adding
 *          custom symbols.
 * @retval  VERR_DBG_SYMBOL_NAME_OUT_OF_RANGE
 * @retval  VERR_DBG_INVALID_RVA
 * @retval  VERR_DBG_INVALID_SEGMENT_INDEX
 * @retval  VERR_DBG_INVALID_SEGMENT_OFFSET
 * @retval  VERR_INVALID_PARAMETER
 *
 * @param   hDbgMod         The module handle.
 * @param   pszSymbol       The symbol name.
 * @param   iSeg            The segment index.
 * @param   off             The segment offset.
 * @param   cb              The size of the symbol.
 * @param   fFlags          Symbol flags.
 * @param   piOrdinal       Where to return the symbol ordinal on success. If
 *                          the interpreter doesn't do ordinals, this will be set to
 *                          UINT32_MAX. Optional
 */
RTDECL(int) RTDbgModSymbolAdd(RTDBGMOD hDbgMod, const char *pszSymbol, RTDBGSEGIDX iSeg, RTUINTPTR off, RTUINTPTR cb, uint32_t fFlags, uint32_t *piOrdinal)
{
    /*
     * Validate input.
     */
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, VERR_INVALID_HANDLE);
    AssertPtr(pszSymbol);
    size_t cchSymbol = strlen(pszSymbol);
    AssertReturn(cchSymbol, VERR_DBG_SYMBOL_NAME_OUT_OF_RANGE);
    AssertReturn(cchSymbol < RTDBG_SYMBOL_NAME_LENGTH, VERR_DBG_SYMBOL_NAME_OUT_OF_RANGE);
    AssertMsgReturn(   iSeg <= RTDBGSEGIDX_LAST
                    || (    iSeg >= RTDBGSEGIDX_SPECIAL_FIRST
                        &&  iSeg <= RTDBGSEGIDX_SPECIAL_LAST),
                    ("%#x\n", iSeg),
                    VERR_DBG_INVALID_SEGMENT_INDEX);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER); /* currently reserved. */

    RTDBGMOD_LOCK(pDbgMod);

    /*
     * Convert RVAs.
     */
    if (iSeg == RTDBGSEGIDX_RVA)
    {
        iSeg = pDbgMod->pDbgVt->pfnRvaToSegOff(pDbgMod, off, &off);
        if (iSeg == NIL_RTDBGSEGIDX)
        {
            RTDBGMOD_UNLOCK(pDbgMod);
            return VERR_DBG_INVALID_RVA;
        }
    }

    /*
     * Get down to business.
     */
    int rc = pDbgMod->pDbgVt->pfnSymbolAdd(pDbgMod, pszSymbol, cchSymbol, iSeg, off, cb, fFlags, piOrdinal);

    RTDBGMOD_UNLOCK(pDbgMod);
    return rc;
}


RTDECL(uint32_t)    RTDbgModSymbolCount(RTDBGMOD hDbgMod)
{
    return 1;
}

RTDECL(int)         RTDbgModSymbolByIndex(RTDBGMOD hDbgMod, uint32_t iSymbol, PRTDBGSYMBOL pSymbol)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModSymbolByAddr(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGSYMBOL pSymbol)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModSymbolByAddrA(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGSYMBOL *ppSymbol)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModSymbolByName(RTDBGMOD hDbgMod, const char *pszSymbol, PRTDBGSYMBOL pSymbol)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModSymbolByNameA(RTDBGMOD hDbgMod, const char *pszSymbol, PRTDBGSYMBOL *ppSymbol)
{
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Adds a line number to the module.
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 * @retval  VERR_NOT_SUPPORTED if the module interpret doesn't support adding
 *          custom symbols.
 * @retval  VERR_DBG_FILE_NAME_OUT_OF_RANGE
 * @retval  VERR_DBG_INVALID_RVA
 * @retval  VERR_DBG_INVALID_SEGMENT_INDEX
 * @retval  VERR_DBG_INVALID_SEGMENT_OFFSET
 * @retval  VERR_INVALID_PARAMETER
 *
 * @param   hDbgMod         The module handle.
 * @param   pszFile         The file name.
 * @param   uLineNo         The line number.
 * @param   iSeg            The segment index.
 * @param   off             The segment offset.
 * @param   piOrdinal       Where to return the line number ordinal on success.
 *                          If the interpreter doesn't do ordinals, this will be
 *                          set to UINT32_MAX. Optional.
 */
RTDECL(int) RTDbgModLineAdd(RTDBGMOD hDbgMod, const char *pszFile, uint32_t uLineNo, RTDBGSEGIDX iSeg, RTUINTPTR off, uint32_t *piOrdinal)
{
    /*
     * Validate input.
     */
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, VERR_INVALID_HANDLE);
    AssertPtr(pszFile);
    size_t cchFile = strlen(pszFile);
    AssertReturn(cchFile, VERR_DBG_FILE_NAME_OUT_OF_RANGE);
    AssertReturn(cchFile < RTDBG_FILE_NAME_LENGTH, VERR_DBG_FILE_NAME_OUT_OF_RANGE);
    AssertMsgReturn(   iSeg <= RTDBGSEGIDX_LAST
                    || iSeg == RTDBGSEGIDX_RVA,
                    ("%#x\n", iSeg),
                    VERR_DBG_INVALID_SEGMENT_INDEX);
    AssertReturn(uLineNo > 0 && uLineNo < UINT32_MAX, VERR_INVALID_PARAMETER);

    RTDBGMOD_LOCK(pDbgMod);

    /*
     * Convert RVAs.
     */
    if (iSeg == RTDBGSEGIDX_RVA)
    {
        iSeg = pDbgMod->pDbgVt->pfnRvaToSegOff(pDbgMod, off, &off);
        if (iSeg == NIL_RTDBGSEGIDX)
        {
            RTDBGMOD_UNLOCK(pDbgMod);
            return VERR_DBG_INVALID_RVA;
        }
    }

    /*
     * Get down to business.
     */
    int rc = pDbgMod->pDbgVt->pfnLineAdd(pDbgMod, pszFile, cchFile, uLineNo, iSeg, off, piOrdinal);

    RTDBGMOD_UNLOCK(pDbgMod);
    return rc;
}


RTDECL(int)         RTDbgModLineByAddr(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGLINE pLine)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModLineByAddrA(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGLINE *ppLine)
{
    return VERR_NOT_IMPLEMENTED;
}

