/* $Id: RTLocaleQueryNormalizedBaseLocaleName-win.cpp 82968 2020-02-04 10:35:17Z vboxsync $ */
/** @file
 * IPRT - RTLocaleQueryNormalizedBaseLocaleName, ring-3, Windows.
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/win/windows.h>

#include <iprt/locale.h>
#include "internal/iprt.h"

#include <iprt/ctype.h>
#include <iprt/errcore.h>
#include <iprt/string.h>


RTDECL(int) RTLocaleQueryNormalizedBaseLocaleName(char *pszName, size_t cbName)
{
    /*
     * Note! This part is duplicate of r3/generic/RTLocaleQueryNormalizedBaseLocaleName-r3-generic.cpp!
     */
    char szLocale[_1K];
    int rc = RTLocaleQueryLocaleName(szLocale, sizeof(szLocale));
    if (RT_SUCCESS(rc))
    {
        /*
         * May return some complicated "LC_XXX=yyy;LC.." sequence if
         * partially set (like IPRT does).  Try get xx_YY sequence first
         * because 'C' or 'POSIX' may be LC_xxx variants that haven't been
         * set yet.
         *
         * ASSUMES complicated locale mangling is done in a certain way...
         */
        const char *pszLocale = strchr(szLocale, '=');
        if (!pszLocale)
            pszLocale = szLocale;
        else
            pszLocale++;
        bool fSeenC = false;
        bool fSeenPOSIX = false;
        do
        {
            const char *pszEnd = strchr(pszLocale, ';');

            if (   RTLOCALE_IS_LANGUAGE2_UNDERSCORE_COUNTRY2(pszLocale)
                && (   pszLocale[5] == '\0'
                    || RT_C_IS_PUNCT(pszLocale[5])) )
                return RTStrCopyEx(pszName, cbName, pszLocale, 5);

            if (   pszLocale[0] == 'C'
                && (   pszLocale[1] == '\0'
                    || RT_C_IS_PUNCT(pszLocale[1])) )
                fSeenC = true;
            else if (   strncmp(pszLocale, "POSIX", 5) == 0
                     && (   pszLocale[5] == '\0'
                        || RT_C_IS_PUNCT(pszLocale[5])) )
                fSeenPOSIX = true;

            /* advance */
            pszLocale = pszEnd ? strchr(pszEnd + 1, '=') : NULL;
        } while (pszLocale++);

        if (fSeenC || fSeenPOSIX)
            return RTStrCopy(pszName, cbName, "C"); /* C and POSIX should be identical IIRC, so keep it simple. */

        rc = VERR_NOT_AVAILABLE;
    }

    /*
     * Fallback.
     */
    if (   GetLocaleInfoA(GetUserDefaultLCID(), LOCALE_SISO639LANGNAME, szLocale, sizeof(szLocale)) == 3
        && GetLocaleInfoA(GetUserDefaultLCID(), LOCALE_SISO3166CTRYNAME, &szLocale[3], sizeof(szLocale) - 4) == 3)
    {
        szLocale[2] = '_';
        Assert(RTLOCALE_IS_LANGUAGE2_UNDERSCORE_COUNTRY2(szLocale));
        return RTStrCopy(pszName, cbName, szLocale);
    }

    return rc;
}

