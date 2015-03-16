
#ifdef WIN32
#include <io.h>
#define mktemp _mktemp
#endif
#include <iostream>
#include <ctime>
#include <boost/filesystem.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>

#include "mkvmuxer.hpp"

using namespace std;


namespace MuxerImpl
{

Muxer * CreateMkvMuxer()
{
	return new MkvMuxer();
}

};

// file parameters
static const char *OUTPUT_FILE_NAME   = "test1.mkv";
static const bool bWriteDefaultValues = false;
static const bool isOneSubtitleForEachCluster = true;

static bool isRandomSeed = false;
static boost::mt19937 seedGenerator;
static boost::uniform_int<unsigned int> randByte(0, 0xFF);
static boost::uniform_int<unsigned int> rand4Bytes(0, 0xFFFFFFFF);

static inline void InitRandomSeed()
{
	if (!isRandomSeed)
	{
		seedGenerator.seed(static_cast<unsigned int>(std::time(0)));
		isRandomSeed = true;
	}
}

static void GenerateRandomBytes(void *buffer_, size_t size)
{
	unsigned char *buffer = (unsigned char *)buffer_;

	InitRandomSeed();

	for (size_t i = 0; i < size; i++)
	{
		buffer[i] = randByte(seedGenerator);
	}
}

static unsigned int GenerateRandomUInteger()
{
	InitRandomSeed();

	return rand4Bytes(seedGenerator);
}

static const unsigned char H264_VIDEO_TRACK_DEFAULT_CODEC_PRIVATE[] =
{
		0x01, 0x42, 0xE0, 0x1E,  0xFF, 0xE1, 0x00, 0x00,  0x01, 0x00, 0x00
};

// G.711 u-law, (1 channel, 8 bits, 8000 Hz)
static const unsigned char G711_ULAW_AUDIO_TRACK_CODEC_PRIVATE[] =
{
		0x07, 0x00, 0x01, 0x00,  0x40, 0x1F, 0x00, 0x00,
		0x40, 0x1F, 0x00, 0x00,  0x01, 0x00, 0x08, 0x00,
		0x00, 0x00
};

// G.711 a-law, (1 channel, 8 bits, 8000 Hz)
static const unsigned char G711_ALAW_AUDIO_TRACK_CODEC_PRIVATE[] =
{
		0x06, 0x00, 0x01, 0x00,  0x40, 0x1F, 0x00, 0x00,
		0x40, 0x1F, 0x00, 0x00,  0x01, 0x00, 0x08, 0x00,
		0x00, 0x00
};

// ADPCM, (1 channel, 4 bits, 8000 Hz)
static const unsigned char ADPCM_AUDIO_TRACK_CODEC_PRIVATE[] =
{
		0x45, 0x00, 0x01, 0x00,  0x40, 0x1F, 0x00, 0x00,
		0xA0, 0x0F, 0x00, 0x00,  0x01, 0x00, 0x04, 0x00,
		0x02, 0x00, 0x02, 0x00
};
/*static const unsigned char ADPCM_AUDIO_TRACK_CODEC_PRIVATE[] =
{
		0x02, 0x00, 0x01, 0x00,  0x40, 0x1F, 0x00, 0x00,
		0xA0, 0x0F, 0x00, 0x00,  0x00, 0x04, 0x04, 0x00,
		0x00, 0x00
};*/

// AMR
/*static const unsigned char AMR_AUDIO_TRACK_CODEC_PRIVATE[] =
{
		0x36, 0x01, 0x01, 0x00,  0x40, 0x1F, 0x00, 0x00,
		0xA0, 0x0F, 0x00, 0x00,  0x00, 0x04, 0x04, 0x00,
		0x00, 0x00
};*/

const MuxerImpl::Codec * const MkvMuxer::MkvVideoCodec::CODECS[] =
{
	new MkvMuxer::MkvVideoCodec(0, "V_UNCOMPRESSED",  NULL, 0),  // raw data
	new MkvMuxer::MkvVideoCodec(0, "V_UNCOMPRESSED",  NULL, 0),  // HikVision
	new MkvMuxer::MkvVideoCodec(2, "V_MPEG4/ISO/ASP", NULL, 0),  // MPEG4
	new MkvMuxer::MkvVideoCodec(3, "V_MJPEG",         NULL, 0),  // Motion JPEG
	new MkvMuxer::MkvVideoCodec(4, "V_MPEG4/ISO/AVC", H264_VIDEO_TRACK_DEFAULT_CODEC_PRIVATE, sizeof(H264_VIDEO_TRACK_DEFAULT_CODEC_PRIVATE) / sizeof(unsigned char)),  // H.264
	new MkvMuxer::MkvVideoCodec(4, "V_MPEG4/ISO/AVC", H264_VIDEO_TRACK_DEFAULT_CODEC_PRIVATE, sizeof(H264_VIDEO_TRACK_DEFAULT_CODEC_PRIVATE) / sizeof(unsigned char))  // New HikVision
};

