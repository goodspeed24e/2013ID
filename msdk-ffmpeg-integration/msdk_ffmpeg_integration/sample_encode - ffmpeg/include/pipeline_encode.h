//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2005-2012 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __PIPELINE_ENCODE_H__
#define __PIPELINE_ENCODE_H__

#include "sample_defs.h"

#ifdef D3D_SURFACES_SUPPORT
#pragma warning(disable : 4201)
#include "hw_device.h"
#endif

#include "sample_utils.h"
#include "base_allocator.h"

#include "mfxmvc.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxplugin++.h"

#include <vector>
#include <memory>

// =========== ffmpeg muxer integration ============
#define MAXSPSPPSBUFFERSIZE 1000  // Arbitrary size... but 1000 should be enough...
// =========== ffmpeg muxer integration end ============

enum {
    MVC_DISABLED          = 0x0,
    MVC_ENABLED           = 0x1,
    MVC_VIEWOUTPUT        = 0x2,    // 2 output bitstreams
};

enum MemType {
    SYSTEM_MEMORY = 0x00,
    D3D9_MEMORY   = 0x01,
    D3D11_MEMORY  = 0x02,
};
struct sInputParams
{
    mfxU16 nTargetUsage;
    mfxU32 CodecId;
    mfxU32 ColorFormat;
    mfxU16 nPicStruct;
    mfxU16 nWidth; // source picture width
    mfxU16 nHeight; // source picture height
    mfxF64 dFrameRate;
    mfxU16 nBitRate;   
    mfxU16 MVC_flags;

    mfxU32 numViews; // number of views for Multi-View-Codec

    mfxU16 nDstWidth; // destination picture width, specified if resizing required
    mfxU16 nDstHeight; // destination picture height, specified if resizing required

    MemType memType; 
    bool bUseHWLib; // true if application wants to use HW mfx library

    msdk_char strSrcFile[MSDK_MAX_FILENAME_LEN];

    std::vector<msdk_char*> srcFileBuff;
    std::vector<msdk_char*> dstFileBuff;

    mfxU8 nRotationAngle; // if specified, enables rotation plugin in mfx pipeline
    msdk_char strPluginDLLPath[MSDK_MAX_FILENAME_LEN]; // plugin dll path and name

	// =========== ffmpeg muxer integration ============
	bool bMux;
	bool bMuxMkv;
	// =========== ffmpeg muxer integration end ============
};

struct sTask
{
    mfxBitstream mfxBS;
    mfxSyncPoint EncSyncP;
    std::list<mfxSyncPoint> DependentVppTasks;
    CSmplBitstreamWriter *pWriter;

    sTask();
    mfxStatus WriteBitstream();
    mfxStatus Reset();
    mfxStatus Init(mfxU32 nBufferSize, CSmplBitstreamWriter *pWriter = NULL);
    mfxStatus Close();
};

class CEncTaskPool
{
public:
    CEncTaskPool();
    virtual ~CEncTaskPool();

    virtual mfxStatus Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize, CSmplBitstreamWriter *pOtherWriter = NULL);
    virtual mfxStatus GetFreeTask(sTask **ppTask);
    virtual mfxStatus SynchronizeFirstTask();      
    virtual void Close();

protected:
    sTask* m_pTasks;
    mfxU32 m_nPoolSize;    
    mfxU32 m_nTaskBufferStart;

    MFXVideoSession* m_pmfxSession;

    virtual mfxU32 GetFreeTaskIndex();    
};

/* This class implements a pipeline with 2 mfx components: vpp (video preprocessing) and encode */
class CEncodingPipeline
{
public:
    CEncodingPipeline();
    virtual ~CEncodingPipeline();

    virtual mfxStatus Init(sInputParams *pParams);
    virtual mfxStatus Run();
    virtual void Close();
    virtual mfxStatus ResetMFXComponents(sInputParams* pParams);
    virtual mfxStatus ResetDevice();

    void SetMultiView();
    void SetNumView(mfxU32 numViews) { m_nNumView = numViews; }
    virtual void  PrintInfo();

protected:
    std::pair<CSmplBitstreamWriter *,
              CSmplBitstreamWriter *> m_FileWriters;
    CSmplYUVReader m_FileReader;  
    CEncTaskPool m_TaskPool; 
    mfxU16 m_nAsyncDepth; // depth of asynchronous pipeline, this number can be tuned to achieve better performance

