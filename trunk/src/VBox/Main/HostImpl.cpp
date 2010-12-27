/* $Id$ */
/** @file
 * VirtualBox COM class implementation: Host
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

// for some reason Windows burns in sdk\...\winsock.h if this isn't included first
#include "VBox/com/ptr.h"

#include "HostImpl.h"

#ifdef VBOX_WITH_USB
# include "HostUSBDeviceImpl.h"
# include "USBDeviceFilterImpl.h"
# include "USBProxyService.h"
#endif // VBOX_WITH_USB

#include "HostNetworkInterfaceImpl.h"
#include "MachineImpl.h"
#include "AutoCaller.h"
#include "Logging.h"
#include "Performance.h"

#include "MediumImpl.h"
#include "HostPower.h"

#if defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)
# include <HostHardwareLinux.h>
#endif

#ifdef VBOX_WITH_RESOURCE_USAGE_API
# include "PerformanceImpl.h"
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

#if defined(RT_OS_WINDOWS) && defined(VBOX_WITH_NETFLT)
# include <VBox/WinNetConfig.h>
#endif /* #if defined(RT_OS_WINDOWS) && defined(VBOX_WITH_NETFLT) */

#ifdef RT_OS_LINUX
# include <sys/ioctl.h>
# include <errno.h>
# include <net/if.h>
# include <net/if_arp.h>
#endif /* RT_OS_LINUX */

#ifdef RT_OS_SOLARIS
# include <fcntl.h>
# include <unistd.h>
# include <stropts.h>
# include <errno.h>
# include <limits.h>
# include <stdio.h>
# include <libdevinfo.h>
# include <sys/mkdev.h>
# include <sys/scsi/generic/inquiry.h>
# include <net/if.h>
# include <sys/socket.h>
# include <sys/sockio.h>
# include <net/if_arp.h>
# include <net/if.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/cdio.h>
# include <sys/dkio.h>
# include <sys/mnttab.h>
# include <sys/mntent.h>
/* Dynamic loading of libhal on Solaris hosts */
# ifdef VBOX_USE_LIBHAL
#  include "vbox-libhal.h"
extern "C" char *getfullrawname(char *);
# endif
# include "solaris/DynLoadLibSolaris.h"

/**
 * Solaris DVD drive list as returned by getDVDInfoFromDevTree().
 */
typedef struct SOLARISDVD
{
    struct SOLARISDVD *pNext;
    char szDescription[512];
    char szRawDiskPath[PATH_MAX];
} SOLARISDVD;
/** Pointer to a Solaris DVD descriptor. */
typedef SOLARISDVD *PSOLARISDVD;

#endif /* RT_OS_SOLARIS */

#ifdef RT_OS_WINDOWS
# define _WIN32_DCOM
# include <windows.h>
# include <shellapi.h>
# define INITGUID
# include <guiddef.h>
# include <devguid.h>
# include <objbase.h>
//# include <setupapi.h>
# include <shlobj.h>
# include <cfgmgr32.h>

#endif /* RT_OS_WINDOWS */

#ifdef RT_OS_DARWIN
# include "darwin/iokit.h"
#endif

#ifdef VBOX_WITH_CROGL
extern bool is3DAccelerationSupported();
#endif /* VBOX_WITH_CROGL */

#include <iprt/asm-amd64-x86.h>
#include <iprt/string.h>
#include <iprt/mp.h>
#include <iprt/time.h>
#include <iprt/param.h>
#include <iprt/env.h>
#include <iprt/mem.h>
#include <iprt/system.h>
#ifdef RT_OS_SOLARIS
# include <iprt/path.h>
# include <iprt/ctype.h>
#endif
#ifdef VBOX_WITH_HOSTNETIF_API
# include "netif.h"
#endif

/* XXX Solaris: definitions in /usr/include/sys/regset.h clash with hwacc_svm.h */
#undef DS
#undef ES
#undef CS
#undef SS
#undef FS
#undef GS

#include <VBox/usb.h>
#include <VBox/x86.h>
#include <VBox/vmm/hwacc_svm.h>
#include <VBox/err.h>
#include <VBox/settings.h>
#include <VBox/sup.h>

#include "VBox/com/MultiResult.h"

#include <stdio.h>

#include <algorithm>

////////////////////////////////////////////////////////////////////////////////
//
// Host private data definition
//
////////////////////////////////////////////////////////////////////////////////

struct Host::Data
{
    Data()
        :
#ifdef VBOX_WITH_USB
          usbListsLock(LOCKCLASS_USBLIST),
#endif
          drivesLock(LOCKCLASS_LISTOFMEDIA),
          fDVDDrivesListBuilt(false),
          fFloppyDrivesListBuilt(false)
    {};

    VirtualBox              *pParent;

#ifdef VBOX_WITH_USB
    WriteLockHandle         usbListsLock;               // protects the below two lists

    USBDeviceFilterList     llChildren;                 // all USB device filters
    USBDeviceFilterList     llUSBDeviceFilters;         // USB device filters in use by the USB proxy service

    /** Pointer to the USBProxyService object. */
    USBProxyService         *pUSBProxyService;
#endif /* VBOX_WITH_USB */

    // list of host drives; lazily created by getDVDDrives() and getFloppyDrives()
    WriteLockHandle         drivesLock;                 // protects the below two lists and the bools
    MediaList               llDVDDrives,
                            llFloppyDrives;
    bool                    fDVDDrivesListBuilt,
                            fFloppyDrivesListBuilt;

#if defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)
    /** Object with information about host drives */
    VBoxMainDriveInfo       hostDrives;
#endif
    /* Features that can be queried with GetProcessorFeature */
    BOOL                    fVTSupported,
                            fLongModeSupported,
                            fPAESupported,
                            fNestedPagingSupported;

    /* 3D hardware acceleration supported? */
    BOOL                    f3DAccelerationSupported;

    HostPowerService        *pHostPowerService;
};


////////////////////////////////////////////////////////////////////////////////
//
// Constructor / destructor
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Host::FinalConstruct()
{
    return S_OK;
}

void Host::FinalRelease()
{
    uninit();
}

/**
 * Initializes the host object.
 *
 * @param aParent   VirtualBox parent object.
 */
HRESULT Host::init(VirtualBox *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    m->pParent = aParent;

#ifdef VBOX_WITH_USB
    /*
     * Create and initialize the USB Proxy Service.
     */
# if defined (RT_OS_DARWIN)
    m->pUSBProxyService = new USBProxyServiceDarwin(this);
# elif defined (RT_OS_LINUX)
    m->pUSBProxyService = new USBProxyServiceLinux(this);
# elif defined (RT_OS_OS2)
    m->pUSBProxyService = new USBProxyServiceOs2(this);
# elif defined (RT_OS_SOLARIS)
    m->pUSBProxyService = new USBProxyServiceSolaris(this);
# elif defined (RT_OS_WINDOWS)
    m->pUSBProxyService = new USBProxyServiceWindows(this);
# elif defined (RT_OS_FREEBSD)
    m->pUSBProxyService = new USBProxyServiceFreeBSD(this);
# else
    m->pUSBProxyService = new USBProxyService(this);
# endif
    HRESULT hrc = m->pUSBProxyService->init();
    AssertComRCReturn(hrc, hrc);
#endif /* VBOX_WITH_USB */

#ifdef VBOX_WITH_RESOURCE_USAGE_API
    registerMetrics(aParent->performanceCollector());
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

#if defined (RT_OS_WINDOWS)
    m->pHostPowerService = new HostPowerServiceWin(m->pParent);
#elif defined (RT_OS_DARWIN)
    m->pHostPowerService = new HostPowerServiceDarwin(m->pParent);
#else
    m->pHostPowerService = new HostPowerService(m->pParent);
#endif

    /* Cache the features reported by GetProcessorFeature. */
    m->fVTSupported = false;
    m->fLongModeSupported = false;
    m->fPAESupported = false;
    m->fNestedPagingSupported = false;

    if (ASMHasCpuId())
    {
        uint32_t u32FeaturesECX;
        uint32_t u32Dummy;
        uint32_t u32FeaturesEDX;
        uint32_t u32VendorEBX, u32VendorECX, u32VendorEDX, u32AMDFeatureEDX, u32AMDFeatureECX;

        ASMCpuId(0, &u32Dummy, &u32VendorEBX, &u32VendorECX, &u32VendorEDX);
        ASMCpuId(1, &u32Dummy, &u32Dummy, &u32FeaturesECX, &u32FeaturesEDX);
        /* Query AMD features. */
        ASMCpuId(0x80000001, &u32Dummy, &u32Dummy, &u32AMDFeatureECX, &u32AMDFeatureEDX);

        m->fLongModeSupported = !!(u32AMDFeatureEDX & X86_CPUID_AMD_FEATURE_EDX_LONG_MODE);
        m->fPAESupported      = !!(u32FeaturesEDX & X86_CPUID_FEATURE_EDX_PAE);

        if (    u32VendorEBX == X86_CPUID_VENDOR_INTEL_EBX
            &&  u32VendorECX == X86_CPUID_VENDOR_INTEL_ECX
            &&  u32VendorEDX == X86_CPUID_VENDOR_INTEL_EDX
           )
        {
            if (    (u32FeaturesECX & X86_CPUID_FEATURE_ECX_VMX)
                 && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_MSR)
                 && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_FXSR)
               )
            {
                int rc = SUPR3QueryVTxSupported();
                if (RT_SUCCESS(rc))
                    m->fVTSupported = true;
            }
        }
        else
        if (    u32VendorEBX == X86_CPUID_VENDOR_AMD_EBX
            &&  u32VendorECX == X86_CPUID_VENDOR_AMD_ECX
            &&  u32VendorEDX == X86_CPUID_VENDOR_AMD_EDX
           )
        {
            if (   (u32AMDFeatureECX & X86_CPUID_AMD_FEATURE_ECX_SVM)
                && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_MSR)
                && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_FXSR)
               )
            {
                uint32_t u32SVMFeatureEDX;

                m->fVTSupported = true;

                /* Query AMD features. */
                ASMCpuId(0x8000000A, &u32Dummy, &u32Dummy, &u32Dummy, &u32SVMFeatureEDX);
                if (u32SVMFeatureEDX & AMD_CPUID_SVM_FEATURE_EDX_NESTED_PAGING)
                    m->fNestedPagingSupported = true;
            }
        }
    }

#if 0 /* needs testing */
    if (m->fVTSupported)
    {
        uint32_t u32Caps = 0;

        int rc = SUPR3QueryVTCaps(&u32Caps);
        if (RT_SUCCESS(rc))
        {
            if (u32Caps & SUPVTCAPS_NESTED_PAGING)
                m->fNestedPagingSupported = true;
        }
        /* else @todo; report BIOS trouble in some way. */
    }
#endif

    /* Test for 3D hardware acceleration support */
    m->f3DAccelerationSupported = false;

#ifdef VBOX_WITH_CROGL
    m->f3DAccelerationSupported = is3DAccelerationSupported();
#endif /* VBOX_WITH_CROGL */

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the host object and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Host::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

