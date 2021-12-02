/* $Id: UIProgressTask.h 90254 2021-07-20 10:54:00Z vboxsync $ */
/** @file
 * VBox Qt GUI - UIProgressTask class declaration.
 */

/*
 * Copyright (C) 2020-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_globals_UIProgressTask_h
#define FEQT_INCLUDED_SRC_globals_UIProgressTask_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>
#include <QPointer>

/* GUI includes: */
#include "UILibraryDefs.h"
#include "UIProgressObject.h"

/* COM includes: */
#include "CProgress.h"

/* Forward declarations: */
class QTimer;

/** QObject-based interface allowing to plan UIProgressObject-based tasks
  * to be seamlessly and asynchronously scheduled (in time) and executed. */
class SHARED_LIBRARY_STUFF UIProgressTask : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listeners about progress has started. */
    void sigProgressStarted();
    /** Notifies listeners about progress has changed.
      * @param  uPercent  Brings the progress percentage. */
    void sigProgressChange(ulong uPercent);
    /** Notifies listeners about progress has finished. */
    void sigProgressFinished();

public:

    /** Creates progress task passing @a pParent to the base-class. */
    UIProgressTask(QObject *pParent);
    /** Creates progress task passing @a pParent to the base-class. */
    virtual ~UIProgressTask() /* override */;

    /** Returns whether task is scheduled. */
    bool isScheduled() const;

    /** Returns whether task is running. */
    bool isRunning() const;

    /** Returns whether task is cancelable. */
    bool isCancelable() const;

public slots:

    /** Schedules task to be executed in @a iMsec. */
    void schedule(int iMsec);

    /** Starts the task directly.
      * @note It will also be started automatically if scheduled. */
    void start();
    /** Cancels the task directly. */
    void cancel();

protected:

    /** Creates and returns started progress-wrapper required to init UIProgressObject. */
    virtual CProgress createProgress() = 0;
    /** Allows sub-class to handle finished @a comProgress wrapper. */
    virtual void handleProgressFinished(CProgress &comProgress) = 0;

private slots:

    /** Handles progress change.
      * @param  iOperations   Brings the number of operations CProgress have.
      * @param  strOperation  Brings the description of the current CProgress operation.
      * @param  iOperation    Brings the index of the current CProgress operation.
      * @param  iPercent      Brings the percentage of the current CProgress operation. */
    void sltHandleProgressChange(ulong uOperations, QString strOperation,
                                 ulong uOperation, ulong uPercent);
    /** Handles progress event handling finished signal. */
    void sltHandleProgressEventHandlingFinished();

private:

    /** Prepares all. */
    void prepare();
    /** Cleanups all. */
    void cleanup();

    /** Holds the schedule timer instance. */
    QTimer                     *m_pTimer;
    /** Holds the progress-wrapper instance. */
    CProgress                   m_comProgress;
    /** Holds the progress-object instance. */
    QPointer<UIProgressObject>  m_pProgressObject;
};

#endif /* !FEQT_INCLUDED_SRC_globals_UIProgressTask_h */
