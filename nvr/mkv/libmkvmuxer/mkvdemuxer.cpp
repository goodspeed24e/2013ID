
#include "ebml/StdIOCallback.h"

#include "ebml/EbmlHead.h"
#include "ebml/EbmlSubHead.h"
#include "ebml/EbmlVoid.h"
#include "ebml/EbmlStream.h"
#include "matroska/FileKax.h"
#include "matroska/KaxSegment.h"
#include "matroska/KaxTracks.h"
#include "matroska/KaxTrackEntryData.h"
#include "matroska/KaxTrackAudio.h"
#include "matroska/KaxTrackVideo.h"
#include "matroska/KaxCluster.h"
#include "matroska/KaxClusterData.h"
#include "matroska/KaxSeekHead.h"
#include "matroska/KaxCues.h"
#include "matroska/KaxInfo.h"
#include "matroska/KaxInfoData.h"
#include "matroska/KaxTags.h"
#include "matroska/KaxTag.h"
#include "matroska/KaxChapters.h"
#include "matroska/KaxContentEncoding.h"
#include "matroska/KaxVersion.h"

#include "demuxer.hpp"

using namespace LIBMATROSKA_NAMESPACE;

#define DEBUG_LEVEL 0
//#define DEBUG_LEVEL 2

#ifdef WIN32
#if (DEBUG_LEVEL > 0)
	#define MESSAGE printf
	#define CLUSTER_MESSAGE (void)
#elif (DEBUG_LEVEL > 1)
	#define MESSAGE printf
	#define CLUSTER_MESSAGE MESSAGE
#else
	#define MESSAGE (void)
	#define CLUSTER_MESSAGE (void)
#endif
#else
#if (DEBUG_LEVEL > 0)
	#define MESSAGE(arg...) \
		do \
		{ \
			printf(arg); \
			fflush(stdout); \
		} while (false)
#else
	#define MESSAGE(arg...) do {} while (false)
#endif

#if (DEBUG_LEVEL > 1)
	#define CLUSTER_MESSAGE(arg...) MESSAGE(arg)
#else
	#define CLUSTER_MESSAGE(arg...) do {} while (false)
#endif
#endif

#define PARSING_LOOP(upper, current, lower, ebmlStream, upperLevel) \
	do \
	{ \
		(current) = (ebmlStream)->FindNextElement((upper)->Generic().Context, (upperLevel), 0xFFFFFFFFL, false); \
		while ((current) != NULL) \
		{ \
			if ((upperLevel) > 0) \
			{ \
				break; \
			} \
			if ((upperLevel) < 0) \
			{ \
				(upperLevel) = 0; \
			}

#define END_LOOP(upper, current, lower, ebmlStream, upperLevel) \
			if ((upperLevel) > 0) \
			{ \
				(upperLevel)--; \
				delete (current); \
				(current) = (lower); \
				if ((upperLevel) > 0) \
					break; \
			} \
			else \
			{ \
				(current)->SkipData(*(ebmlStream), (current)->Generic().Context); \
				delete (current); \
				(current) = (ebmlStream)->FindNextElement((upper)->Generic().Context, (upperLevel), 0xFFFFFFFFL, false); \
			} \
		} \
	} while (false)

#define END_LOOP_AND_JUMP(jumpPosition, upper, current, lower, ebmlStream, upperLevel) \
			if ((upperLevel) > 0) \
			{ \
				(upperLevel)--; \
				delete (current); \
				(current) = (lower); \
				if ((upperLevel) > 0) \
					break; \
			} \
			else \
			{ \
				(current)->SkipData(*(ebmlStream), (current)->Generic().Context); \
				delete (current); \
				if ((jumpPosition) != 0ull) \
				{ \
					((IOCallback &)*ebmlStream).setFilePointer(jumpPosition); \
					(jumpPosition) = 0ull; \
				} \
				(current) = (ebmlStream)->FindNextElement((upper)->Generic().Context, (upperLevel), 0xFFFFFFFFL, false); \
			} \
		} \
	} while (false)

#define CHECK_TYPE(current, type) \
	(EbmlId(*(current)) == (type::ClassInfos).GlobalId)

#define READ_DATA(target, ebmlStream) \
	((target)->ReadData((ebmlStream)->I_O()))

