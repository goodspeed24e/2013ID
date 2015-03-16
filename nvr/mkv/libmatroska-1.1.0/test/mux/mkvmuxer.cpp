
#include <sys/time.h>
#include <time.h>
#include "mkvmuxer.hpp"

using namespace std;

// file parameters
static const char *OUTPUT_FILE_NAME   = "test1.mkv";
static const bool bWriteDefaultValues = false;

static bool isRandomSeed = false;

static inline void InitRandomSeed()
{
	if (!isRandomSeed)
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		srand(tv.tv_sec + tv.tv_usec);
		isRandomSeed = true;
	}
}

static void GenerateRandomBytes(void *buffer_, size_t size)
{
	unsigned char *buffer = (unsigned char *)buffer_;

	InitRandomSeed();

	for (size_t i = 0; i < size; i++)
	{
		buffer[i] = 256.0 * (rand() / (RAND_MAX + 1.0));
	}
}

static unsigned int GenerateRandomUInteger()
{
	unsigned int result;

	InitRandomSeed();

	result = 65536.0 * (rand() / (RAND_MAX + 1.0));
	result |= (unsigned int)(65536.0 * (rand() / (RAND_MAX + 1.0))) << 16;

	return result;
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

const MKVMuxer::Stream::Codec * const MKVMuxer::VideoStream::CODECS[] =
{
	new MKVMuxer::Stream::Codec(0, "V_UNCOMPRESSED",  NULL, 0),  // raw data
	new MKVMuxer::Stream::Codec(0, "V_UNCOMPRESSED",  NULL, 0),  // HikVision
	new MKVMuxer::Stream::Codec(2, "V_MPEG4/ISO/ASP", NULL, 0),  // MPEG4
	new MKVMuxer::Stream::Codec(3, "V_MJPEG",         NULL, 0),  // Motion JPEG
	new MKVMuxer::Stream::Codec(4, "V_MPEG4/ISO/AVC", H264_VIDEO_TRACK_DEFAULT_CODEC_PRIVATE, sizeof(H264_VIDEO_TRACK_DEFAULT_CODEC_PRIVATE) / sizeof(unsigned char)),  // H.264
	new MKVMuxer::Stream::Codec(4, "V_MPEG4/ISO/AVC", H264_VIDEO_TRACK_DEFAULT_CODEC_PRIVATE, sizeof(H264_VIDEO_TRACK_DEFAULT_CODEC_PRIVATE) / sizeof(unsigned char))  // New HikVision
};

const MKVMuxer::Stream::Codec * const MKVMuxer::AudioStream::CODECS[] =
{
	new MKVMuxer::Stream::Codec(0, "A_PCM/INT/BIG", NULL, 0),  // PCM Big Endian
	new MKVMuxer::Stream::Codec(1, "A_MS/ACM",      G711_ULAW_AUDIO_TRACK_CODEC_PRIVATE, sizeof(G711_ULAW_AUDIO_TRACK_CODEC_PRIVATE) / sizeof(unsigned char)),  // G.711 u-law
	new MKVMuxer::Stream::Codec(2, "A_PCM/INT/LIT", NULL, 0),  // PCM Little Endian
	new MKVMuxer::Stream::Codec(3, "A_MS/ACM",      ADPCM_AUDIO_TRACK_CODEC_PRIVATE,     sizeof(ADPCM_AUDIO_TRACK_CODEC_PRIVATE) / sizeof(unsigned char)),  // ADPCM
	new MKVMuxer::Stream::Codec(4, "A_AMR/NB",      NULL, 0),  // AMR
	new MKVMuxer::Stream::Codec(2, "A_PCM/INT/LIT", NULL, 0),  // MP2
	new MKVMuxer::Stream::Codec(2, "A_PCM/INT/LIT", NULL, 0),  // MP4
	new MKVMuxer::Stream::Codec(7, "A_MS/ACM",      G711_ALAW_AUDIO_TRACK_CODEC_PRIVATE, sizeof(G711_ALAW_AUDIO_TRACK_CODEC_PRIVATE) / sizeof(unsigned char))  // G.711 a-law
};

const MKVMuxer::Stream::Codec * const MKVMuxer::SubtitleStream::CODECS[] =
{
	new MKVMuxer::Stream::Codec(0, "S_TEXT/UTF8", NULL, 0),  // Message
	new MKVMuxer::Stream::Codec(1, "S_TEXT/UTF8", NULL, 0)  // OSD
};

MKVMuxer::VideoStream::~VideoStream()
{
	if (pMyCodec != NULL)
	{
		if (pMyCodec->privateData != NULL)
		{
			delete pMyCodec->privateData;
			pMyCodec->privateData = NULL;
		}
		delete pMyCodec;
		pMyCodec = NULL;
	}
}

void MKVMuxer::VideoStream::FixCodec(const Frame &frame)
{
	if (pCodec->id == CODEC_ID_H264)
	{
		if (pMyCodec == NULL)
		{
			unsigned char *pMyPrivateData = new unsigned char[Codec::MAX_PRIVATE_DATA_SIZE];
			pMyCodec = new Codec(pCodec->id, pCodec->identifier, pMyPrivateData, Codec::MAX_PRIVATE_DATA_SIZE);
		}

		const unsigned char *pSPS    = NULL;
		const unsigned char *pPPS    = NULL;
		size_t               spsSize = 0;
		size_t               ppsSize = 0;

		const unsigned char *pBuffer = frame.data;
		size_t               size    = Codec::MAX_PRIVATE_DATA_SIZE;

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

		// is it ready for sps & pps?
		if ((pSPS != NULL) && (pPPS != NULL)
		    && (spsSize > 0) && (ppsSize > 0)
		    && (6 + 2 + spsSize + 1 + 2 + ppsSize <= Codec::MAX_PRIVATE_DATA_SIZE))
		{
			// construct the private data
			unsigned char *pPrivateData = (unsigned char *)pMyCodec->privateData;

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

			// setup pCodec
			pMyCodec->privateDataSize = 6 + 2 + spsSize + 1 + 2 + ppsSize;
			pCodec = pMyCodec;
		}
		else
		{
			// use the default private data
		}
	}
}

void MKVMuxer::VideoStream::FixFrameData(Frame &frame) const
{
	if (pCodec->id == CODEC_ID_H264)
	{
		// the byte stream format --> the NAL unit stream format
		unsigned char *pBuffer        = frame.data;
		size_t         size           = frame.size;
		unsigned char *pLastStartCode = NULL;
		int            counter        = 0;

		while ((size > 3) && (++counter <= Codec::MAX_PRIVATE_DATA_SIZE))
		{
			// find start code: <00 00 00 01>
			// warning: use <00 00 01> for convenient
			if ((pBuffer[0] == 0) && (pBuffer[2] == 1) && (pBuffer[1] == 0))
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
}

MKVMuxer::MKVMuxer()
	: state(STOPPED), pListener(NULL), pStreams(NULL),
	  pMKVFile(NULL), pMKVHead(NULL), pSegment(NULL), segmentSize(0),
	  pMetaSeek(NULL), pMetaSeekDummy(NULL), pAllCues(NULL), pAllCuesDummy(NULL),
	  pVideoTrack(NULL), pAudioTrack(NULL), pSubtitleTrack(NULL)
{
}

MKVMuxer::~MKVMuxer()
{
	if (state != STOPPED)
	{
		// FIXME:
	}
}

bool MKVMuxer::SetFileConfig(const FileConfig &config)
{
	if (state != STOPPED)
	{
		// error: unable to set file configuration while muxing
		return false;
	}

	this->fileConfig = config;
	return true;
}

bool MKVMuxer::AddEventListener(const EventListener &listener)
{
	pListener = &listener;
	return true;
}

bool MKVMuxer::RemovEventListener(const EventListener &listener)
{
	pListener = NULL;
	return true;
}

bool MKVMuxer::StartMuxing(const Streams &streams, const Frame &myFrame, const char *fileName)
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
    cout << "Creating \"" << pOutFileName << "\"" << endl;

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

		pMetaSeekDummy->SetSize(fileConfig.metaSeekElementSize);
		pMetaSeekDummy->Render(*pMKVFile, bWriteDefaultValues);
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

		pStreams->pVideo->FixCodec(myFrame);

		//*static_cast<EbmlUInteger *>(&GetChild<KaxTrackMinCache>(*pVideoTrack)) = 1;
		//*static_cast<EbmlUInteger *>(&GetChild<KaxTrackDefaultDuration>(*pVideoTrack)) = pStreams->pVideo->GetFrameDuration();
		*static_cast<EbmlString *>(&GetChild<KaxTrackLanguage>(*pVideoTrack)) = pStreams->pVideo->language;

		*static_cast<EbmlString *>(&GetChild<KaxCodecID>(*pVideoTrack)) = pStreams->pVideo->pCodec->identifier;
		if (pStreams->pVideo->pCodec->privateData != NULL)
		{
			GetChild<KaxCodecPrivate>(*pVideoTrack).CopyBuffer(pStreams->pVideo->pCodec->privateData, pStreams->pVideo->pCodec->privateDataSize);
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

		//*static_cast<EbmlUInteger *>(&GetChild<KaxTrackDefaultDuration>(*pAudioTrack)) = pStreams->pAudio->GetFrameDuration();
		*static_cast<EbmlString *>(&GetChild<KaxTrackLanguage>(*pAudioTrack)) = pStreams->pAudio->language;

		*static_cast<EbmlString *>(&GetChild<KaxCodecID>(*pAudioTrack)) = pStreams->pAudio->pCodec->identifier;
		if (pStreams->pAudio->pCodec->privateData != NULL)
		{
			GetChild<KaxCodecPrivate>(*pAudioTrack).CopyBuffer(pStreams->pAudio->pCodec->privateData, pStreams->pAudio->pCodec->privateDataSize);
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

		*static_cast<EbmlString *>(&GetChild<KaxTrackLanguage>(*pSubtitleTrack)) = pStreams->pOther->language;

		*static_cast<EbmlString *>(&GetChild<KaxCodecID>(*pSubtitleTrack)) = pStreams->pOther->pCodec->identifier;

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

		pAllCuesDummy->SetSize(fileConfig.GetCueingDataElementSize());
		pAllCuesDummy->Render(*pMKVFile, bWriteDefaultValues);
	}

	{
		isFirstCluster = true;
		pCluster = NULL;
		pBlockBlob = NULL, pLastBlockBlob = NULL, pCueCandidate = NULL;
		pLastSubtitleData = NULL;
		lastSubtitleTimecode = 0ull;
		lastCueTimecode = 0.0;  // force the first cluster to be a cue
		lastFrameTimecode = 0ull;
	}

	state = STARTED;
	AppendFrame(myFrame);
	return true;
}

bool MKVMuxer::StopMuxing()
{
	if ((state == STOPPED) || (state == STOPPING))
	{
		// warning: already stopped
		return false;
	}
	state = STOPPING;

	if (pCluster != NULL)
	{
		if (pLastSubtitleData != NULL)
		{
			// add OSD : FIXME
			KaxBlockBlob *pb = new KaxBlockBlob(BLOCK_BLOB_NO_SIMPLE);
			pCluster->AddBlockBlob(pb);
			pb->SetParent(*pCluster);
			pb->AddFrameAuto(*pSubtitleTrack, lastSubtitleTimecode, *pLastSubtitleData, LACING_AUTO, NULL);
			pb->SetBlockDuration(fileConfig.GetMaxBlockDuration());
		}

		// add a cue for the last cluster
		if (pCueCandidate != NULL)
		{
			pAllCues->AddBlockBlob(*pCueCandidate);
		}

		// release the last cluster
		segmentSize += pCluster->Render(*pMKVFile, *pAllCues, bWriteDefaultValues);
		pCluster->ReleaseFrames();

		// re-calculate segDuration
		KaxDuration &segDuration = GetChild<KaxDuration>(GetChild<KaxInfo>(*pSegment));
		*static_cast<EbmlFloat *>(&segDuration) = (lastFrameTimecode - pStreams->dateUTC * 1000000000ull) / fileConfig.timecodeScale;

		// correct segDuration in the pMKVFile
		uint64 currentPosition = pMKVFile->getFilePointer();
		pMKVFile->setFilePointer(segDuration.GetElementPosition());
		segDuration.Render(*pMKVFile, false, true, true);
		pMKVFile->setFilePointer(currentPosition);
	}

	//segmentSize += pAllCues->Render(*pMKVFile, bWriteDefaultValues);
	segmentSize += pAllCuesDummy->ReplaceWith(*pAllCues, *pMKVFile, bWriteDefaultValues);
	pMetaSeek->IndexThis(*pAllCues, *pSegment);

	segmentSize += pMetaSeekDummy->ReplaceWith(*pMetaSeek, *pMKVFile, bWriteDefaultValues);

	// let's assume we know the size of the Segment element
	// the size of the pSegment is also computed because mandatory elements we don't write ourself exist
	if (pSegment->ForceSize(segmentSize - pSegment->HeadSize()))
	{
		pSegment->OverwriteHead(*pMKVFile);
	}

	pMKVFile->close();

	state = STOPPED;
	if (pListener != NULL)
	{
		// send event
		pListener->FileClosed(pOutFileName);
	}
	cout << "... ok!" << endl;
	return true;
}

bool MKVMuxer::AppendFrame(const Frame &myFrame)
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

	// use a new cluster if this is a key frame
	if ((myFrame.isKey) || (pCluster == NULL))
	{
		// release the last cluster
		if (pCluster != NULL)
		{
			segmentSize += pCluster->Render(*pMKVFile, *pAllCues, bWriteDefaultValues);
			pCluster->ReleaseFrames();
			if (isFirstCluster)
			{
				isFirstCluster = false;
				pMetaSeek->IndexThis(*pCluster, *pSegment);
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

		pLastBlockBlob = NULL;
	}

	// prepare a frame
	((Frame &)myFrame).FixData();  // WARNING: force to modify the data of myFrame
	DataBuffer *pMyData = new DataBuffer((binary *)myFrame.data, myFrame.size);

	// allocate current blob
	pBlockBlob = new KaxBlockBlob(BLOCK_BLOB_ALWAYS_SIMPLE);
	pCluster->AddBlockBlob(pBlockBlob);
	pBlockBlob->SetParent(*pCluster);

	// add a frame
	pBlockBlob->AddFrameAuto(myFrame.pStream->codecType == Stream::CODEC_TYPE_AUDIO ? *pAudioTrack : *pVideoTrack, myFrame.timecode, *pMyData, LACING_AUTO, pLastBlockBlob);

	// add a cue for any new cluster
	if (pLastBlockBlob == NULL)
	{
		if (lastCueTimecode == 0)
		{
			// add the 1st OSD : FIXME
			DataBuffer *pSubtitleData = new DataBuffer((binary *)"Camera 1", 8);

			pLastSubtitleData = pSubtitleData;
			lastSubtitleTimecode = myFrame.timecode;
		}
		else if (myFrame.timecode - lastCueTimecode >= fileConfig.GetVideoCueTimecodeThreshold())
		{
			// add OSD : FIXME
			DataBuffer *pSubtitleData = new DataBuffer((binary *)"ALERT: test 1 2 3", 17);

			KaxBlockBlob *pb = new KaxBlockBlob(BLOCK_BLOB_NO_SIMPLE);
			pCluster->AddBlockBlob(pb);
			pb->SetParent(*pCluster);
			pb->AddFrameAuto(*pSubtitleTrack, lastSubtitleTimecode, *pLastSubtitleData, LACING_AUTO, NULL);
			pb->SetBlockDuration(myFrame.timecode - lastSubtitleTimecode);

			pLastSubtitleData = pSubtitleData;
			lastSubtitleTimecode = myFrame.timecode;
		}

		if ((lastCueTimecode == 0) || (myFrame.timecode - lastCueTimecode >= fileConfig.GetVideoCueTimecodeThreshold()))
		{
			lastCueTimecode = myFrame.timecode;
			pAllCues->AddBlockBlob(*pBlockBlob);
			pCueCandidate = NULL;
		}
		else
		{
			pCueCandidate = pBlockBlob;
		}
	}

	pLastBlockBlob = pBlockBlob;
	lastFrameTimecode = myFrame.timecode;

	return true;
}

