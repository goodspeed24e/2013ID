
#ifndef STREAMING_MEDIA_RECORDER_HPP
#define STREAMING_MEDIA_RECORDER_HPP

#include <stdlib.h>

typedef unsigned long long uint64_t;

class StreamingMediaRecorder
{
public:
	enum FrameType
	{
		FRAME_TYPE_UNKNOWN = 0,
		FRAME_TYPE_AUDIO   = 1,
		FRAME_TYPE_VIDEO   = 2,
		FRAME_TYPE_OSD     = 3,
		FRAME_TYPE_DEFAULT = FRAME_TYPE_UNKNOWN
	};

	enum VideoCodecId
	{
		VIDEO_CODEC_ID_NONE         = -1,
		VIDEO_CODEC_ID_RAWDATA      =  0,
		VIDEO_CODEC_ID_HIKVISION    =  1,
		VIDEO_CODEC_ID_MPEG4        =  2,
		VIDEO_CODEC_ID_MJPEG        =  3,
		VIDEO_CODEC_ID_H264         =  4,
		VIDEO_CODEC_ID_NEWHIKVISION =  5,
		VIDEO_CODEC_ID_DEFAULT      = VIDEO_CODEC_ID_NONE,
		VIDEO_CODEC_ID_MAX          = VIDEO_CODEC_ID_NEWHIKVISION
	};

	enum AudioCodecId
	{
		AUDIO_CODEC_ID_NONE              = -1,
		AUDIO_CODEC_ID_PCM_BIG_ENDIAN    =  0,
		AUDIO_CODEC_ID_G711_ULAW         =  1,
		AUDIO_CODEC_ID_PCM_LITTLE_ENDIAN =  2,
		AUDIO_CODEC_ID_ADPCM             =  3,
		AUDIO_CODEC_ID_AMR               =  4,
		AUDIO_CODEC_ID_MP2               =  5,
		AUDIO_CODEC_ID_MP4               =  6,
		AUDIO_CODEC_ID_G711_ALAW         =  7,
		AUDIO_CODEC_ID_DEFAULT           = AUDIO_CODEC_ID_NONE,
		AUDIO_CODEC_ID_MAX               = AUDIO_CODEC_ID_G711_ALAW
	};

	enum PlaybackDirection
	{
		PLAYBACK_DIRECTION_FORWARD  = 0,
		PLAYBACK_DIRECTION_BACKWARD = 1,
		PLAYBACK_DIRECTION_DEFAULT  = PLAYBACK_DIRECTION_FORWARD
	};

	enum PlaybackSpeed
	{
		PLAYBACK_SPEED_0X      =  0,
		PLAYBACK_SPEED_1X      =  1,
		PLAYBACK_SPEED_2X      =  2,
		PLAYBACK_SPEED_4X      =  4,
		PLAYBACK_SPEED_8X      =  8,
		PLAYBACK_SPEED_16X     = 16,
		PLAYBACK_SPEED_32X     = 32,
		PLAYBACK_SPEED_NORMAL  = PLAYBACK_SPEED_1X,
		PLAYBACK_SPEED_DEFAULT = PLAYBACK_SPEED_NORMAL
	};

	class Frame
	{
	public:
		Frame(FrameType _type = FRAME_TYPE_DEFAULT, uint64_t _timecode = 0ull, size_t _size = 0ul, unsigned char *_data = NULL)
			: type(_type), isKey(false), timecode(_timecode), size(_size), data(_data),
		      needCopyBuffer(false), pFreeBuffer(NULL), pFreeBufferParam(NULL)
		{
		}

		FrameType      type;  // audio or video?
		bool           isKey;  // is this a key frame?
		uint64_t       timecode;  // nanosec
		size_t         size;
		unsigned char *data;

		bool           needCopyBuffer;
		bool         (*pFreeBuffer) (void *pParam, unsigned char *pBuffer);
		void          *pFreeBufferParam;
	};

	class RecordingConfig
	{
	public:
		RecordingConfig(VideoCodecId _videoCodec = VIDEO_CODEC_ID_DEFAULT, AudioCodecId _audioCodec = AUDIO_CODEC_ID_DEFAULT)
			: videoCodec(_videoCodec), audioCodec(_audioCodec), extraSize(0ul), extraData(NULL),
		      needCopyBuffer(false), pFreeBuffer(NULL), pFreeBufferParam(NULL)
		{
		}

		VideoCodecId   videoCodec;
		AudioCodecId   audioCodec;
		size_t         extraSize;
		unsigned char *extraData;  // for H.264 sps & pps

		bool           needCopyBuffer;
		bool         (*pFreeBuffer) (void *pParam, unsigned char *pBuffer);
		void          *pFreeBufferParam;
	};

	class StreamingRequest
	{
	public:
		StreamingRequest(time_t _seek, PlaybackDirection _direction = PLAYBACK_DIRECTION_DEFAULT, PlaybackSpeed _speed = PLAYBACK_SPEED_DEFAULT)
			: seek(_seek), direction(_direction), speed(_speed)
		{
		}

		time_t            seek;
		PlaybackDirection direction;
		PlaybackSpeed     speed;
	};

	// recording APIs
	static StreamingMediaRecorder * GetStreamingMediaRecorder();
	virtual bool StartRecording(int chId, const RecordingConfig &) { return false; }
	virtual bool StopRecording(int chId)                           { return true; }
	virtual bool StopAllChannels()                                 { return true; }
	virtual bool AppendFrame(int chId, const Frame &)              { return false; }

	// streaming APIs
	static StreamingMediaRecorder * GetReadOnlyStreamingMediaRecorder();
	virtual int           StartStreaming(int chId, const StreamingRequest &)            { return -1; }  // return reqId
	virtual bool          ResetStreaming(int reqId, int chId, const StreamingRequest &) { return false; }  // for seeking
	virtual bool          StopStreaming(int reqId)                                      { return true; }
	virtual const Frame * GetOneFrame(int reqId)                                        { return NULL; }

	virtual ~StreamingMediaRecorder() {}

protected:
	StreamingMediaRecorder() {}
};

#endif  // STREAMING_MEDIA_RECORDER_HPP
