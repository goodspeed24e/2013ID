
#ifdef _MSC_VER
#include <windows.h>      // for min/max
#endif // _MSC_VER

#include <iostream>

#include "ebml/StdIOCallback.h"

#include "ebml/EbmlHead.h"
#include "ebml/EbmlSubHead.h"
#include "ebml/EbmlVoid.h"
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

using namespace LIBMATROSKA_NAMESPACE;

/*
 * MKVMuxer: Mux A/V streams into a single MKV file
 * 
 * sample:
 *   MKVMuxer muxer;
 *   muxer.SetFileConfig(...);
 *   muxer.AddEventListner(myMKVMuxerEventListener);
 *   muxer.StartMuxing(...);
 *   while (newFrameArrived)
 *   {
 *   	muxer.AppendFrame(...);
 *   }
 *   muxer.StopMuxing();
 */
class MKVMuxer
{
public:
	class FileConfig
	{
	public:
		FileConfig() : videoCueThreshold(5.0), maxDuration(600.0), timecodeScale(1000000ull), metaSeekElementSize(300), applicationName(L"mkvmuxer") {}

		double videoCueThreshold;  // sec
		double maxDuration;  // sec
		uint64 timecodeScale;  // nanosec
		int metaSeekElementSize;
		const wchar_t *applicationName;

	private:
		friend class MKVMuxer;

		uint64 GetVideoCueTimecodeThreshold() const { return (unsigned long long)videoCueThreshold * 1000000000ull; }
		int GetCueingDataElementSize() const { return ((int)(maxDuration / (videoCueThreshold / 1000000000.0)) + 1) * 20 + 200; }
		uint64 GetMaxBlockDuration() const { return (unsigned long long)maxDuration * 1000000000ull; }
	};

	MKVMuxer();
	virtual ~MKVMuxer();
	virtual bool SetFileConfig(const FileConfig&);

public:
	// interface for event listeners
	class EventListener
	{
	public:
		virtual void FileClosed(const char *fileName) const = 0;
		virtual void SuggestSplitting() const = 0;
	};

	virtual bool AddEventListener(const EventListener&);
	virtual bool RemovEventListener(const EventListener&);

public:
	// interface for muxing A/V streams
	class Stream;
	class VideoStream;
	class AudioStream;

	class Streams
	{
	public:
		Streams() : pVideo(NULL), pAudio(NULL), pOther(NULL), dateUTC(0) {}
		virtual ~Streams() {}
		bool HasVideo() const { return pVideo != NULL; }
		bool HasAudio() const { return pAudio != NULL; }
		bool HasOthers() const { return pOther != NULL; }

		// FIXME: memory management
		VideoStream *pVideo;
		AudioStream *pAudio;
		Stream *pOther;
		unsigned int dateUTC;
	};

	class Frame
	{
	public:
		void FixData() { pStream->FixFrameData(*this); }

		const Stream *pStream;
		bool isKey;
		uint64 timecode;  // nanosec
		uint64 timecodeEnd;  // nanosec
		size_t size;
		unsigned char *data;
	};

	virtual bool StartMuxing(const Streams&, const Frame&, const char * = NULL);
	virtual bool StopMuxing();
	virtual bool AppendFrame(const Frame&);

public:
	// other public inner classes
	class Stream
	{
	public:
		enum CodecType
		{
			CODEC_TYPE_VIDEO    = 1,
			CODEC_TYPE_AUDIO    = 2,
			CODEC_TYPE_SUBTITLE = 3,
			CODEC_TYPE_DEFAULT  = CODEC_TYPE_VIDEO
		};

		Stream(CodecType type = CODEC_TYPE_DEFAULT) : pNext(NULL), codecType(type), language(NULL), pCodec(NULL) {}
		virtual ~Stream() {}
		unsigned int GetCodec() const { return pCodec->id; }
		virtual void SetCodec(unsigned int) = 0;
		virtual void FixCodec(const Frame &) {}
		virtual void FixFrameData(Frame &) const {}

		Stream *pNext;
		CodecType codecType;
		int trackNumber;
		const char *language;

	protected:
		friend class MKVMuxer;

		class Codec
		{
		public:
			enum
			{
				MAX_PRIVATE_DATA_SIZE = 120
			};

			Codec() : id(0), identifier(NULL), privateData(NULL), privateDataSize(0) {}
			Codec(unsigned char _id, const char *_identifier, const unsigned char *_privateData, size_t _privateDataSize) :id(_id), identifier(_identifier), privateData(_privateData), privateDataSize(_privateDataSize) {}

			unsigned char id;
			const char *identifier;
			const unsigned char *privateData;
			size_t privateDataSize;
		};

