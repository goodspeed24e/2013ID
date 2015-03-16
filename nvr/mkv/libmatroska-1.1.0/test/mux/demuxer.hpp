
#include <stdint.h>
#include <stdlib.h>

/*
 * Demuxer: Demux a file into separate A/V streams
 * 
 * sample:
 *   Demuxer *pDemuxer = DemuxerUtilities::CreateMKVDemuxer();
 *   pDemuxer->StartDemuxing(filePath);
 *   while (pDemuxer->GetOneFrame() != NULL)
 *     ;
 *   pDemuxer->StopDemuxing();
 */
class Demuxer
{
protected:
	Demuxer() {}

public:
	class Streams;
	class Frame;

	virtual ~Demuxer() {}

	// public methods
	virtual const Streams * StartDemuxing(const char *, uint64_t = 0ull) = 0;
	virtual bool            StopDemuxing() = 0;
	virtual const Frame   * GetOneFrame(Frame * = NULL) = 0;

public:
	class Stream;
	class VideoStream;
	class AudioStream;

	// public inner classes
	class Frame
	{
	public:
		Frame() : pStream(NULL), isKey(false), timecode(0ull), timecodeEnd(0ull), size(0ul), data(NULL) {}
		virtual ~Frame() {}
		void    FixData() { pStream->FixFrameData(*this); }

		const Stream  *pStream;
		bool           isKey;
		uint64_t       timecode;  // nanosec
		uint64_t       timecodeEnd;  // nanosec
		size_t         size;
		unsigned char *data;
	};

	class Streams
	{
	public:
		Streams() : pVideo(NULL), pAudio(NULL), pOther(NULL), dateUTC(0), duration(.0) {}
		virtual ~Streams() {}
		bool    HasVideo() const { return pVideo != NULL; }
		bool    HasAudio() const { return pAudio != NULL; }
		bool    HasOthers() const { return pOther != NULL; }

		// FIXME: memory management
		VideoStream *pVideo;
		AudioStream *pAudio;
		Stream      *pOther;
		unsigned int dateUTC;  // sec
		double       duration;  // sec

	protected:
		virtual void clear()
		{
			// reset all streams
			Stream *pStream = pVideo;
			pVideo = NULL;
			while (pStream != NULL)
			{
				Stream *pNext = pStream->pNext;
				delete pStream;
				pStream = pNext;
			}

			pStream = pAudio;
			pAudio = NULL;
			while (pStream != NULL)
			{
				Stream *pNext = pStream->pNext;
				delete pStream;
				pStream = pNext;
			}

			pStream = pOther;
			pOther = NULL;
			while (pStream != NULL)
			{
				Stream *pNext = pStream->pNext;
				delete pStream;
				pStream = pNext;
			}

			dateUTC = 0;
			duration = .0;
		}
	};

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

		Stream(CodecType type = CODEC_TYPE_DEFAULT) : pNext(NULL), codecType(type), language(NULL), codec(0) {}
		virtual      ~Stream() {}
		virtual void FixFrameData(Frame &) const {}

		Stream     *pNext;
		CodecType   codecType;
		int         trackNumber;
		const char *language;
		int         codec;
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

		VideoStream() : Stream(CODEC_TYPE_VIDEO), width(640), height(480), frameRate(30) { codec = CODEC_ID_DEFAULT; }
		virtual  ~VideoStream() {}
		uint64_t GetFrameDuration() { return 1000000000ull / frameRate; }

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

		AudioStream() : Stream(CODEC_TYPE_AUDIO), samplingRate(8000.0), channelCount(1), bitDepth(8) { codec = CODEC_ID_DEFAULT; }
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

		SubtitleStream() : Stream(CODEC_TYPE_SUBTITLE) { codec = CODEC_ID_DEFAULT; }
		virtual ~SubtitleStream() {}
	};
};

class DemuxerUtilities
{
public:
	static Demuxer * CreateMkvDemuxer();
};