    MFXVideoSession m_mfxSession;
    MFXVideoENCODE* m_pmfxENC;
    MFXVideoVPP* m_pmfxVPP;

    mfxVideoParam m_mfxEncParams; 
    mfxVideoParam m_mfxVppParams;
    
    mfxU16 m_MVCflags; // MVC codec is in use
    
    MFXFrameAllocator* m_pMFXAllocator; 
    mfxAllocatorParams* m_pmfxAllocatorParams;
    MemType m_memType; 
    bool m_bExternalAlloc; // use memory allocator as external for Media SDK

    mfxFrameSurface1* m_pEncSurfaces; // frames array for encoder input (vpp output)
    mfxFrameSurface1* m_pVppSurfaces; // frames array for vpp input
    mfxFrameAllocResponse m_EncResponse;  // memory allocation response for encoder  
    mfxFrameAllocResponse m_VppResponse;  // memory allocation response for vpp  

    mfxU32 m_nNumView;

    // for disabling VPP algorithms
    mfxExtVPPDoNotUse m_VppDoNotUse; 
    // for MVC encoder and VPP configuration
    mfxExtMVCSeqDesc m_MVCSeqDesc;
    mfxExtCodingOption m_CodingOption;

    // external parameters for each component are stored in a vector
    std::vector<mfxExtBuffer*> m_VppExtParams;
    std::vector<mfxExtBuffer*> m_EncExtParams;

#if D3D_SURFACES_SUPPORT
    CHWDevice *m_hwdev;
#endif  

	// =========== ffmpeg muxer integration ============
	bool m_bMux;
	mfxU8 SPSBuffer[MAXSPSPPSBUFFERSIZE];
	mfxU8 PPSBuffer[MAXSPSPPSBUFFERSIZE];
	// =========== ffmpeg muxer integration end ============

    virtual mfxStatus DetermineMinimumRequiredVersion(const sInputParams &pParams, mfxVersion &version);

    virtual mfxStatus InitMfxEncParams(sInputParams *pParams);
    virtual mfxStatus InitMfxVppParams(sInputParams *pParams);
    
    virtual mfxStatus InitFileWriters(sInputParams *pParams);
    virtual void FreeFileWriters();
    virtual mfxStatus InitFileWriter(CSmplBitstreamWriter **ppWriter, const msdk_char *filename);

    virtual mfxStatus AllocAndInitVppDoNotUse();
    virtual void FreeVppDoNotUse(); 

    virtual mfxStatus AllocAndInitMVCSeqDesc();
    virtual void FreeMVCSeqDesc();   
  
    virtual mfxStatus CreateAllocator();
    virtual void DeleteAllocator();    

    virtual mfxStatus CreateHWDevice(); 
    virtual void DeleteHWDevice();

    virtual mfxStatus AllocFrames();
    virtual void DeleteFrames();         
        
    virtual mfxStatus AllocateSufficientBuffer(mfxBitstream* pBS);

    virtual mfxStatus GetFreeTask(sTask **ppTask);
    virtual mfxStatus SynchronizeFirstTask();
};

// =========== ffmpeg muxer integration ============

// Disable type conversion warning to remove the annoying complaint about the ffmpeg "libavutil\common.h" file
#pragma warning( disable : 4244 )

#ifdef __cplusplus

#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <stdint.h>

extern "C"
{
#endif

#include "libavformat/avformat.h"
#include "libavformat/avio.h"

#ifdef __cplusplus
} // extern "C"
#endif

// If set, encodes audio stream and muxes it into container
#define ENCODE_AUDIO


class FFMPEGWriter: public CSmplBitstreamWriter
{
public :
    FFMPEGWriter();
    virtual ~FFMPEGWriter();

    virtual mfxStatus Init(const TCHAR *strFileName, sInputParams* pParams, mfxU8* SPSbuf, int SPSbufsize, mfxU8* PPSbuf, int PPSbufsize);
    virtual mfxStatus WriteNextFrame(mfxBitstream *pMfxBitstream, bool isPrint = true);
    virtual void Close();

protected:
    mfxU32				m_nProcessedFramesNum;
    bool				m_bInited;    

	AVOutputFormat*		m_pFmt;
	AVFormatContext*	m_pFormatCtx;
	AVStream*			m_pVideoStream;
	mfxU8*				m_pExtDataBuffer;	 
};

// =========== ffmpeg muxer integration end ============

#endif // __PIPELINE_ENCODE_H__ 