#ifdef VBOX_WITH_RESOURCE_USAGE_API
    unregisterMetrics (m->pParent->performanceCollector());
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

#ifdef VBOX_WITH_USB
    /* wait for USB proxy service to terminate before we uninit all USB
     * devices */
    LogFlowThisFunc(("Stopping USB proxy service...\n"));
    delete m->pUSBProxyService;
    m->pUSBProxyService = NULL;
    LogFlowThisFunc(("Done stopping USB proxy service.\n"));
#endif

    delete m->pHostPowerService;

#ifdef VBOX_WITH_USB
    /* uninit all USB device filters still referenced by clients
     * Note! HostUSBDeviceFilter::uninit() will modify llChildren. */
    while (!m->llChildren.empty())
    {
        ComObjPtr<HostUSBDeviceFilter> &pChild = m->llChildren.front();
        pChild->uninit();
    }

    m->llUSBDeviceFilters.clear();
#endif

    delete m;
    m = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// ISnapshot public methods
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Returns a list of host DVD drives.
 *
 * @returns COM status code
 * @param drives address of result pointer
 */
STDMETHODIMP Host::COMGETTER(DVDDrives)(ComSafeArrayOut(IMedium *, aDrives))
{
    CheckComArgOutSafeArrayPointerValid(aDrives);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(m->drivesLock COMMA_LOCKVAL_SRC_POS);

    MediaList *pList;
    HRESULT rc = getDrives(DeviceType_DVD, true /* fRefresh */, pList);
    if (SUCCEEDED(rc))
    {
        SafeIfaceArray<IMedium> array(*pList);
        array.detachTo(ComSafeArrayOutArg(aDrives));
    }

    return rc;
}

/**
 * Returns a list of host floppy drives.
 *
 * @returns COM status code
 * @param drives address of result pointer
 */
STDMETHODIMP Host::COMGETTER(FloppyDrives)(ComSafeArrayOut(IMedium *, aDrives))
{
    CheckComArgOutPointerValid(aDrives);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(m->drivesLock COMMA_LOCKVAL_SRC_POS);

    MediaList *pList;
    HRESULT rc = getDrives(DeviceType_Floppy, true /* fRefresh */, pList);
    if (SUCCEEDED(rc))
    {
        SafeIfaceArray<IMedium> collection(*pList);
        collection.detachTo(ComSafeArrayOutArg(aDrives));
    }

    return rc;
}


#if defined(RT_OS_WINDOWS) && defined(VBOX_WITH_NETFLT)
# define VBOX_APP_NAME L"VirtualBox"

static int vboxNetWinAddComponent(std::list< ComObjPtr<HostNetworkInterface> > *pPist,
                                  INetCfgComponent *pncc)
{
    LPWSTR              lpszName;
    GUID                IfGuid;
    HRESULT hr;
    int rc = VERR_GENERAL_FAILURE;

    hr = pncc->GetDisplayName( &lpszName );
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        Bstr name((CBSTR)lpszName);

        hr = pncc->GetInstanceGuid(&IfGuid);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            /* create a new object and add it to the list */
            ComObjPtr<HostNetworkInterface> iface;
            iface.createObject();
            /* remove the curly bracket at the end */
            if (SUCCEEDED(iface->init (name, Guid (IfGuid), HostNetworkInterfaceType_Bridged)))
            {
//                iface->setVirtualBox(m->pParent);
                pPist->push_back(iface);
                rc = VINF_SUCCESS;
            }
            else
            {
                Assert(0);
            }
        }
        CoTaskMemFree(lpszName);
    }

    return rc;
}
#endif /* defined(RT_OS_WINDOWS) && defined(VBOX_WITH_NETFLT) */

/**
 * Returns a list of host network interfaces.
 *
 * @returns COM status code
 * @param drives address of result pointer
 */
STDMETHODIMP Host::COMGETTER(NetworkInterfaces)(ComSafeArrayOut(IHostNetworkInterface*, aNetworkInterfaces))
{
#if defined(RT_OS_WINDOWS) ||  defined(VBOX_WITH_NETFLT) /*|| defined(RT_OS_OS2)*/
    if (ComSafeArrayOutIsNull(aNetworkInterfaces))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    std::list<ComObjPtr<HostNetworkInterface> > list;

# ifdef VBOX_WITH_HOSTNETIF_API
    int rc = NetIfList(list);
    if (rc)
    {
        Log(("Failed to get host network interface list with rc=%Rrc\n", rc));
    }
# else

#  if defined(RT_OS_DARWIN)
    PDARWINETHERNIC pEtherNICs = DarwinGetEthernetControllers();
    while (pEtherNICs)
    {
        ComObjPtr<HostNetworkInterface> IfObj;
        IfObj.createObject();
        if (SUCCEEDED(IfObj->init(Bstr(pEtherNICs->szName), Guid(pEtherNICs->Uuid), HostNetworkInterfaceType_Bridged)))
            list.push_back(IfObj);

        /* next, free current */
        void *pvFree = pEtherNICs;
        pEtherNICs = pEtherNICs->pNext;
        RTMemFree(pvFree);
    }

#  elif defined RT_OS_WINDOWS
#   ifndef VBOX_WITH_NETFLT
    hr = E_NOTIMPL;
#   else /* #  if defined VBOX_WITH_NETFLT */
    INetCfg              *pNc;
    INetCfgComponent     *pMpNcc;
    INetCfgComponent     *pTcpIpNcc;
    LPWSTR               lpszApp;
    HRESULT              hr;
    IEnumNetCfgBindingPath      *pEnumBp;
    INetCfgBindingPath          *pBp;
    IEnumNetCfgBindingInterface *pEnumBi;
    INetCfgBindingInterface *pBi;

    /* we are using the INetCfg API for getting the list of miniports */
    hr = VBoxNetCfgWinQueryINetCfg( FALSE,
                       VBOX_APP_NAME,
                       &pNc,
                       &lpszApp );
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
#    ifdef VBOX_NETFLT_ONDEMAND_BIND
        /* for the protocol-based approach for now we just get all miniports the MS_TCPIP protocol binds to */
        hr = pNc->FindComponent(L"MS_TCPIP", &pTcpIpNcc);
#    else
        /* for the filter-based approach we get all miniports our filter (sun_VBoxNetFlt)is bound to */
        hr = pNc->FindComponent(L"sun_VBoxNetFlt", &pTcpIpNcc);
#     ifndef VBOX_WITH_HARDENING
        if (hr != S_OK)
        {
            /* TODO: try to install the netflt from here */
        }
#     endif

#    endif

        if (hr == S_OK)
        {
            hr = VBoxNetCfgWinGetBindingPathEnum(pTcpIpNcc, EBP_BELOW, &pEnumBp);
            Assert(hr == S_OK);
            if ( hr == S_OK )
            {
                hr = VBoxNetCfgWinGetFirstBindingPath(pEnumBp, &pBp);
                Assert(hr == S_OK || hr == S_FALSE);
                while( hr == S_OK )
                {
                    /* S_OK == enabled, S_FALSE == disabled */
                    if (pBp->IsEnabled() == S_OK)
                    {
                        hr = VBoxNetCfgWinGetBindingInterfaceEnum(pBp, &pEnumBi);
                        Assert(hr == S_OK);
                        if ( hr == S_OK )
                        {
                            hr = VBoxNetCfgWinGetFirstBindingInterface(pEnumBi, &pBi);
                            Assert(hr == S_OK);
                            while(hr == S_OK)
                            {
                                hr = pBi->GetLowerComponent( &pMpNcc );
                                Assert(hr == S_OK);
                                if (hr == S_OK)
                                {
                                    ULONG uComponentStatus;
                                    hr = pMpNcc->GetDeviceStatus(&uComponentStatus);
                                    Assert(hr == S_OK);
                                    if (hr == S_OK)
                                    {
                                        if (uComponentStatus == 0)
                                        {
                                            vboxNetWinAddComponent(&list, pMpNcc);
                                        }
                                    }
                                    VBoxNetCfgWinReleaseRef( pMpNcc );
                                }
                                VBoxNetCfgWinReleaseRef(pBi);

                                hr = VBoxNetCfgWinGetNextBindingInterface(pEnumBi, &pBi);
                            }
                            VBoxNetCfgWinReleaseRef(pEnumBi);
                        }
                    }
                    VBoxNetCfgWinReleaseRef(pBp);

                    hr = VBoxNetCfgWinGetNextBindingPath(pEnumBp, &pBp);
                }
                VBoxNetCfgWinReleaseRef(pEnumBp);
            }
            VBoxNetCfgWinReleaseRef(pTcpIpNcc);
        }
        else
        {
            LogRel(("failed to get the sun_VBoxNetFlt component, error (0x%x)", hr));
        }

        VBoxNetCfgWinReleaseINetCfg(pNc, FALSE);
    }
#   endif /* #  if defined VBOX_WITH_NETFLT */


#  elif defined RT_OS_LINUX
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0)
    {
        char pBuffer[2048];
        struct ifconf ifConf;
        ifConf.ifc_len = sizeof(pBuffer);
        ifConf.ifc_buf = pBuffer;
        if (ioctl(sock, SIOCGIFCONF, &ifConf) >= 0)
        {
            for (struct ifreq *pReq = ifConf.ifc_req; (char*)pReq < pBuffer + ifConf.ifc_len; pReq++)
            {
                if (ioctl(sock, SIOCGIFHWADDR, pReq) >= 0)
                {
                    if (pReq->ifr_hwaddr.sa_family == ARPHRD_ETHER)
                    {
                        RTUUID uuid;
                        Assert(sizeof(uuid) <= sizeof(*pReq));
                        memcpy(&uuid, pReq, sizeof(uuid));

                        ComObjPtr<HostNetworkInterface> IfObj;
                        IfObj.createObject();
                        if (SUCCEEDED(IfObj->init(Bstr(pReq->ifr_name), Guid(uuid), HostNetworkInterfaceType_Bridged)))
                            list.push_back(IfObj);
                    }
                }
            }
        }
        close(sock);
    }
#  endif /* RT_OS_LINUX */
# endif

    std::list <ComObjPtr<HostNetworkInterface> >::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        (*it)->setVirtualBox(m->pParent);
    }

    SafeIfaceArray<IHostNetworkInterface> networkInterfaces (list);
    networkInterfaces.detachTo(ComSafeArrayOutArg(aNetworkInterfaces));

    return S_OK;