#define FILL_ELEMENT(target, type, lower, ebmlStream, upperLevel) \
	((target)->Read(*(ebmlStream), (type::ClassInfos).Context, (upperLevel), (lower), false))

// must call FILL_ELEMENT(...) at the first
#define FIND_ELEMENT(current, type) \
	(static_cast<type *>((current)->FindElt(type::ClassInfos)))

class MkvDemuxer : public Demuxer
{
public:
	MkvDemuxer();
	virtual ~MkvDemuxer();

	virtual const Streams * StartDemuxing(const char *, uint64_t = 0ull);
	virtual bool            StopDemuxing();
	virtual const Frame   * GetOneFrame(Frame * = NULL);

	static int TranslateCodecIdentifier(const char *, const Stream * = NULL);
	static void FixCodecIdentifier(Stream *);

protected:
	enum
	{
		MAX_PRIVATE_DATA_SIZE = 120
	};

	class MyStreams : public Streams
	{
	public:
		MyStreams() : timecodeScale(1000000ull) {}
		virtual void Clear()
		{
			Streams::Clear();
			timecodeScale = 1000000ull;
		}

		uint64_t timecodeScale;  // nanosec
	};

	class MyVideoStream : public VideoStream
	{
	public:
		virtual void FixFrameData(Frame &) const;
	};

protected:
	enum State
	{
		STOPPED = 0,
		STOPPING,
		STARTING,
		STARTED
	};

	// protected members
	State     state;
	MyStreams streams;
	Frame     myFrame;

protected:
	// protected temporary members
	void ResetAllMembers();

	IOCallback *pMKVFile;
	EbmlStream *pRawdata;
	KaxSegment *pSegment;

	//KaxSeekHead *pMetaSeek;
	//KaxCues *pAllCues;

	int             relativeUpperLevel;
	EbmlElement    *pElementLevel1;
	EbmlElement    *pElementLevel2;
	uint64          clusterPosition;

	KaxCluster     *pCluster;
	unsigned int    clusterIndex;
	KaxSimpleBlock *pSimpleBlock;
	KaxBlockGroup  *pBlockGroup;
	KaxBlock       *pBlock;
};

Demuxer * DemuxerUtilities::CreateMkvDemuxer()
{
	return new MkvDemuxer();
}

#if 1
static int codecIdHashTable[] =
{
	// audio
	Demuxer::AudioStream::CODEC_ID_PCM_LITTLE_ENDIAN,
	Demuxer::AudioStream::CODEC_ID_G711_ULAW,
	Demuxer::AudioStream::CODEC_ID_AMR,
	Demuxer::AudioStream::CODEC_ID_PCM_BIG_ENDIAN,

	// video
	Demuxer::VideoStream::CODEC_ID_MPEG4,
	Demuxer::VideoStream::CODEC_ID_H264,
	Demuxer::VideoStream::CODEC_ID_RAWDATA,
	Demuxer::VideoStream::CODEC_ID_MJPEG,

	// others
	Demuxer::SubtitleStream::CODEC_ID_OSD
};

int MkvDemuxer::TranslateCodecIdentifier(const char *codecIdentifier, const Stream *pStream)
{
	int len;
	if ((codecIdentifier != NULL) && ((len = strnlen(codecIdentifier, 32)) > 0))
	{
		int codecType = (codecIdentifier[0] & 3) - 1;
		int hashKey = (codecType << 2) + ((codecIdentifier[len-1] >> codecType) & 3);
		int maxKey = (sizeof(codecIdHashTable) / sizeof(int)) - 1;
		if ((hashKey < 0) || (hashKey > maxKey))
		{
			hashKey = maxKey;
		}
		return codecIdHashTable[hashKey];
	}
	else
	{
		return 0;
	}
}

#else

