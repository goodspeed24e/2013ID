//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2012 Intel Corporation. All Rights Reserved.
//

#include <tchar.h>
#include <windows.h>
#include <numeric>
#include <algorithm>
#include "pipeline_decode.h"
#include "sysmem_allocator.h"
#include "d3d_allocator.h"
#include "d3d11_allocator.h"

#include "d3d_device.h"
#include "d3d11_device.h"

#pragma warning(disable : 4100)

mfxStatus CDecodingPipeline::InitMfxParams(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(m_pmfxDEC, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 &numViews = pParams->numViews;

    // try to find a sequence header in the stream
    // if header is not found this function exits with error (e.g. if device was lost and there's no header in the remaining stream)
    for(;;)
    {
        // trying to find PicStruct information in AVI headers
        if ( m_mfxVideoParams.mfx.CodecId == MFX_CODEC_JPEG )
            MJPEG_AVI_ParsePicStruct(&m_mfxBS);

        // parse bit stream and fill mfx params
        sts = m_pmfxDEC->DecodeHeader(&m_mfxBS, &m_mfxVideoParams);

        if (MFX_ERR_MORE_DATA == sts)
        {
            if (m_mfxBS.MaxLength == m_mfxBS.DataLength)
            {
                sts = ExtendMfxBitstream(&m_mfxBS, m_mfxBS.MaxLength * 2); 
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            // read a portion of data             
            sts = m_FileReader->ReadNextFrame(&m_mfxBS);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            continue;
        }
        else
        {
            // Enter MVC mode
            if (m_bIsMVC)
            {
                // Check for attached external parameters - if we have them already,
                // we don't need to attach them again
                if (NULL != m_mfxVideoParams.ExtParam)
                    break;

                // allocate and attach external parameters for MVC decoder
                sts = AllocateExtBuffer<mfxExtMVCSeqDesc>(); 
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

                AttachExtParam();
                sts = m_pmfxDEC->DecodeHeader(&m_mfxBS, &m_mfxVideoParams);

                if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
                {
                    sts = AllocateExtMVCBuffers();
                    SetExtBuffersFlag();

                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                    MSDK_CHECK_POINTER(m_mfxVideoParams.ExtParam, MFX_ERR_MEMORY_ALLOC);
                    continue;
                }
            }

            // if input is interlaced JPEG stream
            if ( m_mfxBS.PicStruct == MFX_PICSTRUCT_FIELD_TFF || m_mfxBS.PicStruct == MFX_PICSTRUCT_FIELD_BFF)
            {
                m_mfxVideoParams.mfx.FrameInfo.CropH *= 2;
                m_mfxVideoParams.mfx.FrameInfo.Height = MSDK_ALIGN16(m_mfxVideoParams.mfx.FrameInfo.CropH);
                m_mfxVideoParams.mfx.FrameInfo.PicStruct = m_mfxBS.PicStruct;
            }

            break;
        }
    }

    // check DecodeHeader status
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // If MVC mode we need to detect number of views in stream
    if (m_bIsMVC)
    {
        mfxExtMVCSeqDesc* pSequenceBuffer;
        pSequenceBuffer = (mfxExtMVCSeqDesc*) GetExtBuffer(m_mfxVideoParams.ExtParam, m_mfxVideoParams.NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
        MSDK_CHECK_POINTER(pSequenceBuffer, MFX_ERR_INVALID_VIDEO_PARAM);

        mfxU32 i = 0;
        numViews = 0;
        for (i = 0; i < pSequenceBuffer->NumView; ++i)
        {
            /* Some MVC streams can contain different information about
               number of views and view IDs, e.x. numVews = 2 
               and ViewId[0, 1] = 0, 2 instead of ViewId[0, 1] = 0, 1.
               numViews should be equal (max(ViewId[i]) + 1) 
               to prevent crashes during output files writing */
            if (pSequenceBuffer->View[i].ViewId >= numViews)
                numViews = pSequenceBuffer->View[i].ViewId + 1;
        }
    }
    else
    {
        numViews = 1;
    }
    
    // specify memory type 
    m_mfxVideoParams.IOPattern = (mfxU16)(m_memType != SYSTEM_MEMORY ? MFX_IOPATTERN_OUT_VIDEO_MEMORY : MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    //reduce memory usage by allocation less surfaces
    if(m_bIsVideoWall || pParams->bLowLat)
        m_mfxVideoParams.AsyncDepth = 1;

    return MFX_ERR_NONE;
}

mfxStatus CDecodingPipeline::CreateHWDevice()
{
#if D3D_SURFACES_SUPPORT
    mfxStatus sts = MFX_ERR_NONE;

    HWND window = NULL;

    if (!m_bIsRender)
    {
        POINT point = {0, 0};
        window = WindowFromPoint(point);
    }
    else
        window = m_d3dRender.GetWindowHandle();

#if MFX_D3D11_SUPPORT
    if (D3D11_MEMORY == m_memType)
        m_hwdev = new CD3D11Device();
    else
#endif // #if MFX_D3D11_SUPPORT
        m_hwdev = new CD3D9Device();

    if (NULL == m_hwdev)
        return MFX_ERR_MEMORY_ALLOC;
    if (m_bIsRender && m_bIsMVC)
    {
        sts = m_hwdev->SetHandle((mfxHandleType)MFX_HANDLE_GFXS3DCONTROL, m_pS3DControl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    sts = m_hwdev->Init(
        window, 
        m_bIsRender ? (m_bIsMVC ? 2 : 1) : 0,
        GetMSDKAdapterNumber(m_mfxSession));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (m_bIsRender)
        m_d3dRender.SetHWDevice(m_hwdev);    
#endif
    return MFX_ERR_NONE;
}

mfxStatus CDecodingPipeline::ResetDevice()
{
    return m_hwdev->Reset();
}

mfxStatus CDecodingPipeline::AllocFrames()
{
    MSDK_CHECK_POINTER(m_pmfxDEC, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    mfxFrameAllocRequest Request;

    mfxU16 nSurfNum = 0; // number of surfaces for decoder

    MSDK_ZERO_MEMORY(Request);

    sts = m_pmfxDEC->Query(&m_mfxVideoParams, &m_mfxVideoParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM); 
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    // calculate number of surfaces required for decoder
    sts = m_pmfxDEC->QueryIOSurf(&m_mfxVideoParams, &Request);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    nSurfNum = MSDK_MAX(Request.NumFrameSuggested, 1);

    // prepare allocation request
    Request.NumFrameMin = nSurfNum;
    Request.NumFrameSuggested = nSurfNum;
    memcpy(&(Request.Info), &(m_mfxVideoParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
    Request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE; 

    // add info about memory type to request 
    Request.Type |= (m_memType != SYSTEM_MEMORY) ? MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET : MFX_MEMTYPE_SYSTEM_MEMORY; 
    // alloc frames for decoder
    sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &Request, &m_mfxResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // prepare mfxFrameSurface1 array for decoder
    nSurfNum = m_mfxResponse.NumFrameActual;
    m_pmfxSurfaces = new mfxFrameSurface1 [nSurfNum];
    MSDK_CHECK_POINTER(m_pmfxSurfaces, MFX_ERR_MEMORY_ALLOC);       

    for (int i = 0; i < nSurfNum; i++)
    {       
        memset(&(m_pmfxSurfaces[i]), 0, sizeof(mfxFrameSurface1));
        memcpy(&(m_pmfxSurfaces[i].Info), &(m_mfxVideoParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

        if (m_bExternalAlloc)
        {
            m_pmfxSurfaces[i].Data.MemId = m_mfxResponse.mids[i];    
        }
        else
        {
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_mfxResponse.mids[i], &(m_pmfxSurfaces[i].Data));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }  

    return MFX_ERR_NONE;
}

mfxStatus CDecodingPipeline::CreateAllocator()
{   
    mfxStatus sts = MFX_ERR_NONE;
 
    if (m_memType != SYSTEM_MEMORY)
    {
#if D3D_SURFACES_SUPPORT  
        sts = CreateHWDevice();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // provide device manager to MediaSDK
        mfxHDL hdl = NULL;
        mfxHandleType hdl_t = 
#if MFX_D3D11_SUPPORT
            D3D11_MEMORY == m_memType ? MFX_HANDLE_D3D11_DEVICE : 
#endif // #if MFX_D3D11_SUPPORT
            MFX_HANDLE_D3D9_DEVICE_MANAGER;

        sts = m_hwdev->GetHandle(hdl_t, &hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        
        sts = m_mfxSession.SetHandle(hdl_t, hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);        

        // create D3D allocator
#if MFX_D3D11_SUPPORT
        if (D3D11_MEMORY == m_memType)
        {
            m_pMFXAllocator = new D3D11FrameAllocator;
            MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

            D3D11AllocatorParams *pd3dAllocParams = new D3D11AllocatorParams;
            MSDK_CHECK_POINTER(pd3dAllocParams, MFX_ERR_MEMORY_ALLOC);
            pd3dAllocParams->pDevice = reinterpret_cast<ID3D11Device *>(hdl);

            m_pmfxAllocatorParams = pd3dAllocParams;
        }
        else
#endif // #if MFX_D3D11_SUPPORT
        {
            m_pMFXAllocator = new D3DFrameAllocator;
            MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

            D3DAllocatorParams *pd3dAllocParams = new D3DAllocatorParams;        
            MSDK_CHECK_POINTER(pd3dAllocParams, MFX_ERR_MEMORY_ALLOC);
            pd3dAllocParams->pManager = reinterpret_cast<IDirect3DDeviceManager9 *>(hdl);

            m_pmfxAllocatorParams = pd3dAllocParams;
        }

        /* In case of video memory we must provide MediaSDK with external allocator 
        thus we demonstrate "external allocator" usage model.
        Call SetAllocator to pass allocator to mediasdk */
        sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        m_bExternalAlloc = true;
#endif

    }
    else
    {
        // create system memory allocator       
        m_pMFXAllocator = new SysMemFrameAllocator;         
        MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

        /* In case of system memory we demonstrate "no external allocator" usage model.  
        We don't call SetAllocator, MediaSDK uses internal allocator. 
        We use system memory allocator simply as a memory manager for application*/           
    }

    // initialize memory allocator
    sts = m_pMFXAllocator->Init(m_pmfxAllocatorParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

void CDecodingPipeline::DeleteFrames()
{
    // delete surfaces array
    MSDK_SAFE_DELETE_ARRAY(m_pmfxSurfaces);    

    // delete frames
    if (m_pMFXAllocator)
    {
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_mfxResponse);
    }

    return;
}

void CDecodingPipeline::DeleteAllocator()
{
    // delete allocator
    MSDK_SAFE_DELETE(m_pMFXAllocator);   
    MSDK_SAFE_DELETE(m_pmfxAllocatorParams);
    MSDK_SAFE_DELETE(m_hwdev);
}

CDecodingPipeline::CDecodingPipeline()
{
    m_nFrameIndex = 0;
    m_pmfxDEC = NULL;
    m_pMFXAllocator = NULL;
    m_pmfxAllocatorParams = NULL;
    m_memType = SYSTEM_MEMORY;
    m_bExternalAlloc = false;
    m_pmfxSurfaces = NULL; 
    m_bIsMVC = false;
    m_bIsExtBuffers = false;
    m_bIsRender = false;
    m_bOutput = true;
    m_bIsVideoWall = false;
    m_bIsCompleteFrame = false;
    m_bPrintLatency = false;
    m_vLatency.reserve(1000); // reserve some space to reduce dynamic reallocation impact on pipeline execution
#if D3D_SURFACES_SUPPORT
    m_pS3DControl = NULL;
#endif


    m_hwdev = NULL;

    MSDK_ZERO_MEMORY(m_mfxVideoParams);
    MSDK_ZERO_MEMORY(m_mfxResponse);
    MSDK_ZERO_MEMORY(m_mfxBS);
}

CDecodingPipeline::~CDecodingPipeline()
{
    Close();
}

void CDecodingPipeline::SetMultiView()
{
    m_FileWriter.SetMultiView();
    m_bIsMVC = true;
}

#if D3D_SURFACES_SUPPORT
bool operator < (const IGFX_DISPLAY_MODE &l, const IGFX_DISPLAY_MODE& r)
{
    if (r.ulResWidth >= 0xFFFF || r.ulResHeight >= 0xFFFF || r.ulRefreshRate >= 0xFFFF)
        return false;

         if (l.ulResWidth < r.ulResWidth) return true;
    else if (l.ulResHeight < r.ulResHeight) return true;
    else if (l.ulRefreshRate < r.ulRefreshRate) return true;    
        
    return false;
}
#endif
mfxStatus CDecodingPipeline::DetermineMinimumRequiredVersion(const sInputParams &pParams, mfxVersion &version)
{
    version.Major = 1;
    version.Minor = 0;

    if (pParams.bIsMVC || pParams.bLowLat || pParams.videoType == MFX_CODEC_JPEG)
        version.Minor = 3;
    return MFX_ERR_NONE;
}
mfxStatus CDecodingPipeline::Init(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    // prepare input stream file reader
    // creating reader that support completeframe mode
    if (pParams->bLowLat || pParams->bCalLat)
    {
        switch (pParams->videoType)
        {
        case MFX_CODEC_AVC:
            m_FileReader.reset(new CH264FrameReader());
            m_bIsCompleteFrame = true;
            m_bPrintLatency =  pParams->bCalLat;
            break;
        case MFX_CODEC_JPEG:
            m_FileReader.reset(new CJPEGFrameReader());
            m_bIsCompleteFrame = true;
            m_bPrintLatency =  pParams->bCalLat;
            break;
        default:
            return MFX_ERR_UNSUPPORTED; // latency mode is supported only for H.264 and JPEG codecs
        }
        
    }
    else
    {
		if(pParams->bSplit)
		{
			// =========== ffmpeg splitter integration ============

			FFMPEGReader *ffmpegFR = new FFMPEGReader;
			MSDK_CHECK_POINTER(ffmpegFR, MFX_ERR_MEMORY_ALLOC);
			m_FileReader.reset(ffmpegFR);

			sts = ffmpegFR->Init(pParams->strSrcFile, pParams->videoType);
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

			// =========== ffmpeg splitter integration ============
		}
		else
		{
			m_FileReader.reset(new CSmplBitstreamReader());
		}
    }

	if(!pParams->bSplit)
	{
		sts = m_FileReader->Init(pParams->strSrcFile);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);     
	}
    
    // API version   
    mfxVersion version;
    sts = DetermineMinimumRequiredVersion(*pParams, version);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    // Init session
    if (pParams->bUseHWLib)
    {             
        // try searching on all display adapters
        mfxIMPL impl = MFX_IMPL_HARDWARE_ANY;
                
        // if d3d11 surfaces are used ask the library to run acceleration through D3D11
        // feature may be unsupported due to OS or MSDK API version
        if (D3D11_MEMORY == pParams->memType) 
            impl |= MFX_IMPL_VIA_D3D11;

        sts = m_mfxSession.Init(impl, &version);

        // MSDK API version may not support multiple adapters - then try initialize on the default
        if (MFX_ERR_NONE != sts)
            sts = m_mfxSession.Init(impl & !MFX_IMPL_HARDWARE_ANY | MFX_IMPL_HARDWARE, &version);        
    }
    else
        sts = m_mfxSession.Init(MFX_IMPL_SOFTWARE, &version);    

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // create decoder
    m_pmfxDEC = new MFXVideoDECODE(m_mfxSession);
    MSDK_CHECK_POINTER(m_pmfxDEC, MFX_ERR_MEMORY_ALLOC);    

    // set video type in parameters
    m_mfxVideoParams.mfx.CodecId = pParams->videoType; 
    // set memory type
    m_memType = pParams->memType;

    // Initialize rendering window
    if (pParams->bRendering)
    {
        if (m_bIsMVC)
        {
#if D3D_SURFACES_SUPPORT
            m_pS3DControl = CreateIGFXS3DControl();
            MSDK_CHECK_POINTER(m_pS3DControl, MFX_ERR_DEVICE_FAILED);

            // check if s3d supported and get a list of supported display modes
            IGFX_S3DCAPS caps;
            MSDK_ZERO_MEMORY(caps);
            HRESULT hr = m_pS3DControl->GetS3DCaps(&caps);
            if (FAILED(hr) || 0 >= caps.ulNumEntries)
            {
                MSDK_SAFE_DELETE(m_pS3DControl);
                return MFX_ERR_DEVICE_FAILED;
            } 
            
            // switch to 3D mode
            ULONG max = 0;
            MSDK_CHECK_POINTER(caps.S3DSupportedModes, MFX_ERR_NOT_INITIALIZED);
            for (ULONG i = 0; i < caps.ulNumEntries; i++)
            {
                if (caps.S3DSupportedModes[max] < caps.S3DSupportedModes[i]) 
                    max = i;
            }
            
            if (0 == pParams->nWallCell)
            {
                hr = m_pS3DControl->SwitchTo3D(&caps.S3DSupportedModes[max]);
                if (FAILED(hr))
                {
                    MSDK_SAFE_DELETE(m_pS3DControl);
                    return MFX_ERR_DEVICE_FAILED;
                }
            }
#endif
        }
#if D3D_SURFACES_SUPPORT
        sWindowParams windowParams;

        windowParams.lpWindowName = pParams->bNoTitle ? NULL : MSDK_STRING("sample_decode");
        windowParams.nx           = pParams->nWallW;
        windowParams.ny           = pParams->nWallH;
        windowParams.nWidth       = CW_USEDEFAULT;
        windowParams.nHeight      = CW_USEDEFAULT;        
        windowParams.ncell        = pParams->nWallCell;
        windowParams.nAdapter     = pParams->nWallMonitor;
        windowParams.nMaxFPS      = pParams->nWallFPS;

        windowParams.lpClassName  = MSDK_STRING("Render Window Class");
        windowParams.dwStyle      = WS_OVERLAPPEDWINDOW;
        windowParams.hWndParent   = NULL;
        windowParams.hMenu        = NULL;
        windowParams.hInstance    = GetModuleHandle(NULL);
        windowParams.lpParam      = NULL;
        windowParams.bFullScreen  = FALSE;

        m_d3dRender.Init(windowParams);
        
        SetRenderingFlag();
        //setting videowall flag
        m_bIsVideoWall = 0 != windowParams.nx;
#endif
    }

    // prepare bit stream
    sts = InitMfxBitstream(&m_mfxBS, 1024 * 1024);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);    

    // create device and allocator. SetHandle must be called after session Init and before any other MSDK calls, 
    // otherwise an own device will be created by MSDK
    sts = CreateAllocator();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Populate parameters. Involves DecodeHeader call
    sts = InitMfxParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    SetOutputfileFlag(pParams->bOutput);
    if (m_bOutput)
    {
        // prepare YUV file writer
        sts = m_FileWriter.Init(pParams->strDstFile, pParams->numViews);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    
    // if allocator is provided to MediaSDK as external, frames must be allocated prior to decoder initialization
    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // init decoder
    sts = m_pmfxDEC->Init(&m_mfxVideoParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

// function for allocating a specific external buffer
template <typename Buffer>
mfxStatus CDecodingPipeline::AllocateExtBuffer()
{
    std::auto_ptr<Buffer> pExtBuffer (new Buffer()); 
    if (!pExtBuffer.get())
        return MFX_ERR_MEMORY_ALLOC;

    init_ext_buffer(*pExtBuffer);

    m_ExtBuffers.push_back(reinterpret_cast<mfxExtBuffer*>(pExtBuffer.release()));

    return MFX_ERR_NONE;
}

void CDecodingPipeline::AttachExtParam()
{
    m_mfxVideoParams.ExtParam = reinterpret_cast<mfxExtBuffer**>(&m_ExtBuffers[0]);
    m_mfxVideoParams.NumExtParam = static_cast<mfxU16>(m_ExtBuffers.size());
}

void CDecodingPipeline::DeleteExtBuffers()
{
    for (std::vector<mfxExtBuffer *>::iterator it = m_ExtBuffers.begin(); it != m_ExtBuffers.end(); ++it)
    {
        mfxExtBuffer *tmp = *it;
        MSDK_SAFE_DELETE(tmp);
    }
    m_ExtBuffers.clear();
}

mfxStatus CDecodingPipeline::AllocateExtMVCBuffers()
{
    mfxU32 i;

    mfxExtMVCSeqDesc* pExtMVCBuffer = (mfxExtMVCSeqDesc*) m_mfxVideoParams.ExtParam[0];
    MSDK_CHECK_POINTER(pExtMVCBuffer, MFX_ERR_MEMORY_ALLOC);

    pExtMVCBuffer->View = new mfxMVCViewDependency[pExtMVCBuffer->NumView];
    MSDK_CHECK_POINTER(pExtMVCBuffer->View, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < pExtMVCBuffer->NumView; ++i)
    {
        MSDK_ZERO_MEMORY(pExtMVCBuffer->View[i]);
    }
    pExtMVCBuffer->NumViewAlloc = pExtMVCBuffer->NumView;

    pExtMVCBuffer->ViewId = new mfxU16[pExtMVCBuffer->NumViewId];
    MSDK_CHECK_POINTER(pExtMVCBuffer->ViewId, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < pExtMVCBuffer->NumViewId; ++i)
    {
        MSDK_ZERO_MEMORY(pExtMVCBuffer->ViewId[i]);
    }
    pExtMVCBuffer->NumViewIdAlloc = pExtMVCBuffer->NumViewId;

    pExtMVCBuffer->OP = new mfxMVCOperationPoint[pExtMVCBuffer->NumOP];
    MSDK_CHECK_POINTER(pExtMVCBuffer->OP, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < pExtMVCBuffer->NumOP; ++i)
    {
        MSDK_ZERO_MEMORY(pExtMVCBuffer->OP[i]);
    }
    pExtMVCBuffer->NumOPAlloc = pExtMVCBuffer->NumOP;

    return MFX_ERR_NONE;
}

void CDecodingPipeline::DeallocateExtMVCBuffers()
{
    mfxExtMVCSeqDesc* pExtMVCBuffer = (mfxExtMVCSeqDesc*) m_mfxVideoParams.ExtParam[0];
    if (pExtMVCBuffer != NULL)
    {
        MSDK_SAFE_DELETE_ARRAY(pExtMVCBuffer->View);
        MSDK_SAFE_DELETE_ARRAY(pExtMVCBuffer->ViewId);
        MSDK_SAFE_DELETE_ARRAY(pExtMVCBuffer->OP);
    }

    //MSDK_SAFE_DELETE(m_mfxVideoParams.ExtParam[0]);

    m_bIsExtBuffers = false;
}

void CDecodingPipeline::Close()
{
#if D3D_SURFACES_SUPPORT
    if (NULL != m_pS3DControl)
    {
        m_pS3DControl->SwitchTo2D(NULL);
        MSDK_SAFE_DELETE(m_pS3DControl);
    }
#endif
    WipeMfxBitstream(&m_mfxBS);
    MSDK_SAFE_DELETE(m_pmfxDEC);   

    DeleteFrames();
    
    // allocator if used as external for MediaSDK must be deleted after decoder
    DeleteAllocator();

    if (m_bIsExtBuffers)
    {
        DeallocateExtMVCBuffers();
        DeleteExtBuffers();
    }
#if D3D_SURFACES_SUPPORT
    if (m_pRenderSurface != NULL)
        m_pRenderSurface = NULL;
#endif
    m_mfxSession.Close();
    m_FileWriter.Close();
    if (m_FileReader.get())
        m_FileReader->Close();

    return;
}

mfxStatus CDecodingPipeline::ResetDecoder(sInputParams *pParams)
{
    mfxStatus sts = MFX_ERR_NONE;    

    // close decoder
    sts = m_pmfxDEC->Close();
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // free allocated frames
    DeleteFrames();
    
    // initialize parameters with values from parsed header 
    sts = InitMfxParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // allocate frames prior to decoder initialization (if allocator used as external)
    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // init decoder
    sts = m_pmfxDEC->Init(&m_mfxVideoParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

mfxStatus CDecodingPipeline::RunDecoding()
{   
    mfxSyncPoint        syncp;
    mfxFrameSurface1    *pmfxOutSurface = NULL;
    mfxStatus           sts = MFX_ERR_NONE;
    mfxU16              nIndex = 0; // index of free surface   
    CTimeInterval<>     decodeTimer(m_bIsCompleteFrame);


    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts)          
    {
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            MSDK_SLEEP(1); // just wait and then repeat the same call to DecodeFrameAsync
            if (m_bIsCompleteFrame)
            {
                //in low latency mode device busy lead to increasing of latency, printing is optional since 15 ms s more than threshold
                msdk_printf(MSDK_STRING("Warning : latency increased due to MFX_WRN_DEVICE_BUSY\n"));
            }
        }
        else if (MFX_ERR_MORE_DATA == sts || m_bIsCompleteFrame )
        {
            sts = m_FileReader->ReadNextFrame(&m_mfxBS); // read more data to input bit stream

            if (m_bIsCompleteFrame)
            {
                m_mfxBS.TimeStamp = (mfxU64)(decodeTimer.Commit() * 90000);
            }

            //videowall mode: decoding in a loop
            if (MFX_ERR_MORE_DATA == sts && m_bIsVideoWall)
            {
                m_FileReader->Reset();
                sts = MFX_ERR_NONE;
                continue;
            }
            else
            {
                MSDK_BREAK_ON_ERROR(sts);            
            }
        }
        if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts)
        {
            nIndex = GetFreeSurfaceIndex(m_pmfxSurfaces, m_mfxResponse.NumFrameActual); // find new working surface 
            if (MSDK_INVALID_SURF_IDX == nIndex)
            {
                return MFX_ERR_MEMORY_ALLOC;            
            }
        }        
        
        sts = m_pmfxDEC->DecodeFrameAsync(&m_mfxBS, &(m_pmfxSurfaces[nIndex]), &pmfxOutSurface, &syncp);

        // ignore warnings if output is available, 
        // if no output and no action required just repeat the same call
        if (MFX_ERR_NONE < sts && syncp) 
        {
            sts = MFX_ERR_NONE;
        }

        if (MFX_ERR_NONE == sts)
        {
            sts = m_mfxSession.SyncOperation(syncp, MSDK_DEC_WAIT_INTERVAL);             
        }          
        
        if (MFX_ERR_NONE == sts)
        {                
            decodeTimer.Commit();
            if (m_bExternalAlloc) 
            {
                bool bRenderStatus = m_bIsRender;

                if (m_bOutput)
                {
                    sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
                    MSDK_BREAK_ON_ERROR(sts);

                    sts = m_FileWriter.WriteNextFrame(pmfxOutSurface);
                    MSDK_BREAK_ON_ERROR(sts);

                    sts = m_pMFXAllocator->Unlock(m_pMFXAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
                    MSDK_BREAK_ON_ERROR(sts);
                }

                if (bRenderStatus)
                {
#if D3D_SURFACES_SUPPORT
                    sts = m_d3dRender.RenderFrame(pmfxOutSurface, m_pMFXAllocator);
#endif
                    if (sts == MFX_ERR_NULL_PTR)
                        sts = MFX_ERR_NONE;
                    MSDK_BREAK_ON_ERROR(sts);
                }

            }
            else 
            {
                sts = m_FileWriter.WriteNextFrame(pmfxOutSurface);
                MSDK_BREAK_ON_ERROR(sts);
            }            

            if (!m_bIsVideoWall)
            {
                ++m_nFrameIndex;
                if (m_bIsCompleteFrame)
                {
                    mfxF64 dStartTime = (mfxF64)pmfxOutSurface->Data.TimeStamp / (mfxF64)90000;                    
                    m_vLatency.push_back((decodeTimer.Last() - dStartTime)  * 1000);                    
                }
                else 
                {
                    //decoding progress
                    msdk_printf(MSDK_STRING("Frame number: %d\r"), ++m_nFrameIndex);
                }
            }
        }

    } //while processing    

    //save the main loop exit status (required for the case of ERR_INCOMPATIBLE_PARAMS)
    mfxStatus mainloop_sts = sts; 

    // means that file has ended, need to go to buffering loop
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // incompatible video parameters detected, 
    //need to go to the buffering loop prior to reset procedure 
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM); 
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);          
      
    // loop to retrieve the buffered decoded frames
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_SURFACE == sts)        
    {        
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            MSDK_SLEEP(1);
        }

        mfxU16 nIndex = GetFreeSurfaceIndex(m_pmfxSurfaces, m_mfxResponse.NumFrameActual);

        if (MSDK_INVALID_SURF_IDX == nIndex)
        {
            return MFX_ERR_MEMORY_ALLOC;            
        }

        sts = m_pmfxDEC->DecodeFrameAsync(NULL, &(m_pmfxSurfaces[nIndex]), &pmfxOutSurface, &syncp);

        // ignore warnings if output is available, 
        // if no output and no action required just repeat the same call        
        if (MFX_ERR_NONE < sts && syncp) 
        {
            sts = MFX_ERR_NONE;
        }

        if (MFX_ERR_NONE == sts)
        {
            sts = m_mfxSession.SyncOperation(syncp, MSDK_DEC_WAIT_INTERVAL);
        }

        if (MFX_ERR_NONE == sts)
        {
            if (m_bExternalAlloc) 
            {

                bool bRenderStatus = m_bIsRender;

                if (m_bOutput)
                {
                    sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
                    MSDK_BREAK_ON_ERROR(sts);

                    sts = m_FileWriter.WriteNextFrame(pmfxOutSurface);
                    MSDK_BREAK_ON_ERROR(sts);

                    sts = m_pMFXAllocator->Unlock(m_pMFXAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
                    MSDK_BREAK_ON_ERROR(sts);
                }

                if (bRenderStatus)
                {
#if D3D_SURFACES_SUPPORT
                    sts = m_d3dRender.RenderFrame(pmfxOutSurface, m_pMFXAllocator);
#endif
                    if (sts == MFX_ERR_NULL_PTR)
                        sts = MFX_ERR_NONE;
                    MSDK_BREAK_ON_ERROR(sts);
                }

            }
            else 
            {
                sts = m_FileWriter.WriteNextFrame(pmfxOutSurface);
                MSDK_BREAK_ON_ERROR(sts);
            }            

            //decoding progress
            msdk_printf(MSDK_STRING("Frame number: %d\r"), ++m_nFrameIndex);          
        }
    } 

    // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // if we exited main decoding loop with ERR_INCOMPATIBLE_PARAM we need to send this status to caller
    if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == mainloop_sts) 
    {
        sts = mainloop_sts; 
    }

    if (m_bPrintLatency)
    {
        unsigned int frame_idx = 0;        
        for (std::vector<mfxF64>::iterator it = m_vLatency.begin(); it != m_vLatency.end(); ++it)
        {            
            msdk_printf(MSDK_STRING("Frame %4d, latency=%5.5f ms\n"), ++frame_idx, *it);            
        }
        msdk_printf(MSDK_STRING("\nLatency summary:\n"));
        msdk_printf(MSDK_STRING("\nAVG=%5.5f ms, MAX=%5.5f ms, MIN=%5.5f ms"), 
            std::accumulate(m_vLatency.begin(), m_vLatency.end(), 0.0)/m_vLatency.size(),
            *std::max_element(m_vLatency.begin(), m_vLatency.end()),
            *std::min_element(m_vLatency.begin(), m_vLatency.end()));
    }

    return sts; // ERR_NONE or ERR_INCOMPATIBLE_VIDEO_PARAM
}

void CDecodingPipeline::PrintInfo()
{       
    msdk_printf(MSDK_STRING("Intel(R) Media SDK Decoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);
    msdk_printf(MSDK_STRING("\nInput video\t%s\n"), CodecIdToStr(m_mfxVideoParams.mfx.CodecId).c_str());
    msdk_printf(MSDK_STRING("Output format\t%s\n"), MSDK_STRING("YUV420"));

    mfxFrameInfo Info = m_mfxVideoParams.mfx.FrameInfo;
    msdk_printf(MSDK_STRING("Resolution\t%dx%d\n"), Info.Width, Info.Height);
    msdk_printf(MSDK_STRING("Crop X,Y,W,H\t%d,%d,%d,%d\n"), Info.CropX, Info.CropY, Info.CropW, Info.CropH);

    mfxF64 dFrameRate = CalculateFrameRate(Info.FrameRateExtN, Info.FrameRateExtD);
    msdk_printf(MSDK_STRING("Frame rate\t%.2f\n"), dFrameRate);

    msdk_char* sMemType = m_memType == D3D9_MEMORY  ? MSDK_STRING("d3d") 
                       : (m_memType == D3D11_MEMORY ? MSDK_STRING("d3d11") 
                                                    : MSDK_STRING("system"));
    msdk_printf(MSDK_STRING("Memory type\t\t%s\n"), sMemType);

    mfxIMPL impl;
    m_mfxSession.QueryIMPL(&impl);

    msdk_char* sImpl = (MFX_IMPL_VIA_D3D11 & impl) ? MSDK_STRING("hw_d3d11")
                     : (MFX_IMPL_HARDWARE & impl)  ? MSDK_STRING("hw")
                                                   : MSDK_STRING("sw");
    msdk_printf(MSDK_STRING("MediaSDK impl\t\t%s\n"), sImpl);

    mfxVersion ver;
    m_mfxSession.QueryVersion(&ver);
    msdk_printf(MSDK_STRING("MediaSDK version\t%d.%d\n"), ver.Major, ver.Minor);

    msdk_printf(MSDK_STRING("\n"));

    return;
}



// =========== ffmpeg splitter integration ============

#ifdef DECODE_AUDIO

static FILE*			fAudioOutPCM = NULL;
static AVCodec*			pAudioCodec;
static AVCodecContext*	pCodecContext;

static bool open_audio(AVCodecContext* inCodecContext)
{
	pCodecContext = inCodecContext;

	// Open file for raw audio data output
	fopen_s(&fAudioOutPCM, "audio.dat", "wb");
	if (!fAudioOutPCM) {
		_tcprintf(_T("FFMPEG: Could not open audio output file\n"));
		return false;
	}

	// Find codec
	pAudioCodec = avcodec_find_decoder(pCodecContext->codec_id);
	if (!pAudioCodec) {
		_tcprintf(_T("FFMPEG: Could not find audio decoder\n"));
		return false;
	}

	// Open codec
	if(avcodec_open2(pCodecContext, pAudioCodec, NULL) < 0) {
		_tcprintf(_T("FFMPEG: Could not open audio decoder\n"));
		return false;
	}

	return true;
}

static bool close_audio()
{
	if(fAudioOutPCM)
		fclose(fAudioOutPCM);
	return true;
}

static bool decode_write_audio(AVPacket* pPacket)
{
	AVFrame aFrame;
	int got_frame;

	int len = avcodec_decode_audio4(pCodecContext, &aFrame, &got_frame, pPacket);
	if (len < 0 || !got_frame) {
		_tcprintf(_T("FFMPEG: Could not decode audio\n"));
		return false;
	}

	// Write the decoded signed 16bit integer sample PCM data to file
	fwrite(aFrame.data[0], 1, aFrame.linesize[0], fAudioOutPCM);

	return true;
}

#endif

// ==================================================

FFMPEGReader::FFMPEGReader()
{
    m_bInited		= false;
	m_pFormatCtx	= NULL;
	m_pBsfc			= NULL;
}

FFMPEGReader::~FFMPEGReader()
{
    Close();
}

void FFMPEGReader::Close()
{
	if(m_bInited)
	{
		if(m_pBsfc)
			av_bitstream_filter_close(m_pBsfc);

		av_close_input_file(m_pFormatCtx);

#ifdef DECODE_AUDIO
		close_audio();
#endif
	}

    m_bInited = false;
}

mfxStatus FFMPEGReader::Init(const TCHAR *strFileName, mfxU32 videoType)
{
    MSDK_CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(_tcslen(strFileName), 0, MFX_ERR_NOT_INITIALIZED);

	int res;

    Close();

	m_videoType = videoType;

	// Initialize libavcodec, and register all codecs and formats
	av_register_all();

	// Convert wide to a char filename
    size_t origsize = wcslen(strFileName) + 1;
    size_t convertedChars = 0;
    char nstring[MSDK_MAX_FILENAME_LEN];
    wcstombs_s(&convertedChars, nstring, origsize, strFileName, _TRUNCATE);

	// Open input container
	res = avformat_open_input(&m_pFormatCtx, nstring, NULL, NULL);
	if(res) {
		_tcprintf(_T("FFMPEG: Could not open input container\n"));
		return MFX_ERR_UNKNOWN;
	}

	// Retrieve stream information
	res = avformat_find_stream_info(m_pFormatCtx, NULL);
	if(res < 0) {
		_tcprintf(_T("FFMPEG: Couldn't find stream information\n"));
        return MFX_ERR_UNKNOWN;
	}

	// Dump container info to console
	av_dump_format(m_pFormatCtx, 0, nstring, 0);

    // Find the streams in the container
    m_videoStreamIdx = -1;
    for(unsigned int i=0; i<m_pFormatCtx->nb_streams; i++)
	{
        if(m_pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && m_videoStreamIdx == -1)
        {
            m_videoStreamIdx = i;

			if(videoType == MFX_CODEC_AVC)
			{
				// Retrieve required h264_mp4toannexb filter
				m_pBsfc = av_bitstream_filter_init("h264_mp4toannexb");
				if (!m_pBsfc) {
					_tcprintf(_T("FFMPEG: Could not aquire h264_mp4toannexb filter\n"));
					return MFX_ERR_UNKNOWN;
				}
			}
        }
#ifdef DECODE_AUDIO
		else if(m_pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			m_audioStreamIdx = i;

			open_audio(m_pFormatCtx->streams[i]->codec);
		}
#endif
	}
    if(m_videoStreamIdx == -1)
        return MFX_ERR_UNKNOWN; // Didn't find any video streams in container

    m_bInited = true;
    return MFX_ERR_NONE;
}

mfxStatus FFMPEGReader::ReadNextFrame(mfxBitstream *pBS)
{
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

	AVPacket packet;
	bool videoFrameFound = false;

	// Read until video frame is found or no more video frames in container.
	while(!videoFrameFound)
	{
		if(!av_read_frame(m_pFormatCtx, &packet))
		{
			if(packet.stream_index == m_videoStreamIdx)
			{
				if(m_videoType == MFX_CODEC_AVC)
				{
					//
					// Apply MP4 to H264 Annex B filter on buffer
					//
					uint8_t *pOutBuf;
					int outBufSize;
					int isKeyFrame = packet.flags & AV_PKT_FLAG_KEY;
					av_bitstream_filter_filter(m_pBsfc, m_pFormatCtx->streams[m_videoStreamIdx]->codec, NULL, &pOutBuf, &outBufSize, packet.data, packet.size, isKeyFrame);

					// Current approach leads to a duplicate SPS and PPS....., does not seem to be an issue!

					//
					// Copy filtered buffer to bitstream
					//
					memmove(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
					pBS->DataOffset = 0;
					memcpy(pBS->Data + pBS->DataLength, pOutBuf, outBufSize);
					pBS->DataLength += outBufSize; 

					av_free(pOutBuf);
				}
				else  // MPEG2
				{
					memmove(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
					pBS->DataOffset = 0;
					memcpy(pBS->Data + pBS->DataLength, packet.data, packet.size);
					pBS->DataLength += packet.size; 
				}

				// We are required to tell MSDK that complete frame is in the bitstream!
				pBS->DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;

				videoFrameFound = true;
			}
#ifdef DECODE_AUDIO
			else if(packet.stream_index == m_audioStreamIdx)
			{
				// Decode and write raw audio samples
				decode_write_audio(&packet);
			}
#endif

			// Free the packet that was allocated by av_read_frame
			av_free_packet(&packet);
		}
		else {
			return MFX_ERR_MORE_DATA;  // Indicates that we reached end of container and to stop video decode
		}
	}

    return MFX_ERR_NONE;
}

void FFMPEGReader::Reset()
{
	// Move position to beginning of stream
    int res = av_seek_frame(m_pFormatCtx, m_videoStreamIdx, 0, 0);
	if(res < 0) {
		_tcprintf(_T("FFMPEG: Couldn't seek to beginning of stream!!!\n"));
	}
}

// =========== ffmpeg splitter integration end ============
