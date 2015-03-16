//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright(c) 2005-2012 Intel Corporation. All Rights Reserved.
//
#include "pipeline_decode.h"
#include <sstream>

void PrintHelp(msdk_char *strAppName, const msdk_char *strErrorMessage)
{
    msdk_printf(MSDK_STRING("Intel(R) Media SDK Decoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);

    if (strErrorMessage)
    {
        msdk_printf(MSDK_STRING("Error: %s\n"), strErrorMessage);
    } 
    
    msdk_printf(MSDK_STRING("Usage: %s mpeg2|h264|vc1|mvc|jpeg| -i InputBitstream -o OutputYUVFile\n"), strAppName);
    msdk_printf(MSDK_STRING("Options: \n"));
    msdk_printf(MSDK_STRING("   [-hw]               - use platform specific SDK implementation, if not specified software implementation is used\n"));
#if D3D_SURFACES_SUPPORT   
    msdk_printf(MSDK_STRING("   [-d3d]              - work with d3d9 surfaces\n"));
    msdk_printf(MSDK_STRING("   [-d3d11]            - work with d3d11 surfaces\n"));
    msdk_printf(MSDK_STRING("   [-r]                - render decoded data in a separate window \n"));
    msdk_printf(MSDK_STRING("   [-wall w h n m f t] - same as -r, and positioned rendering window in a particular cell on specific monitor \n"));
    msdk_printf(MSDK_STRING("       w               - number of columns of video windows on selected monitor\n"));
    msdk_printf(MSDK_STRING("       h               - number of rows of video windows on selected monitor\n"));
    msdk_printf(MSDK_STRING("       n(0,.,w*h-1)    - order of video window in table that will be rendered\n"));
    msdk_printf(MSDK_STRING("       m(0,1..)        - monitor id \n"));
    msdk_printf(MSDK_STRING("       f               - rendering framerate\n"));
    msdk_printf(MSDK_STRING("       t(0/1)          - enable/disable window's title\n"));
#endif
	msdk_printf(MSDK_STRING("   [-split]                  - split(demux) mp4 (containing H.264) or mpeg2 (containing MPEG2) container\n"));
    msdk_printf(MSDK_STRING("   [-low_latency]            - configures decoder for low latency mode (supported only for H.264 and JPEG codec)\n"));
    msdk_printf(MSDK_STRING("   [-calc_latency]           - calculates latency during decoding and prints log (supported only for H.264 and JPEG codec)\n"));
    msdk_printf(MSDK_STRING("\nFeatures: \n"));
    msdk_printf(MSDK_STRING("   Press 1 to toggle fullscreen rendering on/off\n"));
    msdk_printf(MSDK_STRING("\n"));
}

#define GET_OPTION_POINTER(PTR)        \
{                                      \
    if (2 == msdk_strlen(strInput[i]))     \
    {                                  \
        i++;                           \
        if (strInput[i][0] == MSDK_CHAR('-')) \
        {                              \
            i = i - 1;                 \
        }                              \
        else                           \
        {                              \
            PTR = strInput[i];         \
        }                              \
    }                                  \
    else                               \
    {                                  \
        PTR = strInput[i] + 2;         \
    }                                  \
}

mfxStatus ParseInputString(msdk_char* strInput[], mfxU8 nArgNum, sInputParams* pParams)
{   
    msdk_char* strArgument = MSDK_STRING("");

    if (1 == nArgNum)
    {
        PrintHelp(strInput[0], NULL);
        return MFX_ERR_UNSUPPORTED;
    }

    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    

    for (mfxU8 i = 1; i < nArgNum; i++)
    {
        if (MSDK_CHAR('-') != strInput[i][0])
        {
            if (0 == msdk_strcmp(strInput[i], MSDK_STRING("mpeg2")))
            {
                pParams->videoType = MFX_CODEC_MPEG2;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("h264")))
            {
                pParams->videoType = MFX_CODEC_AVC;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("vc1")))
            {
                pParams->videoType = MFX_CODEC_VC1;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("mvc")))
            {
                pParams->videoType = MFX_CODEC_AVC;
                pParams->bIsMVC = true;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("jpeg")))
            {
                pParams->videoType = MFX_CODEC_JPEG;
            }
            else
            {
                PrintHelp(strInput[0], MSDK_STRING("Unknown codec"));
                return MFX_ERR_UNSUPPORTED;
            }
            continue;
        }

        // multi-character options
        if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-hw")))
        {
            pParams->bUseHWLib = true;
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
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-r")))
        {
            if (SYSTEM_MEMORY == pParams->memType)
                pParams->memType = D3D9_MEMORY;
            pParams->bRendering = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-wall")))
        {
            if(i + 6 >= nArgNum)
            {
                PrintHelp(strInput[0], MSDK_STRING("Not enough parameters for -wall key"));
                return MFX_ERR_UNSUPPORTED;
            }
            pParams->memType = D3D9_MEMORY;
            pParams->bRendering = true;

            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &pParams->nWallW);
            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &pParams->nWallH);
            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &pParams->nWallCell);
            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &pParams->nWallMonitor);
            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &pParams->nWallFPS);
            int nTitle;
            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &nTitle);

            pParams->bNoTitle = 0 == nTitle;
        }
#endif
		// =========== ffmpeg splitter integration ============
		else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-split")))
        {
            pParams->bSplit = true;
        }
		// =========== ffmpeg splitter integration end ============
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-low_latency")))
        {            
            switch (pParams->videoType)
            {
                case MFX_CODEC_AVC:
                case MFX_CODEC_JPEG:
                {
                    pParams->bLowLat = true;
                    if (!pParams->bIsMVC)
                        break;
                }
                default:
                {
                     PrintHelp(strInput[0], MSDK_STRING("-low_latency mode is suppoted only for H.264 and JPEG codecs"));
                     return MFX_ERR_UNSUPPORTED;
                }
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-calc_latency")))
        {            
            switch (pParams->videoType)
            {
                case MFX_CODEC_AVC:
                case MFX_CODEC_JPEG:
                {
                    pParams->bCalLat = true;
                    if (!pParams->bIsMVC)               
                        break;
                }
                default:
                {
                     PrintHelp(strInput[0], MSDK_STRING("-calc_latency mode is suppoted only for H.264 and JPEG codecs"));
                     return MFX_ERR_UNSUPPORTED;
                }
            }
        }
        else // 1-character options
        {
            switch (strInput[i][1])
            {
            case MSDK_CHAR('i'):
                GET_OPTION_POINTER(strArgument);
                msdk_strcopy(pParams->strSrcFile, strArgument);
                break;
            case MSDK_CHAR('o'):
                GET_OPTION_POINTER(strArgument);
                pParams->bOutput = true;
                msdk_strcopy(pParams->strDstFile, strArgument);
                break;
            case MSDK_CHAR('?'):
                PrintHelp(strInput[0], NULL);
                return MFX_ERR_UNSUPPORTED;
            default:
                {
                    std::basic_stringstream<msdk_char> stream;
                    stream << MSDK_STRING("Unknown option: ") << strInput[i];
                    PrintHelp(strInput[0], stream.str().c_str());
                    return MFX_ERR_UNSUPPORTED;
                }
            }
        }
    }

    if (0 == msdk_strlen(pParams->strSrcFile))
    {
        PrintHelp(strInput[0], MSDK_STRING("Source file name not found"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (0 == msdk_strlen(pParams->strDstFile))
    {
        if (!pParams->bRendering)
        {
            PrintHelp(strInput[0], MSDK_STRING("Destination file name not found"));
            return MFX_ERR_UNSUPPORTED;
        }

        pParams->bOutput = false;
    }

    if (MFX_CODEC_MPEG2 != pParams->videoType && 
        MFX_CODEC_AVC   != pParams->videoType && 
        MFX_CODEC_VC1   != pParams->videoType &&
        MFX_CODEC_JPEG  != pParams->videoType)
    {
        PrintHelp(strInput[0], MSDK_STRING("Unknown codec"));
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

int _tmain(int argc, TCHAR *argv[])
{
    sInputParams        Params;   // input parameters from command line
    CDecodingPipeline   Pipeline; // pipeline for decoding, includes input file reader, decoder and output file writer

    mfxStatus sts = MFX_ERR_NONE; // return value check

    sts = ParseInputString(argv, (mfxU8)argc, &Params);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    if (Params.bIsMVC)
        Pipeline.SetMultiView();

    sts = Pipeline.Init(&Params);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);   

    // print stream info 
    Pipeline.PrintInfo(); 

    msdk_printf(MSDK_STRING("Decoding started\n"));

    for (;;)
    {
        sts = Pipeline.RunDecoding();

        if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts || MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts)
        {
            if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts)
            {
                msdk_printf(MSDK_STRING("\nERROR: Incompatible video parameters detected. Recovering...\n"));
            }
            else
            {
                msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned unexpected error. Recovering...\n"));
                sts = Pipeline.ResetDevice();
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            }           

            sts = Pipeline.ResetDecoder(&Params);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);            
            continue;
        }        
        else
        {
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            break;
        }
    }
    
    msdk_printf(MSDK_STRING("\nDecoding finished\n"));

    Pipeline.Close();

    return 0;
}
