/* $Id: UIShortcutConfigurationEditor.h 86124 2020-09-14 17:16:23Z vboxsync $ */
/** @file
 * VBox Qt GUI - UIShortcutConfigurationEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIShortcutConfigurationEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIShortcutConfigurationEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declartions: */
class QLineEdit;
class QTabWidget;
class UIShortcutConfigurationModel;
class UIShortcutConfigurationTable;

/** Shortcut search functor template. */
template <class BaseClass>
class UIShortcutSearchFunctor : public BaseClass
{
public:

    /** Constructs shortcut search functor. */
    UIShortcutSearchFunctor() {}

    /** Returns the position of the 1st occurrence of the
      * @a shortcut in the @a shortcuts list, or -1 otherwise. */
    int operator()(const QList<BaseClass> &shortcuts, const BaseClass &shortcut)
    {
        for (int i = 0; i < shortcuts.size(); ++i)
        {
            const BaseClass &iteratedShortcut = shortcuts.at(i);
            if (iteratedShortcut.key() == shortcut.key())
                return i;
        }
        return -1;
    }
};

/** Shortcut configuration item. */
class SHARED_LIBRARY_STUFF UIShortcutConfigurationItem
{
public:

    /** Constructs shortcut configuration item on the basis of passed arguments.
      * @param  strKey              Brings the unique key identifying held sequence.
      * @param  strScope            Brings the scope of the held sequence.
      * @param  strDescription      Brings the deescription for the held sequence.
      * @param  strCurrentSequence  Brings the current held sequence.
      * @param  strDefaultSequence  Brings the default held sequence. */
    UIShortcutConfigurationItem(const QString &strKey = QString(),
                                const QString &strScope = QString(),
                                const QString &strDescription = QString(),
                                const QString &strCurrentSequence = QString(),
                                const QString &strDefaultSequence = QString())
        : m_strKey(strKey)
        , m_strScope(strScope)
        , m_strDescription(strDescription)
        , m_strCurrentSequence(strCurrentSequence)
        , m_strDefaultSequence(strDefaultSequence)
    {}

    /** Constructs shortcut configuration item on the basis of @a another one. */
    UIShortcutConfigurationItem(const UIShortcutConfigurationItem &another)
        : m_strKey(another.key())
        , m_strScope(another.scope())
        , m_strDescription(another.description())
        , m_strCurrentSequence(another.currentSequence())
        , m_strDefaultSequence(another.defaultSequence())
    {}

    /** Returns the key. */
    QString key() const { return m_strKey; }
    /** Returns the scope. */
    QString scope() const { return m_strScope; }
    /** Returns the description. */
    QString description() const { return m_strDescription; }
    /** Returns the current sequence. */
    QString currentSequence() const { return m_strCurrentSequence; }
    /** Returns the default sequence. */
    QString defaultSequence() const { return m_strDefaultSequence; }

    /** Defines @a strCurrentSequence. */
    void setCurrentSequence(const QString &strCurrentSequence) { m_strCurrentSequence = strCurrentSequence; }

    /** Copies an item from @a another one. */
    UIShortcutConfigurationItem &operator=(const UIShortcutConfigurationItem &another)
    {
        /* Reassign variables: */
        m_strKey = another.key();
        m_strScope = another.scope();
        m_strDescription = another.description();
        m_strCurrentSequence = another.currentSequence();
        m_strDefaultSequence = another.defaultSequence();

        /* Return this: */
        return *this;
    }

    /** Returns whether this item equals to @a another one. */
    bool operator==(const UIShortcutConfigurationItem &another) const
    {
        /* Compare by key, scope and current sequence: */
        return    true
               && (key() == another.key())
               && (scope() == another.scope())
               && (currentSequence() == another.currentSequence())
                  ;
    }

private:

    /** Holds the key. */
    QString m_strKey;
    /** Holds the scope. */
    QString m_strScope;
    /** Holds the description. */
    QString m_strDescription;
    /** Holds the current sequence. */
    QString m_strCurrentSequence;
    /** Holds the default sequence. */
    QString m_strDefaultSequence;
};

/** Shortcut configuration list. */
typedef QList<UIShortcutConfigurationItem> UIShortcutConfigurationList;

/** QWidget subclass used as a shortcut configuration editor. */
class SHARED_LIBRARY_STUFF UIShortcutConfigurationEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

    /** Table indexes. */
    enum { TableIndex_Manager, TableIndex_Runtime };

signals:

    /** Notifies listeners about value change. */
    void sigValueChanged();

public:

    /** Constructs shortcut configuration editor passing @a pParent to the base-class. */
    UIShortcutConfigurationEditor(QWidget *pParent = 0);

    /** Loads shortcut configuration list from passed @a value. */
    void load(const UIShortcutConfigurationList &value);
    /** Saves shortcut configuration list to passed @a value. */
    void save(UIShortcutConfigurationList &value) const;

    /** Returns whether manager shortcuts are unique. */
    bool isShortcutsUniqueManager() const;
    /** Returns whether runtime shortcuts are unique. */
    bool isShortcutsUniqueRuntime() const;

    /** Returns manager tab name. */
    QString tabNameManager() const;
    /** Returns runtime tab name. */
    QString tabNameRuntime() const;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares Manager UI tab. */
    void prepareTabManager();
    /** Prepares Runtime UI tab. */
    void prepareTabRuntime();
    /** Prepares connections. */
    void prepareConnections();

    /** Holds the Manager UI shortcut configuration model instance. */
    UIShortcutConfigurationModel *m_pModelManager;
    /** Holds the Runtime UI shortcut configuration model instance. */
    UIShortcutConfigurationModel *m_pModelRuntime;

    /** Holds the tab-widget instance. */
    QTabWidget                   *m_pTabWidget;
    /** Holds the Manager UI shortcuts filter instance. */
    QLineEdit                    *m_pEditorFilterManager;
    /** Holds the Manager UI shortcuts table instance. */
    UIShortcutConfigurationTable *m_pTableManager;
    /** Holds the Runtime UI shortcuts filter instance. */
    QLineEdit                    *m_pEditorFilterRuntime;
    /** Holds the Runtime UI shortcuts table instance. */
    UIShortcutConfigurationTable *m_pTableRuntime;
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIShortcutConfigurationEditor_h */
