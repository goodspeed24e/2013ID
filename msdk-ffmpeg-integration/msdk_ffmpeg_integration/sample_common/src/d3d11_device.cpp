/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 - 2012 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "d3d11_device.h"

#if MFX_D3D11_SUPPORT

#include "sample_defs.h"

CD3D11Device::CD3D11Device()
{
}

CD3D11Device::~CD3D11Device()
{
}

mfxStatus CD3D11Device::FillSCD(mfxHDL hWindow, DXGI_SWAP_CHAIN_DESC& scd)
{
    scd.Windowed = TRUE;
    scd.OutputWindow = (HWND)hWindow;
    scd.SampleDesc.Count = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 1;

    return MFX_ERR_NONE;
}

mfxStatus CD3D11Device::Init(
    mfxHDL hWindow,
    mfxU16 /*nViews*/,
    mfxU32 /*nAdapterNum*/)
{
    mfxStatus sts = MFX_ERR_NONE;
    HRESULT hres = S_OK;

    static D3D_FEATURE_LEVEL FeatureLevels[] = { 
        D3D_FEATURE_LEVEL_11_1, 
        D3D_FEATURE_LEVEL_11_0, 
        D3D_FEATURE_LEVEL_10_1, 
        D3D_FEATURE_LEVEL_10_0 
    };
    D3D_FEATURE_LEVEL pFeatureLevelsOut;

    hres =  D3D11CreateDevice(NULL,    // provide real adapter
                            D3D_DRIVER_TYPE_HARDWARE,
                            NULL,
                            0,
                            FeatureLevels,
                            MSDK_ARRAY_LEN(FeatureLevels),
                            D3D11_SDK_VERSION,
                            &m_pD3D11Device,
                            &pFeatureLevelsOut,
                            &m_pD3D11Ctx);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    m_pDXGIDev = m_pD3D11Device;
    m_pDX11VideoDevice = m_pD3D11Device;
    m_pVideoContext = m_pD3D11Ctx;
    
    MSDK_CHECK_POINTER(m_pDXGIDev.p, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pDX11VideoDevice.p, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pVideoContext.p, MFX_ERR_NULL_PTR);

    // turn on multithreading for the Context 
    CComQIPtr<ID3D10Multithread> p_mt(m_pVideoContext);

    if (p_mt)
        p_mt->SetMultithreadProtected(true);
    else
        return MFX_ERR_DEVICE_FAILED;

    IDXGIAdapter *pTempAdapter;
    m_pDXGIDev->GetAdapter( &pTempAdapter );
    m_pAdapter = pTempAdapter;
    m_pAdapter->GetParent( __uuidof( IDXGIFactory1 ), (LPVOID*) &m_pDXGIFactory ) ;
    MSDK_SAFE_RELEASE ( pTempAdapter );

    MSDK_CHECK_POINTER (m_pDXGIFactory.p, MFX_ERR_NULL_PTR);
    hres = m_pDXGIFactory->MakeWindowAssociation( (HWND)hWindow, 0 );
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    DXGI_SWAP_CHAIN_DESC scd;
    ZeroMemory(&scd, sizeof(scd));
    sts = FillSCD(hWindow, scd);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    hres = m_pDXGIFactory->CreateSwapChain(m_pD3D11Device, &scd, &m_pSwapChain);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    return sts;
}

mfxStatus CD3D11Device::CreateVideoProcessor(mfxFrameSurface1 * pSrf)
{
    HRESULT hres = S_OK;

    if (m_VideoProcessorEnum.p || NULL == pSrf)
        return MFX_ERR_NONE;

    //create video processor
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
    MSDK_ZERO_MEMORY( ContentDesc );

    ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    ContentDesc.InputFrameRate.Numerator = 30000;
    ContentDesc.InputFrameRate.Denominator = 1000;
    ContentDesc.InputWidth  = pSrf->Info.CropW;
    ContentDesc.InputHeight = pSrf->Info.CropH;
    ContentDesc.OutputWidth = pSrf->Info.CropW;
    ContentDesc.OutputHeight = pSrf->Info.CropH;
    ContentDesc.OutputFrameRate.Numerator = 30000;
    ContentDesc.OutputFrameRate.Denominator = 1000;

    ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    hres = m_pDX11VideoDevice->CreateVideoProcessorEnumerator( &ContentDesc, &m_VideoProcessorEnum );
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;
    hres = m_pDX11VideoDevice->CreateVideoProcessor( m_VideoProcessorEnum, 0, &m_pVideoProcessor );
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    return MFX_ERR_NONE;
}

