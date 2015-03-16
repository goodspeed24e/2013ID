//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2012 Intel Corporation. All Rights Reserved.
//


#include "pipeline_encode.h"
#include "sysmem_allocator.h"

#if D3D_SURFACES_SUPPORT
#include "d3d_allocator.h"
#include "d3d11_allocator.h"

#include "d3d_device.h"
#include "d3d11_device.h"
#endif
CEncTaskPool::CEncTaskPool()
{
    m_pTasks  = NULL;
    m_pmfxSession       = NULL;
    m_nTaskBufferStart  = 0;
    m_nPoolSize         = 0;
}

CEncTaskPool::~CEncTaskPool()
{
    Close();
}

mfxStatus CEncTaskPool::Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize, CSmplBitstreamWriter *pOtherWriter)
{
    MSDK_CHECK_POINTER(pmfxSession, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pWriter, MFX_ERR_NULL_PTR);

    MSDK_CHECK_ERROR(nPoolSize, 0, MFX_ERR_UNDEFINED_BEHAVIOR);
    MSDK_CHECK_ERROR(nBufferSize, 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    // nPoolSize must be even in case of 2 output bitstreams
    if (pOtherWriter && (0 != nPoolSize % 2))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    m_pmfxSession = pmfxSession;
    m_nPoolSize = nPoolSize;

    m_pTasks = new sTask [m_nPoolSize];  
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_MEMORY_ALLOC);

    mfxStatus sts = MFX_ERR_NONE;
    
    if (pOtherWriter) // 2 bitstreams on output
    {
        for (mfxU32 i = 0; i < m_nPoolSize; i+=2)
        {
            sts = m_pTasks[i+0].Init(nBufferSize, pWriter);
            sts = m_pTasks[i+1].Init(nBufferSize, pOtherWriter);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }        
    }
    else
    {
        for (mfxU32 i = 0; i < m_nPoolSize; i++)
        {  
            sts = m_pTasks[i].Init(nBufferSize, pWriter);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);        
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncTaskPool::SynchronizeFirstTask()
{    
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(m_pmfxSession, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts  = MFX_ERR_NONE;

    // non-null sync point indicates that task is in execution
    if (NULL != m_pTasks[m_nTaskBufferStart].EncSyncP)
    { 
        sts = m_pmfxSession->SyncOperation(m_pTasks[m_nTaskBufferStart].EncSyncP, MSDK_WAIT_INTERVAL);        

        if (MFX_ERR_NONE == sts)
        {
            sts = m_pTasks[m_nTaskBufferStart].WriteBitstream();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            sts = m_pTasks[m_nTaskBufferStart].Reset();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // move task buffer start to the next executing task  
            // the first transform frame to the right with non zero sync point
            for (mfxU32 i = 0; i < m_nPoolSize; i++)
            {
                m_nTaskBufferStart = (m_nTaskBufferStart + 1) % m_nPoolSize;
                if (NULL != m_pTasks[m_nTaskBufferStart].EncSyncP)
                {
                    break;
                }
            }             
        } 
        else if (MFX_ERR_ABORTED == sts) 
        {
            while (!m_pTasks[m_nTaskBufferStart].DependentVppTasks.empty())
            {
                // find out if the error occurred in a VPP task to perform recovery procedure if applicable 
                sts = m_pmfxSession->SyncOperation(*m_pTasks[m_nTaskBufferStart].DependentVppTasks.begin(), 0);                

                if (MFX_ERR_NONE == sts)
                {
                    m_pTasks[m_nTaskBufferStart].DependentVppTasks.pop_front();
                    sts = MFX_ERR_ABORTED; // save the status of the encode task
                    continue; // go to next vpp task
                }
                else
                {
                    break;
                }
            }            
        }

        return sts;
    } 
    else
    {
        return MFX_ERR_NOT_FOUND; // no tasks left in task buffer
    }    
}

mfxU32 CEncTaskPool::GetFreeTaskIndex()
{
    mfxU32 off = 0;

    if (m_pTasks)
    {
        for (off = 0; off < m_nPoolSize; off++)
        {            
            if (NULL == m_pTasks[(m_nTaskBufferStart + off) % m_nPoolSize].EncSyncP)
            {                
                break;
            }
        }
    } 

    if (off >= m_nPoolSize)
        return m_nPoolSize;

    return (m_nTaskBufferStart + off) % m_nPoolSize;
}

mfxStatus CEncTaskPool::GetFreeTask(sTask **ppTask)
{   
    MSDK_CHECK_POINTER(ppTask, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_NOT_INITIALIZED);

    mfxU32 index = GetFreeTaskIndex();

    if (index >= m_nPoolSize)
    {
        return MFX_ERR_NOT_FOUND;
    }

    // return the address of the task
    *ppTask = &m_pTasks[index];    

    return MFX_ERR_NONE; 
}

void CEncTaskPool::Close()
{  
    if (m_pTasks)
    {         
        for (mfxU32 i = 0; i < m_nPoolSize; i++)
        {    
            m_pTasks[i].Close();
        }
    }

    MSDK_SAFE_DELETE_ARRAY(m_pTasks);

    m_pmfxSession = NULL;
    m_nTaskBufferStart = 0;
    m_nPoolSize = 0;        
}

sTask::sTask() 
    : EncSyncP(0)
    , pWriter(NULL)
{
    MSDK_ZERO_MEMORY(mfxBS);
}

mfxStatus sTask::Init(mfxU32 nBufferSize, CSmplBitstreamWriter *pwriter)
{
    Close();

    pWriter = pwriter;

    mfxStatus sts = Reset();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = InitMfxBitstream(&mfxBS, nBufferSize);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(&mfxBS));

    return sts;
}

mfxStatus sTask::Close()
{    
    WipeMfxBitstream(&mfxBS);
    EncSyncP = 0;
    DependentVppTasks.clear();

    return MFX_ERR_NONE;
}

mfxStatus sTask::WriteBitstream()
{
    if (!pWriter)
        return MFX_ERR_NOT_INITIALIZED;

    return pWriter->WriteNextFrame(&mfxBS);
}

mfxStatus sTask::Reset()
{
    // mark sync point as free
    EncSyncP = NULL;

    // prepare bit stream
    mfxBS.DataOffset = 0;
    mfxBS.DataLength = 0;

    DependentVppTasks.clear();

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocAndInitMVCSeqDesc()
{
    // a simple example of mfxExtMVCSeqDesc structure filling
    // actually equal to the "Default dependency mode" - when the structure fields are left 0,
    // but we show how to properly allocate and fill the fields

    mfxU32 i;       

    // mfxMVCViewDependency array
    m_MVCSeqDesc.NumView = m_nNumView; 
    m_MVCSeqDesc.NumViewAlloc = m_nNumView;
    m_MVCSeqDesc.View = new mfxMVCViewDependency[m_MVCSeqDesc.NumViewAlloc];
    MSDK_CHECK_POINTER(m_MVCSeqDesc.View, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < m_MVCSeqDesc.NumViewAlloc; ++i)
    {
        MSDK_ZERO_MEMORY(m_MVCSeqDesc.View[i]);
        m_MVCSeqDesc.View[i].ViewId = (mfxU16) i; // set view number as view id
    }        

    // set up dependency for second view
    m_MVCSeqDesc.View[1].NumAnchorRefsL0 = 1;
    m_MVCSeqDesc.View[1].AnchorRefL0[0] = 0;     // ViewId 0 - base view

    m_MVCSeqDesc.View[1].NumNonAnchorRefsL0 = 1;
    m_MVCSeqDesc.View[1].NonAnchorRefL0[0] = 0;  // ViewId 0 - base view

    // viewId array
    m_MVCSeqDesc.NumViewId = m_nNumView;    
    m_MVCSeqDesc.NumViewIdAlloc = m_nNumView;
    m_MVCSeqDesc.ViewId = new mfxU16[m_MVCSeqDesc.NumViewIdAlloc];
    MSDK_CHECK_POINTER(m_MVCSeqDesc.ViewId, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < m_MVCSeqDesc.NumViewIdAlloc; ++i)
    {
        m_MVCSeqDesc.ViewId[i] = (mfxU16) i; 
    }   

    // create a single operation point containing all views
    m_MVCSeqDesc.NumOP = 1;
    m_MVCSeqDesc.NumOPAlloc = 1;  
    m_MVCSeqDesc.OP = new mfxMVCOperationPoint[m_MVCSeqDesc.NumOPAlloc];
    MSDK_CHECK_POINTER(m_MVCSeqDesc.OP, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < m_MVCSeqDesc.NumOPAlloc; ++i)
    {
        MSDK_ZERO_MEMORY(m_MVCSeqDesc.OP[i]);
        m_MVCSeqDesc.OP[i].NumViews = (mfxU16) m_nNumView;
        m_MVCSeqDesc.OP[i].NumTargetViews = (mfxU16) m_nNumView;
        m_MVCSeqDesc.OP[i].TargetViewId = m_MVCSeqDesc.ViewId; // points to mfxExtMVCSeqDesc::ViewId
    } 

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocAndInitVppDoNotUse()
{    
    m_VppDoNotUse.NumAlg = 4;

    m_VppDoNotUse.AlgList = new mfxU32 [m_VppDoNotUse.NumAlg];    
    MSDK_CHECK_POINTER(m_VppDoNotUse.AlgList,  MFX_ERR_MEMORY_ALLOC);

    m_VppDoNotUse.AlgList[0] = MFX_EXTBUFF_VPP_DENOISE; // turn off denoising (on by default)
    m_VppDoNotUse.AlgList[1] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS; // turn off scene analysis (on by default)
    m_VppDoNotUse.AlgList[2] = MFX_EXTBUFF_VPP_DETAIL; // turn off detail enhancement (on by default)
    m_VppDoNotUse.AlgList[3] = MFX_EXTBUFF_VPP_PROCAMP; // turn off processing amplified (on by default)

    return MFX_ERR_NONE;

} // CEncodingPipeline::AllocAndInitVppDoNotUse()

void CEncodingPipeline::FreeMVCSeqDesc()
{

    MSDK_SAFE_DELETE_ARRAY(m_MVCSeqDesc.View);
    MSDK_SAFE_DELETE_ARRAY(m_MVCSeqDesc.ViewId);
    MSDK_SAFE_DELETE_ARRAY(m_MVCSeqDesc.OP);      
}

void CEncodingPipeline::FreeVppDoNotUse()
{
    MSDK_SAFE_DELETE_ARRAY(m_VppDoNotUse.AlgList);
}

mfxStatus CEncodingPipeline::InitMfxEncParams(sInputParams *pInParams)
{
    m_mfxEncParams.mfx.CodecId                 = pInParams->CodecId;
    m_mfxEncParams.mfx.TargetUsage             = pInParams->nTargetUsage; // trade-off between quality and speed
    m_mfxEncParams.mfx.TargetKbps              = pInParams->nBitRate; // in Kbps
    m_mfxEncParams.mfx.RateControlMethod       = MFX_RATECONTROL_CBR;
    ConvertFrameRate(pInParams->dFrameRate, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtN, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtD);
    m_mfxEncParams.mfx.EncodedOrder            = 0; // binary flag, 0 signals encoder to take frames in display order

    // specify memory type
    if (D3D9_MEMORY == pInParams->memType || D3D11_MEMORY == pInParams->memType)
    {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }
    else
    {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }    

    // frame info parameters
    m_mfxEncParams.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
    m_mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_mfxEncParams.mfx.FrameInfo.PicStruct    = pInParams->nPicStruct;

    // set frame size and crops
    // width must be a multiple of 16 
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_mfxEncParams.mfx.FrameInfo.Width  = MSDK_ALIGN16(pInParams->nDstWidth);
    m_mfxEncParams.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxEncParams.mfx.FrameInfo.PicStruct)?
        MSDK_ALIGN16(pInParams->nDstHeight) : MSDK_ALIGN32(pInParams->nDstHeight);
    
    m_mfxEncParams.mfx.FrameInfo.CropX = 0; 
    m_mfxEncParams.mfx.FrameInfo.CropY = 0;
    m_mfxEncParams.mfx.FrameInfo.CropW = pInParams->nDstWidth;
    m_mfxEncParams.mfx.FrameInfo.CropH = pInParams->nDstHeight;
    
    // we don't specify profile and level and let the encoder choose those basing on parameters
    // we must specify profile only for MVC codec
    if (MVC_ENABLED & m_MVCflags)
        m_mfxEncParams.mfx.CodecProfile = MFX_PROFILE_AVC_STEREO_HIGH;

    // configure and attach external parameters 
    
    if (MVC_ENABLED & pInParams->MVC_flags)
        m_EncExtParams.push_back((mfxExtBuffer *)&m_MVCSeqDesc);
        
    if (MVC_VIEWOUTPUT & pInParams->MVC_flags)
    {
        // ViewOuput option requested
        m_CodingOption.ViewOutput = MFX_CODINGOPTION_ON;
        m_EncExtParams.push_back((mfxExtBuffer *)&m_CodingOption);
    }

    if (!m_EncExtParams.empty())
    {
        m_mfxEncParams.ExtParam = &m_EncExtParams[0]; // vector is stored linearly in memory
        m_mfxEncParams.NumExtParam = (mfxU16)m_EncExtParams.size();
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::InitMfxVppParams(sInputParams *pInParams)
{
    MSDK_CHECK_POINTER(pInParams,  MFX_ERR_NULL_PTR);

    // specify memory type
    if (D3D9_MEMORY == pInParams->memType || D3D11_MEMORY == pInParams->memType)
    {
        m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }
    else
    {
        m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }

    // input frame info
    m_mfxVppParams.vpp.In.FourCC    = MFX_FOURCC_NV12;
    m_mfxVppParams.vpp.In.PicStruct = pInParams->nPicStruct;;
    ConvertFrameRate(pInParams->dFrameRate, &m_mfxVppParams.vpp.In.FrameRateExtN, &m_mfxVppParams.vpp.In.FrameRateExtD);

    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_mfxVppParams.vpp.In.Width     = MSDK_ALIGN16(pInParams->nWidth);
    m_mfxVppParams.vpp.In.Height    = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxVppParams.vpp.In.PicStruct)?
        MSDK_ALIGN16(pInParams->nHeight) : MSDK_ALIGN32(pInParams->nHeight);

    // set crops in input mfxFrameInfo for correct work of file reader
    // VPP itself ignores crops at initialization
    m_mfxVppParams.vpp.In.CropW = pInParams->nWidth;
    m_mfxVppParams.vpp.In.CropH = pInParams->nHeight;

    // fill output frame info
    memcpy(&m_mfxVppParams.vpp.Out, &m_mfxVppParams.vpp.In, sizeof(mfxFrameInfo));

    // only resizing is supported
    m_mfxVppParams.vpp.Out.Width = MSDK_ALIGN16(pInParams->nDstWidth);
    m_mfxVppParams.vpp.Out.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxVppParams.vpp.Out.PicStruct)?
        MSDK_ALIGN16(pInParams->nDstHeight) : MSDK_ALIGN32(pInParams->nDstHeight);

    // configure and attach external parameters
    AllocAndInitVppDoNotUse();
    m_VppExtParams.push_back((mfxExtBuffer *)&m_VppDoNotUse);

    if (MVC_ENABLED & pInParams->MVC_flags)
        m_VppExtParams.push_back((mfxExtBuffer *)&m_MVCSeqDesc);

    m_mfxVppParams.ExtParam = &m_VppExtParams[0]; // vector is stored linearly in memory
    m_mfxVppParams.NumExtParam = (mfxU16)m_VppExtParams.size();

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::CreateHWDevice()
{
#if D3D_SURFACES_SUPPORT
    mfxStatus sts = MFX_ERR_NONE;

    POINT point = {0, 0};
    HWND window = WindowFromPoint(point);

#if MFX_D3D11_SUPPORT
    if (D3D11_MEMORY == m_memType)
        m_hwdev = new CD3D11Device();
    else
#endif // #if MFX_D3D11_SUPPORT
        m_hwdev = new CD3D9Device();

    if (NULL == m_hwdev)
        return MFX_ERR_MEMORY_ALLOC;

    sts = m_hwdev->Init(
        window,
        0,
        GetMSDKAdapterNumber(m_mfxSession));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
#endif // #if D3D_SURFACES_SUPPORT
}

mfxStatus CEncodingPipeline::ResetDevice()
{
    if (D3D9_MEMORY == m_memType || D3D11_MEMORY == m_memType)
    {
#if D3D_SURFACES_SUPPORT
        return m_hwdev->Reset();
#endif
    }
    else
    {
        return MFX_ERR_NONE;
    }
}

mfxStatus CEncodingPipeline::AllocFrames()
{
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest EncRequest;
    mfxFrameAllocRequest VppRequest[2];

    mfxU16 nEncSurfNum = 0; // number of surfaces for encoder
    mfxU16 nVppSurfNum = 0; // number of surfaces for vpp

    MSDK_ZERO_MEMORY(EncRequest);
    MSDK_ZERO_MEMORY(VppRequest[0]);
    MSDK_ZERO_MEMORY(VppRequest[1]);

    // Calculate the number of surfaces for components.
    // QueryIOSurf functions tell how many surfaces are required to produce at least 1 output.
    // To achieve better performance we provide extra surfaces.
    // 1 extra surface at input allows to get 1 extra output.

    sts = m_pmfxENC->QueryIOSurf(&m_mfxEncParams, &EncRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (m_pmfxVPP)
    {
        // VppRequest[0] for input frames request, VppRequest[1] for output frames request
        sts = m_pmfxVPP->QueryIOSurf(&m_mfxVppParams, VppRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // The number of surfaces shared by vpp output and encode input.
    // When surfaces are shared 1 surface at first component output contains output frame that goes to next component input
    nEncSurfNum = EncRequest.NumFrameSuggested + MSDK_MAX(VppRequest[1].NumFrameSuggested, 1) - 1 + (m_nAsyncDepth - 1);

    // The number of surfaces for vpp input - so that vpp can work at async depth = m_nAsyncDepth
    nVppSurfNum = VppRequest[0].NumFrameSuggested + (m_nAsyncDepth - 1);

    // prepare allocation requests
    EncRequest.NumFrameMin = nEncSurfNum;
    EncRequest.NumFrameSuggested = nEncSurfNum;
    memcpy(&(EncRequest.Info), &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));    
    if (m_pmfxVPP)
    {
        EncRequest.Type |= MFX_MEMTYPE_FROM_VPPOUT; // surfaces are shared between vpp output and encode input 
    }
    
    // alloc frames for encoder
    sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &EncRequest, &m_EncResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // alloc frames for vpp if vpp is enabled
    if (m_pmfxVPP)
    {
        VppRequest[0].NumFrameMin = nVppSurfNum;
        VppRequest[0].NumFrameSuggested = nVppSurfNum;
        memcpy(&(VppRequest[0].Info), &(m_mfxVppParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
        
        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &(VppRequest[0]), &m_VppResponse);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // prepare mfxFrameSurface1 array for encoder    
    m_pEncSurfaces = new mfxFrameSurface1 [m_EncResponse.NumFrameActual];
    MSDK_CHECK_POINTER(m_pEncSurfaces, MFX_ERR_MEMORY_ALLOC);

    for (int i = 0; i < m_EncResponse.NumFrameActual; i++)
    {
        memset(&(m_pEncSurfaces[i]), 0, sizeof(mfxFrameSurface1));
        memcpy(&(m_pEncSurfaces[i].Info), &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

        if (m_bExternalAlloc)
        {
            m_pEncSurfaces[i].Data.MemId = m_EncResponse.mids[i];
        }
        else
        {
            // get YUV pointers
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_EncResponse.mids[i], &(m_pEncSurfaces[i].Data));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    // prepare mfxFrameSurface1 array for vpp if vpp is enabled
    if (m_pmfxVPP)
    {
        m_pVppSurfaces = new mfxFrameSurface1 [m_VppResponse.NumFrameActual];
        MSDK_CHECK_POINTER(m_pVppSurfaces, MFX_ERR_MEMORY_ALLOC);

        for (int i = 0; i < m_VppResponse.NumFrameActual; i++)
        {
            memset(&(m_pVppSurfaces[i]), 0, sizeof(mfxFrameSurface1));
            memcpy(&(m_pVppSurfaces[i].Info), &(m_mfxVppParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

            if (m_bExternalAlloc)
            {
                m_pVppSurfaces[i].Data.MemId = m_VppResponse.mids[i];
            }
            else
            {
                sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_VppResponse.mids[i], &(m_pVppSurfaces[i].Data));
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
        }        
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::CreateAllocator()
{   
    mfxStatus sts = MFX_ERR_NONE;

    if (D3D9_MEMORY == m_memType || D3D11_MEMORY == m_memType)
    {
#if D3D_SURFACES_SUPPORT
        sts = CreateHWDevice();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

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
        Call SetAllocator to pass allocator to Media SDK */
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
        We don't call SetAllocator, Media SDK uses internal allocator.
        We use system memory allocator simply as a memory manager for application*/
    }

    // initialize memory allocator
    sts = m_pMFXAllocator->Init(m_pmfxAllocatorParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

void CEncodingPipeline::DeleteFrames()
{
    // delete surfaces array
    MSDK_SAFE_DELETE_ARRAY(m_pEncSurfaces);
    MSDK_SAFE_DELETE_ARRAY(m_pVppSurfaces);

    // delete frames
    if (m_pMFXAllocator)
    {        
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_EncResponse);
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_VppResponse);
    }    
}

void CEncodingPipeline::DeleteHWDevice()
{
#if D3D_SURFACES_SUPPORT
    MSDK_SAFE_DELETE(m_hwdev);
#endif
}

void CEncodingPipeline::DeleteAllocator()
{
    // delete allocator
    MSDK_SAFE_DELETE(m_pMFXAllocator);
    MSDK_SAFE_DELETE(m_pmfxAllocatorParams);

    DeleteHWDevice();
}

CEncodingPipeline::CEncodingPipeline()
{
    m_pmfxENC = NULL;
    m_pmfxVPP = NULL;
    m_pMFXAllocator = NULL;
    m_pmfxAllocatorParams = NULL;
    m_memType = SYSTEM_MEMORY;
    m_bExternalAlloc = false;
    m_pEncSurfaces = NULL;
    m_pVppSurfaces = NULL;
    m_nAsyncDepth = 0;

    m_MVCflags = MVC_DISABLED;
    m_nNumView = 0;

    m_FileWriters.first = m_FileWriters.second = NULL;

    MSDK_ZERO_MEMORY(m_MVCSeqDesc);
    m_MVCSeqDesc.Header.BufferId = MFX_EXTBUFF_MVC_SEQ_DESC;
    m_MVCSeqDesc.Header.BufferSz = sizeof(m_MVCSeqDesc);

    MSDK_ZERO_MEMORY(m_VppDoNotUse);
    m_VppDoNotUse.Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
    m_VppDoNotUse.Header.BufferSz = sizeof(m_VppDoNotUse);

    MSDK_ZERO_MEMORY(m_CodingOption);
    m_CodingOption.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    m_CodingOption.Header.BufferSz = sizeof(m_CodingOption);

#if D3D_SURFACES_SUPPORT
    m_hwdev = NULL;
#endif

    MSDK_ZERO_MEMORY(m_mfxEncParams);
    MSDK_ZERO_MEMORY(m_mfxVppParams);

    MSDK_ZERO_MEMORY(m_EncResponse);
    MSDK_ZERO_MEMORY(m_VppResponse);
}

CEncodingPipeline::~CEncodingPipeline()
{
    Close();
}

void CEncodingPipeline::SetMultiView()
{
    m_FileReader.SetMultiView();
}

mfxStatus CEncodingPipeline::InitFileWriter(CSmplBitstreamWriter **ppWriter, const msdk_char *filename)
{
    MSDK_CHECK_ERROR(ppWriter, NULL, MFX_ERR_NULL_PTR);

    MSDK_SAFE_DELETE(*ppWriter);
    *ppWriter = new CSmplBitstreamWriter;
    MSDK_CHECK_POINTER(*ppWriter, MFX_ERR_MEMORY_ALLOC);
    mfxStatus sts = (*ppWriter)->Init(filename);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

mfxStatus CEncodingPipeline::InitFileWriters(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    // prepare output file writers

    // ViewOutput mode: output in single bitstream
    if ( (MVC_VIEWOUTPUT & pParams->MVC_flags) && (pParams->dstFileBuff.size() <= 1))
    {
        sts = InitFileWriter(&m_FileWriters.first, pParams->dstFileBuff[0]);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        m_FileWriters.second = m_FileWriters.first;
    }
    // ViewOutput mode: 2 bitstreams in separate files
    else if ( (MVC_VIEWOUTPUT & pParams->MVC_flags) && (pParams->dstFileBuff.size() <= 2))
    {
        sts = InitFileWriter(&m_FileWriters.first, pParams->dstFileBuff[0]);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        sts = InitFileWriter(&m_FileWriters.second, pParams->dstFileBuff[1]);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    // ViewOutput mode: 3 bitstreams - 2 separate & 1 merged
    else if ( (MVC_VIEWOUTPUT & pParams->MVC_flags) && (pParams->dstFileBuff.size() <= 3))
    {
        std::auto_ptr<CSmplBitstreamDuplicateWriter> first(new CSmplBitstreamDuplicateWriter);

        // init first duplicate writer
        MSDK_CHECK_POINTER(first.get(), MFX_ERR_MEMORY_ALLOC);
        sts = first->Init(pParams->dstFileBuff[0]);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        sts = first->InitDuplicate(pParams->dstFileBuff[2]);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // init second duplicate writer
        std::auto_ptr<CSmplBitstreamDuplicateWriter> second(new CSmplBitstreamDuplicateWriter);
        MSDK_CHECK_POINTER(second.get(), MFX_ERR_MEMORY_ALLOC);
        sts = second->Init(pParams->dstFileBuff[1]);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        sts = second->JoinDuplicate(first.get());
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        m_FileWriters.first = first.release();
        m_FileWriters.second = second.release();
    }
    // not ViewOutput mode
    else
    {
        sts = InitFileWriter(&m_FileWriters.first, pParams->dstFileBuff[0]);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    return sts;
}

mfxStatus CEncodingPipeline::DetermineMinimumRequiredVersion(const sInputParams &pParams, mfxVersion &version)
{
    version.Major = 1;
    version.Minor = 0;

    if (MVC_DISABLED != pParams.MVC_flags)
        version.Minor = 3;
    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::Init(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    // prepare input file reader
    sts = m_FileReader.Init(pParams->strSrcFile,
                            pParams->ColorFormat,
                            pParams->numViews,
                            pParams->srcFileBuff);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	m_bMux = pParams->bMux;

    m_MVCflags = pParams->MVC_flags;

	FFMPEGWriter *ffmpegFW = NULL;
	if(!pParams->bMux)  
	{
		sts = InitFileWriters(pParams);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);     
	}
	else
	{
		// =========== ffmpeg muxer integration ============

		// For FFMPEG muxing defer file writer init until after encoder init to gather complete info.
		ffmpegFW = new FFMPEGWriter;
		m_FileWriters.first = ffmpegFW;
		MSDK_CHECK_POINTER(m_FileWriters.first, MFX_ERR_MEMORY_ALLOC);

		// =========== ffmpeg muxer integration end ============
	}

    mfxVersion version;
    sts = DetermineMinimumRequiredVersion(*pParams,version);
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

    // set memory type
    m_memType = pParams->memType;

    // create and init frame allocator
    sts = CreateAllocator();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = InitMfxEncParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = InitMfxVppParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // MVC specific options
    if (MVC_ENABLED & m_MVCflags)
    {
        sts = AllocAndInitMVCSeqDesc();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // create encoder
    m_pmfxENC = new MFXVideoENCODE(m_mfxSession);
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_MEMORY_ALLOC);

    // create preprocessor if resizing was requested from command line
    // or if different FourCC is set in InitMfxVppParams
    if (pParams->nWidth  != pParams->nDstWidth ||
        pParams->nHeight != pParams->nDstHeight ||
        m_mfxVppParams.vpp.In.FourCC != m_mfxVppParams.vpp.Out.FourCC)
    {
        m_pmfxVPP = new MFXVideoVPP(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxVPP, MFX_ERR_MEMORY_ALLOC);
    }

    m_nAsyncDepth = 4; // this number can be tuned for better performance

    sts = ResetMFXComponents(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// =========== ffmpeg muxer integration ============

	if(pParams->bMux)
	{
		mfxExtCodingOptionSPSPPS m_extSPSPPS;
		MSDK_ZERO_MEMORY(m_extSPSPPS);
		m_extSPSPPS.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
		m_extSPSPPS.Header.BufferSz = sizeof(mfxExtCodingOptionSPSPPS);
		m_extSPSPPS.PPSBuffer		= PPSBuffer;
		m_extSPSPPS.SPSBuffer		= SPSBuffer;
		m_extSPSPPS.PPSBufSize		= MAXSPSPPSBUFFERSIZE;
		m_extSPSPPS.SPSBufSize		= MAXSPSPPSBUFFERSIZE;

		m_EncExtParams.push_back((mfxExtBuffer *)&m_extSPSPPS);
		m_mfxEncParams.ExtParam		= &m_EncExtParams[0];
		m_mfxEncParams.NumExtParam	= (mfxU16)m_EncExtParams.size();

		// Retrieve encoder parameters incl. sequence header 
		sts = m_pmfxENC->GetVideoParam(&m_mfxEncParams);

		// Initalize FFMPEG muxer with configured parameters and retrieved encoding SPS/PPS buffers.
		sts = ffmpegFW->Init(pParams->dstFileBuff[0], pParams, m_extSPSPPS.SPSBuffer, m_extSPSPPS.SPSBufSize, m_extSPSPPS.PPSBuffer, m_extSPSPPS.PPSBufSize);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	}

	// =========== ffmpeg muxer integration end ============

    return MFX_ERR_NONE;
}

void CEncodingPipeline::Close()
{
    if (m_FileWriters.first)
        msdk_printf(MSDK_STRING("Frame number: %u\r"), m_FileWriters.first->m_nProcessedFramesNum);

    MSDK_SAFE_DELETE(m_pmfxENC);
    MSDK_SAFE_DELETE(m_pmfxVPP);

    FreeMVCSeqDesc();
    FreeVppDoNotUse();

    DeleteFrames();
    // allocator if used as external for MediaSDK must be deleted after SDK components
    DeleteAllocator();

    m_TaskPool.Close();
    m_mfxSession.Close();

    m_FileReader.Close();

	// =========== ffmpeg muxer integration ============
	// "FreeFileWriters()" added. Bug in original sample code! Fix required to ensure proper finalization of file/containers
	FreeFileWriters();
	// =========== ffmpeg muxer integration end ============
}

void CEncodingPipeline::FreeFileWriters()
{
    if (m_FileWriters.second == m_FileWriters.first)
    {
        m_FileWriters.second = NULL; // second do not own the writer - just forget pointer
    }

    if (m_FileWriters.first)
        m_FileWriters.first->Close();
    MSDK_SAFE_DELETE(m_FileWriters.first);

    if (m_FileWriters.second)
        m_FileWriters.second->Close();
    MSDK_SAFE_DELETE(m_FileWriters.second);
}

mfxStatus CEncodingPipeline::ResetMFXComponents(sInputParams* pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    sts = m_pmfxENC->Close();
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (m_pmfxVPP)
    {
        sts = m_pmfxVPP->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // free allocated frames
    DeleteFrames();

    m_TaskPool.Close();

    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_pmfxENC->Init(&m_mfxEncParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (m_pmfxVPP)
    {
        sts = m_pmfxVPP->Init(&m_mfxVppParams);
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    mfxU32 nEncodedDataBufferSize = m_mfxEncParams.mfx.FrameInfo.Width * m_mfxEncParams.mfx.FrameInfo.Height * 4;
    sts = m_TaskPool.Init(&m_mfxSession, m_FileWriters.first, m_nAsyncDepth * 2, nEncodedDataBufferSize, m_FileWriters.second);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocateSufficientBuffer(mfxBitstream* pBS)
{
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxVideoParam par;
    MSDK_ZERO_MEMORY(par);

    // find out the required buffer size
    mfxStatus sts = m_pmfxENC->GetVideoParam(&par);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // reallocate bigger buffer for output
    sts = ExtendMfxBitstream(pBS, par.mfx.BufferSizeInKB * 1000);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(pBS));

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::GetFreeTask(sTask **ppTask)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_TaskPool.GetFreeTask(ppTask);
    if (MFX_ERR_NOT_FOUND == sts)
    {
        sts = SynchronizeFirstTask();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // try again
        sts = m_TaskPool.GetFreeTask(ppTask);
    }

    return sts;
}

mfxStatus CEncodingPipeline::SynchronizeFirstTask()
{
    mfxStatus sts = m_TaskPool.SynchronizeFirstTask();

    return sts;
}

mfxStatus CEncodingPipeline::Run()
{
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;    
    
    mfxFrameSurface1* pSurf = NULL; // dispatching pointer    

    sTask *pCurrentTask = NULL; // a pointer to the current task
    mfxU16 nEncSurfIdx = 0; // index of free surface for encoder input (vpp output)
    mfxU16 nVppSurfIdx = 0; // index of free surface for vpp input
    
    mfxSyncPoint VppSyncPoint = NULL; // a sync point associated with an asynchronous vpp call
    bool bVppMultipleOutput = false; // this flag is true if VPP produces more frames at output 
                                     // than consumes at input. E.g. framerate conversion 30 fps -> 60 fps

    
    // Since in sample we support just 2 views 
    // we will change this value between 0 and 1 in case of MVC
    mfxU16 currViewNum = 0;     
    
    PrintInfo();
    sts = MFX_ERR_NONE;       

    // main loop, preprocessing and encoding
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)        
    {        
        // get a pointer to a free task (bit stream and sync point for encoder)
        sts = GetFreeTask(&pCurrentTask);
        MSDK_BREAK_ON_ERROR(sts);

        // find free surface for encoder input
        nEncSurfIdx = GetFreeSurface(m_pEncSurfaces, m_EncResponse.NumFrameActual);
        MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);

        // point pSurf to encoder surface
        pSurf = &m_pEncSurfaces[nEncSurfIdx];        

        if (!bVppMultipleOutput)
        {
            // if vpp is enabled find free surface for vpp input and point pSurf to vpp surface
            if (m_pmfxVPP)
            {          
                nVppSurfIdx = GetFreeSurface(m_pVppSurfaces, m_VppResponse.NumFrameActual);
                MSDK_CHECK_ERROR(nVppSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);

                pSurf = &m_pVppSurfaces[nVppSurfIdx];
            }        

            // load frame from file to surface data
            // if we share allocator with Media SDK we need to call Lock to access surface data and...
            if (m_bExternalAlloc) 
            {
                // get YUV pointers
                sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
                MSDK_BREAK_ON_ERROR(sts);
            }

            pSurf->Info.FrameId.ViewId = currViewNum;
            sts = m_FileReader.LoadNextFrame(pSurf);
            MSDK_BREAK_ON_ERROR(sts);
            if (MVC_ENABLED & m_MVCflags) currViewNum ^= 1; // Flip between 0 and 1 for ViewId

            // ... after we're done call Unlock
            if (m_bExternalAlloc)
            {
                sts = m_pMFXAllocator->Unlock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
                MSDK_BREAK_ON_ERROR(sts);
            }
        }        

        // perform preprocessing if required
        if (m_pmfxVPP)
        {            
            bVppMultipleOutput = false; // reset the flag before a call to VPP
            for (;;)
            {
                sts = m_pmfxVPP->RunFrameVPPAsync(&m_pVppSurfaces[nVppSurfIdx], &m_pEncSurfaces[nEncSurfIdx], 
                    NULL, &VppSyncPoint); 

                if (MFX_ERR_NONE < sts && !VppSyncPoint) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                }
                else if (MFX_ERR_NONE < sts && VppSyncPoint)                 
                {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available                                    
                    break;
                }
                else 
                    break; // not a warning               
            } 
            
            // process errors            
            if (MFX_ERR_MORE_DATA == sts)
            {
                continue;
            }
            else if (MFX_ERR_MORE_SURFACE == sts)
            {
                bVppMultipleOutput = true;
            }
            else
            {
                MSDK_BREAK_ON_ERROR(sts); 
            }
        }                       
        
        // save the id of preceding vpp task which will produce input data for the encode task
        if (VppSyncPoint)
        {
            pCurrentTask->DependentVppTasks.push_back(VppSyncPoint);
            VppSyncPoint = NULL;
        }

        for (;;)
        {
            // at this point surface for encoder contains either a frame from file or a frame processed by vpp        
            sts = m_pmfxENC->EncodeFrameAsync(NULL, &m_pEncSurfaces[nEncSurfIdx], &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);
            
            if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                    MSDK_SLEEP(1); // wait if device is busy
            }
            else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP)                 
            {
                sts = MFX_ERR_NONE; // ignore warnings if output is available                                    
                break;
            }
            else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
            {
                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);                
            }
            else
            {
                // get next surface and new task for 2nd bitstream in ViewOutput mode
                MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                break;
            }
        }            
    }

    // means that the input file has ended, need to go to buffering loops
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (m_pmfxVPP)
    {
        // loop to get buffered frames from vpp
        while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts)
            // MFX_ERR_MORE_SURFACE can be returned only by RunFrameVPPAsync
            // MFX_ERR_MORE_DATA is accepted only from EncodeFrameAsync
        {      
            // find free surface for encoder input (vpp output)
            nEncSurfIdx = GetFreeSurface(m_pEncSurfaces, m_EncResponse.NumFrameActual);
            MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
                      
            for (;;)
            {
                sts = m_pmfxVPP->RunFrameVPPAsync(NULL, &m_pEncSurfaces[nEncSurfIdx], NULL, &VppSyncPoint); 

                if (MFX_ERR_NONE < sts && !VppSyncPoint) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                }
                else if (MFX_ERR_NONE < sts && VppSyncPoint)                 
                {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available                                    
                    break;
                }
                else 
                    break; // not a warning               
            }    

            if (MFX_ERR_MORE_SURFACE == sts)
            {
                continue;
            }
            else
            {
                MSDK_BREAK_ON_ERROR(sts); 
            }                        

            // get a free task (bit stream and sync point for encoder)
            sts = GetFreeTask(&pCurrentTask);
            MSDK_BREAK_ON_ERROR(sts);

            // save the id of preceding vpp task which will produce input data for the encode task
            if (VppSyncPoint)
            {
                pCurrentTask->DependentVppTasks.push_back(VppSyncPoint);
                VppSyncPoint = NULL;
            }

            for (;;)
            {                
                sts = m_pmfxENC->EncodeFrameAsync(NULL, &m_pEncSurfaces[nEncSurfIdx], &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);                

                if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                }
                else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP)                 
                {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available                                    
                    break;
                }
                else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
                {
                    sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                }
                else
                {
                    // get next surface and new task for 2nd bitstream in ViewOutput mode
                    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                    break;
                }
            }            
        }

        // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
        // indicates that there are no more buffered frames
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
        // exit in case of other errors
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);  
    }    

    // loop to get buffered frames from encoder
    while (MFX_ERR_NONE <= sts)
    {       
        // get a free task (bit stream and sync point for encoder)
        sts = GetFreeTask(&pCurrentTask);
        MSDK_BREAK_ON_ERROR(sts);
        
        for (;;)
        {                
            sts = m_pmfxENC->EncodeFrameAsync(NULL, NULL, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);            

            if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                    MSDK_SLEEP(1); // wait if device is busy
            }
            else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP)                 
            {
                sts = MFX_ERR_NONE; // ignore warnings if output is available                                    
                break;
            }
            else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
            {
                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            else
            {
                // get new task for 2nd bitstream in ViewOutput mode
                MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                break;
            }
        }            
        MSDK_BREAK_ON_ERROR(sts); 
    }    

    // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
    // indicates that there are no more buffered frames
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // synchronize all tasks that are left in task pool
    while (MFX_ERR_NONE == sts)
    {
        sts = m_TaskPool.SynchronizeFirstTask();
    }  

    // MFX_ERR_NOT_FOUND is the correct status to exit the loop with
    // EncodeFrameAsync and SyncOperation don't return this status
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_FOUND); 
    // report any errors that occurred in asynchronous part
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);     

    return sts; 
}

void CEncodingPipeline::PrintInfo()
{
    msdk_printf(MSDK_STRING("Intel(R) Media SDK Encoding Sample Version %s\n"), MSDK_SAMPLE_VERSION);
    msdk_printf(MSDK_STRING("\nInput file format\t%s\n"), ColorFormatToStr(m_FileReader.m_ColorFormat));
    msdk_printf(MSDK_STRING("Output video\t\t%s\n"), CodecIdToStr(m_mfxEncParams.mfx.CodecId).c_str());

    mfxFrameInfo SrcPicInfo = m_mfxVppParams.vpp.In;
    mfxFrameInfo DstPicInfo = m_mfxEncParams.mfx.FrameInfo;

    msdk_printf(MSDK_STRING("Source picture:\n"));
    msdk_printf(MSDK_STRING("\tResolution\t%dx%d\n"), SrcPicInfo.Width, SrcPicInfo.Height);
    msdk_printf(MSDK_STRING("\tCrop X,Y,W,H\t%d,%d,%d,%d\n"), SrcPicInfo.CropX, SrcPicInfo.CropY, SrcPicInfo.CropW, SrcPicInfo.CropH);

    msdk_printf(MSDK_STRING("Destination picture:\n"));
    msdk_printf(MSDK_STRING("\tResolution\t%dx%d\n"), DstPicInfo.Width, DstPicInfo.Height);
    msdk_printf(MSDK_STRING("\tCrop X,Y,W,H\t%d,%d,%d,%d\n"), DstPicInfo.CropX, DstPicInfo.CropY, DstPicInfo.CropW, DstPicInfo.CropH);

    msdk_printf(MSDK_STRING("Frame rate\t%.2f\n"), DstPicInfo.FrameRateExtN * 1.0 / DstPicInfo.FrameRateExtD);
    msdk_printf(MSDK_STRING("Bit rate(Kbps)\t%d\n"), m_mfxEncParams.mfx.TargetKbps);
    msdk_printf(MSDK_STRING("Target usage\t%s\n"), TargetUsageToStr(m_mfxEncParams.mfx.TargetUsage));

    msdk_char* sMemType = m_memType == D3D9_MEMORY  ? MSDK_STRING("d3d")
                       : (m_memType == D3D11_MEMORY ? MSDK_STRING("d3d11")
                                                    : MSDK_STRING("system"));
    msdk_printf(MSDK_STRING("Memory type\t%s\n"), sMemType);

    mfxIMPL impl;
    m_mfxSession.QueryIMPL(&impl);

    msdk_char* sImpl = (MFX_IMPL_VIA_D3D11 & impl) ? MSDK_STRING("hw_d3d11")
                     : (MFX_IMPL_HARDWARE & impl)  ? MSDK_STRING("hw")
                                                   : MSDK_STRING("sw");
    msdk_printf(MSDK_STRING("Media SDK impl\t\t%s\n"), sImpl);

    mfxVersion ver;
    m_mfxSession.QueryVersion(&ver);
    msdk_printf(MSDK_STRING("Media SDK version\t%d.%d\n"), ver.Major, ver.Minor);

    msdk_printf(MSDK_STRING("\n"));
}


// =========== ffmpeg muxer integration ============

#ifdef ENCODE_AUDIO

static uint8_t*		pAudioOutbuf;
static int			audioOutbufSize;
static int16_t*		pSamples;
static AVStream*	pAudioStream	= NULL;
static FILE*		fAudioInPCM		= NULL;
static int64_t		currentPts		= 0; 

static bool add_audio_stream(AVFormatContext *oc, enum CodecID codec_id)
{
	// Create new audio stream for the container
	pAudioStream = avformat_new_stream(oc, NULL);
	if (!pAudioStream) {
		_tcprintf(_T("FFMPEG: Could not alloc audio stream\n"));
		return false;
	}
 
	AVCodecContext *c;
	c = pAudioStream->codec;

	c->codec_id		= codec_id;
	c->codec_type	= AVMEDIA_TYPE_AUDIO;
	c->sample_rate	= 44100;				// sample rate = 44100 Hz
	c->channels		= 2;					// 2 channels
 	c->bit_rate		= 64000;				// Desired encoding bit rate

	if(c->codec_id == CODEC_ID_VORBIS) {
		c->sample_fmt	= AV_SAMPLE_FMT_FLT; // OGG assumes samples are of type float..... input raw PCM data should be 32 bit float
	}
	else {
		c->sample_fmt	= AV_SAMPLE_FMT_S16; // This assumes the input raw PCM data samples are of type 16 bit signed integer
	}

	// Some formats want stream headers to be separate
	if(oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
 
	return true;
}

static bool open_audio()
{
    AVCodecContext *c;
    AVCodec *codec;
 
    c = pAudioStream->codec;
 
	// Assumes raw PCM file data : 44100 Hz, 2 channels, 16bit/sample
	// or float 32 bit/sample in case using mkv container which defaults to ogg vorbis encoder
	fopen_s(&fAudioInPCM, "audio.dat", "rb");
	if (!fAudioInPCM) {
		_tcprintf(_T("FFMPEG: Could not open audio input file\n"));
		return false;
	}

    // find the audio encoder
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec) {
		_tcprintf(_T("FFMPEG: audio codec not found\n"));
        return false;
    }
 
    // open audio encoder
	if(avcodec_open2(c, codec, NULL) < 0) {
		_tcprintf(_T("FFMPEG: could not open audio encoder\n"));
        return false;
    }

    audioOutbufSize = 10000;		// Arbitrary(!), but should be large enough....
    pAudioOutbuf	= (uint8_t *)av_malloc(audioOutbufSize);

	if(c->codec_id == CODEC_ID_VORBIS)
		pSamples	= (int16_t *)av_malloc(c->frame_size * c->channels * 4);
	else
		pSamples	= (int16_t *)av_malloc(c->frame_size * c->channels * 2);

	if(!pAudioOutbuf || !pSamples) {
		_tcprintf(_T("FFMPEG: could not allocate required buffers\n"));
        return false;
	}

	currentPts = c->frame_size;

	return true;
}

static bool encode_audio_frame(AVPacket *pkt, bool flush)
{
	AVCodecContext *c;
	int res, got_packet;
 
    c = pAudioStream->codec;

	AVFrame aFrame;
	aFrame.data[0]		= (uint8_t *)pSamples;
	aFrame.pts			= AV_NOPTS_VALUE;
	aFrame.nb_samples	= c->frame_size;

	pkt->data			= pAudioOutbuf;  
	pkt->size			= audioOutbufSize;

	// Should be able to call with no pre-allocated buffer, but using FFmpeg in this way does not work for some reason?
	if(flush)
		res = avcodec_encode_audio2(c, pkt, NULL, &got_packet);
	else
		res = avcodec_encode_audio2(c, pkt, &aFrame, &got_packet);
	if (res != 0) {
		_tcprintf(_T("FFMPEG: Error encoding audio\n"));
		return false;
	}

	pkt->flags			|= AV_PKT_FLAG_KEY;
	pkt->stream_index	= pAudioStream->index;
	pkt->pts			= av_rescale_q(currentPts, c->time_base, pAudioStream->time_base);

	currentPts += c->frame_size;

	return true;
}

static bool write_audio_frame(AVFormatContext *oc, float videoFramePts)
{
    AVCodecContext *c;
    c = pAudioStream->codec;

	float realAudioPts = (float)currentPts * c->time_base.num / c->time_base.den;

	while(realAudioPts < videoFramePts)
	{
		AVPacket pkt;
		av_init_packet(&pkt);

		size_t chunksize;
		if(c->codec_id == CODEC_ID_VORBIS)
			chunksize = c->frame_size * c->channels * 4;
		else
			chunksize = c->frame_size * c->channels * 2;
		size_t readBytes = fread(pSamples, 1, chunksize, fAudioInPCM);
		if(readBytes != chunksize) {
			_tcprintf(_T("FFMPEG: Reached end of audio PCM file\n"));
			// Need to free packet here...
			return false;
		}
	
		if(!encode_audio_frame(&pkt, false))
			return false;
 
		// Write the compressed frame
		int res = av_interleaved_write_frame(oc, &pkt);
		if (res != 0) {
			_tcprintf(_T("FFMPEG: Error while writing audio frame\n"));
			return false;
		}

		realAudioPts += (float)c->frame_size * c->time_base.num / c->time_base.den;
	}

	return true;
}

static bool flush_audio(AVFormatContext *oc)
{
	AVPacket pkt;
	av_init_packet(&pkt);
	
	do {
		if(!encode_audio_frame(&pkt, true))
			return false;

		if(pkt.size > 0) {
			// Write the compressed frame
			int res = av_interleaved_write_frame(oc, &pkt);
			if (res != 0) {
				_tcprintf(_T("FFMPEG: Error while writing audio frame (flush)\n"));
				return false;
			}
		}
	} while(pkt.size > 0);

	return true;
}

static bool close_audio()
{
	avcodec_close(pAudioStream->codec);
	av_free(pSamples);
	av_free(pAudioOutbuf);

	if(fAudioInPCM)
		fclose(fAudioInPCM);

	return true;
}

#endif


// ===================================================

FFMPEGWriter::FFMPEGWriter()
{
    m_bInited				= false;
    m_nProcessedFramesNum	= 0;

	m_pFmt					= NULL;
	m_pFormatCtx			= NULL;
	m_pVideoStream			= NULL;
	m_pExtDataBuffer		= NULL;
}

FFMPEGWriter::~FFMPEGWriter()
{
	Close();
}

mfxStatus FFMPEGWriter::Init(const TCHAR *strFileName, sInputParams* pParams, mfxU8* SPSbuf, int SPSbufsize, mfxU8* PPSbuf, int PPSbufsize)
{
	MSDK_CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(_tcslen(strFileName), 0, MFX_ERR_NOT_INITIALIZED);

    Close();

	// Initialize libavcodec, and register all codecs and formats
	av_register_all();

	if(pParams->bMuxMkv)
	{
		// Defaults to H.264 video and Vorbis audio
		m_pFmt = av_guess_format(NULL, "dummy.mkv", NULL);
		
		if(pParams->CodecId == MFX_CODEC_MPEG2) {
			// Override codec to MPEG2
			m_pFmt->video_codec = CODEC_ID_MPEG2VIDEO;
		}
	}
	else if(pParams->CodecId == MFX_CODEC_AVC)
	{
		// Defaults to H.264 video and AAC audio
		m_pFmt = av_guess_format("mp4", NULL, NULL);
	}
	else if(pParams->CodecId == MFX_CODEC_MPEG2)
	{
		m_pFmt = av_guess_format("mpeg", NULL, NULL);
		m_pFmt->video_codec = CODEC_ID_MPEG2VIDEO;
		// Defaults to MPEG-PS
	}
	else
		return MFX_ERR_UNSUPPORTED;

	if (!m_pFmt) {
		_tcprintf(_T("FFMPEG: Could not find suitable output format\n"));
		return MFX_ERR_UNSUPPORTED;
	}

	// Allocate the output media context
	m_pFormatCtx = avformat_alloc_context();
	if (!m_pFormatCtx) {
		_tcprintf(_T("FFMPEG: avformat_alloc_context error\n"));
		return MFX_ERR_UNSUPPORTED;
	}

	m_pFormatCtx->oformat = m_pFmt;

	// Convert wide to a char filename
    size_t origsize = wcslen(strFileName) + 1;
    size_t convertedChars = 0;
    char nstring[MSDK_MAX_FILENAME_LEN];
    wcstombs_s(&convertedChars, nstring, origsize, strFileName, _TRUNCATE);
	sprintf_s(m_pFormatCtx->filename, "%s", nstring);
	
	if (m_pFmt->video_codec == CODEC_ID_NONE) 
		return MFX_ERR_UNSUPPORTED;

	m_pVideoStream = avformat_new_stream(m_pFormatCtx, NULL);
	if (!m_pVideoStream) {
		_tcprintf(_T("FFMPEG: Could not alloc video stream\n"));
		return MFX_ERR_UNKNOWN;
	}

	AVCodecContext *c = m_pVideoStream->codec;
	c->codec_id		= m_pFmt->video_codec;
	c->codec_type	= AVMEDIA_TYPE_VIDEO;
	c->bit_rate		= pParams->nBitRate*1000;
	c->width		= pParams->nDstWidth; 
	c->height		= pParams->nDstHeight;

	// time base: this is the fundamental unit of time (in seconds) in terms of which frame timestamps are represented. for fixed-fps content,
	//            timebase should be 1/framerate and timestamp increments should be identically 1.
	mfxU32 rateD, rateN;
	ConvertFrameRate(pParams->dFrameRate, &rateD, &rateN); 
	c->time_base.den = rateD;
	c->time_base.num = rateN;

	// Some formats want stream headers to be separate
	if(m_pFormatCtx->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	
#ifdef ENCODE_AUDIO
	if (m_pFmt->audio_codec != CODEC_ID_NONE) {
		if(!add_audio_stream(m_pFormatCtx, m_pFmt->audio_codec))
			return MFX_ERR_UNKNOWN;

		if(!open_audio())
			return MFX_ERR_UNKNOWN;
	}
#endif

	// Open the output container file
	if (avio_open(&m_pFormatCtx->pb, m_pFormatCtx->filename, AVIO_FLAG_WRITE) < 0)
	{
		_tcprintf(_T("FFMPEG: Could not open '%s'\n"), m_pFormatCtx->filename);
		return MFX_ERR_UNKNOWN;
	}
 

	m_pExtDataBuffer = (mfxU8*)av_malloc(SPSbufsize + PPSbufsize);
	if(!m_pExtDataBuffer) {
		_tcprintf(_T("FFMPEG: could not allocate required buffer\n"));
        return MFX_ERR_UNKNOWN;
	}

	memcpy(m_pExtDataBuffer, SPSbuf, SPSbufsize);
	memcpy(m_pExtDataBuffer + SPSbufsize, PPSbuf, PPSbufsize);

	// Codec "extradata" conveys the H.264 stream SPS and PPS info (MPEG2: sequence header is housed in SPS buffer, PPS buffer is empty)
	c->extradata		= m_pExtDataBuffer;
	c->extradata_size	= SPSbufsize + PPSbufsize;


	// Write container header
	if(avformat_write_header(m_pFormatCtx, NULL)) {
		_tcprintf(_T("FFMPEG: avformat_write_header error!\n"));
		return MFX_ERR_UNKNOWN;
	}

    m_bInited = true;

    return MFX_ERR_NONE;
}


mfxStatus FFMPEGWriter::WriteNextFrame(mfxBitstream *pMfxBitstream, bool isPrint)
{
	MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(pMfxBitstream, MFX_ERR_NULL_PTR);

	// Note for H.264 :
	//    At the moment the SPS/PPS will be written to container again for the first frame here
	//    To eliminate this we would have to search to first slice (or right after PPS)

	++m_nProcessedFramesNum;

	AVPacket pkt;
	av_init_packet(&pkt);

	AVCodecContext *c = m_pVideoStream->codec;

	m_pVideoStream->pts.val = m_nProcessedFramesNum;

	pkt.pts = av_rescale_q(m_pVideoStream->pts.val, c->time_base, m_pVideoStream->time_base);
	//pkt.dts				= AV_NOPTS_VALUE;
	pkt.stream_index	= m_pVideoStream->index;
	pkt.data			= pMfxBitstream->Data;
	pkt.size			= pMfxBitstream->DataLength;

	if(pMfxBitstream->FrameType == (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR))
		pkt.flags |= AV_PKT_FLAG_KEY;

	// Write the compressed frame in the media file
	if (av_interleaved_write_frame(m_pFormatCtx, &pkt)) {
		_tcprintf(_T("FFMPEG: Error while writing video frame\n"));
		return MFX_ERR_UNKNOWN;
	}

#ifdef ENCODE_AUDIO
	// Note that video + audio muxing timestamp handling in this sample is very rudimentary
	//  - Audio stream length is adjusted to same as video steram length 
	float realVideoPts = (float)pkt.pts * m_pVideoStream->time_base.num / m_pVideoStream->time_base.den;
	write_audio_frame(m_pFormatCtx, realVideoPts);
#endif

    // Print encoding progress to console every certain number of frames (not to affect performance too much)
    if (isPrint && (1 == m_nProcessedFramesNum  || (0 == (m_nProcessedFramesNum % 100))))
        _tcprintf(_T("Frame number: %hd\r"), m_nProcessedFramesNum);  

    return MFX_ERR_NONE;
}

void FFMPEGWriter::Close()
{
	if(m_bInited)
	{
#ifdef ENCODE_AUDIO
		flush_audio(m_pFormatCtx);
#endif

		// Write the trailer, if any. 
		//   The trailer must be written before you close the CodecContexts open when you wrote the
		//   header; otherwise write_trailer may try to use memory that was freed on av_codec_close()
		av_write_trailer(m_pFormatCtx);

#ifdef ENCODE_AUDIO
		close_audio();
#endif

		// Free the streams
		for(unsigned int i = 0; i < m_pFormatCtx->nb_streams; i++) {
			av_freep(&m_pFormatCtx->streams[i]->codec);
			av_freep(&m_pFormatCtx->streams[i]);
		}

		avio_close(m_pFormatCtx->pb);

		av_free(m_pFormatCtx);

		if(m_pExtDataBuffer)
			av_free(m_pExtDataBuffer);
	}

    m_bInited = false;
    m_nProcessedFramesNum = 0;
}


// =========== ffmpeg muxer integration end ============