		const Codec *pCodec;
	};

	class VideoStream : public Stream
	{
	public:
		enum VideoCodecId
		{
			// a list of all video codec id
			CODEC_ID_RAWDATA      = 0,
			CODEC_ID_HIKVISION    = 1,
			CODEC_ID_MPEG4        = 2,
			CODEC_ID_MJPEG        = 3,
			CODEC_ID_H264         = 4,
			CODEC_ID_NEWHIKVISION = 5,
			CODEC_ID_DEFAULT      = CODEC_ID_RAWDATA,
			CODEC_ID_MAX          = CODEC_ID_NEWHIKVISION
		};

		VideoStream() : Stream(CODEC_TYPE_VIDEO), width(640), height(480), frameRate(30), pMyCodec(NULL) { SetCodec(CODEC_ID_DEFAULT); }
		virtual ~VideoStream();
		virtual void SetCodec(unsigned int id) { if (id > CODEC_ID_MAX) id = CODEC_ID_DEFAULT;  pCodec = CODECS[id]; }
		virtual void FixCodec(const Frame &);
		virtual void FixFrameData(Frame &) const;
		uint64 GetFrameDuration() { return 1000000000ull / frameRate; }

		unsigned int width;
		unsigned int height;
		unsigned int frameRate;

	protected:
		static const Codec * const CODECS[];
		Codec *pMyCodec;
	};

	class AudioStream : public Stream
	{
	public:
		enum AudioCodecId
		{
			// a list of all audio codec id
			CODEC_ID_PCM_BIG_ENDIAN    = 0,
			CODEC_ID_G711_ULAW         = 1,
			CODEC_ID_PCM_LITTLE_ENDIAN = 2,
			CODEC_ID_ADPCM             = 3,
			CODEC_ID_AMR               = 4,
			CODEC_ID_MP2               = 5,
			CODEC_ID_MP4               = 6,
			CODEC_ID_G711_ALAW         = 7,
			CODEC_ID_DEFAULT           = CODEC_ID_PCM_LITTLE_ENDIAN,
			CODEC_ID_MAX               = CODEC_ID_G711_ALAW
		};

		AudioStream() : Stream(CODEC_TYPE_AUDIO), samplingRate(8000.0), channelCount(1), bitDepth(8) { SetCodec(CODEC_ID_DEFAULT); }
		virtual ~AudioStream() {}
		virtual void SetCodec(unsigned int id) { if (id > CODEC_ID_MAX) id = CODEC_ID_DEFAULT;  pCodec = CODECS[id]; }

		double samplingRate;
		unsigned int channelCount;
		unsigned int bitDepth;

	protected:
		static const Codec * const CODECS[];
	};

	class SubtitleStream : public Stream
	{
	public:
		enum SubtitleCodecId
		{
			// a list of all subtitle codec id
			CODEC_ID_MESSAGE = 0,
			CODEC_ID_OSD     = 1,
			CODEC_ID_DEFAULT = CODEC_ID_MESSAGE,
			CODEC_ID_MAX     = CODEC_ID_OSD
		};

		SubtitleStream() : Stream(CODEC_TYPE_SUBTITLE) { SetCodec(CODEC_ID_DEFAULT); }
		virtual ~SubtitleStream() {}
		virtual void SetCodec(unsigned int id) { if (id > CODEC_ID_MAX) id = CODEC_ID_DEFAULT;  pCodec = CODECS[id]; }

	protected:
		static const Codec * const CODECS[];
	};

protected:
	enum State
	{
		STOPPED = 0,
		STOPPING,
		STARTING,
		STARTED,
		SUGGEST_SPLITTING
	};

	// protected members
	State state;
	FileConfig fileConfig;
	const char *pOutFileName;
	char myOutFileName[9];
	const EventListener *pListener;
	const Streams *pStreams;

protected:
	// protected temporary members
	IOCallback *pMKVFile;
	EbmlHead *pMKVHead;
	KaxSegment *pSegment;
	filepos_t segmentSize;

	KaxSeekHead *pMetaSeek;
	EbmlVoid *pMetaSeekDummy;
	KaxCues *pAllCues;
	EbmlVoid *pAllCuesDummy;

	KaxTrackEntry *pVideoTrack;
	KaxTrackEntry *pAudioTrack;
	KaxTrackEntry *pSubtitleTrack;

	bool isFirstCluster;
	KaxCluster *pCluster;
	KaxBlockBlob *pBlockBlob, *pLastBlockBlob, *pCueCandidate;
	DataBuffer *pLastSubtitleData;
	uint64 lastSubtitleTimecode;
	double lastCueTimecode;
	uint64 lastFrameTimecode;
};

