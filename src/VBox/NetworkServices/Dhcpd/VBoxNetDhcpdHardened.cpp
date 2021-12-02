/* $Id: VBoxNetDhcpdHardened.cpp 82968 2020-02-04 10:35:17Z vboxsync $ */
/** @file
 * VBoxNetDhcpd - Hardened main().
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/sup.h>


int main(int argc, char **argv, char **envp)
{
    return SUPR3HardenedMain("VBoxNetDHCP", 0 /* fFlags */, argc, argv, envp);
}