#else
    /* Not implemented / supported on this platform. */
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP Host::COMGETTER(USBDevices)(ComSafeArrayOut(IHostUSBDevice*, aUSBDevices))
{
#ifdef VBOX_WITH_USB
    CheckComArgOutSafeArrayPointerValid(aUSBDevices);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    MultiResult rc = checkUSBProxyService();
    if (FAILED(rc)) return rc;

    return m->pUSBProxyService->getDeviceCollection(ComSafeArrayOutArg(aUSBDevices));

#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treating it as a failure), for example, as in OSE. */
    NOREF(aUSBDevices);
# ifndef RT_OS_WINDOWS
    NOREF(aUSBDevicesSize);
# endif
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP Host::COMGETTER(USBDeviceFilters)(ComSafeArrayOut(IHostUSBDeviceFilter*, aUSBDeviceFilters))
{
#ifdef VBOX_WITH_USB
    CheckComArgOutSafeArrayPointerValid(aUSBDeviceFilters);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoMultiWriteLock2 alock(this->lockHandle(), &m->usbListsLock COMMA_LOCKVAL_SRC_POS);

    MultiResult rc = checkUSBProxyService();
    if (FAILED(rc)) return rc;

    SafeIfaceArray<IHostUSBDeviceFilter> collection(m->llUSBDeviceFilters);
    collection.detachTo(ComSafeArrayOutArg(aUSBDeviceFilters));

    return rc;
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treating it as a failure), for example, as in OSE. */
    NOREF(aUSBDeviceFilters);
# ifndef RT_OS_WINDOWS
    NOREF(aUSBDeviceFiltersSize);
# endif
    ReturnComNotImplemented();
#endif
}

/**
 * Returns the number of installed logical processors
 *
 * @returns COM status code
 * @param   count address of result variable
 */
STDMETHODIMP Host::COMGETTER(ProcessorCount)(ULONG *aCount)
{
    CheckComArgOutPointerValid(aCount);
    // no locking required

    *aCount = RTMpGetPresentCount();
    return S_OK;
}

/**
 * Returns the number of online logical processors
 *
 * @returns COM status code
 * @param   count address of result variable
 */
STDMETHODIMP Host::COMGETTER(ProcessorOnlineCount)(ULONG *aCount)
{
    CheckComArgOutPointerValid(aCount);
    // no locking required

    *aCount = RTMpGetOnlineCount();
    return S_OK;
}

/**
 * Returns the number of installed physical processor cores.
 *
 * @returns COM status code
 * @param   count address of result variable
 */
STDMETHODIMP Host::COMGETTER(ProcessorCoreCount)(ULONG *aCount)
{
    CheckComArgOutPointerValid(aCount);
    // no locking required

    return E_NOTIMPL;
}

/**
 * Returns the (approximate) maximum speed of the given host CPU in MHz
 *
 * @returns COM status code
 * @param   cpu id to get info for.
 * @param   speed address of result variable, speed is 0 if unknown or aCpuId is invalid.
 */
STDMETHODIMP Host::GetProcessorSpeed(ULONG aCpuId, ULONG *aSpeed)
{
    CheckComArgOutPointerValid(aSpeed);
    // no locking required

    *aSpeed = RTMpGetMaxFrequency(aCpuId);
    return S_OK;
}

/**
 * Returns a description string for the host CPU
 *
 * @returns COM status code
 * @param   cpu id to get info for.
 * @param   description address of result variable, empty string if not known or aCpuId is invalid.
 */
STDMETHODIMP Host::GetProcessorDescription(ULONG aCpuId, BSTR *aDescription)
{
    CheckComArgOutPointerValid(aDescription);
    // no locking required

    char szCPUModel[80];
    int vrc = RTMpGetDescription(aCpuId, szCPUModel, sizeof(szCPUModel));
    if (RT_FAILURE(vrc))
        return E_FAIL; /** @todo error reporting? */
    Bstr (szCPUModel).cloneTo(aDescription);
    return S_OK;
}

/**
 * Returns whether a host processor feature is supported or not
 *
 * @returns COM status code
 * @param   Feature to query.
 * @param   address of supported bool result variable
 */
STDMETHODIMP Host::GetProcessorFeature(ProcessorFeature_T aFeature, BOOL *aSupported)
{
    CheckComArgOutPointerValid(aSupported);
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    switch (aFeature)
    {
        case ProcessorFeature_HWVirtEx:
            *aSupported = m->fVTSupported;
            break;

        case ProcessorFeature_PAE:
            *aSupported = m->fPAESupported;
            break;

        case ProcessorFeature_LongMode:
            *aSupported = m->fLongModeSupported;
            break;

        case ProcessorFeature_NestedPaging:
            *aSupported = m->fNestedPagingSupported;
            break;

        default:
            ReturnComNotImplemented();
    }
    return S_OK;
}

/**
 * Returns the specific CPUID leaf.
 *
 * @returns COM status code
 * @param   aCpuId              The CPU number. Mostly ignored.
 * @param   aLeaf               The leaf number.
 * @param   aSubLeaf            The sub-leaf number.
 * @param   aValEAX             Where to return EAX.
 * @param   aValEBX             Where to return EBX.
 * @param   aValECX             Where to return ECX.
 * @param   aValEDX             Where to return EDX.
 */
STDMETHODIMP Host::GetProcessorCPUIDLeaf(ULONG aCpuId, ULONG aLeaf, ULONG aSubLeaf,
                                         ULONG *aValEAX, ULONG *aValEBX, ULONG *aValECX, ULONG *aValEDX)
{
    CheckComArgOutPointerValid(aValEAX);
    CheckComArgOutPointerValid(aValEBX);
    CheckComArgOutPointerValid(aValECX);
    CheckComArgOutPointerValid(aValEDX);
    // no locking required

    /* Check that the CPU is online. */
    /** @todo later use RTMpOnSpecific. */
    if (!RTMpIsCpuOnline(aCpuId))
        return RTMpIsCpuPresent(aCpuId)
             ? setError(E_FAIL, tr("CPU no.%u is not present"), aCpuId)
             : setError(E_FAIL, tr("CPU no.%u is not online"), aCpuId);

    uint32_t uEAX, uEBX, uECX, uEDX;
    ASMCpuId_Idx_ECX(aLeaf, aSubLeaf, &uEAX, &uEBX, &uECX, &uEDX);
    *aValEAX = uEAX;
    *aValEBX = uEBX;
    *aValECX = uECX;
    *aValEDX = uEDX;

    return S_OK;
}

/**
 * Returns the amount of installed system memory in megabytes
 *
 * @returns COM status code
 * @param   size address of result variable
 */
STDMETHODIMP Host::COMGETTER(MemorySize)(ULONG *aSize)
{
    CheckComArgOutPointerValid(aSize);
    // no locking required

    /* @todo This is an ugly hack. There must be a function in IPRT for that. */
    pm::CollectorHAL *hal = pm::createHAL();
    if (!hal)
        return E_FAIL;
    ULONG tmp;
    int rc = hal->getHostMemoryUsage(aSize, &tmp, &tmp);
    *aSize /= 1024;
    delete hal;
    return rc;
}

/**
 * Returns the current system memory free space in megabytes
 *
 * @returns COM status code
 * @param   available address of result variable
 */
STDMETHODIMP Host::COMGETTER(MemoryAvailable)(ULONG *aAvailable)
{
    CheckComArgOutPointerValid(aAvailable);
    // no locking required

    /* @todo This is an ugly hack. There must be a function in IPRT for that. */
    pm::CollectorHAL *hal = pm::createHAL();
    if (!hal)
        return E_FAIL;
    ULONG tmp;
    int rc = hal->getHostMemoryUsage(&tmp, &tmp, aAvailable);
    *aAvailable /= 1024;
    delete hal;
    return rc;
}

/**
 * Returns the name string of the host operating system
 *
 * @returns COM status code
 * @param   os address of result variable
 */
STDMETHODIMP Host::COMGETTER(OperatingSystem)(BSTR *aOs)
{
    CheckComArgOutPointerValid(aOs);
    // no locking required

    char szOSName[80];
    int vrc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szOSName, sizeof(szOSName));
    if (RT_FAILURE(vrc))
        return E_FAIL; /** @todo error reporting? */
    Bstr (szOSName).cloneTo(aOs);
    return S_OK;
}

/**
 * Returns the version string of the host operating system
 *
 * @returns COM status code
 * @param   os address of result variable
 */
STDMETHODIMP Host::COMGETTER(OSVersion)(BSTR *aVersion)
{
    CheckComArgOutPointerValid(aVersion);
    // no locking required

    /* Get the OS release. Reserve some buffer space for the service pack. */
    char szOSRelease[128];
    int vrc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szOSRelease, sizeof(szOSRelease) - 32);
    if (RT_FAILURE(vrc))
        return E_FAIL; /** @todo error reporting? */

    /* Append the service pack if present. */
    char szOSServicePack[80];
    vrc = RTSystemQueryOSInfo(RTSYSOSINFO_SERVICE_PACK, szOSServicePack, sizeof(szOSServicePack));
    if (RT_FAILURE(vrc))
    {
        if (vrc != VERR_NOT_SUPPORTED)
            return E_FAIL; /** @todo error reporting? */
        szOSServicePack[0] = '\0';
    }
    if (szOSServicePack[0] != '\0')
    {
        char *psz = strchr(szOSRelease, '\0');
        RTStrPrintf(psz, &szOSRelease[sizeof(szOSRelease)] - psz, "sp%s", szOSServicePack);
    }

    Bstr(szOSRelease).cloneTo(aVersion);
    return S_OK;
}

/**
 * Returns the current host time in milliseconds since 1970-01-01 UTC.
 *
 * @returns COM status code
 * @param   time address of result variable
 */
STDMETHODIMP Host::COMGETTER(UTCTime)(LONG64 *aUTCTime)
{
    CheckComArgOutPointerValid(aUTCTime);
    // no locking required

    RTTIMESPEC now;
    *aUTCTime = RTTimeSpecGetMilli(RTTimeNow(&now));

    return S_OK;
}

STDMETHODIMP Host::COMGETTER(Acceleration3DAvailable)(BOOL *aSupported)
{
    CheckComArgOutPointerValid(aSupported);
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aSupported = m->f3DAccelerationSupported;

    return S_OK;
}

