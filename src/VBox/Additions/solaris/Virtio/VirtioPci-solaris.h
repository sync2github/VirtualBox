/* $Id: VirtioPci-solaris.h 82968 2020-02-04 10:35:17Z vboxsync $ */
/** @file
 * VirtualBox Guest Additions: Virtio Driver for Solaris, PCI Hypervisor Interface.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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

#ifndef GA_INCLUDED_SRC_solaris_Virtio_VirtioPci_solaris_h
#define GA_INCLUDED_SRC_solaris_Virtio_VirtioPci_solaris_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "Virtio-solaris.h"

extern VIRTIOHYPEROPS g_VirtioHyperOpsPci;

#endif /* !GA_INCLUDED_SRC_solaris_Virtio_VirtioPci_solaris_h */

