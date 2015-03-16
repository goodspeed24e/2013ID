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
   
#ifndef __PIPELINE_DECODE_H__
#define __PIPELINE_DECODE_H__

#include "sample_defs.h"

#if D3D_SURFACES_SUPPORT
#pragma warning(disable : 4201)
#include <d3d9.h>
#include <dxva2api.h>
#endif

#include <vector>
#include "hw_device.h"
#include "decode_render.h"
#include <memory>

#include "sample_utils.h"
#include "base_allocator.h"

#include "mfxmvc.h"
#include "mfxjpeg.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"

enum MemType {
    SYSTEM_MEMORY = 0x00,
    D3D9_MEMORY   = 0x01,
    D3D11_MEMORY  = 0x02,
};
struct sInputParams
{
    mfxU32 videoType;
    bool   bOutput; // if renderer is enabled, possibly no need in output file
    MemType memType; 
    bool   bUseHWLib; // true if application wants to use HW mfx library
    bool   bIsMVC; // true if Multi-View-Codec is in use
    bool   bRendering; // true if d3d rendering is in use
    bool   bLowLat; // low latency mode
    bool   bCalLat; // latency calculation
    mfxU32 nWallCell;
    mfxU32 nWallW;//number of windows located in each row
    mfxU32 nWallH;//number of windows located in each column
    mfxU32 nWallMonitor;//monitor id, 0,1,.. etc
    mfxU32 nWallFPS;//rendering limited by certain fps
    bool   bNoTitle;//whether to show title for each window with fps value
    mfxU32 numViews; // number of views for Multi-View-Codec

	// =========== ffmpeg splitter integration ============
	bool bSplit;
	// =========== ffmpeg splitter integration end ============

    msdk_char  strSrcFile[MSDK_MAX_FILENAME_LEN];
    msdk_char  strDstFile[MSDK_MAX_FILENAME_LEN];
    sInputParams()
    {
        MSDK_ZERO_MEMORY(*this);
    }
};

template<>struct mfx_ext_buffer_id<mfxExtMVCSeqDesc>{
    enum {id = MFX_EXTBUFF_MVC_SEQ_DESC};
};
class CDecodingPipeline
{
public:

    CDecodingPipeline();
    virtual ~CDecodingPipeline();

    virtual mfxStatus Init(sInputParams *pParams);
    virtual mfxStatus RunDecoding();
    virtual void Close();
    virtual mfxStatus ResetDecoder(sInputParams *pParams);
    virtual mfxStatus ResetDevice();

    void SetMultiView();
    void SetExtBuffersFlag()       { m_bIsExtBuffers = true; }
    void SetRenderingFlag()        { m_bIsRender = true; }
    void SetOutputfileFlag(bool b) { m_bOutput = b; }
    virtual void PrintInfo();

protected:
    CSmplYUVWriter          m_FileWriter;
    std::auto_ptr<CSmplBitstreamReader>  m_FileReader;
    mfxU32                  m_nFrameIndex; // index of processed frame
    mfxBitstream            m_mfxBS; // contains encoded data

    MFXVideoSession     m_mfxSession;
    MFXVideoDECODE*     m_pmfxDEC;
    mfxVideoParam       m_mfxVideoParams; 

    std::vector<mfxExtBuffer *> m_ExtBuffers;
    
    MFXFrameAllocator*      m_pMFXAllocator; 
    mfxAllocatorParams*     m_pmfxAllocatorParams;
    MemType                 m_memType;      // memory type of surfaces to use
    bool                    m_bExternalAlloc; // use memory allocator as external for Media SDK
    mfxFrameSurface1*       m_pmfxSurfaces; // frames array
    mfxFrameAllocResponse   m_mfxResponse;  // memory allocation response for decoder  

    bool                    m_bIsMVC; // enables MVC mode (need to support several files as an output)
    bool                    m_bIsExtBuffers; // indicates if external buffers were allocated
    bool                    m_bIsRender; // enables rendering mode
    bool                    m_bOutput; // enables/disables output file
    bool                    m_bIsVideoWall;//indicates special mode: decoding will be done in a loop
    bool                    m_bIsCompleteFrame;
    bool                    m_bPrintLatency;

    std::vector<mfxF64>     m_vLatency;

    CHWDevice               *m_hwdev;
#if D3D_SURFACES_SUPPORT
    IGFXS3DControl          *m_pS3DControl;
    
    IDirect3DSurface9*       m_pRenderSurface;
    CDecodeD3DRender         m_d3dRender; 
#endif

    virtual mfxStatus DetermineMinimumRequiredVersion(const sInputParams &pParams, mfxVersion &version);

    virtual mfxStatus InitMfxParams(sInputParams *pParams);

    // function for allocating a specific external buffer
    template <typename Buffer>
    mfxStatus AllocateExtBuffer();
    virtual void DeleteExtBuffers();

    virtual mfxStatus AllocateExtMVCBuffers();
    virtual void    DeallocateExtMVCBuffers();

    virtual void AttachExtParam();

    virtual mfxStatus CreateAllocator();
    virtual mfxStatus CreateHWDevice(); 
    virtual mfxStatus AllocFrames();
    virtual void DeleteFrames();         
    virtual void DeleteAllocator();       
};


// =========== ffmpeg splitter integration ============

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
#include "libavcodec/avcodec.h"

#ifdef __cplusplus
} // extern "C"
#endif

// If set, decodes audio stream into raw PCM samples
#define DECODE_AUDIO

class FFMPEGReader : public CSmplBitstreamReader
{
public :

    FFMPEGReader();
    virtual ~FFMPEGReader();

    virtual void      Reset();
    virtual void      Close();
    virtual mfxStatus Init(const TCHAR *strFileName, mfxU32 videoType);
    virtual mfxStatus ReadNextFrame(mfxBitstream *pBS);

protected:
    bool						m_bInited;

	mfxU32						m_videoType;
	AVFormatContext*			m_pFormatCtx;
	int							m_videoStreamIdx;
	AVBitStreamFilterContext*	m_pBsfc;
#ifdef DECODE_AUDIO
	int							m_audioStreamIdx;
#endif
};

// =========== ffmpeg splitter integration end ============

#endif // __PIPELINE_DECODE_H__ 