mfxStatus CD3D11Device::Reset()
{
    return MFX_ERR_NONE;
}

mfxStatus CD3D11Device::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
    if (type == MFX_HANDLE_D3D11_DEVICE)
    {
        *pHdl = m_pD3D11Device.p;
        return MFX_ERR_NONE;
    }
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CD3D11Device::SetHandle(mfxHandleType /*type*/, mfxHDL /*hdl*/)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CD3D11Device::RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator * pAlloc)
{
    HRESULT hres = S_OK;

    mfxStatus sts = CreateVideoProcessor(pSrf);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    
    // Get Backbuffer
    ID3D11VideoProcessorInputView* pInputView = NULL;
    ID3D11VideoProcessorOutputView* pOutputView = NULL;
    ID3D11Texture2D* pDXGIBackBuffer = NULL;

    hres = m_pSwapChain->GetBuffer(0, __uuidof( ID3D11Texture2D ), (void**)&pDXGIBackBuffer);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;
    
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC OutputViewDesc;
    OutputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    OutputViewDesc.Texture2D.MipSlice = 0;
    hres = m_pDX11VideoDevice->CreateVideoProcessorOutputView( 
        pDXGIBackBuffer,
        m_VideoProcessorEnum,
        &OutputViewDesc,
        &pOutputView );
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC InputViewDesc;
    InputViewDesc.FourCC = 0;
    InputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    InputViewDesc.Texture2D.MipSlice = 0;
    InputViewDesc.Texture2D.ArraySlice = 0;

    mfxHDLPair pair = {NULL};
    sts = pAlloc->GetHDL(pAlloc->pthis, pSrf->Data.MemId, (mfxHDL*)&pair);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    ID3D11Texture2D  *pRTTexture2D = reinterpret_cast<ID3D11Texture2D*>(pair.first);

    hres = m_pDX11VideoDevice->CreateVideoProcessorInputView( 
        pRTTexture2D,
        m_VideoProcessorEnum,
        &InputViewDesc,
        &pInputView );
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    D3D11_VIDEO_PROCESSOR_STREAM StreamData;
    StreamData.Enable = TRUE;
    StreamData.OutputIndex = 0;
    StreamData.InputFrameOrField = 0;
    StreamData.PastFrames = 0;
    StreamData.FutureFrames = 0;
    StreamData.ppPastSurfaces = NULL;
    StreamData.ppFutureSurfaces = NULL;
    StreamData.pInputSurface = pInputView;
    StreamData.ppPastSurfacesRight = NULL;
    StreamData.ppFutureSurfacesRight = NULL;
    StreamData.pInputSurfaceRight = NULL;

    //  NV12 surface to RGB backbuffer ...
    RECT rect = {0};            
    
    rect.right  = pSrf->Info.CropW;
    rect.bottom = pSrf->Info.CropH;
    m_pVideoContext->VideoProcessorSetStreamSourceRect(m_pVideoProcessor, 0, true, &rect);
    m_pVideoContext->VideoProcessorSetStreamFrameFormat( m_pVideoProcessor, 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    m_pVideoContext->VideoProcessorBlt( m_pVideoProcessor, pOutputView, 0, 1, &StreamData );                         

    hres = m_pSwapChain->Present( 0, 0 );
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;
    
    return MFX_ERR_NONE;
}

void CD3D11Device::Close()
{
}

#endif // #if MFX_D3D11_SUPPORT