int MkvDemuxer::TranslateCodecIdentifier(const char *codecIdentifier, const Stream *pStream)
{
	int codecType = pStream != NULL ? pStream->codecType : Stream::CODEC_TYPE_AUDIO;

	switch (codecType)
	{
	default:
		// audio codec identifiers
		if (strcmp(codecIdentifier, "A_PCM/INT/BIG") == 0)
		{
			return AudioStream::CODEC_ID_PCM_BIG_ENDIAN;
		}
		else if (strcmp(codecIdentifier, "A_MS/ACM") == 0)
		{
			return AudioStream::CODEC_ID_G711_ULAW;
		}
		else if (strcmp(codecIdentifier, "A_PCM/INT/LIT") == 0)
		{
			return AudioStream::CODEC_ID_PCM_LITTLE_ENDIAN;
		}
		else if (strcmp(codecIdentifier, "A_AMR/NB") == 0)
		{
			return AudioStream::CODEC_ID_AMR;
		}

		if (pStream != NULL)
		{
			return AudioStream::CODEC_ID_DEFAULT;
		}
		// do NOT break

	case Stream::CODEC_TYPE_VIDEO:
		// video codec identifiers
		if (strcmp(codecIdentifier, "V_MPEG4/ISO/AVC") == 0)
		{
			return VideoStream::CODEC_ID_H264;
		}
		else if (strcmp(codecIdentifier, "V_MJPEG") == 0)
		{
			return VideoStream::CODEC_ID_MJPEG;
		}
		else if (strcmp(codecIdentifier, "V_MPEG4/ISO/ASP") == 0)
		{
			return VideoStream::CODEC_ID_MPEG4;
		}
		else if (strcmp(codecIdentifier, "V_UNCOMPRESSED") == 0)
		{
			return VideoStream::CODEC_ID_RAWDATA;
		}

		if (pStream != NULL)
		{
			return VideoStream::CODEC_ID_DEFAULT;
		}
		// do NOT break

	case Stream::CODEC_TYPE_SUBTITLE:
		// subtitle codec identifier
		if (strcmp(codecIdentifier, "S_TEXT/UTF8") == 0)
		{
			return SubtitleStream::CODEC_ID_OSD;
		}

		if (pStream != NULL)
		{
			return SubtitleStream::CODEC_ID_OSD;
		}
		// do NOT break
	}

	return 0;
}
#endif

inline void MkvDemuxer::FixCodecIdentifier(Stream *pStream)
{
	if ((pStream != NULL)
	    && (pStream->codecType == Stream::CODEC_TYPE_AUDIO)
	    && (pStream->codec == AudioStream::CODEC_ID_G711_ULAW)
	    && (pStream->pCodecPrivate != NULL)
	    && (pStream->codecPrivateSize > 1))
	{
		if (pStream->pCodecPrivate[0] == 0x06)
		{
			pStream->codec = AudioStream::CODEC_ID_G711_ALAW;
		}
		else if (pStream->pCodecPrivate[0] == 0x45)
		{
			pStream->codec = AudioStream::CODEC_ID_ADPCM;
		}
		else
		{
			// no change
		}
	}
}

void MkvDemuxer::MyVideoStream::FixFrameData(Frame &frame) const
{
	if ((frame.pStream != NULL)
	    && (frame.pStream->codecType == Stream::CODEC_TYPE_VIDEO)
	    && (frame.pStream->codec == VideoStream::CODEC_ID_H264))
	{
		unsigned char *ptr = frame.data;
		int count = 1;
		size_t size;

		// get size
		size = (ptr[0] << 16)
		       + (ptr[1] <<  8)
		       + (ptr[2]      );

		// fill 00 00 01
		ptr[0] = 0;
		ptr[1] = 0;
		ptr[2] = 1;

		// increase ptr
		ptr += size + 3;

		while ((++count <= 3)
		       && ((signed int)size > 0)
		       && (ptr > frame.data)
		       && (ptr - frame.data <= MAX_PRIVATE_DATA_SIZE)
		       && (ptr + 4 < frame.data + frame.size))
		{
			// get size
			size = (ptr[0] << 24)
			       + (ptr[1] << 16)
			       + (ptr[2] <<  8)
			       + (ptr[3]      );

			// fill 00 00 00 01
			ptr[0] = 0;
			ptr[1] = 0;
			ptr[2] = 0;
			ptr[3] = 1;

			// increase ptr
			ptr += size + 4;
		}
	}
}

MkvDemuxer::MkvDemuxer()
	: state(STOPPED)
{
	ResetAllMembers();
}

MkvDemuxer::~MkvDemuxer()
{
	if (state != STOPPED)
	{
		StopDemuxing();
	}
}

