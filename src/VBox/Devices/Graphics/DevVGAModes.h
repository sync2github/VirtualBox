/* $Id: DevVGAModes.h 82968 2020-02-04 10:35:17Z vboxsync $ */
/** @file
 * DevVGA - VBox VGA/VESA device, VBE modes.
 *
 * List of static mode information, containing all "supported" VBE
 * modes and their 'settings'.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Graphics_DevVGAModes_h
#define VBOX_INCLUDED_SRC_Graphics_DevVGAModes_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBoxVideoVBE.h>
#include <VBoxVideoVBEPrivate.h>

#include "vbetables.h"

#define MODE_INFO_SIZE ( sizeof(mode_info_list) / sizeof(ModeInfoListItem) )

#endif /* !VBOX_INCLUDED_SRC_Graphics_DevVGAModes_h */