const MuxerImpl::Codec * const MkvMuxer::MkvAudioCodec::CODECS[] =
{
	new MkvMuxer::MkvAudioCodec(0, "A_PCM/INT/BIG", NULL, 0),  // PCM Big Endian
	new MkvMuxer::MkvAudioCodec(1, "A_MS/ACM",      G711_ULAW_AUDIO_TRACK_CODEC_PRIVATE, sizeof(G711_ULAW_AUDIO_TRACK_CODEC_PRIVATE) / sizeof(unsigned char)),  // G.711 u-law
	new MkvMuxer::MkvAudioCodec(2, "A_PCM/INT/LIT", NULL, 0),  // PCM Little Endian
	new MkvMuxer::MkvAudioCodec(3, "A_MS/ACM",      ADPCM_AUDIO_TRACK_CODEC_PRIVATE,     sizeof(ADPCM_AUDIO_TRACK_CODEC_PRIVATE) / sizeof(unsigned char)),  // ADPCM
	new MkvMuxer::MkvAudioCodec(4, "A_AMR/NB",      NULL, 0),  // AMR
	new MkvMuxer::MkvAudioCodec(2, "A_PCM/INT/LIT", NULL, 0),  // MP2
	new MkvMuxer::MkvAudioCodec(2, "A_PCM/INT/LIT", NULL, 0),  // MP4
	new MkvMuxer::MkvAudioCodec(7, "A_MS/ACM",      G711_ALAW_AUDIO_TRACK_CODEC_PRIVATE, sizeof(G711_ALAW_AUDIO_TRACK_CODEC_PRIVATE) / sizeof(unsigned char))  // G.711 a-law
};

const MuxerImpl::Codec * const MkvMuxer::MkvSubtitleCodec::CODECS[] =
{
	new MkvMuxer::MkvSubtitleCodec(0, "S_TEXT/UTF8", NULL, 0),  // Message
	new MkvMuxer::MkvSubtitleCodec(1, "S_TEXT/UTF8", NULL, 0)  // OSD
};

MuxerImpl::Codec * MkvMuxer::MkvVideoCodec::FixByFrame(const Frame &frame, MuxerImpl::Codec *pMyCodec) const
{
	if ((frame.pStream != NULL)
	    && (frame.pStream->pImpl->pCodec == this)
	    && (id == VideoStream::CODEC_ID_H264))
	{
		if (pMyCodec == NULL)
		{
			unsigned char *pMyPrivateData = new unsigned char[MAX_PRIVATE_DATA_SIZE];
			pMyCodec = new MkvVideoCodec(id, identifier, pMyPrivateData, MAX_PRIVATE_DATA_SIZE, true);
		}
		MkvVideoCodec *pMkvCodec = dynamic_cast<MkvVideoCodec *>(pMyCodec);

		const unsigned char *pSPS    = NULL;
		const unsigned char *pPPS    = NULL;
		size_t               spsSize = 0;
		size_t               ppsSize = 0;

		const unsigned char *pBuffer = frame.data;
		size_t               size    = frame.size < MAX_PRIVATE_DATA_SIZE ? frame.size : MAX_PRIVATE_DATA_SIZE;

		const unsigned char *pLastStartCode     = NULL;
		size_t              *pLastStartCodeSize = NULL;

		while (size > 4)
		{
			// find start code: <00 00 00 01>
			// warning: use <00 00 01> for convenient
			if ((pBuffer[0] == 0) && (pBuffer[2] == 1) && (pBuffer[1] == 0))
			{
				if (pLastStartCodeSize != NULL)
				{
					// caculate the size of last NAL unit
					*pLastStartCodeSize = pBuffer - pLastStartCode - 4;
				}

				// find sps & pps
				if ((pBuffer[3] & 0x1F) == 0x07)
				{
					pSPS = pBuffer;
					pLastStartCodeSize = &spsSize;
				}
				else if ((pBuffer[3] & 0x1F) == 0x08)
				{
					pPPS = pBuffer;
					pLastStartCodeSize = &ppsSize;
				}
				else
				{
					pLastStartCodeSize = NULL;
				}

				pLastStartCode = pBuffer;
			}
			size--;
			pBuffer++;
		}

		if (pLastStartCodeSize != NULL)
		{
			// caculate the size of last NAL unit
			*pLastStartCodeSize = frame.data + frame.size - pLastStartCode - 3;
		}

		// is it ready for sps & pps?
		if ((pSPS != NULL) && (pPPS != NULL)
		    && (spsSize > 0) && (ppsSize > 0)
		    && (6 + 2 + spsSize + 1 + 2 + ppsSize <= MAX_PRIVATE_DATA_SIZE))
		{
			// construct the private data
			unsigned char *pPrivateData = (unsigned char *)pMkvCodec->privateData;

			// header
			pPrivateData[0] = 0x01;
			pPrivateData[1] = pSPS[3];
			pPrivateData[2] = pSPS[4];
			pPrivateData[3] = pSPS[5];
			pPrivateData[4] = 0xFF;
			pPrivateData[5] = 0xE1;

			// sps
			pPrivateData[6] = (spsSize >> 8) & 0xFF;
			pPrivateData[7] = (spsSize     ) & 0xFF;
			memcpy(pPrivateData + 6 + 2, pSPS + 3, spsSize);

			// pps
			pPrivateData[6 + 2 + spsSize    ] = 0x01;
			pPrivateData[6 + 2 + spsSize + 1] = (ppsSize >> 8) & 0xFF;
			pPrivateData[6 + 2 + spsSize + 2] = (ppsSize     ) & 0xFF;
			memcpy(pPrivateData + 6 + 2 + spsSize + 1 + 2, pPPS + 3, ppsSize);

			// setup pMkvCodec
			pMkvCodec->privateDataSize = 6 + 2 + spsSize + 1 + 2 + ppsSize;
			return pMkvCodec;
		}
		else
		{
			// use the default private data
		}
	}
	return NULL;
}

