/* $Id: UIDownloaderExtensionPack.h 90560 2021-08-07 07:36:02Z vboxsync $ */
/** @file
 * VBox Qt GUI - UIDownloaderExtensionPack class declaration.
 */

/*
 * Copyright (C) 2011-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_networking_UIDownloaderExtensionPack_h
#define FEQT_INCLUDED_SRC_networking_UIDownloaderExtensionPack_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIDownloader.h"

/* Forward declarations: */
class QByteArray;

/** UIDownloader extension for background extension-pack downloading. */
class SHARED_LIBRARY_STUFF UIDownloaderExtensionPack : public UIDownloader
{
    Q_OBJECT;

signals:

    /** Notifies listeners about downloading finished.
      * @param  strSource  Brings the downloading source.
      * @param  strTarget  Brings the downloading target.
      * @param  strHash    Brings the downloaded file hash. */
    void sigDownloadFinished(const QString &strSource, const QString &strTarget, const QString &strHash);

public:

    /** Constructs downloader. */
    UIDownloaderExtensionPack();

private:

    /** Returns description of the current network operation. */
    virtual QString description() const /* override */;

    /** Asks user for downloading confirmation for passed @a pReply. */
    virtual bool askForDownloadingConfirmation(UINetworkReply *pReply) /* override */;
    /** Handles downloaded object for passed @a pReply. */
    virtual void handleDownloadedObject(UINetworkReply *pReply) /* override */;
    /** Handles verified object for passed @a pReply. */
    virtual void handleVerifiedObject(UINetworkReply *pReply) /* override */;

    /** Holds the cached received data awaiting for verification. */
    QByteArray m_receivedData;
};

#endif /* !FEQT_INCLUDED_SRC_networking_UIDownloaderExtensionPack_h */
