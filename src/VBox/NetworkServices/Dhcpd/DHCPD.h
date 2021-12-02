/* $Id: DHCPD.h 82968 2020-02-04 10:35:17Z vboxsync $ */
/** @file
 * DHCP server - protocol logic
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
 */

#ifndef VBOX_INCLUDED_SRC_Dhcpd_DHCPD_h
#define VBOX_INCLUDED_SRC_Dhcpd_DHCPD_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "DhcpdInternal.h"
#include <iprt/cpp/ministring.h>
#include "Config.h"
#include "DhcpMessage.h"
#include "Db.h"


/**
 * The core of the DHCP server.
 *
 * This class is feed DhcpClientMessages that VBoxNetDhcpd has picked up from
 * the network.  After processing a message it returns the appropriate response
 * (if any) which VBoxNetDhcpd sends out.
 */
class DHCPD
{
    /** The DHCP configuration. */
    const Config   *m_pConfig;
    /** The lease database. */
    Db              m_db;

public:
    DHCPD();

    int init(const Config *) RT_NOEXCEPT;

    DhcpServerMessage *process(const std::unique_ptr<DhcpClientMessage> &req) RT_NOEXCEPT
    {
        if (req.get() != NULL)
            return process(*req.get());
        return NULL;
    }

    DhcpServerMessage *process(DhcpClientMessage &req) RT_NOEXCEPT;

private:
    /** @name DHCP message processing methods
     * @{ */
    DhcpServerMessage *i_doDiscover(const DhcpClientMessage &req);
    DhcpServerMessage *i_doRequest(const DhcpClientMessage &req);
    DhcpServerMessage *i_doInform(const DhcpClientMessage &req);
    DhcpServerMessage *i_doDecline(const DhcpClientMessage &req) RT_NOEXCEPT;
    DhcpServerMessage *i_doRelease(const DhcpClientMessage &req) RT_NOEXCEPT;

    DhcpServerMessage *i_createMessage(int type, const DhcpClientMessage &req);
    /** @} */

    /** @name Lease database handling
     * @{ */
    int                i_loadLeases() RT_NOEXCEPT;
    void               i_saveLeases() RT_NOEXCEPT;
    /** @} */
};

#endif /* !VBOX_INCLUDED_SRC_Dhcpd_DHCPD_h */