static bool JustFreeBuffer(void *pParam, unsigned char *pBuffer)
{
	if (pBuffer != NULL)
	{
		delete[] pBuffer;
	}
	return true;
}

unsigned char * MkvMuxer::MkvVideoCodec::FixFrameData(Frame &frame) const
{
	unsigned char *pRealBuffer = frame.data;

	if (id == VideoStream::CODEC_ID_H264)
	{
		// the byte stream format --> the NAL unit stream format
		unsigned char *pBuffer        = frame.data;
		size_t         size           = frame.size;
		unsigned char *pLastStartCode = NULL;
		int            counter        = 0;

		while ((size > 4) && (++counter <= MAX_PRIVATE_DATA_SIZE))
		{
			// find start code: <00 00 00 01>
			// warning: use <00 00 01> for convenient
			if ((pBuffer[0] == 0) && (pBuffer[2] == 1) && (pBuffer[1] == 0)
			    && ((pBuffer[3] & 0x1F) > 0) && ((pBuffer[3] & 0x1F) <= 8))
			{
				if (pLastStartCode != NULL)
				{
					// start code --> the size of last NAL unit
					size_t nalUnitSize = pBuffer - pLastStartCode - 4;
					pLastStartCode[0] = (nalUnitSize >> 16) & 0xFF;
					pLastStartCode[1] = (nalUnitSize >>  8) & 0xFF;
					pLastStartCode[2] = (nalUnitSize      ) & 0xFF;
				}
				else
				{
					int offset = pBuffer - frame.data;
					frame.data += offset;
					frame.size -= offset;

					if (frame.needCopyBuffer)
					{
						frame.needCopyBuffer = false;

						pBuffer = new unsigned char[frame.size];
						memcpy(pBuffer, frame.data, frame.size);
						frame.data = pBuffer;

						if (frame.pFreeBuffer != NULL)
						{
							frame.pFreeBuffer(frame.pFreeBufferParam, pRealBuffer);
						}

						frame.pFreeBuffer = JustFreeBuffer;
						pRealBuffer = pBuffer;
					}
				}
				pLastStartCode = pBuffer;
			}
			size--;
			pBuffer++;
		}

		if (pLastStartCode != NULL)
		{
			// start code --> the size of last NAL unit
			size_t nalUnitSize = frame.data + frame.size - pLastStartCode - 4;
			pLastStartCode[0] = (nalUnitSize >> 16) & 0xFF;
			pLastStartCode[1] = (nalUnitSize >>  8) & 0xFF;
			pLastStartCode[2] = (nalUnitSize      ) & 0xFF;
		}
	}

	return pRealBuffer;
}

MkvMuxer::MkvMuxer()
	: state(STOPPED), pListener(NULL), pStreams(NULL),
	  pMKVFile(NULL), pMKVHead(NULL), pSegment(NULL), segmentSize(0),
	  pMetaSeek(NULL), pMetaSeekDummy(NULL), pAllCues(NULL), pAllCuesDummy(NULL),
	  pVideoTrack(NULL), pAudioTrack(NULL), pSubtitleTrack(NULL)
{
}

MkvMuxer::~MkvMuxer()
{
	if (state != STOPPED)
	{
		// FIXME:
	}
}

bool MkvMuxer::SetFileConfig(const FileConfig &config)
{
	if (state != STOPPED)
	{
		// error: unable to set file configuration while muxing
		return false;
	}

	fileConfig = config;
	return true;
}

bool MkvMuxer::AddEventListener(const EventListener &listener)
{
	pListener = &listener;
	return true;
}

bool MkvMuxer::RemoveEventListener(const EventListener &listener)
{
	pListener = NULL;
	return true;
}

