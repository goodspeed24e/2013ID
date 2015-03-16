//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2012 Intel Corporation. All Rights Reserved.
//

#include <memory>
#include "pipeline_encode.h"

void PrintHelp(msdk_char *strAppName, msdk_char *strErrorMessage)
{
    msdk_printf(MSDK_STRING("Intel(R) Media SDK Encoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);
    
    if (strErrorMessage)
    {
        msdk_printf(MSDK_STRING("Error: %s\n\n"), strErrorMessage);
    }    
    msdk_printf(MSDK_STRING("Usage: %s h264|mpeg2|mvc [Options] -i InputYUVFile -o OutputEncodedFile -w width -h height\n"), strAppName);    
    msdk_printf(MSDK_STRING("Options: \n"));
    msdk_printf(MSDK_STRING("   [-nv12] - input is in NV12 color format, if not specified YUV420 is expected\n"));  
    msdk_printf(MSDK_STRING("   [-tff|bff] - input stream is interlaced, top|bottom fielf first, if not specified progressive is expected\n"));      
    msdk_printf(MSDK_STRING("   [-f frameRate] - video frame rate (frames per second)\n"));
    msdk_printf(MSDK_STRING("   [-b bitRate] - encoded bit rate (Kbits per second)\n"));
    msdk_printf(MSDK_STRING("   [-u speed|quality|balanced] - target usage\n"));
    msdk_printf(MSDK_STRING("   [-t numThreads] - number of threads; default value 0 \n"));
    msdk_printf(MSDK_STRING("   [-q quality] - quality parameter for JPEG codec. In range [1,100]. 100 is the best quality. \n"));
    msdk_printf(MSDK_STRING("   [-dstw width] - destination picture width, invokes VPP resizing\n")); 
    msdk_printf(MSDK_STRING("   [-dsth height] - destination picture height, invokes VPP resizing\n")); 
    msdk_printf(MSDK_STRING("   [-hw] - use platform specific SDK implementation, if not specified software implementation is used\n"));
#if D3D_SURFACES_SUPPORT
    msdk_printf(MSDK_STRING("   [-d3d] - work with d3d surfaces\n"));
    msdk_printf(MSDK_STRING("   [-d3d11] - work with d3d11 surfaces\n"));
    msdk_printf(MSDK_STRING("Example: %s h264|mpeg2|mvc|vp8 -i InputYUVFile -o OutputEncodedFile -w width -h height -angle 180 -opencl \n"), strAppName);
#endif
	msdk_printf(MSDK_STRING("   [-mux] - mux into container (mpeg for mpeg2, mp4 for H.264)\n"));
	msdk_printf(MSDK_STRING("   [-mkv] - mux into mkv container\n"));
    msdk_printf(MSDK_STRING("   [-viewoutput] - instruct the MVC encoder to output each view in separate bitstream buffer. Depending on the number of -o options behaves as follows:\n"));
    msdk_printf(MSDK_STRING("                   1: two views are encoded in single file\n"));
    msdk_printf(MSDK_STRING("                   2: two views are encoded in separate files\n"));
    msdk_printf(MSDK_STRING("                   3: behaves like 2 -o opitons was used and then one -o\n\n"));
    msdk_printf(MSDK_STRING("Example: %s h264|mpeg2 -i InputYUVFile -o OutputEncodedFile -w width -h height -angle 180 -opencl \n"), strAppName);
    msdk_printf(MSDK_STRING("Example for MVC: %s mvc -i InputYUVFile_1 -i InputYUVFile_2 -o OutputEncodedFile -w width -h height \n"), strAppName);

    msdk_printf(MSDK_STRING("\n"));
}

#define GET_OPTION_POINTER(PTR)         \
{                                       \
    if (2 == msdk_strlen(strInput[i]))      \
    {                                   \
        i++;                            \
        if (strInput[i][0] == MSDK_CHAR('-'))  \
        {                               \
            i = i - 1;                  \
        }                               \
        else                            \
        {                               \
            PTR = strInput[i];          \
        }                               \
    }                                   \
    else                                \
    {                                   \
        PTR = strInput[i] + 2;          \
    }                                   \
}

mfxStatus ParseInputString(msdk_char* strInput[], mfxU8 nArgNum, sInputParams* pParams)
{
    msdk_char* strArgument = MSDK_STRING("");
    msdk_char* stopCharacter;

    if (1 == nArgNum)
    {
        PrintHelp(strInput[0], NULL);
        return MFX_ERR_UNSUPPORTED;
    }

    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);   

    // parse command line parameters
    for (mfxU8 i = 1; i < nArgNum; i++)
    {
        MSDK_CHECK_POINTER(strInput[i], MFX_ERR_NULL_PTR);

        if (MSDK_CHAR('-') != strInput[i][0])
        {
            if (0 == msdk_strcmp(strInput[i], MSDK_STRING("h264")))
            {
                pParams->CodecId = MFX_CODEC_AVC;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("mpeg2")))
            {
                pParams->CodecId = MFX_CODEC_MPEG2;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("mvc")))
            {
                pParams->CodecId = MFX_CODEC_AVC;
                pParams->MVC_flags |= MVC_ENABLED;
            }
            else
            {
                PrintHelp(strInput[0], MSDK_STRING("Unknown codec"));
                return MFX_ERR_UNSUPPORTED;
            }
            continue;
        }

        // process multi-character options
        if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dstw")))
        {
            i++;
            pParams->nDstWidth = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dsth")))
        {
            i++;
            pParams->nDstHeight = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-hw")))
        {
            pParams->bUseHWLib = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-nv12")))
        {
            pParams->ColorFormat = MFX_FOURCC_NV12;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-tff")))
        {
            pParams->nPicStruct = MFX_PICSTRUCT_FIELD_TFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bff")))
        {
            pParams->nPicStruct = MFX_PICSTRUCT_FIELD_BFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-viewoutput")))
        {
            if (!(MVC_ENABLED & pParams->MVC_flags))
            {
                PrintHelp(strInput[0], MSDK_STRING("-viewoutput option is supported only when mvc codec specified"));
                return MFX_ERR_UNSUPPORTED;
            }
            pParams->MVC_flags |= MVC_VIEWOUTPUT;
        }
#if D3D_SURFACES_SUPPORT
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d")))
        {
            pParams->memType = D3D9_MEMORY;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d11")))
        {
            pParams->memType = D3D11_MEMORY;
        }
#endif
		// =========== ffmpeg muxer integration ============
		else if (0 == msdk_strcmp(strInput[i], _T("-mux")))
        {
            pParams->bMux = true;
        }
		else if (0 == msdk_strcmp(strInput[i], _T("-mkv")))
        {
            pParams->bMux = true;
			pParams->bMuxMkv = true;
        }
		// =========== ffmpeg muxer integration end ============
        else // 1-character options
        {
            switch (strInput[i][1])
            {
            case MSDK_CHAR('u'):
                GET_OPTION_POINTER(strArgument);
                pParams->nTargetUsage = StrToTargetUsage(strArgument);
                break;
            case MSDK_CHAR('w'):
                GET_OPTION_POINTER(strArgument);
                pParams->nWidth = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('h'):
                GET_OPTION_POINTER(strArgument);
                pParams->nHeight = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('f'):
                GET_OPTION_POINTER(strArgument);
                pParams->dFrameRate = (mfxF64)msdk_strtod(strArgument, &stopCharacter);
                break;
            case MSDK_CHAR('b'):
                GET_OPTION_POINTER(strArgument);
                pParams->nBitRate = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('i'):
                GET_OPTION_POINTER(strArgument);
                msdk_strcopy(pParams->strSrcFile, strArgument);
                if (MVC_ENABLED & pParams->MVC_flags)
                {
                    pParams->srcFileBuff.push_back(strArgument);
                }
                break;
            case MSDK_CHAR('o'):
                GET_OPTION_POINTER(strArgument);
                pParams->dstFileBuff.push_back(strArgument);
                break;
            case MSDK_CHAR('?'):
                PrintHelp(strInput[0], NULL);
                return MFX_ERR_UNSUPPORTED;
            default:
                PrintHelp(strInput[0], MSDK_STRING("Unknown options"));
            }
        }
    }

    // check if all mandatory parameters were set
    if (0 == msdk_strlen(pParams->strSrcFile))
    {
        PrintHelp(strInput[0], MSDK_STRING("Source file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (pParams->dstFileBuff.empty())
    {
        PrintHelp(strInput[0], MSDK_STRING("Destination file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (0 == pParams->nWidth || 0 == pParams->nHeight)
    {
        PrintHelp(strInput[0], MSDK_STRING("-w, -h must be specified"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (MFX_CODEC_MPEG2 != pParams->CodecId && MFX_CODEC_AVC != pParams->CodecId)
    {
        PrintHelp(strInput[0], MSDK_STRING("Unknown codec"));
        return MFX_ERR_UNSUPPORTED;
    }

    // set default values for optional parameters that were not set or were set incorrectly
    mfxU32 nviews = (mfxU32)pParams->srcFileBuff.size();
    if ((nviews <= 1) || (nviews > 2))
    {
        if (!(MVC_ENABLED & pParams->MVC_flags))
        {
            pParams->numViews = 1;
        }
        else
        {
            PrintHelp(strInput[0], MSDK_STRING("Only 2 views are supported right now in this sample."));
            return MFX_ERR_UNSUPPORTED;
        }
    }
    else
    {
        pParams->numViews = nviews;
    }
    
    if (MFX_TARGETUSAGE_BEST_QUALITY != pParams->nTargetUsage && MFX_TARGETUSAGE_BEST_SPEED != pParams->nTargetUsage)
    {
        pParams->nTargetUsage = MFX_TARGETUSAGE_BALANCED;
    }
    
    if (pParams->dFrameRate <= 0)
    {
        pParams->dFrameRate = 30;
    }    

    // if no destination picture width or height wasn't specified set it to the source picture size
    if (pParams->nDstWidth == 0)
    {
        pParams->nDstWidth = pParams->nWidth;
    }

    if (pParams->nDstHeight == 0)
    {
        pParams->nDstHeight = pParams->nHeight;
    }

    // calculate default bitrate based on the resolution (a parameter for encoder, so Dst resolution is used)
    if (pParams->nBitRate == 0)
    {        
        pParams->nBitRate = CalculateDefaultBitrate(pParams->CodecId, pParams->nTargetUsage, pParams->nDstWidth,
            pParams->nDstHeight, pParams->dFrameRate);         
    }    

    // if nv12 option wasn't specified we expect input YUV file in YUV420 color format
    if (!pParams->ColorFormat)
    {
        pParams->ColorFormat = MFX_FOURCC_YV12;
    }

    if (!pParams->nPicStruct)
    {
        pParams->nPicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    return MFX_ERR_NONE;
}

int _tmain(int argc, msdk_char *argv[])
{  
    sInputParams Params = {};   // input parameters from command line
    std::auto_ptr<CEncodingPipeline>  pPipeline;

    mfxStatus sts = MFX_ERR_NONE; // return value check

    sts = ParseInputString(argv, (mfxU8)argc, &Params);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

	pPipeline.reset(new CEncodingPipeline);
    MSDK_CHECK_POINTER(pPipeline.get(), MFX_ERR_MEMORY_ALLOC);

    if (MVC_ENABLED & Params.MVC_flags)
    {
        pPipeline->SetMultiView();
        pPipeline->SetNumView(Params.numViews);
    }

    sts = pPipeline->Init(&Params);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);   

    pPipeline->PrintInfo();

    msdk_printf(MSDK_STRING("Processing started\n"));

    for (;;)
    {
        sts = pPipeline->Run();

        if (MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts)
        {            
            msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned an unexpected error. Recovering...\n"));
            sts = pPipeline->ResetDevice();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);         

            sts = pPipeline->ResetMFXComponents(&Params);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            continue;
        }        
        else
        {            
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            break;
        }
    }    

    pPipeline->Close();  
    msdk_printf(MSDK_STRING("\nProcessing finished\n"));    
    
    return 0;
}