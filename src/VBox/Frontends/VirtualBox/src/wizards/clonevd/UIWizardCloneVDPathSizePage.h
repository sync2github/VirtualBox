/* $Id: UIWizardCloneVDPathSizePage.h 91062 2021-09-01 14:43:18Z vboxsync $ */
/** @file
 * VBox Qt GUI - UIWizardCloneVDPathSizePage class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDPathSizePage_h
#define FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDPathSizePage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QVariant>
#include <QSet>

/* GUI includes: */
#include "UINativeWizardPage.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class UIMediumSizeAndPathGroupBox;

/** 4th page of the Clone Virtual Disk Image wizard (basic extension): */
class UIWizardCloneVDPathSizePage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs basic page. */
    UIWizardCloneVDPathSizePage(qulonglong uSourceDiskLogicaSize);

private slots:

    /** Handles command to open target disk. */
    void sltSelectLocationButtonClicked();
    void sltMediumPathChanged(const QString &strPath);
    void sltMediumSizeChanged(qulonglong uSize);

private:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;
    void prepare(qulonglong uSourceDiskLogicaSize);

    /** Prepares the page. */
    virtual void initializePage() /* override */;

    /** Returns whether the page is complete. */
    virtual bool isComplete() const /* override */;

    /** Returns whether the page is valid. */
    virtual bool validatePage() /* override */;

    UIMediumSizeAndPathGroupBox *m_pMediumSizePathGroupBox;
    QSet<QString> m_userModifiedParameters;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDPathSizePage_h */