bool MkvMuxer::StartMuxing(const Streams &streams, const Frame &myFrame, const char *fileName)
{
	if (state != STOPPED)
	{
		// error: already started
		return false;
	}
	state = STARTING;

	if (streams.pVideo == NULL)
	{
		state = STOPPED;
		return false;
	}
	pStreams = &streams;

	if (fileName != NULL)
	{
		pOutFileName = fileName;
	}
	else
	{
		// create a temporary file name
		strncpy(myOutFileName, "tmXXXXXX", 9);
		if (mktemp(myOutFileName) == NULL)
		{
			// FIXME: error
			pOutFileName = OUTPUT_FILE_NAME;
		}
		else
		{
			pOutFileName = myOutFileName;
		}
	}

	// write the head of the file (with everything already configured)
	{
		if (pMKVFile != NULL)
		{
			delete pMKVFile;
			pMKVFile = NULL;
		}

		pMKVFile = new StdIOCallback(pOutFileName, MODE_CREATE);
		//pMKVFile = new MemIOCallback(1024*1024);
	}

	// Writing EBML test
	{
		if (pMKVHead == NULL)
		{
			pMKVHead = new EbmlHead();
			*static_cast<EbmlString *>(&GetChild<EDocType>(*pMKVHead)) = "matroska";
			*static_cast<EbmlUInteger *>(&GetChild<EDocTypeVersion>(*pMKVHead)) = MATROSKA_VERSION;
			*static_cast<EbmlUInteger *>(&GetChild<EDocTypeReadVersion>(*pMKVHead)) = MATROSKA_VERSION;
		}

		pMKVHead->Render(*pMKVFile, true);
	}

	{
		if (pSegment != NULL)
		{
			delete pSegment;
			pSegment = NULL;
		}
		pSegment = new KaxSegment();

		// size is unknown and will always be, we can render it right away
		// 5 octets can represent 2^35, it is wide enough for almost any file
		segmentSize = pSegment->WriteHead(*pMKVFile, 5, bWriteDefaultValues);
	}

	// reserve some space for the Meta Seek writen at the end
	{
		if (pMetaSeek != NULL)
		{
			delete pMetaSeek;
			pMetaSeek = NULL;
		}
		pMetaSeek = new KaxSeekHead();

		if (pMetaSeekDummy == NULL)
		{
			pMetaSeekDummy = new EbmlVoid();
		}

		pMetaSeekDummy->SetSize(fileConfig.pImpl->GetMetaSeekElementSize());
		segmentSize += pMetaSeekDummy->Render(*pMKVFile, bWriteDefaultValues);
	}

	// fill the mandatory Info section
	{
		KaxInfo &segmentInfo = GetChild<KaxInfo>(*pSegment);

		*static_cast<EbmlUInteger *>(&GetChild<KaxTimecodeScale>(segmentInfo)) = fileConfig.timecodeScale;
		*static_cast<EbmlFloat *>(&GetChild<KaxDuration>(segmentInfo)) = 0.0;  // fix it at StopMuxing()

		std::string muxingAppString = "libebml v";
		muxingAppString += EbmlCodeVersion.c_str();
		muxingAppString += " + libmatroska v";
		muxingAppString += KaxCodeVersion.c_str();
		UTFstring muxingAppUTFstring;
		muxingAppUTFstring.SetUTF8(muxingAppString);
		*(EbmlUnicodeString *)&GetChild<KaxMuxingApp>(segmentInfo)  = muxingAppUTFstring;
		*(EbmlUnicodeString *)&GetChild<KaxWritingApp>(segmentInfo) = fileConfig.applicationName;

		//GetChild<KaxDateUTC>(segmentInfo).SetEpochDate(time(NULL));  // get current time
		GetChild<KaxDateUTC>(segmentInfo).SetEpochDate(pStreams->dateUTC);  // FIXME: use the timecode of 1st frame

		unsigned char segmentUIDBuffer[128 / 8];  // an UID with a length of 128 bits
		GenerateRandomBytes(segmentUIDBuffer, 128 / 8);
		GetChild<KaxSegmentUID>(segmentInfo).CopyBuffer(segmentUIDBuffer, 128 / 8);

		segmentSize += segmentInfo.Render(*pMKVFile, true);
		pMetaSeek->IndexThis(segmentInfo, *pSegment);
	}

	KaxTracks &AllTracks = GetChild<KaxTracks>(*pSegment);
	pVideoTrack = &GetChild<KaxTrackEntry>(AllTracks);
	pAudioTrack = pStreams->HasAudio() ? &GetNextChild<KaxTrackEntry>(AllTracks, *pVideoTrack) : NULL;
	pSubtitleTrack = pStreams->HasOthers() ? &GetNextChild<KaxTrackEntry>(AllTracks, pStreams->HasAudio() ? *pAudioTrack : *pVideoTrack) : NULL;

	// fill video track params
	{
		pVideoTrack->SetGlobalTimecodeScale(fileConfig.timecodeScale);

		*static_cast<EbmlUInteger *>(&GetChild<KaxTrackNumber>(*pVideoTrack)) = pStreams->pVideo->trackNumber;
		*static_cast<EbmlUInteger *>(&GetChild<KaxTrackUID>(*pVideoTrack)) = GenerateRandomUInteger();
		*static_cast<EbmlUInteger *>(&GetChild<KaxTrackType>(*pVideoTrack)) = track_video;

		pStreams->pVideo->pImpl->CheckCodecInstance<MkvVideoCodec>();
		pStreams->pVideo->pImpl->FixCodecByFrame(myFrame);

		//*static_cast<EbmlUInteger *>(&GetChild<KaxTrackMinCache>(*pVideoTrack)) = 1;
		//*static_cast<EbmlUInteger *>(&GetChild<KaxTrackDefaultDuration>(*pVideoTrack)) = pStreams->pVideo->GetFrameDuration();
		*static_cast<EbmlString *>(&GetChild<KaxTrackLanguage>(*pVideoTrack)) = pStreams->pVideo->language;

		const MkvVideoCodec *pVideoCodec = dynamic_cast<const MkvVideoCodec *>(pStreams->pVideo->pImpl->pCodec);
		*static_cast<EbmlString *>(&GetChild<KaxCodecID>(*pVideoTrack)) = pVideoCodec->identifier;
		if (pVideoCodec->privateData != NULL)
		{
			GetChild<KaxCodecPrivate>(*pVideoTrack).CopyBuffer(pVideoCodec->privateData, pVideoCodec->privateDataSize);
		}

		pVideoTrack->EnableLacing(false);

		// video specific params
		KaxTrackVideo &videoTrackParameters = GetChild<KaxTrackVideo>(*pVideoTrack);
		*static_cast<EbmlUInteger *>(&GetChild<KaxVideoPixelWidth>(videoTrackParameters)) = pStreams->pVideo->width;
		*static_cast<EbmlUInteger *>(&GetChild<KaxVideoPixelHeight>(videoTrackParameters)) = pStreams->pVideo->height;

		// content encoding params
		if (pStreams->pVideo->GetCodec() == VideoStream::CODEC_ID_H264)
		{
			KaxContentEncodings &videoTrackContentEncodings = GetChild<KaxContentEncodings>(*pVideoTrack);
			KaxContentEncoding &videoTrackContentEncoding = GetChild<KaxContentEncoding>(videoTrackContentEncodings);
			KaxContentCompression &videoTrackContentComp = GetChild<KaxContentCompression>(videoTrackContentEncoding);

			unsigned char videoTrackContentCompSettings[] = { 0 };
			*static_cast<EbmlUInteger *>(&GetChild<KaxContentCompAlgo>(videoTrackContentComp)) = 3;
			GetChild<KaxContentCompSettings>(videoTrackContentComp).CopyBuffer(videoTrackContentCompSettings, 1);
		}
	}

	// fill audio track params
	if (pStreams->HasAudio())
	{
		pAudioTrack->SetGlobalTimecodeScale(fileConfig.timecodeScale);

		*static_cast<EbmlUInteger *>(&GetChild<KaxTrackNumber>(*pAudioTrack)) = pStreams->pAudio->trackNumber;
		*static_cast<EbmlUInteger *>(&GetChild<KaxTrackUID>(*pAudioTrack)) = GenerateRandomUInteger();
		*static_cast<EbmlUInteger *>(&GetChild<KaxTrackType>(*pAudioTrack)) = track_audio;

		pStreams->pAudio->pImpl->CheckCodecInstance<MkvAudioCodec>();

		//*static_cast<EbmlUInteger *>(&GetChild<KaxTrackDefaultDuration>(*pAudioTrack)) = pStreams->pAudio->GetFrameDuration();
		*static_cast<EbmlString *>(&GetChild<KaxTrackLanguage>(*pAudioTrack)) = pStreams->pAudio->language;

		const MkvAudioCodec *pAudioCodec = dynamic_cast<const MkvAudioCodec *>(pStreams->pAudio->pImpl->pCodec);
		*static_cast<EbmlString *>(&GetChild<KaxCodecID>(*pAudioTrack)) = pAudioCodec->identifier;
		if (pAudioCodec->privateData != NULL)
		{
			GetChild<KaxCodecPrivate>(*pAudioTrack).CopyBuffer(pAudioCodec->privateData, pAudioCodec->privateDataSize);
		}

		pAudioTrack->EnableLacing(true);

		// audio specific params
		KaxTrackAudio &audioTrackParameters = GetChild<KaxTrackAudio>(*pAudioTrack);
		*static_cast<EbmlFloat *>(&GetChild<KaxAudioSamplingFreq>(audioTrackParameters)) = pStreams->pAudio->samplingRate;
		*static_cast<EbmlUInteger *>(&GetChild<KaxAudioChannels>(audioTrackParameters)) = pStreams->pAudio->channelCount;
		*static_cast<EbmlUInteger *>(&GetChild<KaxAudioBitDepth>(audioTrackParameters)) = pStreams->pAudio->bitDepth;
	}

	// fill subtitle track params
	if (pStreams->HasOthers())
	{
		pSubtitleTrack->SetGlobalTimecodeScale(fileConfig.timecodeScale);

		*static_cast<EbmlUInteger *>(&GetChild<KaxTrackNumber>(*pSubtitleTrack)) = pStreams->pOther->trackNumber;
		*static_cast<EbmlUInteger *>(&GetChild<KaxTrackUID>(*pSubtitleTrack)) = GenerateRandomUInteger();
		*static_cast<EbmlUInteger *>(&GetChild<KaxTrackType>(*pSubtitleTrack)) = track_subtitle;

		pStreams->pOther->pImpl->CheckCodecInstance<MkvSubtitleCodec>();

		*static_cast<EbmlString *>(&GetChild<KaxTrackLanguage>(*pSubtitleTrack)) = pStreams->pOther->language;

		*static_cast<EbmlString *>(&GetChild<KaxCodecID>(*pSubtitleTrack)) = dynamic_cast<const MkvSubtitleCodec *>(pStreams->pOther->pImpl->pCodec)->identifier;

		pSubtitleTrack->EnableLacing(false);
	}

	segmentSize += AllTracks.Render(*pMKVFile, bWriteDefaultValues);
	pMetaSeek->IndexThis(AllTracks, *pSegment);

	{
		if (pAllCues != NULL)
		{
			delete pAllCues;
			pAllCues = NULL;
		}
		pAllCues = new KaxCues();
		pAllCues->SetGlobalTimecodeScale(fileConfig.timecodeScale);

		if (pAllCuesDummy == NULL)
		{
			pAllCuesDummy = new EbmlVoid();
		}

		pAllCuesDummy->SetSize(fileConfig.pImpl->GetCueingDataElementSize());
		segmentSize += pAllCuesDummy->Render(*pMKVFile, bWriteDefaultValues);
	}

	{
		isSuggest = true;
		isFirstCluster = true;
		pCluster = NULL;
		pBlockBlob = NULL, pLastVideoBlockBlob = NULL;
		clusterMinTimecode = 0ull;
		firstFrameTimecode = 0ull;
		lastFrameTimecode = 0ull;
		pLastSubtitleBlockGroup = NULL;
		lastSubtitlePosition = 0ull;
		pSubtitleData = NULL;
		subtitleDataSize = 0;
	}

	state = STARTED;
	AppendFrame(myFrame);
	return true;
}

