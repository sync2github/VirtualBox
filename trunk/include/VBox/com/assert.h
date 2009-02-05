/** @file
 * MS COM / XPCOM Abstraction Layer:
 * Assertion macros for COM/XPCOM
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef ___VBox_com_assert_h
#define ___VBox_com_assert_h

#include <iprt/assert.h>

/**
 *  Asserts that the COM result code is succeeded in strict builds.
 *  In non-strict builds the result code will be NOREF'ed to kill compiler warnings.
 *
 *  @param rc   COM result code
 */
#define AssertComRC(rc)      \
    do { AssertMsg (SUCCEEDED (rc), ("COM RC = %Rhrc (0x%08X)\n", rc, rc)); NOREF (rc); } while (0)

/**
 *  A special version of AssertComRC that returns the given expression
 *  if the result code is failed.
 *
 *  @param rc   COM result code
 *  @param ret  the expression to return
 */
#define AssertComRCReturn(rc, ret)      \
    AssertMsgReturn (SUCCEEDED (rc), ("COM RC = %Rhrc (0x%08X)\n", rc, rc), ret)

/**
 *  A special version of AssertComRC that returns the given result code
 *  if it is failed.
 *
 *  @param rc   COM result code
 *  @param ret  the expression to return
 */
#define AssertComRCReturnRC(rc)         \
    AssertMsgReturn (SUCCEEDED (rc), ("COM RC = %Rhrc (0x%08X)\n", rc, rc), rc)

/**
 *  A special version of AssertComRC that returns if the result code is failed.
 *
 *  @param rc   COM result code
 *  @param ret  the expression to return
 */
#define AssertComRCReturnVoid(rc)      \
    AssertMsgReturnVoid (SUCCEEDED (rc), ("COM RC = %Rhrc (0x%08X)\n", rc, rc))

/**
 *  A special version of AssertComRC that evaluates the given expression and
 *  breaks if the result code is failed.
 *
 *  @param rc   COM result code
 *  @param eval the expression to evaluate
 */
#define AssertComRCBreak(rc, eval)      \
    if (!SUCCEEDED (rc)) { AssertComRC (rc); eval; break; } else do {} while (0)

/**
 *  A special version of AssertComRC that evaluates the given expression and
 *  throws it if the result code is failed.
 *
 *  @param rc   COM result code
 *  @param eval the expression to throw
 */
#define AssertComRCThrow(rc, eval)      \
    if (!SUCCEEDED (rc)) { AssertComRC (rc); throw (eval); } else do {} while (0)

/**
 *  A special version of AssertComRC that just breaks if the result code is
 *  failed.
 *
 *  @param rc   COM result code
 */
#define AssertComRCBreakRC(rc)          \
    if (!SUCCEEDED (rc)) { AssertComRC (rc); break; } else do {} while (0)

/**
 *  A special version of AssertComRC that just throws @a rc if the result code is
 *  failed.
 *
 *  @param rc   COM result code
 */
#define AssertComRCThrowRC(rc)          \
    if (!SUCCEEDED (rc)) { AssertComRC (rc); throw rc; } else do {} while (0)

/**
 *  Checks whether the given COM result code is successful.
 *  If not, executes the return statement with this result code.
 *
 *  @param rc   COM result code
 */
#define CheckComRCReturnRC(rc)      \
    if (!SUCCEEDED (rc)) { return (rc); } else do {} while (0)

/**
 *  Checks whether the given COM result code is successful.
 *  If not, executes the break statement.
 *
 *  @param rc   COM result code
 */
#define CheckComRCBreakRC(rc)      \
    if (!SUCCEEDED (rc)) { break; } else do {} while (0)

/**
 *  Checks whether the given COM result code is successful.
 *  If not, throws the given COM result.
 *
 *  @param rc   COM result code
 */
#define CheckComRCThrowRC(rc)      \
    if (!SUCCEEDED (rc)) { throw rc; } else do {} while (0)


#endif

