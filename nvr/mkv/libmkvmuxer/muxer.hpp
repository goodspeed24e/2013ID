
#ifndef MUXER_HPP
#define MUXER_HPP

#include <stdlib.h>

typedef unsigned long long uint64_t;

namespace MuxerImpl
{
	class Stream;
	class FileConfig;
};

/*
 * Muxer: Mux A/V streams into a single media file
 * 
 * sample:
 *   Muxer *muxer = Muxer::GetInstance(Muxer::CONTAINER_FORMAT_MKV);
 *   muxer->SetFileConfig(...);
 *   muxer->AddEventListner(myMuxerEventListener);
 *   muxer->StartMuxing(...);
 *   while (newFrameArrived)
 *   {
 *   	muxer->AppendFrame(...);
 *   }
 *   muxer->StopMuxing();
 */
class Muxer
{
public:
	enum ContainerFormat
	{
		// declare your container formats here
		CONTAINER_FORMAT_MKV = 0
	};

	class FileConfig
	{
	public:
		FileConfig();
		virtual ~FileConfig();

		double         videoCueThreshold;  // sec
		double         maxDuration;  // sec
		uint64_t       timecodeScale;  // nanosec
		const wchar_t *applicationName;

		MuxerImpl::FileConfig *pImpl;

	protected:
		enum
		{
			DEFAULT_VIDEO_CUE_THRESHOLD = 5,
			DEFAULT_MAX_DURATION = 600
		};
	};

	Muxer() {}
	virtual ~Muxer() {}
	virtual bool SetFileConfig(const FileConfig &) = 0;

	static Muxer * GetInstance(ContainerFormat);

public:
	// interface for event listeners
	struct FileClosedEvent
	{
		FileClosedEvent(const char *_fileName, time_t _startTime, time_t _endTime)
			: fileName(_fileName), startTime(_startTime), endTime(_endTime)
		{}
		const char *fileName;
		time_t      startTime;
		time_t      endTime;
	};

	class EventListener
	{
	public:
		virtual void FileClosed(const FileClosedEvent &) const {}
		virtual void SuggestSplitting() const {}
		virtual void SuggestFreeBuffers() const {}
	};

	virtual bool AddEventListener(const EventListener &) = 0;
	virtual bool RemoveEventListener(const EventListener &) = 0;

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
		bool    HasVideo() const { return pVideo != NULL; }
		bool    HasAudio() const { return pAudio != NULL; }
		bool    HasOthers() const { return pOther != NULL; }

		virtual void ClearAllStreams();

		VideoStream *pVideo;
		AudioStream *pAudio;
		Stream      *pOther;
		unsigned int dateUTC;
	};

	class Frame
	{
	public:
		Frame()
			: pStream(NULL), isKey(true), timecode(0ull), timecodeEnd(0ull), size(0), data(NULL),
			  needCopyBuffer(false), pFreeBuffer(NULL), pFreeBufferParam(NULL)
		{
		}
		unsigned char * FixData();

		const Stream  *pStream;
		bool           isKey;
		uint64_t       timecode;  // nanosec
		uint64_t       timecodeEnd;  // nanosec
		size_t         size;
		unsigned char *data;

		bool   needCopyBuffer;
		bool (*pFreeBuffer) (void *pParam, unsigned char *pBuffer);
		void  *pFreeBufferParam;
	};

	virtual bool StartMuxing(const Streams &, const Frame &, const char * = NULL) = 0;
	virtual bool StopMuxing() = 0;
	virtual bool AppendFrame(const Frame &) = 0;

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

		Stream(CodecType type = CODEC_TYPE_DEFAULT);
		virtual      ~Stream();
		unsigned int GetCodec() const;
		void         SetCodec(unsigned int id);

		Stream     *pNext;
		CodecType   codecType;
		int         trackNumber;
		const char *language;

		MuxerImpl::Stream *pImpl;
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

		VideoStream()
			: Stream(CODEC_TYPE_VIDEO), width(640), height(480), frameRate(30)
		{
			SetCodec(CODEC_ID_DEFAULT);
		}
		virtual  ~VideoStream() {}
		uint64_t GetFrameDuration() const { return 1000000000ull / frameRate; }

		unsigned int width;
		unsigned int height;
		unsigned int frameRate;
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

		AudioStream()
			: Stream(CODEC_TYPE_AUDIO), samplingRate(8000.0), channelCount(1), bitDepth(8)
		{
			SetCodec(CODEC_ID_DEFAULT);
		}
		virtual ~AudioStream() {}

		double       samplingRate;
		unsigned int channelCount;
		unsigned int bitDepth;
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

		SubtitleStream() : Stream(CODEC_TYPE_SUBTITLE)
		{
			SetCodec(CODEC_ID_DEFAULT);
		}
		virtual ~SubtitleStream() {}
	};

protected:
	FileConfig fileConfig;
};

#endif  // MUXER_HPP
