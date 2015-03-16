/* ****************************************************************************** *\ 
 
 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2012 Intel Corporation. All Rights Reserved.
  
\* ****************************************************************************** */

#ifndef __THREAD_DEFS_H__
#define __THREAD_DEFS_H__

#include "mfxdefs.h"
#include <windows.h>
#include <process.h>

class MSDKMutex
{
public:
    MSDKMutex(void);
    ~MSDKMutex(void);

    mfxStatus Lock(void);
    mfxStatus Unlock(void);
    int Try(void);
    
private:
    bool m_bInitialized;
    CRITICAL_SECTION m_CritSec;
};

class AutomaticMutex
{
public:
    AutomaticMutex(MSDKMutex& mutex);
    ~AutomaticMutex(void);

private: 
    void Lock(void);
    void Unlock(void);

    MSDKMutex* m_pMutex;
    bool m_bLocked;
};

class MSDKEvent
{
public:
    MSDKEvent(mfxStatus &sts, bool manual, bool state);
    ~MSDKEvent(void);

    void Signal(void);
    void Reset(void);
    void Wait(void);
    mfxStatus TimedWait(mfxU32 msec);

private:
    void* m_event;
};

#define MSDK_THREAD_CALLCONVENTION __stdcall

typedef unsigned int (MSDK_THREAD_CALLCONVENTION * msdk_thread_callback)(void*);

class MSDKThread
{
public:
    MSDKThread(mfxStatus &sts, msdk_thread_callback func, void* arg);
    ~MSDKThread(void);

    void Wait(void);
    mfxStatus TimedWait(mfxU32 msec);
    mfxStatus GetExitCode();

protected:
    void* m_thread;
};

#endif //__THREAD_DEFS_H__