void MkvDemuxer::ResetAllMembers()
{
	pMKVFile = NULL;
	pRawdata = NULL;
	pSegment = NULL;

	relativeUpperLevel = 0;
	pElementLevel1     = NULL;
	pElementLevel2     = NULL;
	clusterPosition    = 0ull;

	pCluster     = NULL;
	pSimpleBlock = NULL;
	pBlockGroup  = NULL;
	pBlock       = NULL;
}

bool MkvDemuxer::StopDemuxing()
{
	if ((state == STOPPED) || (state == STOPPING))
	{
		// warning: already stopped
		return false;
	}
	state = STOPPING;

	// close file
	if (pMKVFile != NULL)
	{
		delete pMKVFile;
		pMKVFile = NULL;
	}

	if (pRawdata != NULL)
	{
		delete pRawdata;
		pRawdata = NULL;
	}

	if (pSegment != NULL)
	{
		delete pSegment;
		pSegment = NULL;
	}

	if (pCluster != NULL)
	{
		delete pCluster;
		pCluster = NULL;
	}

	ResetAllMembers();
	state = STOPPED;
	return true;
}

const Demuxer::Streams * MkvDemuxer::StartDemuxing(const char *pFileName, uint64_t seekTime)
{
	if (state != STOPPED)
	{
		// error: already started
		return NULL;
	}
	state = STARTING;

	if (pFileName == NULL)
	{
		// error: invalid parameter
		state = STOPPED;
		return NULL;
	}

	// open the file
	{
		if (pMKVFile != NULL)
		{
			delete pMKVFile;
			pMKVFile = NULL;
		}
		pMKVFile = new StdIOCallback(pFileName, MODE_READ);

		if (pRawdata != NULL)
		{
			delete pRawdata;
			pRawdata = NULL;
		}
		pRawdata = new EbmlStream(*pMKVFile);

		streams.Clear();
	}

	// skip the EBML head
	{
		EbmlElement *pElement = pRawdata->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
		if (pElement != NULL)
		{
			pElement->SkipData(*pRawdata, pElement->Generic().Context);
			delete pElement;
		}
	}

	// find the segment
	{
		if (pSegment != NULL)
		{
			delete pSegment;
			pSegment = NULL;
		}

		EbmlElement *pElement = pRawdata->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFL);
		if ((pElement == NULL) || (!CHECK_TYPE(pElement, KaxSegment)))
		{
			// error: invalid contents of the file
			if (pElement != NULL)
			{
				delete pElement;
			}
			state = STOPPED;
			return NULL;
		}

		pSegment = static_cast<KaxSegment*>(pElement);
	}

	// parse the EBML stream:
	//   find all meta-seek information
	//   find segment info for TimecodeScale & Duration & DateUTC
	//   find all tracks for streams information
	//   find cueing data for the specific seek time
	{
		relativeUpperLevel = 0;
		pElementLevel1     = NULL;
		pElementLevel2     = NULL;
		clusterPosition    = 0ull;

		EbmlElement *pElementLevel3 = NULL;
		EbmlElement *pElementLevel4 = NULL;

		pCluster     = NULL;
		pSimpleBlock = NULL;
		pBlockGroup  = NULL;
		pBlock       = NULL;

		PARSING_LOOP (pSegment, pElementLevel1, pElementLevel2, pRawdata, relativeUpperLevel)
		{
			// FIXME: efficiency
			if (CHECK_TYPE(pElementLevel1, KaxCluster))
			{
				MESSAGE("\n- Segment Clusters found\n");
				pCluster = static_cast<KaxCluster *>(pElementLevel1);

				// stop parsing
				// seek cluster

				state = STARTED;
				return &streams;
			}
			else if (CHECK_TYPE(pElementLevel1, KaxSeekHead))
			{
				// find all meta-seek information
				MESSAGE("\n- Meta Seek found\n");
			}
			else if (CHECK_TYPE(pElementLevel1, KaxInfo))
			{
				// find segment info for TimecodeScale & Duration & DateUTC
				MESSAGE("\n- Segment Informations found\n");
				PARSING_LOOP (pElementLevel1, pElementLevel2, pElementLevel3, pRawdata, relativeUpperLevel)
				{
					if (CHECK_TYPE(pElementLevel2, KaxTimecodeScale))
					{
						KaxTimecodeScale *pTimecodeScale = static_cast<KaxTimecodeScale*>(pElementLevel2);
						READ_DATA(pTimecodeScale, pRawdata);
						streams.timecodeScale = (uint64_t)*pTimecodeScale;
						MESSAGE("\tTimecode Scale: %lld\n", streams.timecodeScale);
					}
					else if (CHECK_TYPE(pElementLevel2, KaxDuration))
					{
						KaxDuration *pDuration = static_cast<KaxDuration*>(pElementLevel2);
						READ_DATA(pDuration, pRawdata);
						streams.duration = (double)*pDuration * streams.timecodeScale / 1000000000;
						MESSAGE("\tDuration: %g\n", streams.duration);
					}
					else if (CHECK_TYPE(pElementLevel2, KaxDateUTC))
					{
						KaxDateUTC *pDateUTC = static_cast<KaxDateUTC*>(pElementLevel2);
						READ_DATA(pDateUTC, pRawdata);
						streams.dateUTC = pDateUTC->GetEpochDate();
						MESSAGE("\tDate UTC: 0x%08x\n", streams.dateUTC);
					}
					else
					{
						// do nothing
					}
				} END_LOOP (pElementLevel1, pElementLevel2, pElementLevel3, pRawdata, relativeUpperLevel);
			}
			else if (CHECK_TYPE(pElementLevel1, KaxTracks))
			{
				// find all tracks for streams information
				MESSAGE("\n- Segment Tracks found\n");
				PARSING_LOOP (pElementLevel1, pElementLevel2, pElementLevel3, pRawdata, relativeUpperLevel)
				{
					if (CHECK_TYPE(pElementLevel2, KaxTrackEntry))
					{
						MESSAGE("\n    Found a track\n");
						unsigned int   trackNumber = 0;
						Stream        *pTrackStream = NULL;
						const char    *pTrackLanguage = NULL;
						int            trackCodecId = 0;
						unsigned char *pTrackCodecPrivate = NULL;
						size_t         trackCodecPrivateSize = 0;

						PARSING_LOOP (pElementLevel2, pElementLevel3, pElementLevel4, pRawdata, relativeUpperLevel)
						{
							if (CHECK_TYPE(pElementLevel3, KaxTrackNumber))
							{
								KaxTrackNumber *pNumber = static_cast<KaxTrackNumber*>(pElementLevel3);
								READ_DATA(pNumber, pRawdata);
								trackNumber = (unsigned int)*pNumber;
								MESSAGE("\tId: %d\n", trackNumber);
							}
							else if (CHECK_TYPE(pElementLevel3, KaxTrackType))
							{
								KaxTrackType *pType = static_cast<KaxTrackType*>(pElementLevel3);
								READ_DATA(pType, pRawdata);
								MESSAGE("\tType: ");
								switch ((unsigned char)*pType)
								{
								case track_video:
									MESSAGE("video");
									streams.pVideo = new MyVideoStream();
									pTrackStream = streams.pVideo;
									break;

								case track_audio:
									MESSAGE("audio");
									streams.pAudio = new AudioStream();
									pTrackStream = streams.pAudio;
									break;

								case track_subtitle:
									MESSAGE("subtitle");
									streams.pOther = new SubtitleStream();
									pTrackStream = streams.pOther;
									break;

								default:
									MESSAGE("unknown");
									streams.pOther = new Stream();
									pTrackStream = streams.pOther;
									break;
								}
								MESSAGE("\n");
							}
							else if (CHECK_TYPE(pElementLevel3, KaxTrackLanguage))
							{
								KaxTrackLanguage *pLanguage = static_cast<KaxTrackLanguage*>(pElementLevel3);
								READ_DATA(pLanguage, pRawdata);
								pTrackLanguage = ((const std::string &)*pLanguage).c_str();
								MESSAGE("\tLanguage: %s\n", pTrackLanguage);
							}
							else if (CHECK_TYPE(pElementLevel3, KaxCodecID))
							{
								KaxCodecID *pCodecID = static_cast<KaxCodecID*>(pElementLevel3);
								READ_DATA(pCodecID, pRawdata);
								const char *pTrackCodecIdentifier = ((const std::string &)*pCodecID).c_str();
								trackCodecId = TranslateCodecIdentifier(pTrackCodecIdentifier);
								MESSAGE("\tCodec: (%s), size=%d\n", pTrackCodecIdentifier, strlen(pTrackCodecIdentifier));
							}
							else if (CHECK_TYPE(pElementLevel3, KaxCodecPrivate))
							{
								KaxCodecPrivate *pCodecPrivate = static_cast<KaxCodecPrivate*>(pElementLevel3);
								READ_DATA(pCodecPrivate, pRawdata);
								trackCodecPrivateSize = pCodecPrivate->GetSize();
								pTrackCodecPrivate = new unsigned char[trackCodecPrivateSize];
								memcpy(pTrackCodecPrivate, pCodecPrivate->GetBuffer(), trackCodecPrivateSize);
								MESSAGE("\tCodec Private: size=%d\n", trackCodecPrivateSize);
							}
							else
							{
								// WARNING: skip KaxContentEncodings, KaxTrackVideo, KaxTrackAudio
								// do nothing
							}
						} END_LOOP (pElementLevel2, pElementLevel3, pElementLevel4, pRawdata, relativeUpperLevel);

						pTrackStream->trackNumber      = trackNumber;
						pTrackStream->language         = pTrackLanguage;
						pTrackStream->codec            = trackCodecId;
						pTrackStream->pCodecPrivate    = pTrackCodecPrivate;
						pTrackStream->codecPrivateSize = trackCodecPrivateSize;
						FixCodecIdentifier(pTrackStream);
					}
					else
					{
						// do nothing
					}
				} END_LOOP (pElementLevel1, pElementLevel2, pElementLevel3, pRawdata, relativeUpperLevel);
			}
			else if (CHECK_TYPE(pElementLevel1, KaxCues))
			{
				// find cueing data for the specific seek time
				MESSAGE("\n- Cue entries found\n");
				KaxCues *pCues = static_cast<KaxCues *>(pElementLevel1);
				FILL_ELEMENT(pCues, KaxCues, pElementLevel2, pRawdata, relativeUpperLevel);
				pCues->SetGlobalTimecodeScale(streams.timecodeScale);

				if ((seekTime != 0ull) && (clusterPosition = pCues->GetTimecodePosition(seekTime)) != 0)
				{
					clusterPosition += pSegment->GetElementPosition() + pSegment->HeadSize();
				}
			}
			else if (CHECK_TYPE(pElementLevel1, KaxChapters))
			{
				MESSAGE("\n- Chapters found\n");
			}
			else if (CHECK_TYPE(pElementLevel1, KaxTags))
			{
				MESSAGE("\n- Tags found\n");
			}
			else
			{
				MESSAGE("\n- Void or Unknown found\n");
			}
		} END_LOOP_AND_JUMP (clusterPosition, pSegment, pElementLevel1, pElementLevel2, pRawdata, relativeUpperLevel);
	}

	// error: no cluster found
	StopDemuxing();
	return NULL;
}