bool MkvMuxer::StopMuxing()
{
	if ((state == STOPPED) || (state == STOPPING))
	{
		// warning: already stopped
		return false;
	}
	state = STOPPING;

	if (pCluster != NULL)
	{
		if (isOneSubtitleForEachCluster && (pLastSubtitleBlockGroup != NULL))
		{
			// update the timecode of the last subtitle frame
			pLastSubtitleBlockGroup->SetBlockDuration(lastFrameTimecode - pLastSubtitleBlockGroup->GlobalTimecode());
			pLastSubtitleBlockGroup = NULL;
		}

		// release the last cluster
		segmentSize += pCluster->Render(*pMKVFile, *pAllCues, bWriteDefaultValues);
		pCluster->ReleaseFrames();
		if (pListener != NULL)
		{
			pListener->SuggestFreeBuffers();
		}
		delete pCluster;
		pCluster = NULL;

		// re-calculate segDuration & subtitleBlockDuration
		KaxDuration &segDuration = GetChild<KaxDuration>(GetChild<KaxInfo>(*pSegment));
		*static_cast<EbmlFloat *>(&segDuration) = (lastFrameTimecode - firstFrameTimecode) / fileConfig.timecodeScale;
		KaxBlockDuration subtitleBlockDuration;
		if (lastSubtitlePosition != 0ull)
		{
			*static_cast<EbmlUInteger *>(&subtitleBlockDuration) = (lastFrameTimecode - lastSubtitleTimecode) / fileConfig.timecodeScale;
			subtitleBlockDuration.SetDefaultSize(8);
		}

		// correct segDuration & subtitleBlockDuration in the pMKVFile
		uint64 currentPosition = pMKVFile->getFilePointer();
		pMKVFile->setFilePointer(segDuration.GetElementPosition());
		segDuration.Render(*pMKVFile, false, true, true);
		if (lastSubtitlePosition != 0ull)
		{
			pMKVFile->setFilePointer(lastSubtitlePosition);
			subtitleBlockDuration.Render(*pMKVFile, false, true, true);
			lastSubtitlePosition = 0ull;
		}
		pMKVFile->setFilePointer(currentPosition);

		//segmentSize += pAllCues->Render(*pMKVFile, bWriteDefaultValues);
		pAllCuesDummy->ReplaceWith(*pAllCues, *pMKVFile, bWriteDefaultValues);
		pMetaSeek->IndexThis(*pAllCues, *pSegment);

		pMetaSeekDummy->ReplaceWith(*pMetaSeek, *pMKVFile, bWriteDefaultValues);

		// let's assume we know the size of the Segment element
		// the size of the pSegment is also computed because mandatory elements we don't write ourself exist
		if (pSegment->ForceSize(segmentSize - pSegment->HeadSize()))
		{
			pSegment->OverwriteHead(*pMKVFile);
		}

		pMKVFile->close();
	}
	else
	{
		// error: empty file
		pMKVFile->close();
		boost::filesystem::remove(pOutFileName);
		pOutFileName = NULL;
	}

	state = STOPPED;
	if (pListener != NULL)
	{
		// send event
		FileClosedEvent event(pOutFileName, firstFrameTimecode/1000000000ull, lastFrameTimecode/1000000000ull);
		pListener->FileClosed(event);
	}
	return true;
}