STDMETHODIMP Host::CreateHostOnlyNetworkInterface(IHostNetworkInterface **aHostNetworkInterface,
                                                  IProgress **aProgress)
{
    CheckComArgOutPointerValid(aHostNetworkInterface);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

#ifdef VBOX_WITH_HOSTNETIF_API
    /* No need to lock anything. If there ever will - watch out, the function
     * called below grabs the VirtualBox lock. */

    int r = NetIfCreateHostOnlyNetworkInterface(m->pParent, aHostNetworkInterface, aProgress);
    if (RT_SUCCESS(r))
        return S_OK;

    return r == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Host::RemoveHostOnlyNetworkInterface(IN_BSTR aId,
                                                  IProgress **aProgress)
{
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

#ifdef VBOX_WITH_HOSTNETIF_API
    /* No need to lock anything, the code below does not touch the state
     * of the host object. If that ever changes then check for lock order
     * violations with the called functions. */

    /* first check whether an interface with the given name already exists */
    {
        ComPtr<IHostNetworkInterface> iface;
        if (FAILED(FindHostNetworkInterfaceById(aId,
                                                iface.asOutParam())))
            return setError(VBOX_E_OBJECT_NOT_FOUND,
                            tr("Host network interface with UUID {%RTuuid} does not exist"),
                            Guid (aId).raw());
    }

    int r = NetIfRemoveHostOnlyNetworkInterface(m->pParent, Guid(aId).ref(), aProgress);
    if (RT_SUCCESS(r))
        return S_OK;

    return r == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Host::CreateUSBDeviceFilter(IN_BSTR aName,
                                         IHostUSBDeviceFilter **aFilter)
{
#ifdef VBOX_WITH_USB
    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgOutPointerValid(aFilter);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<HostUSBDeviceFilter> filter;
    filter.createObject();
    HRESULT rc = filter->init(this, aName);
    ComAssertComRCRet(rc, rc);
    rc = filter.queryInterfaceTo(aFilter);
    AssertComRCReturn(rc, rc);
    return S_OK;
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treating it as a failure), for example, as in OSE. */
    NOREF(aName);
    NOREF(aFilter);
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP Host::InsertUSBDeviceFilter(ULONG aPosition,
                                         IHostUSBDeviceFilter *aFilter)
{
#ifdef VBOX_WITH_USB
    CheckComArgNotNull(aFilter);

    /* Note: HostUSBDeviceFilter and USBProxyService also uses this lock. */
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoMultiWriteLock2 alock(this->lockHandle(), &m->usbListsLock COMMA_LOCKVAL_SRC_POS);

    MultiResult rc = checkUSBProxyService();
    if (FAILED(rc)) return rc;

    ComObjPtr<HostUSBDeviceFilter> pFilter;
    for (USBDeviceFilterList::iterator it = m->llChildren.begin();
         it != m->llChildren.end();
         ++it)
    {
        if (*it == aFilter)
        {
            pFilter = *it;
            break;
        }
    }
    if (pFilter.isNull())
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("The given USB device filter is not created within this VirtualBox instance"));

    if (pFilter->mInList)
        return setError(E_INVALIDARG,
                        tr("The given USB device filter is already in the list"));

    /* iterate to the position... */
    USBDeviceFilterList::iterator itPos = m->llUSBDeviceFilters.begin();
    std::advance(itPos, aPosition);
    /* ...and insert */
    m->llUSBDeviceFilters.insert(itPos, pFilter);
    pFilter->mInList = true;

    /* notify the proxy (only when the filter is active) */
    if (    m->pUSBProxyService->isActive()
         && pFilter->getData().mActive)
    {
        ComAssertRet(pFilter->getId() == NULL, E_FAIL);
        pFilter->getId() = m->pUSBProxyService->insertFilter(&pFilter->getData().mUSBFilter);
    }

    // save the global settings; for that we should hold only the VirtualBox lock
    alock.release();
    AutoWriteLock vboxLock(m->pParent COMMA_LOCKVAL_SRC_POS);
    return rc = m->pParent->saveSettings();
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treating it as a failure), for example, as in OSE. */
    NOREF(aPosition);
    NOREF(aFilter);
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP Host::RemoveUSBDeviceFilter(ULONG aPosition)
{
#ifdef VBOX_WITH_USB

    /* Note: HostUSBDeviceFilter and USBProxyService also uses this lock. */
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoMultiWriteLock2 alock(this->lockHandle(), &m->usbListsLock COMMA_LOCKVAL_SRC_POS);

    MultiResult rc = checkUSBProxyService();
    if (FAILED(rc)) return rc;

    if (!m->llUSBDeviceFilters.size())
        return setError(E_INVALIDARG,
                        tr("The USB device filter list is empty"));

    if (aPosition >= m->llUSBDeviceFilters.size())
        return setError(E_INVALIDARG,
                        tr("Invalid position: %lu (must be in range [0, %lu])"),
                        aPosition, m->llUSBDeviceFilters.size() - 1);

    ComObjPtr<HostUSBDeviceFilter> filter;
    {
        /* iterate to the position... */
        USBDeviceFilterList::iterator it = m->llUSBDeviceFilters.begin();
        std::advance (it, aPosition);
        /* ...get an element from there... */
        filter = *it;
        /* ...and remove */
        filter->mInList = false;
        m->llUSBDeviceFilters.erase(it);
    }

    /* notify the proxy (only when the filter is active) */
    if (m->pUSBProxyService->isActive() && filter->getData().mActive)
    {
        ComAssertRet(filter->getId() != NULL, E_FAIL);
        m->pUSBProxyService->removeFilter(filter->getId());
        filter->getId() = NULL;
    }

    // save the global settings; for that we should hold only the VirtualBox lock
    alock.release();
    AutoWriteLock vboxLock(m->pParent COMMA_LOCKVAL_SRC_POS);
    return rc = m->pParent->saveSettings();
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treating it as a failure), for example, as in OSE. */
    NOREF(aPosition);
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP Host::FindHostDVDDrive(IN_BSTR aName, IMedium **aDrive)
{
    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgOutPointerValid(aDrive);

    *aDrive = NULL;

    SafeIfaceArray<IMedium> drivevec;
    HRESULT rc = COMGETTER(DVDDrives)(ComSafeArrayAsOutParam(drivevec));
    if (FAILED(rc)) return rc;

    for (size_t i = 0; i < drivevec.size(); ++i)
    {
        ComPtr<IMedium> drive = drivevec[i];
        Bstr name, location;
        rc = drive->COMGETTER(Name)(name.asOutParam());
        if (FAILED(rc)) return rc;
        rc = drive->COMGETTER(Location)(location.asOutParam());
        if (FAILED(rc)) return rc;
        if (name == aName || location == aName)
            return drive.queryInterfaceTo(aDrive);
    }

    return setError(VBOX_E_OBJECT_NOT_FOUND,
                    Medium::tr("The host DVD drive named '%ls' could not be found"), aName);
}

STDMETHODIMP Host::FindHostFloppyDrive(IN_BSTR aName, IMedium **aDrive)
{
    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgOutPointerValid(aDrive);

    *aDrive = NULL;

    SafeIfaceArray<IMedium> drivevec;
    HRESULT rc = COMGETTER(FloppyDrives)(ComSafeArrayAsOutParam(drivevec));
    if (FAILED(rc)) return rc;

    for (size_t i = 0; i < drivevec.size(); ++i)
    {
        ComPtr<IMedium> drive = drivevec[i];
        Bstr name;
        rc = drive->COMGETTER(Name)(name.asOutParam());
        if (FAILED(rc)) return rc;
        if (name == aName)
            return drive.queryInterfaceTo(aDrive);
    }

    return setError(VBOX_E_OBJECT_NOT_FOUND,
                    Medium::tr("The host floppy drive named '%ls' could not be found"), aName);
}

STDMETHODIMP Host::FindHostNetworkInterfaceByName(IN_BSTR name, IHostNetworkInterface **networkInterface)
{
#ifndef VBOX_WITH_HOSTNETIF_API
    return E_NOTIMPL;
#else
    if (!name)
        return E_INVALIDARG;
    if (!networkInterface)
        return E_POINTER;

    *networkInterface = NULL;
    ComObjPtr<HostNetworkInterface> found;
    std::list <ComObjPtr<HostNetworkInterface> > list;
    int rc = NetIfList(list);
    if (RT_FAILURE(rc))
    {
        Log(("Failed to get host network interface list with rc=%Rrc\n", rc));
        return E_FAIL;
    }
    std::list <ComObjPtr<HostNetworkInterface> >::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        Bstr n;
        (*it)->COMGETTER(Name) (n.asOutParam());
        if (n == name)
            found = *it;
    }

    if (!found)
        return setError(E_INVALIDARG,
                        HostNetworkInterface::tr("The host network interface with the given name could not be found"));

    found->setVirtualBox(m->pParent);

    return found.queryInterfaceTo(networkInterface);
#endif
}

STDMETHODIMP Host::FindHostNetworkInterfaceById(IN_BSTR id, IHostNetworkInterface **networkInterface)
{
#ifndef VBOX_WITH_HOSTNETIF_API
    return E_NOTIMPL;
#else
    if (Guid(id).isEmpty())
        return E_INVALIDARG;
    if (!networkInterface)
        return E_POINTER;

    *networkInterface = NULL;
    ComObjPtr<HostNetworkInterface> found;
    std::list <ComObjPtr<HostNetworkInterface> > list;
    int rc = NetIfList(list);
    if (RT_FAILURE(rc))
    {
        Log(("Failed to get host network interface list with rc=%Rrc\n", rc));
        return E_FAIL;
    }
    std::list <ComObjPtr<HostNetworkInterface> >::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        Bstr g;
        (*it)->COMGETTER(Id) (g.asOutParam());
        if (g == id)
            found = *it;
    }

    if (!found)
        return setError(E_INVALIDARG,
                        HostNetworkInterface::tr("The host network interface with the given GUID could not be found"));

    found->setVirtualBox(m->pParent);

    return found.queryInterfaceTo(networkInterface);
#endif
}