const Demuxer::Frame * MkvDemuxer::GetOneFrame(Demuxer::Frame *pFrame)
{
	if (state < STARTED)
	{
		// error: not ready to get any frame
		return NULL;
	}

	if (pFrame == NULL)
	{
		pFrame = &myFrame;
	}

	if ((pCluster == NULL) || (pRawdata == NULL) || (pSegment == NULL) || (pElementLevel1 == NULL))
	{
		// error: something wrong
		goto END_OF_FILE;
	}

	// do something
	if ((pSimpleBlock != NULL) || (pBlockGroup != NULL))
	{
		goto END_OF_LEVEL2;
	}
	else
	{
		goto BEGIN_OF_CLUSTER_LOOP;
	}

	PARSING_LOOP (pSegment, pElementLevel1, pElementLevel2, pRawdata, relativeUpperLevel)
	{
		if (CHECK_TYPE(pElementLevel1, KaxCluster))
		{
			MESSAGE("\n- Segment Clusters found\n");
			pCluster = static_cast<KaxCluster *>(pElementLevel1);

BEGIN_OF_CLUSTER_LOOP:
			FILL_ELEMENT(pCluster, KaxCluster, pElementLevel2, pRawdata, relativeUpperLevel);

			if (CHECK_TYPE((*pCluster)[0], KaxClusterTimecode))
			{
				{
					uint64_t clusterTimecode = (uint64_t)*static_cast<KaxClusterTimecode *>((*pCluster)[0]);
					pCluster->InitTimecode(clusterTimecode, streams.timecodeScale);
					MESSAGE("\tcluster timecode: %llu.%llu\n", clusterTimecode*streams.timecodeScale/1000000000ull, clusterTimecode*streams.timecodeScale/1000000ull%1000ull);
				}

				for (clusterIndex = 1; clusterIndex < pCluster->ListSize(); clusterIndex++)
				{
					if (CHECK_TYPE((*pCluster)[clusterIndex], KaxSimpleBlock))
					{
						pSimpleBlock = static_cast<KaxSimpleBlock *>((*pCluster)[clusterIndex]);
						pSimpleBlock->SetParent(*pCluster);
						pFrame->pStream  = streams.HasAudio() && (pSimpleBlock->TrackNum() == streams.pAudio->trackNumber) ? streams.pAudio
						                   : streams.HasVideo() && (pSimpleBlock->TrackNum() == streams.pVideo->trackNumber) ? streams.pVideo
										   : streams.HasOthers() && (pSimpleBlock->TrackNum() == streams.pOther->trackNumber) ? streams.pOther
										   : NULL;
						if (pFrame->pStream == NULL)
						{
							// error track cluster: skip it
							return NULL;
						}
						pFrame->isKey    = pSimpleBlock->IsKeyframe();
						pFrame->timecode = pSimpleBlock->GlobalTimecode();
						pFrame->size     = pSimpleBlock->GetBuffer(0).Size();
						pFrame->data     = pSimpleBlock->GetBuffer(0).Buffer();
						pFrame->FixData();
						CLUSTER_MESSAGE("\tsimple block: key=%d, size=%d, timecode=%llu.%llu\n", pFrame->isKey, pFrame->size, pFrame->timecode/1000000000ull, pFrame->timecode/1000000ull%1000ull);
						return pFrame;
					}
					else if (CHECK_TYPE((*pCluster)[clusterIndex], KaxBlockGroup))
					{
						pBlockGroup = static_cast<KaxBlockGroup *>((*pCluster)[clusterIndex]);
						pBlock      = FIND_ELEMENT(pBlockGroup, KaxBlock);
						pBlockGroup->SetParent(*pCluster);
						pFrame->pStream  = streams.HasAudio() && (pBlock->TrackNum() == streams.pAudio->trackNumber) ? streams.pAudio
						                   : streams.HasVideo() && (pBlock->TrackNum() == streams.pVideo->trackNumber) ? streams.pVideo
										   : streams.HasOthers() && (pBlock->TrackNum() == streams.pOther->trackNumber) ? streams.pOther
										   : NULL;
						if (pFrame->pStream == NULL)
						{
							// error track cluster: skip it
							return NULL;
						}
						pFrame->isKey    = FIND_ELEMENT(pBlockGroup, KaxBlock) == NULL;
						pFrame->timecode = pBlock->GlobalTimecode();
						pFrame->size     = pBlock->GetBuffer(0).Size();
						pFrame->data     = pBlock->GetBuffer(0).Buffer();
						pFrame->FixData();
						CLUSTER_MESSAGE("\tblock: key=%d, size=%d, timecode=%llu.%llu\n", pFrame->isKey, pFrame->size, pFrame->timecode/1000000000ull, pFrame->timecode/1000000ull%1000ull);
						return pFrame;
					}
					else
					{
						CLUSTER_MESSAGE("\tother element\n");
					}

END_OF_LEVEL2:
					pSimpleBlock = NULL;
					pBlockGroup  = NULL;
					pBlock       = NULL;
				}
			}
			else
			{
				// error mkv format: there must be a KaxClusterTimecode first
			}
		}
		else
		{
			CLUSTER_MESSAGE("- %06x\t", EbmlId(*pElementLevel1).Value);
		}
		pCluster = NULL;
	} END_LOOP (pSegment, pElementLevel1, pElementLevel2, pRawdata, relativeUpperLevel);

END_OF_FILE:
	StopDemuxing();
	return NULL;
}
