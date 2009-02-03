/*
 * Copyright (C) 2002 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/*
 * Sun LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Sun elects to use only
 * the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
 * a choice of LGPL license versions is made available with the language indicating
 * that LGPLv2 or any later version may be used, or where a choice of which version
 * of the LGPL is applied is otherwise unspecified.
 */

#ifndef __DSHOW_INCLUDED__
#define __DSHOW_INCLUDED__

#define AM_NOVTABLE

#ifndef __WINESRC__
# include <windows.h>
# include <windowsx.h>
#else
# include <windef.h>
# include <wingdi.h>
# include <objbase.h>
#endif
#include <olectl.h>
#include <ddraw.h>
#include <mmsystem.h>
/* FIXME: #include <strsafe.h>*/

#ifndef NUMELMS
#define NUMELMS(array) (sizeof(array)/sizeof((array)[0]))
#endif

#include <strmif.h>
#include <amvideo.h>
#ifdef DSHOW_USE_AMAUDIO
/* FIXME: #include <amaudio.h>*/
#endif
#include <control.h>
#include <evcode.h>
#include <uuids.h>
#include <errors.h>
/* FIXME: #include <edevdefs.h> */
#include <audevcod.h>
/* FIXME: #include <dvdevcod.h> */

#ifndef OATRUE
#define OATRUE (-1)
#endif
#ifndef OAFALSE
#define OAFALSE (0)
#endif

#endif /* __DSHOW_INCLUDED__ */