STDMETHODIMP Host::FindHostNetworkInterfacesOfType(HostNetworkInterfaceType_T type,
                                                   ComSafeArrayOut(IHostNetworkInterface *, aNetworkInterfaces))
{
#ifdef VBOX_WITH_HOSTNETIF_API
    std::list <ComObjPtr<HostNetworkInterface> > allList;
    int rc = NetIfList(allList);
    if (RT_FAILURE(rc))
        return E_FAIL;

    std::list <ComObjPtr<HostNetworkInterface> > resultList;

    std::list <ComObjPtr<HostNetworkInterface> >::iterator it;
    for (it = allList.begin(); it != allList.end(); ++it)
    {
        HostNetworkInterfaceType_T t;
        HRESULT hr = (*it)->COMGETTER(InterfaceType)(&t);
        if (FAILED(hr))
            return hr;

        if (t == type)
        {
            (*it)->setVirtualBox(m->pParent);
            resultList.push_back (*it);
        }
    }

    SafeIfaceArray<IHostNetworkInterface> filteredNetworkInterfaces (resultList);
    filteredNetworkInterfaces.detachTo(ComSafeArrayOutArg(aNetworkInterfaces));

    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Host::FindUSBDeviceByAddress(IN_BSTR aAddress,
                                          IHostUSBDevice **aDevice)
{
#ifdef VBOX_WITH_USB
    CheckComArgStrNotEmptyOrNull(aAddress);
    CheckComArgOutPointerValid(aDevice);

    *aDevice = NULL;

    SafeIfaceArray<IHostUSBDevice> devsvec;
    HRESULT rc = COMGETTER(USBDevices) (ComSafeArrayAsOutParam(devsvec));
    if (FAILED(rc)) return rc;

    for (size_t i = 0; i < devsvec.size(); ++i)
    {
        Bstr address;
        rc = devsvec[i]->COMGETTER(Address) (address.asOutParam());
        if (FAILED(rc)) return rc;
        if (address == aAddress)
        {
            return ComObjPtr<IHostUSBDevice> (devsvec[i]).queryInterfaceTo(aDevice);
        }
    }

    return setErrorNoLog(VBOX_E_OBJECT_NOT_FOUND,
                         tr("Could not find a USB device with address '%ls'"),
                         aAddress);

#else   /* !VBOX_WITH_USB */
    NOREF(aAddress);
    NOREF(aDevice);
    return E_NOTIMPL;
#endif  /* !VBOX_WITH_USB */
}

STDMETHODIMP Host::FindUSBDeviceById(IN_BSTR aId,
                                     IHostUSBDevice **aDevice)
{
#ifdef VBOX_WITH_USB
    CheckComArgExpr(aId, Guid (aId).isEmpty() == false);
    CheckComArgOutPointerValid(aDevice);

    *aDevice = NULL;

    SafeIfaceArray<IHostUSBDevice> devsvec;
    HRESULT rc = COMGETTER(USBDevices) (ComSafeArrayAsOutParam(devsvec));
    if (FAILED(rc)) return rc;

    for (size_t i = 0; i < devsvec.size(); ++i)
    {
        Bstr id;
        rc = devsvec[i]->COMGETTER(Id) (id.asOutParam());
        if (FAILED(rc)) return rc;
        if (id == aId)
        {
            return ComObjPtr<IHostUSBDevice> (devsvec[i]).queryInterfaceTo(aDevice);
        }
    }

    return setErrorNoLog (VBOX_E_OBJECT_NOT_FOUND, tr (
        "Could not find a USB device with uuid {%RTuuid}"),
        Guid (aId).raw());

#else   /* !VBOX_WITH_USB */
    NOREF(aId);
    NOREF(aDevice);
    return E_NOTIMPL;
#endif  /* !VBOX_WITH_USB */
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

HRESULT Host::loadSettings(const settings::Host &data)
{
    HRESULT rc = S_OK;
#ifdef VBOX_WITH_USB
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoMultiWriteLock2 alock(this->lockHandle(), &m->usbListsLock COMMA_LOCKVAL_SRC_POS);

    for (settings::USBDeviceFiltersList::const_iterator it = data.llUSBDeviceFilters.begin();
         it != data.llUSBDeviceFilters.end();
         ++it)
    {
        const settings::USBDeviceFilter &f = *it;
        ComObjPtr<HostUSBDeviceFilter> pFilter;
        pFilter.createObject();
        rc = pFilter->init(this, f);
        if (FAILED(rc)) break;

        m->llUSBDeviceFilters.push_back(pFilter);
        pFilter->mInList = true;

        /* notify the proxy (only when the filter is active) */
        if (pFilter->getData().mActive)
        {
            HostUSBDeviceFilter *flt = pFilter; /* resolve ambiguity */
            flt->getId() = m->pUSBProxyService->insertFilter(&pFilter->getData().mUSBFilter);
        }
    }
#else
    NOREF(data);
#endif /* VBOX_WITH_USB */
    return rc;
}

HRESULT Host::saveSettings(settings::Host &data)
{
#ifdef VBOX_WITH_USB
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock1(this COMMA_LOCKVAL_SRC_POS);
    AutoReadLock alock2(&m->usbListsLock COMMA_LOCKVAL_SRC_POS);

    data.llUSBDeviceFilters.clear();

    for (USBDeviceFilterList::const_iterator it = m->llUSBDeviceFilters.begin();
         it != m->llUSBDeviceFilters.end();
         ++it)
    {
        ComObjPtr<HostUSBDeviceFilter> pFilter = *it;
        settings::USBDeviceFilter f;
        pFilter->saveSettings(f);
        data.llUSBDeviceFilters.push_back(f);
    }
#else
    NOREF(data);
#endif /* VBOX_WITH_USB */

    return S_OK;
}

/**
 * Sets the given pointer to point to the static list of DVD or floppy
 * drives in the Host instance data, depending on the @a mediumType
 * parameter.
 *
 * This builds the list on the first call; it adds or removes host drives
 * that may have changed if fRefresh == true.
 *
 * The caller must hold the m->drivesLock write lock before calling this.
 * To protect the list to which the caller's pointer points, the caller
 * must also hold that lock.
 *
 * @param mediumType Must be DeviceType_Floppy or DeviceType_DVD.
 * @param fRefresh Whether to refresh the host drives list even if this is not the first call.
 * @param pll Caller's pointer which gets set to the static list of host drives.
 * @return
 */
HRESULT Host::getDrives(DeviceType_T mediumType,
                        bool fRefresh,
                        MediaList *&pll)
{
    HRESULT rc = S_OK;
    Assert(m->drivesLock.isWriteLockOnCurrentThread());

    MediaList llNew;
    MediaList *pllCached;
    bool *pfListBuilt = NULL;

    switch (mediumType)
    {
        case DeviceType_DVD:
            if (!m->fDVDDrivesListBuilt || fRefresh)
            {
                rc = buildDVDDrivesList(llNew);
                if (FAILED(rc))
                    return rc;
                pfListBuilt = &m->fDVDDrivesListBuilt;
            }
            pllCached = &m->llDVDDrives;
        break;

        case DeviceType_Floppy:
            if (!m->fFloppyDrivesListBuilt || fRefresh)
            {
                rc = buildFloppyDrivesList(llNew);
                if (FAILED(rc))
                    return rc;
                pfListBuilt = &m->fFloppyDrivesListBuilt;
            }
            pllCached = &m->llFloppyDrives;
        break;

        default:
            return E_INVALIDARG;
    }

    if (pfListBuilt)
    {
        // a list was built in llNew above:
        if (!*pfListBuilt)
        {
            // this was the first call (instance bool is still false): then just copy the whole list and return
            *pllCached = llNew;
            // and mark the instance data as "built"
            *pfListBuilt = true;
        }
        else
        {
            // list was built, and this was a subsequent call: then compare the old and the new lists

            // remove drives from the cached list which are no longer present
            for (MediaList::iterator itCached = pllCached->begin();
                 itCached != pllCached->end();
                 ++itCached)
            {
                Medium *pCached = *itCached;
                const Utf8Str strLocationCached = pCached->getLocationFull();
                bool fFound = false;
                for (MediaList::iterator itNew = llNew.begin();
                     itNew != llNew.end();
                     ++itNew)
                {
                    Medium *pNew = *itNew;
                    const Utf8Str strLocationNew = pNew->getLocationFull();
                    if (strLocationNew == strLocationCached)
                    {
                        fFound = true;
                        break;
                    }
                }
                if (!fFound)
                    itCached = pllCached->erase(itCached);
            }

            // add drives to the cached list that are not on there yet
            for (MediaList::iterator itNew = llNew.begin();
                 itNew != llNew.end();
                 ++itNew)
            {
                Medium *pNew = *itNew;
                const Utf8Str strLocationNew = pNew->getLocationFull();
                bool fFound = false;
                for (MediaList::iterator itCached = pllCached->begin();
                     itCached != pllCached->end();
                     ++itCached)
                {
                    Medium *pCached = *itCached;
                    const Utf8Str strLocationCached = pCached->getLocationFull();
                    if (strLocationNew == strLocationCached)
                    {
                        fFound = true;
                        break;
                    }
                }

                if (!fFound)
                    pllCached->push_back(pNew);
            }
        }
    }

    // return cached list to caller
    pll = pllCached;

    return rc;
}

/**
 * Goes through the list of host drives that would be returned by getDrives()
 * and looks for a host drive with the given UUID. If found, it sets pMedium
 * to that drive; otherwise returns VBOX_E_OBJECT_NOT_FOUND.
 *
 * @param mediumType Must be DeviceType_DVD or DeviceType_Floppy.
 * @param uuid Medium UUID of host drive to look for.
 * @param fRefresh Whether to refresh the host drives list (see getDrives())
 * @param pMedium Medium object, if found…
 * @return VBOX_E_OBJECT_NOT_FOUND if not found, or S_OK if found, or errors from getDrives().
 */
HRESULT Host::findHostDrive(DeviceType_T mediumType,
                            const Guid &uuid,
                            bool fRefresh,
                            ComObjPtr<Medium> &pMedium)
{
    MediaList *pllMedia;

    AutoWriteLock wlock(m->drivesLock COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = getDrives(mediumType, fRefresh, pllMedia);
    if (SUCCEEDED(rc))
    {
        for (MediaList::iterator it = pllMedia->begin();
             it != pllMedia->end();
             ++it)
        {
            Medium *pThis = *it;
            if (pThis->getId() == uuid)
            {
                pMedium = pThis;
                return S_OK;
            }
        }
    }

    return VBOX_E_OBJECT_NOT_FOUND;
}

/**
 * Called from getDrives() to build the DVD drives list.
 * @param pll
 * @return
 */
HRESULT Host::buildDVDDrivesList(MediaList &list)
{
    HRESULT rc = S_OK;

    Assert(m->drivesLock.isWriteLockOnCurrentThread());

    try
    {
#if defined(RT_OS_WINDOWS)
        int sz = GetLogicalDriveStrings(0, NULL);
        TCHAR *hostDrives = new TCHAR[sz+1];
        GetLogicalDriveStrings(sz, hostDrives);
        wchar_t driveName[3] = { '?', ':', '\0' };
        TCHAR *p = hostDrives;
        do
        {
            if (GetDriveType(p) == DRIVE_CDROM)
            {
                driveName[0] = *p;
                ComObjPtr<Medium> hostDVDDriveObj;
                hostDVDDriveObj.createObject();
                hostDVDDriveObj->init(m->pParent, DeviceType_DVD, Bstr(driveName));
                list.push_back(hostDVDDriveObj);
            }
            p += _tcslen(p) + 1;
        }
        while (*p);
        delete[] hostDrives;

#elif defined(RT_OS_SOLARIS)
# ifdef VBOX_USE_LIBHAL
        if (!getDVDInfoFromHal(list))
# endif
        {
            getDVDInfoFromDevTree(list);
        }

#elif defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)
        if (RT_SUCCESS(m->hostDrives.updateDVDs()))
            for (DriveInfoList::const_iterator it = m->hostDrives.DVDBegin();
                SUCCEEDED(rc) && it != m->hostDrives.DVDEnd(); ++it)
            {
                ComObjPtr<Medium> hostDVDDriveObj;
                Utf8Str location(it->mDevice);
                Utf8Str description(it->mDescription);
                if (SUCCEEDED(rc))
                    rc = hostDVDDriveObj.createObject();
                if (SUCCEEDED(rc))
                    rc = hostDVDDriveObj->init(m->pParent, DeviceType_DVD, location, description);
                if (SUCCEEDED(rc))
                    list.push_back(hostDVDDriveObj);
            }
#elif defined(RT_OS_DARWIN)
        PDARWINDVD cur = DarwinGetDVDDrives();
        while (cur)
        {
            ComObjPtr<Medium> hostDVDDriveObj;
            hostDVDDriveObj.createObject();
            hostDVDDriveObj->init(m->pParent, DeviceType_DVD, Bstr(cur->szName));
            list.push_back(hostDVDDriveObj);

            /* next */
            void *freeMe = cur;
            cur = cur->pNext;
            RTMemFree(freeMe);
        }
#else
    /* PORTME */
#endif
    }
    catch(std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
}

/**
 * Called from getDrives() to build the floppy drives list.
 * @param list
 * @return
 */
HRESULT Host::buildFloppyDrivesList(MediaList &list)
{
    HRESULT rc = S_OK;

    Assert(m->drivesLock.isWriteLockOnCurrentThread());

    try
    {
#ifdef RT_OS_WINDOWS
        int sz = GetLogicalDriveStrings(0, NULL);
        TCHAR *hostDrives = new TCHAR[sz+1];
        GetLogicalDriveStrings(sz, hostDrives);
        wchar_t driveName[3] = { '?', ':', '\0' };
        TCHAR *p = hostDrives;
        do
        {
            if (GetDriveType(p) == DRIVE_REMOVABLE)
            {
                driveName[0] = *p;
                ComObjPtr<Medium> hostFloppyDriveObj;
                hostFloppyDriveObj.createObject();
                hostFloppyDriveObj->init(m->pParent, DeviceType_Floppy, Bstr(driveName));
                list.push_back(hostFloppyDriveObj);
            }
            p += _tcslen(p) + 1;
        }
        while (*p);
        delete[] hostDrives;
#elif defined(RT_OS_LINUX)
        if (RT_SUCCESS(m->hostDrives.updateFloppies()))
            for (DriveInfoList::const_iterator it = m->hostDrives.FloppyBegin();
                SUCCEEDED(rc) && it != m->hostDrives.FloppyEnd(); ++it)
            {
                ComObjPtr<Medium> hostFloppyDriveObj;
                Utf8Str location(it->mDevice);
                Utf8Str description(it->mDescription);
                if (SUCCEEDED(rc))
                    rc = hostFloppyDriveObj.createObject();
                if (SUCCEEDED(rc))
                    rc = hostFloppyDriveObj->init(m->pParent, DeviceType_Floppy, location, description);
                if (SUCCEEDED(rc))
                    list.push_back(hostFloppyDriveObj);
            }
#else
    NOREF(list);
    /* PORTME */
#endif
    }
    catch(std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }

    return rc;
}

#ifdef VBOX_WITH_USB
USBProxyService* Host::usbProxyService()
{
    return m->pUSBProxyService;
}

HRESULT Host::addChild(HostUSBDeviceFilter *pChild)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(&m->usbListsLock COMMA_LOCKVAL_SRC_POS);

    m->llChildren.push_back(pChild);

    return S_OK;
}

HRESULT Host::removeChild(HostUSBDeviceFilter *pChild)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(&m->usbListsLock COMMA_LOCKVAL_SRC_POS);

    for (USBDeviceFilterList::iterator it = m->llChildren.begin();
         it != m->llChildren.end();
         ++it)
    {
        if (*it == pChild)
        {
            m->llChildren.erase(it);
            break;
        }
    }

    return S_OK;
}

VirtualBox* Host::parent()
{
    return m->pParent;
}

/**
 *  Called by setter methods of all USB device filters.
 */
HRESULT Host::onUSBDeviceFilterChange(HostUSBDeviceFilter *aFilter,
                                      BOOL aActiveChanged /* = FALSE */)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (aFilter->mInList)
    {
        if (aActiveChanged)
        {
            // insert/remove the filter from the proxy
            if (aFilter->getData().mActive)
            {
                ComAssertRet(aFilter->getId() == NULL, E_FAIL);
                aFilter->getId() = m->pUSBProxyService->insertFilter(&aFilter->getData().mUSBFilter);
            }
            else
            {
                ComAssertRet(aFilter->getId() != NULL, E_FAIL);
                m->pUSBProxyService->removeFilter(aFilter->getId());
                aFilter->getId() = NULL;
            }
        }
        else
        {
            if (aFilter->getData().mActive)
            {
                // update the filter in the proxy
                ComAssertRet(aFilter->getId() != NULL, E_FAIL);
                m->pUSBProxyService->removeFilter(aFilter->getId());
                aFilter->getId() = m->pUSBProxyService->insertFilter(&aFilter->getData().mUSBFilter);
            }
        }

        // save the global settings... yeah, on every single filter property change
        // for that we should hold only the VirtualBox lock
        alock.release();
        AutoWriteLock vboxLock(m->pParent COMMA_LOCKVAL_SRC_POS);
        return m->pParent->saveSettings();
    }

    return S_OK;
}


/**
 * Interface for obtaining a copy of the USBDeviceFilterList,
 * used by the USBProxyService.
 *
 * @param   aGlobalFilters      Where to put the global filter list copy.
 * @param   aMachines           Where to put the machine vector.
 */
void Host::getUSBFilters(Host::USBDeviceFilterList *aGlobalFilters)
{
    AutoReadLock alock(&m->usbListsLock COMMA_LOCKVAL_SRC_POS);

    *aGlobalFilters = m->llUSBDeviceFilters;
}

#endif /* VBOX_WITH_USB */

// private methods
////////////////////////////////////////////////////////////////////////////////

#if defined(RT_OS_SOLARIS) && defined(VBOX_USE_LIBHAL)

/**
 * Helper function to get the slice number from a device path
 *
 * @param   pszDevLinkPath      Pointer to a device path (/dev/(r)dsk/c7d1t0d0s3 etc.)
 * @returns Pointer to the slice portion of the given path.
 */
static char *solarisGetSliceFromPath(const char *pszDevLinkPath)
{
    char *pszFound = NULL;
    char *pszSlice = strrchr(pszDevLinkPath, 's');
    char *pszDisk  = strrchr(pszDevLinkPath, 'd');
    if (pszSlice && pszSlice > pszDisk)
        pszFound = pszSlice;
    else
        pszFound = pszDisk;

    if (pszFound && RT_C_IS_DIGIT(pszFound[1]))
        return pszFound;

    return NULL;
}

/**
 * Walk device links and returns an allocated path for the first one in the snapshot.
 *
 * @param   DevLink     Handle to the device link being walked.
 * @param   pvArg       Opaque data containing the pointer to the path.
 * @returns Pointer to an allocated device path string.
 */
static int solarisWalkDevLink(di_devlink_t DevLink, void *pvArg)
{
    char **ppszPath = (char **)pvArg;
    *ppszPath = strdup(di_devlink_path(DevLink));
    return DI_WALK_TERMINATE;
}

/**
 * Walk all devices in the system and enumerate CD/DVD drives.
 * @param   Node        Handle to the current node.
 * @param   pvArg       Opaque data (holds list pointer).
 * @returns Solaris specific code whether to continue walking or not.
 */
static int solarisWalkDeviceNodeForDVD(di_node_t Node, void *pvArg)
{
    PSOLARISDVD *ppDrives = (PSOLARISDVD *)pvArg;

    /*
     * Check for "removable-media" or "hotpluggable" instead of "SCSI" so that we also include USB CD-ROMs.
     * As unfortunately the Solaris drivers only export these common properties.
     */
    int *pInt = NULL;
    if (   di_prop_lookup_ints(DDI_DEV_T_ANY, Node, "removable-media", &pInt) >= 0
        || di_prop_lookup_ints(DDI_DEV_T_ANY, Node, "hotpluggable", &pInt) >= 0)
    {
        if (di_prop_lookup_ints(DDI_DEV_T_ANY, Node, "inquiry-device-type", &pInt) > 0
            && (   *pInt == DTYPE_RODIRECT                                              /* CDROM */
                || *pInt == DTYPE_OPTICAL))                                             /* Optical Drive */
        {
            char *pszProduct = NULL;
            if (di_prop_lookup_strings(DDI_DEV_T_ANY, Node, "inquiry-product-id", &pszProduct) > 0)
            {
                char *pszVendor = NULL;
                if (di_prop_lookup_strings(DDI_DEV_T_ANY, Node, "inquiry-vendor-id", &pszVendor) > 0)
                {
                    /*
                     * Found a DVD drive, we need to scan the minor nodes to find the correct
                     * slice that represents the whole drive. "s2" is always the whole drive for CD/DVDs.
                     */
                    int Major = di_driver_major(Node);
                    di_minor_t Minor = DI_MINOR_NIL;
                    di_devlink_handle_t DevLink = di_devlink_init(NULL /* name */, 0 /* flags */);
                    if (DevLink)
                    {
                        while ((Minor = di_minor_next(Node, Minor)) != DI_MINOR_NIL)
                        {
                            dev_t Dev = di_minor_devt(Minor);
                            if (   Major != (int)major(Dev)
                                || di_minor_spectype(Minor) == S_IFBLK
                                || di_minor_type(Minor) != DDM_MINOR)
                            {
                                continue;
                            }

                            char *pszMinorPath = di_devfs_minor_path(Minor);
                            if (!pszMinorPath)
                                continue;

                            char *pszDevLinkPath = NULL;
                            di_devlink_walk(DevLink, NULL, pszMinorPath, DI_PRIMARY_LINK, &pszDevLinkPath, solarisWalkDevLink);
                            di_devfs_path_free(pszMinorPath);

                            if (pszDevLinkPath)
                            {
                                char *pszSlice = solarisGetSliceFromPath(pszDevLinkPath);
                                if (   pszSlice && !strcmp(pszSlice, "s2")
                                    && !strncmp(pszDevLinkPath, "/dev/rdsk", sizeof("/dev/rdsk") - 1))   /* We want only raw disks */
                                {
                                    /*
                                     * We've got a fully qualified DVD drive. Add it to the list.
                                     */
                                    PSOLARISDVD pDrive = (PSOLARISDVD)RTMemAllocZ(sizeof(SOLARISDVD));
                                    if (RT_LIKELY(pDrive))
                                    {
                                        RTStrPrintf(pDrive->szDescription, sizeof(pDrive->szDescription), "%s %s", pszVendor, pszProduct);
                                        RTStrCopy(pDrive->szRawDiskPath, sizeof(pDrive->szRawDiskPath), pszDevLinkPath);
                                        if (*ppDrives)
                                            pDrive->pNext = *ppDrives;
                                        *ppDrives = pDrive;

                                        /* We're not interested in any of the other slices, stop minor nodes traversal. */
                                        free(pszDevLinkPath);
                                        break;
                                    }
                                }
                                free(pszDevLinkPath);
                            }
                        }
                        di_devlink_fini(&DevLink);
                    }
                }
            }
        }
    }
    return DI_WALK_CONTINUE;
}

/**
 * Solaris specific function to enumerate CD/DVD drives via the device tree.
 * Works on Solaris 10 as well as OpenSolaris without depending on libhal.
 */
void Host::getDVDInfoFromDevTree(std::list<ComObjPtr<Medium> > &list)
{
    PSOLARISDVD pDrives = NULL;
    di_node_t RootNode = di_init("/", DINFOCPYALL);
    if (RootNode != DI_NODE_NIL)
        di_walk_node(RootNode, DI_WALK_CLDFIRST, &pDrives, solarisWalkDeviceNodeForDVD);

    di_fini(RootNode);

    while (pDrives)
    {
        ComObjPtr<Medium> hostDVDDriveObj;
        hostDVDDriveObj.createObject();
        hostDVDDriveObj->init(m->pParent, DeviceType_DVD, Bstr(pDrives->szRawDiskPath), Bstr(pDrives->szDescription));
        list.push_back(hostDVDDriveObj);

        void *pvDrive = pDrives;
        pDrives = pDrives->pNext;
        RTMemFree(pvDrive);
    }
}

/* Solaris hosts, loading libhal at runtime */

/**
 * Helper function to query the hal subsystem for information about DVD drives attached to the
 * system.
 *
 * @returns true if information was successfully obtained, false otherwise
 * @retval  list drives found will be attached to this list
 */
bool Host::getDVDInfoFromHal(std::list<ComObjPtr<Medium> > &list)
{
    bool halSuccess = false;
    DBusError dbusError;
    if (!gLibHalCheckPresence())
        return false;
    gDBusErrorInit (&dbusError);
    DBusConnection *dbusConnection = gDBusBusGet(DBUS_BUS_SYSTEM, &dbusError);
    if (dbusConnection != 0)
    {
        LibHalContext *halContext = gLibHalCtxNew();
        if (halContext != 0)
        {
            if (gLibHalCtxSetDBusConnection (halContext, dbusConnection))
            {
                if (gLibHalCtxInit(halContext, &dbusError))
                {
                    int numDevices;
                    char **halDevices = gLibHalFindDeviceStringMatch(halContext,
                                                "storage.drive_type", "cdrom",
                                                &numDevices, &dbusError);
                    if (halDevices != 0)
                    {
                        /* Hal is installed and working, so if no devices are reported, assume
                           that there are none. */
                        halSuccess = true;
                        for (int i = 0; i < numDevices; i++)
                        {
                            char *devNode = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "block.device", &dbusError);
#ifdef RT_OS_SOLARIS
                            /* The CD/DVD ioctls work only for raw device nodes. */
                            char *tmp = getfullrawname(devNode);
                            gLibHalFreeString(devNode);
                            devNode = tmp;
#endif

                            if (devNode != 0)
                            {
//                                if (validateDevice(devNode, true))
//                                {
                                    Utf8Str description;
                                    char *vendor, *product;
                                    /* We do not check the error here, as this field may
                                       not even exist. */
                                    vendor = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "info.vendor", 0);
                                    product = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "info.product", &dbusError);
                                    if ((product != 0 && product[0] != 0))
                                    {
                                        if ((vendor != 0) && (vendor[0] != 0))
                                        {
                                            description = Utf8StrFmt ("%s %s",
                                                                      vendor, product);
                                        }
                                        else
                                        {
                                            description = product;
                                        }
                                        ComObjPtr<Medium> hostDVDDriveObj;
                                        hostDVDDriveObj.createObject();
                                        hostDVDDriveObj->init(m->pParent, DeviceType_DVD,
                                                              Bstr(devNode), Bstr(description));
                                        list.push_back (hostDVDDriveObj);
                                    }
                                    else
                                    {
                                        if (product == 0)
                                        {
                                            LogRel(("Host::COMGETTER(DVDDrives): failed to get property \"info.product\" for device %s.  dbus error: %s (%s)\n",
                                                    halDevices[i], dbusError.name, dbusError.message));
                                            gDBusErrorFree(&dbusError);
                                        }
                                        ComObjPtr<Medium> hostDVDDriveObj;
                                        hostDVDDriveObj.createObject();
                                        hostDVDDriveObj->init(m->pParent, DeviceType_DVD,
                                                              Bstr(devNode));
                                        list.push_back (hostDVDDriveObj);
                                    }
                                    if (vendor != 0)
                                    {
                                        gLibHalFreeString(vendor);
                                    }
                                    if (product != 0)
                                    {
                                        gLibHalFreeString(product);
                                    }
//                                }
//                                else
//                                {
//                                    LogRel(("Host::COMGETTER(DVDDrives): failed to validate the block device %s as a DVD drive\n"));
//                                }
#ifndef RT_OS_SOLARIS
                                gLibHalFreeString(devNode);
#else
                                free(devNode);
#endif
                            }
                            else
                            {
                                LogRel(("Host::COMGETTER(DVDDrives): failed to get property \"block.device\" for device %s.  dbus error: %s (%s)\n",
                                        halDevices[i], dbusError.name, dbusError.message));
                                gDBusErrorFree(&dbusError);
                            }
                        }
                        gLibHalFreeStringArray(halDevices);
                    }
                    else
                    {
                        LogRel(("Host::COMGETTER(DVDDrives): failed to get devices with capability \"storage.cdrom\".  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
                        gDBusErrorFree(&dbusError);
                    }
                    if (!gLibHalCtxShutdown(halContext, &dbusError))  /* what now? */
                    {
                        LogRel(("Host::COMGETTER(DVDDrives): failed to shutdown the libhal context.  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
                        gDBusErrorFree(&dbusError);
                    }
                }
                else
                {
                    LogRel(("Host::COMGETTER(DVDDrives): failed to initialise libhal context.  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
                    gDBusErrorFree(&dbusError);
                }
                gLibHalCtxFree(halContext);
            }
            else
            {
                LogRel(("Host::COMGETTER(DVDDrives): failed to set libhal connection to dbus.\n"));
            }
        }
        else
        {
            LogRel(("Host::COMGETTER(DVDDrives): failed to get a libhal context - out of memory?\n"));
        }
        gDBusConnectionUnref(dbusConnection);
    }
    else
    {
        LogRel(("Host::COMGETTER(DVDDrives): failed to connect to dbus.  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
        gDBusErrorFree(&dbusError);
    }
    return halSuccess;
}


/**
 * Helper function to query the hal subsystem for information about floppy drives attached to the
 * system.
 *
 * @returns true if information was successfully obtained, false otherwise
 * @retval  list drives found will be attached to this list
 */
bool Host::getFloppyInfoFromHal(std::list< ComObjPtr<Medium> > &list)
{
    bool halSuccess = false;
    DBusError dbusError;
    if (!gLibHalCheckPresence())
        return false;
    gDBusErrorInit (&dbusError);
    DBusConnection *dbusConnection = gDBusBusGet(DBUS_BUS_SYSTEM, &dbusError);
    if (dbusConnection != 0)
    {
        LibHalContext *halContext = gLibHalCtxNew();
        if (halContext != 0)
        {
            if (gLibHalCtxSetDBusConnection (halContext, dbusConnection))
            {
                if (gLibHalCtxInit(halContext, &dbusError))
                {
                    int numDevices;
                    char **halDevices = gLibHalFindDeviceStringMatch(halContext,
                                                "storage.drive_type", "floppy",
                                                &numDevices, &dbusError);
                    if (halDevices != 0)
                    {
                        /* Hal is installed and working, so if no devices are reported, assume
                           that there are none. */
                        halSuccess = true;
                        for (int i = 0; i < numDevices; i++)
                        {
                            char *driveType = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "storage.drive_type", 0);
                            if (driveType != 0)
                            {
                                if (strcmp(driveType, "floppy") != 0)
                                {
                                    gLibHalFreeString(driveType);
                                    continue;
                                }
                                gLibHalFreeString(driveType);
                            }
                            else
                            {
                                /* An error occurred.  The attribute "storage.drive_type"
                                   probably didn't exist. */
                                continue;
                            }
                            char *devNode = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "block.device", &dbusError);
                            if (devNode != 0)
                            {
//                                if (validateDevice(devNode, false))
//                                {
                                    Utf8Str description;
                                    char *vendor, *product;
                                    /* We do not check the error here, as this field may
                                       not even exist. */
                                    vendor = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "info.vendor", 0);
                                    product = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "info.product", &dbusError);
                                    if ((product != 0) && (product[0] != 0))
                                    {
                                        if ((vendor != 0) && (vendor[0] != 0))
                                        {
                                            description = Utf8StrFmt ("%s %s",
                                                                      vendor, product);
                                        }
                                        else
                                        {
                                            description = product;
                                        }
                                        ComObjPtr<Medium> hostFloppyDrive;
                                        hostFloppyDrive.createObject();
                                        hostFloppyDrive->init(m->pParent, DeviceType_DVD,
                                                              Bstr(devNode), Bstr(description));
                                        list.push_back (hostFloppyDrive);
                                    }
                                    else
                                    {
                                        if (product == 0)
                                        {
                                            LogRel(("Host::COMGETTER(FloppyDrives): failed to get property \"info.product\" for device %s.  dbus error: %s (%s)\n",
                                                    halDevices[i], dbusError.name, dbusError.message));
                                            gDBusErrorFree(&dbusError);
                                        }
                                        ComObjPtr<Medium> hostFloppyDrive;
                                        hostFloppyDrive.createObject();
                                        hostFloppyDrive->init(m->pParent, DeviceType_DVD,
                                                              Bstr(devNode));
                                        list.push_back (hostFloppyDrive);
                                    }
                                    if (vendor != 0)
                                    {
                                        gLibHalFreeString(vendor);
                                    }
                                    if (product != 0)
                                    {
                                        gLibHalFreeString(product);
                                    }
//                                }
//                                else
//                                {
//                                    LogRel(("Host::COMGETTER(FloppyDrives): failed to validate the block device %s as a floppy drive\n"));
//                                }
                                gLibHalFreeString(devNode);
                            }
                            else
                            {
                                LogRel(("Host::COMGETTER(FloppyDrives): failed to get property \"block.device\" for device %s.  dbus error: %s (%s)\n",
                                        halDevices[i], dbusError.name, dbusError.message));
                                gDBusErrorFree(&dbusError);
                            }
                        }
                        gLibHalFreeStringArray(halDevices);
                    }
                    else
                    {
                        LogRel(("Host::COMGETTER(FloppyDrives): failed to get devices with capability \"storage.cdrom\".  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
                        gDBusErrorFree(&dbusError);
                    }
                    if (!gLibHalCtxShutdown(halContext, &dbusError))  /* what now? */
                    {
                        LogRel(("Host::COMGETTER(FloppyDrives): failed to shutdown the libhal context.  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
                        gDBusErrorFree(&dbusError);
                    }
                }
                else
                {
                    LogRel(("Host::COMGETTER(FloppyDrives): failed to initialise libhal context.  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
                    gDBusErrorFree(&dbusError);
                }
                gLibHalCtxFree(halContext);
            }
            else
            {
                LogRel(("Host::COMGETTER(FloppyDrives): failed to set libhal connection to dbus.\n"));
            }
        }
        else
        {
            LogRel(("Host::COMGETTER(FloppyDrives): failed to get a libhal context - out of memory?\n"));
        }
        gDBusConnectionUnref(dbusConnection);
    }
    else
    {
        LogRel(("Host::COMGETTER(FloppyDrives): failed to connect to dbus.  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
        gDBusErrorFree(&dbusError);
    }
    return halSuccess;
}
#endif  /* RT_OS_SOLARIS and VBOX_USE_HAL */

/** @todo get rid of dead code below - RT_OS_SOLARIS and RT_OS_LINUX are never both set */
#if defined(RT_OS_SOLARIS)

/**
 * Helper function to parse the given mount file and add found entries
 */
void Host::parseMountTable(char *mountTable, std::list< ComObjPtr<Medium> > &list)
{
#ifdef RT_OS_LINUX
    FILE *mtab = setmntent(mountTable, "r");
    if (mtab)
    {
        struct mntent *mntent;
        char *mnt_type;
        char *mnt_dev;
        char *tmp;
        while ((mntent = getmntent(mtab)))
        {
            mnt_type = (char*)malloc(strlen(mntent->mnt_type) + 1);
            mnt_dev = (char*)malloc(strlen(mntent->mnt_fsname) + 1);
            strcpy(mnt_type, mntent->mnt_type);
            strcpy(mnt_dev, mntent->mnt_fsname);
            // supermount fs case
            if (strcmp(mnt_type, "supermount") == 0)
            {
                tmp = strstr(mntent->mnt_opts, "fs=");
                if (tmp)
                {
                    free(mnt_type);
                    mnt_type = strdup(tmp + strlen("fs="));
                    if (mnt_type)
                    {
                        tmp = strchr(mnt_type, ',');
                        if (tmp)
                            *tmp = '\0';
                    }
                }
                tmp = strstr(mntent->mnt_opts, "dev=");
                if (tmp)
                {
                    free(mnt_dev);
                    mnt_dev = strdup(tmp + strlen("dev="));
                    if (mnt_dev)
                    {
                        tmp = strchr(mnt_dev, ',');
                        if (tmp)
                            *tmp = '\0';
                    }
                }
            }
            // use strstr here to cover things fs types like "udf,iso9660"
            if (strstr(mnt_type, "iso9660") == 0)
            {
                /** @todo check whether we've already got the drive in our list! */
                if (validateDevice(mnt_dev, true))
                {
                    ComObjPtr<Medium> hostDVDDriveObj;
                    hostDVDDriveObj.createObject();
                    hostDVDDriveObj->init(m->pParent, DeviceType_DVD, Bstr(mnt_dev));
                    list.push_back (hostDVDDriveObj);
                }
            }
            free(mnt_dev);
            free(mnt_type);
        }
        endmntent(mtab);
    }
#else  // RT_OS_SOLARIS
    FILE *mntFile = fopen(mountTable, "r");
    if (mntFile)
    {
        struct mnttab mntTab;
        while (getmntent(mntFile, &mntTab) == 0)
        {
            const char *mountName = mntTab.mnt_special;
            const char *mountPoint = mntTab.mnt_mountp;
            const char *mountFSType = mntTab.mnt_fstype;
            if (mountName && mountPoint && mountFSType)
            {
                // skip devices we are not interested in
                if ((*mountName && mountName[0] == '/') &&                      // skip 'fake' devices (like -hosts, proc, fd, swap)
                    (*mountFSType && (strncmp(mountFSType, "devfs", 5) != 0 &&  // skip devfs (i.e. /devices)
                                      strncmp(mountFSType, "dev", 3) != 0 &&    // skip dev (i.e. /dev)
                                      strncmp(mountFSType, "lofs", 4) != 0)))   // skip loop-back file-system (lofs)
                {
                    char *rawDevName = getfullrawname((char *)mountName);
                    if (validateDevice(rawDevName, true))
                    {
                        ComObjPtr<Medium> hostDVDDriveObj;
                        hostDVDDriveObj.createObject();
                        hostDVDDriveObj->init(m->pParent, DeviceType_DVD, Bstr(rawDevName));
                        list.push_back (hostDVDDriveObj);
                    }
                    free(rawDevName);
                }
            }
        }

        fclose(mntFile);
    }
#endif
}

/**
 * Helper function to check whether the given device node is a valid drive
 */
bool Host::validateDevice(const char *deviceNode, bool isCDROM)
{
    struct stat statInfo;
    bool retValue = false;

    // sanity check
    if (!deviceNode)
    {
        return false;
    }

    // first a simple stat() call
    if (stat(deviceNode, &statInfo) < 0)
    {
        return false;
    }
    else
    {
        if (isCDROM)
        {
            if (S_ISCHR(statInfo.st_mode) || S_ISBLK(statInfo.st_mode))
            {
                int fileHandle;
                // now try to open the device
                fileHandle = open(deviceNode, O_RDONLY | O_NONBLOCK, 0);
                if (fileHandle >= 0)
                {
                    cdrom_subchnl cdChannelInfo;
                    cdChannelInfo.cdsc_format = CDROM_MSF;
                    // this call will finally reveal the whole truth
#ifdef RT_OS_LINUX
                    if ((ioctl(fileHandle, CDROMSUBCHNL, &cdChannelInfo) == 0) ||
                        (errno == EIO) || (errno == ENOENT) ||
                        (errno == EINVAL) || (errno == ENOMEDIUM))
#else
                    if ((ioctl(fileHandle, CDROMSUBCHNL, &cdChannelInfo) == 0) ||
                        (errno == EIO) || (errno == ENOENT) ||
                        (errno == EINVAL))
#endif
                    {
                        retValue = true;
                    }
                    close(fileHandle);
                }
            }
        } else
        {
            // floppy case
            if (S_ISCHR(statInfo.st_mode) || S_ISBLK(statInfo.st_mode))
            {
                /// @todo do some more testing, maybe a nice IOCTL!
                retValue = true;
            }
        }
    }
    return retValue;
}
#endif // RT_OS_SOLARIS

#ifdef VBOX_WITH_USB
/**
 *  Checks for the presence and status of the USB Proxy Service.
 *  Returns S_OK when the Proxy is present and OK, VBOX_E_HOST_ERROR (as a
 *  warning) if the proxy service is not available due to the way the host is
 *  configured (at present, that means that usbfs and hal/DBus are not
 *  available on a Linux host) or E_FAIL and a corresponding error message
 *  otherwise. Intended to be used by methods that rely on the Proxy Service
 *  availability.
 *
 *  @note This method may return a warning result code. It is recommended to use
 *        MultiError to store the return value.
 *
 *  @note Locks this object for reading.
 */
HRESULT Host::checkUSBProxyService()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(m->pUSBProxyService, E_FAIL);
    if (!m->pUSBProxyService->isActive())
    {
        /* disable the USB controller completely to avoid assertions if the
         * USB proxy service could not start. */

        if (m->pUSBProxyService->getLastError() == VERR_FILE_NOT_FOUND)
            return setWarning(E_FAIL,
                              tr("Could not load the Host USB Proxy Service (%Rrc). The service might not be installed on the host computer"),
                              m->pUSBProxyService->getLastError());
        if (m->pUSBProxyService->getLastError() == VINF_SUCCESS)
#ifdef RT_OS_LINUX
            return setWarning (VBOX_E_HOST_ERROR,
# ifdef VBOX_WITH_DBUS
                tr ("The USB Proxy Service could not be started, because neither the USB file system (usbfs) nor the hardware information service (hal) is available")
# else
                tr ("The USB Proxy Service could not be started, because the USB file system (usbfs) is not available")
# endif
                );
#else  /* !RT_OS_LINUX */
            return setWarning (E_FAIL,
                tr ("The USB Proxy Service has not yet been ported to this host"));
#endif /* !RT_OS_LINUX */
        return setWarning (E_FAIL,
            tr ("Could not load the Host USB Proxy service (%Rrc)"),
            m->pUSBProxyService->getLastError());
    }

    return S_OK;
}
#endif /* VBOX_WITH_USB */

#ifdef VBOX_WITH_RESOURCE_USAGE_API

void Host::registerMetrics(PerformanceCollector *aCollector)
{
    pm::CollectorHAL *hal = aCollector->getHAL();
    /* Create sub metrics */
    pm::SubMetric *cpuLoadUser   = new pm::SubMetric("CPU/Load/User",
        "Percentage of processor time spent in user mode.");
    pm::SubMetric *cpuLoadKernel = new pm::SubMetric("CPU/Load/Kernel",
        "Percentage of processor time spent in kernel mode.");
    pm::SubMetric *cpuLoadIdle   = new pm::SubMetric("CPU/Load/Idle",
        "Percentage of processor time spent idling.");
    pm::SubMetric *cpuMhzSM      = new pm::SubMetric("CPU/MHz",
        "Average of current frequency of all processors.");
    pm::SubMetric *ramUsageTotal = new pm::SubMetric("RAM/Usage/Total",
        "Total physical memory installed.");
    pm::SubMetric *ramUsageUsed  = new pm::SubMetric("RAM/Usage/Used",
        "Physical memory currently occupied.");
    pm::SubMetric *ramUsageFree  = new pm::SubMetric("RAM/Usage/Free",
        "Physical memory currently available to applications.");
    pm::SubMetric *ramVMMUsed = new pm::SubMetric("RAM/VMM/Used",
        "Total physical memory used by the hypervisor.");
    pm::SubMetric *ramVMMFree = new pm::SubMetric("RAM/VMM/Free",
        "Total physical memory free inside the hypervisor.");
    pm::SubMetric *ramVMMBallooned  = new pm::SubMetric("RAM/VMM/Ballooned",
        "Total physical memory ballooned by the hypervisor.");
    pm::SubMetric *ramVMMShared = new pm::SubMetric("RAM/VMM/Shared",
        "Total physical memory shared between VMs.");


    /* Create and register base metrics */
    IUnknown *objptr;
    ComObjPtr<Host> tmp = this;
    tmp.queryInterfaceTo(&objptr);
    pm::BaseMetric *cpuLoad = new pm::HostCpuLoadRaw(hal, objptr, cpuLoadUser, cpuLoadKernel,
                                          cpuLoadIdle);
    aCollector->registerBaseMetric (cpuLoad);
    pm::BaseMetric *cpuMhz = new pm::HostCpuMhz(hal, objptr, cpuMhzSM);
    aCollector->registerBaseMetric (cpuMhz);
    pm::BaseMetric *ramUsage = new pm::HostRamUsage(hal, objptr, ramUsageTotal, ramUsageUsed,
                                           ramUsageFree, ramVMMUsed, ramVMMFree, ramVMMBallooned, ramVMMShared);
    aCollector->registerBaseMetric (ramUsage);

    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadUser, 0));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadUser,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadUser,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadUser,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadKernel, 0));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadKernel,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadKernel,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadKernel,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadIdle, 0));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadIdle,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadIdle,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadIdle,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(cpuMhz, cpuMhzSM, 0));
    aCollector->registerMetric(new pm::Metric(cpuMhz, cpuMhzSM,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(cpuMhz, cpuMhzSM,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(cpuMhz, cpuMhzSM,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageTotal, 0));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageTotal,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageTotal,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageTotal,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageUsed, 0));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageUsed,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageUsed,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageUsed,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageFree, 0));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageFree,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageFree,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageFree,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMUsed, 0));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMUsed,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMUsed,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMUsed,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMFree, 0));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMFree,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMFree,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMFree,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMBallooned, 0));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMBallooned,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMBallooned,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMBallooned,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMShared, 0));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMShared,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMShared,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramVMMShared,
                                              new pm::AggregateMax()));
}

void Host::unregisterMetrics (PerformanceCollector *aCollector)
{
    aCollector->unregisterMetricsFor(this);
    aCollector->unregisterBaseMetricsFor(this);
}

#endif /* VBOX_WITH_RESOURCE_USAGE_API */

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
