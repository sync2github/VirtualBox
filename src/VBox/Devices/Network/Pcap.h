/* $Id: Pcap.h 82968 2020-02-04 10:35:17Z vboxsync $ */
/** @file
 * Helpers for writing libpcap files.
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

#ifndef VBOX_INCLUDED_SRC_Network_Pcap_h
#define VBOX_INCLUDED_SRC_Network_Pcap_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/stream.h>
#include <VBox/types.h>

RT_C_DECLS_BEGIN

int PcapStreamHdr(PRTSTREAM pStream, uint64_t StartNanoTS);
int PcapStreamFrame(PRTSTREAM pStream, uint64_t StartNanoTS, const void *pvFrame, size_t cbFrame, size_t cbMax);
int PcapStreamGsoFrame(PRTSTREAM pStream, uint64_t StartNanoTS, PCPDMNETWORKGSO pGso,
                       const void *pvFrame, size_t cbFrame, size_t cbMax);

int PcapFileHdr(RTFILE File, uint64_t StartNanoTS);
int PcapFileFrame(RTFILE File, uint64_t StartNanoTS, const void *pvFrame, size_t cbFrame, size_t cbMax);
int PcapFileGsoFrame(RTFILE File, uint64_t StartNanoTS, PCPDMNETWORKGSO pGso,
                     const void *pvFrame, size_t cbFrame, size_t cbSegMax);

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_Network_Pcap_h */