static bool MyFreeBuffer(const DataBuffer &aBuffer);

class MyDataBuffer : public DataBuffer
{
public:
	MyDataBuffer(binary *aBuffer, uint32 aSize, bool (*_pUserFreeBuffer)(void *pParam, unsigned char *pBuffer) = NULL, bool _bInternalBuffer = false, unsigned char *_pUserBuffer = NULL, void *_pUserFreeBufferParam = NULL)
		: DataBuffer(aBuffer, aSize, _pUserFreeBuffer != NULL ? MyFreeBuffer : NULL, _bInternalBuffer), pUserBuffer(_pUserBuffer), pUserFreeBuffer(_pUserFreeBuffer), pUserFreeBufferParam(_pUserFreeBufferParam)
	{
	}

	bool CallUserFreeBuffer()
	{
		bool result = pUserFreeBuffer(pUserFreeBufferParam, pUserBuffer != NULL ? pUserBuffer : myBuffer);
		pUserBuffer = NULL;
		return result;
	}

	binary *pUserBuffer;
	bool (*pUserFreeBuffer)(void *pParam, unsigned char *pBuffer);
	void *pUserFreeBufferParam;
};

static bool MyFreeBuffer(const DataBuffer &aBuffer)
{
	return const_cast<MyDataBuffer &>(dynamic_cast<const MyDataBuffer &>(aBuffer)).CallUserFreeBuffer();
}

bool MkvMuxer::AppendFrame(const Frame &myFrame)
{
	if (state < STARTED)
	{
		// error: not ready to append frames
		return false;
	}

	if (myFrame.pStream == NULL)
	{
		// skip this frame
		return false;
	}

	if (firstFrameTimecode == 0ull)
	{
		firstFrameTimecode = myFrame.timecode;
	}

	if (myFrame.timecode - firstFrameTimecode >= fileConfig.pImpl->GetMaxSegmentDuration())
	{
		// error: exceed the maximal duration
		StopMuxing();
		return false;
	}

	if (isSuggest && (pListener != NULL) && (myFrame.timecode - firstFrameTimecode >= fileConfig.pImpl->GetMaxSegmentDuration() / 2))
	{
		// send event
		pListener->SuggestSplitting();
		isSuggest = false;
	}

	// use a new cluster if this is a key frame
	if ((pCluster == NULL)
		|| (myFrame.timecode - clusterMinTimecode >= fileConfig.pImpl->GetMaxClusterDuration())
		|| (myFrame.isKey
		    && (myFrame.pStream->codecType == Stream::CODEC_TYPE_VIDEO)
			&& (myFrame.timecode - clusterMinTimecode >= fileConfig.pImpl->GetVideoCueTimecodeThreshold())))
	{
		// release the last cluster
		if (pCluster != NULL)
		{
			if (isOneSubtitleForEachCluster && (pLastSubtitleBlockGroup != NULL))
			{
				// update the timecode of the last subtitle frame
				pLastSubtitleBlockGroup->SetBlockDuration(myFrame.timecode - pLastSubtitleBlockGroup->GlobalTimecode());
				pLastSubtitleBlockGroup = NULL;
			}

			segmentSize += pCluster->Render(*pMKVFile, *pAllCues, bWriteDefaultValues);

			if (!isOneSubtitleForEachCluster && (pLastSubtitleBlockGroup != NULL))
			{
				lastSubtitleTimecode = pLastSubtitleBlockGroup->GlobalTimecode();
				lastSubtitlePosition = GetChild<KaxBlockDuration>(*pLastSubtitleBlockGroup).GetElementPosition();
				pLastSubtitleBlockGroup = NULL;
			}

			if (isFirstCluster)
			{
				isFirstCluster = false;
				pMetaSeek->IndexThis(*pCluster, *pSegment);
			}

			pCluster->ReleaseFrames();
			if (pListener != NULL)
			{
				pListener->SuggestFreeBuffers();
			}
			delete pCluster;
			pCluster = NULL;
		}

		// allocate a new cluster
		if (pCluster == NULL)
		{
			pCluster = new KaxCluster();
		}
		pCluster->SetParent(*pSegment); // mandatory to store references in this Cluster
		pCluster->InitTimecode(myFrame.timecode / fileConfig.timecodeScale, fileConfig.timecodeScale);
		//pCluster->EnableChecksum();

		pLastVideoBlockBlob = NULL;
		clusterMinTimecode = myFrame.timecode;

		// add an subtitle frame
		if (isOneSubtitleForEachCluster && (pSubtitleData != NULL) && (subtitleDataSize > 0))
		{
			// allocate current blob
			pBlockBlob = new KaxBlockBlob(BLOCK_BLOB_NO_SIMPLE);
			pCluster->AddBlockBlob(pBlockBlob);
			pBlockBlob->SetParent(*pCluster);

			// add a frame
			DataBuffer *pDataBuffer = new DataBuffer((binary *)pSubtitleData, subtitleDataSize);
			pBlockBlob->AddFrameAuto(*pSubtitleTrack, myFrame.timecode, *pDataBuffer, LACING_AUTO, NULL);
			pBlockBlob->SetBlockDuration(fileConfig.pImpl->GetMaxSegmentDuration());  // temporarily be the maximal value
			GetChild<KaxBlockDuration>((KaxBlockGroup &)*pBlockBlob).SetDefaultSize(8);  // with a fixed size
			pLastSubtitleBlockGroup = &(KaxBlockGroup &)*pBlockBlob;
		}
	}

	// prepare a frame
	unsigned char *pRealBuffer = const_cast<Frame &>(myFrame).FixData();  // WARNING: force to modify the data of myFrame
	DataBuffer *pMyData = new MyDataBuffer((binary *)myFrame.data, myFrame.size, myFrame.pFreeBuffer, myFrame.needCopyBuffer, pRealBuffer, myFrame.pFreeBufferParam);
	const_cast<Frame &>(myFrame).pFreeBuffer = NULL;

	KaxTrackEntry *pTrack = (myFrame.pStream->codecType == Stream::CODEC_TYPE_AUDIO) ? pAudioTrack
	                        : (myFrame.pStream->codecType == Stream::CODEC_TYPE_VIDEO) ? pVideoTrack
							: pSubtitleTrack;

	// allocate current blob
	pBlockBlob = new KaxBlockBlob((pTrack == pSubtitleTrack) ? BLOCK_BLOB_NO_SIMPLE : BLOCK_BLOB_ALWAYS_SIMPLE);
	pCluster->AddBlockBlob(pBlockBlob);
	pBlockBlob->SetParent(*pCluster);

	// add a frame
	pBlockBlob->AddFrameAuto(*pTrack, myFrame.timecode, *pMyData, LACING_AUTO, (!myFrame.isKey && (pTrack == pVideoTrack)) ? pLastVideoBlockBlob : NULL);
	if (pTrack == pSubtitleTrack)
	{
		if (pLastSubtitleBlockGroup != NULL)
		{
			// update the timecode of the last subtitle frame
			pLastSubtitleBlockGroup->SetBlockDuration(myFrame.timecode - pLastSubtitleBlockGroup->GlobalTimecode());
		}

		pBlockBlob->SetBlockDuration(fileConfig.pImpl->GetMaxSegmentDuration());  // temporarily be the maximal value
		GetChild<KaxBlockDuration>((KaxBlockGroup &)*pBlockBlob).SetDefaultSize(8);  // with a fixed size
		pLastSubtitleBlockGroup = &(KaxBlockGroup &)*pBlockBlob;

		if (lastSubtitlePosition != 0ull)
		{
			// re-calculate subtitleBlockDuration
			KaxBlockDuration subtitleBlockDuration;
			*static_cast<EbmlUInteger *>(&subtitleBlockDuration) = (myFrame.timecode - lastSubtitleTimecode) / fileConfig.timecodeScale;
			subtitleBlockDuration.SetDefaultSize(8);

			// correct subtitleBlockDuration in the pMKVFile
			uint64 currentPosition = pMKVFile->getFilePointer();
			pMKVFile->setFilePointer(lastSubtitlePosition);
			subtitleBlockDuration.Render(*pMKVFile, false, true, true);
			pMKVFile->setFilePointer(currentPosition);

			lastSubtitlePosition = 0ull;
		}

		if (isOneSubtitleForEachCluster && (myFrame.data != NULL) && (myFrame.size > 0))
		{
			if (pSubtitleData != NULL)
			{
				delete[] pSubtitleData;
			}

			// backup the current subtitle frame
			subtitleDataSize = myFrame.size;
			pSubtitleData = new unsigned char[myFrame.size];
			memcpy(pSubtitleData, myFrame.data, myFrame.size);
		}
	}

	// add a cue for any new cluster
	if ((pTrack == pVideoTrack) && (pLastVideoBlockBlob == NULL))
	{
		pAllCues->AddBlockBlob(*pBlockBlob);
	}

	if (pTrack == pVideoTrack)
	{
		pLastVideoBlockBlob = pBlockBlob;
	}

	lastFrameTimecode = myFrame.timecode;

	return true;
}

