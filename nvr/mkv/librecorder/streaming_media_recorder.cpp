
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <muxer.hpp>
#include "streaming_media_recorder.hpp"
#include "streaming_media_library.hpp"

class Recorder
{
public:
	Recorder(StreamingMediaChannelHelper &);
	~Recorder();

	bool StartRecording(const StreamingMediaRecorder::RecordingConfig &);
	bool StopRecording();
	bool AppendFrame(const StreamingMediaRecorder::Frame &);

protected:
	void RestartRecording();
	void AppendOsdFrameIfExist();
	void BackupExtraBuffer(const StreamingMediaRecorder::RecordingConfig &);
	void BackupOsdBuffer(const StreamingMediaRecorder::Frame &);
	const char * GetFileName() { return fileHelper.AllocateRecordingFile(pMuxerStreams->dateUTC).GetFileName(); }

private:
	friend class MyEventListener;
	class MyEventListener : public Muxer::EventListener
	{
	public:
		MyEventListener(Recorder *_pRecorder) : pRecorder(_pRecorder) {}
		virtual void FileClosed(const Muxer::FileClosedEvent &) const;
		virtual void SuggestSplitting() const;

		Recorder *pRecorder;
	};

	Muxer                 *pMuxer;
	Muxer::FileConfig     *pMuxerConfig;
	Muxer::Streams        *pMuxerStreams;
	Muxer::VideoStream    *pVideoStream;
	Muxer::AudioStream    *pAudioStream;
	Muxer::SubtitleStream *pSubtitleStream;
	Muxer::Frame          *pMuxerFrame;
	MyEventListener       *pMuxerEventListener;

	bool isRecording;
	bool isWaitingForFirstFrame;
	bool isSplitting;

	size_t         extraBufferSize;
	unsigned char *pExtraBuffer;
	bool         (*pFreeExtraBuffer) (void *pParam, unsigned char *pBuffer);
	void          *pFreeExtraBufferParam;

	size_t         osdBufferSize;
	unsigned char *pOsdBuffer;
	bool         (*pFreeOsdBuffer) (void *pParam, unsigned char *pBuffer);
	void          *pFreeOsdBufferParam;

	StreamingMediaChannelHelper &fileHelper;
};

Recorder::Recorder(StreamingMediaChannelHelper &_fileHelper)
	: isRecording(false), isWaitingForFirstFrame(false), isSplitting(false),
	  extraBufferSize(0), pExtraBuffer(NULL), pFreeExtraBuffer(NULL), pFreeExtraBufferParam(NULL),
	  osdBufferSize(0), pOsdBuffer(NULL), pFreeOsdBuffer(NULL), pFreeOsdBufferParam(NULL),
	  fileHelper(_fileHelper)
{
	// create objects
	pMuxer              = Muxer::GetInstance(Muxer::CONTAINER_FORMAT_MKV);
	pMuxerConfig        = new Muxer::FileConfig();
	pMuxerStreams       = new Muxer::Streams();
	pVideoStream        = new Muxer::VideoStream();
	pAudioStream        = new Muxer::AudioStream();
	pSubtitleStream     = new Muxer::SubtitleStream();
	pMuxerFrame         = new Muxer::Frame();
	pMuxerEventListener = new MyEventListener(this);

	// setup muxer
	pMuxerConfig->videoCueThreshold = 5.0;
	pMuxerConfig->maxDuration = 1200.0;  // avg 10min (= 1200sec / 60 / 2)
	pMuxerConfig->applicationName = L"Instek Digital Mkv Muxer";
	pMuxer->SetFileConfig(*pMuxerConfig);
	pMuxer->AddEventListener(*pMuxerEventListener);

	// initialize streams
	pMuxerStreams->pVideo = pVideoStream;
	pMuxerStreams->pAudio = pAudioStream;
	pMuxerStreams->pOther = pSubtitleStream;
	pMuxerStreams->pVideo->trackNumber = 1;
	pMuxerStreams->pAudio->trackNumber = 2;
	pMuxerStreams->pOther->trackNumber = 3;
	pMuxerStreams->pVideo->language = "eng";
	pMuxerStreams->pAudio->language = "eng";
	pMuxerStreams->pOther->SetCodec(Muxer::SubtitleStream::CODEC_ID_OSD);
	pMuxerStreams->pOther->language = "eng";
}

Recorder::~Recorder()
{
	StopRecording();

	if (pMuxer != NULL)
	{
		if (pMuxerEventListener != NULL)
		{
			pMuxer->RemoveEventListener(*pMuxerEventListener);
			delete pMuxerEventListener;
		}
		delete pMuxer;
	}

	if (pMuxerConfig != NULL)
	{
		delete pMuxerConfig;
	}

	if (pMuxerStreams != NULL)
	{
		if (pVideoStream != NULL)
		{
			delete pVideoStream;
		}
		if (pAudioStream != NULL)
		{
			delete pAudioStream;
		}
		if (pSubtitleStream != NULL)
		{
			delete pSubtitleStream;
		}
		delete pMuxerStreams;
	}

	if (pMuxerFrame != NULL)
	{
		delete pMuxerFrame;
	}

	if ((pExtraBuffer != NULL) && (extraBufferSize != 0) && (pFreeExtraBuffer != NULL))
	{
		extraBufferSize = 0;
		pFreeExtraBuffer(pFreeExtraBufferParam, pExtraBuffer);
	}

	if ((pOsdBuffer != NULL) && (osdBufferSize != 0) && (pFreeOsdBuffer != NULL))
	{
		osdBufferSize = 0;
		pFreeOsdBuffer(pFreeOsdBufferParam, pOsdBuffer);
	}

	delete &fileHelper;
}

bool Recorder::StartRecording(const StreamingMediaRecorder::RecordingConfig &config)
{
	StopRecording();

	// setup the audio and the video streams
	if ((config.videoCodec <= StreamingMediaRecorder::VIDEO_CODEC_ID_NONE)
	    || (config.videoCodec > StreamingMediaRecorder::VIDEO_CODEC_ID_MAX))
	{
		// error: no video stream
		return false;
	}
	else
	{
		pVideoStream->SetCodec(config.videoCodec);
		pMuxerStreams->pVideo = pVideoStream;
	}

	if ((config.audioCodec <= StreamingMediaRecorder::AUDIO_CODEC_ID_NONE)
	    || (config.audioCodec > StreamingMediaRecorder::AUDIO_CODEC_ID_MAX))
	{
		// no audio stream
		pMuxerStreams->pAudio = NULL;
	}
	else
	{
		pAudioStream->SetCodec(config.audioCodec);
		pMuxerStreams->pAudio = pAudioStream;
	}

	isRecording = true;
	isWaitingForFirstFrame = true;
	isSplitting = false;

	// backup the extra buffer for the future use
	BackupExtraBuffer(config);

	return true;
}

void Recorder::RestartRecording()
{
	StopRecording();

	isRecording = true;
	isWaitingForFirstFrame = true;
	isSplitting = false;

	// reuse pMuxer, pMuxerStreams, extra buffer, and osd buffer
}

bool Recorder::StopRecording()
{
	if (!isRecording)
	{
		// already stopped
		return false;
	}

	pMuxer->StopMuxing();

	isRecording = false;
	isWaitingForFirstFrame = false;
	isSplitting = false;

	return true;
}

bool Recorder::AppendFrame(const StreamingMediaRecorder::Frame &frame)
{
	if (frame.type == StreamingMediaRecorder::FRAME_TYPE_OSD)
	{
		// backup the osd buffer for the future use
		BackupOsdBuffer(frame);
	}

	if (!isRecording)
	{
		// error: there is no recording file to write
		return false;
	}

	if (isSplitting && (frame.type == StreamingMediaRecorder::FRAME_TYPE_VIDEO) && frame.isKey)
	{
		// do splitting
		isSplitting = false;

		// close the current recording file, and then open a new recording file
		RestartRecording();
	}

	if (isWaitingForFirstFrame && (frame.type != StreamingMediaRecorder::FRAME_TYPE_VIDEO))
	{
		// warning: don't know how to handle this frame, DROP it!!
		return false;
	}

	if (isWaitingForFirstFrame && (extraBufferSize != 0))
	{
		// start the muxer and append the extra data as the first frame
		isWaitingForFirstFrame = false;

		// compose an Muxer::Frame with the content of the extra buffer
		pMuxerFrame->pStream = pMuxerStreams->pVideo;
		pMuxerFrame->isKey = false;
		pMuxerFrame->timecode = frame.timecode;
		pMuxerFrame->size = extraBufferSize;
		pMuxerFrame->data = pExtraBuffer;
		pMuxerFrame->needCopyBuffer = true;
		pMuxerFrame->pFreeBuffer = NULL;
		pMuxerFrame->pFreeBufferParam = NULL;

		pMuxerStreams->dateUTC = frame.timecode / 1000000000ull;
		pMuxerConfig->maxDuration = (double)fileHelper.GetSuggestedDuration(frame.timecode) * 2 / 1000000000.0;
		pMuxer->SetFileConfig(*pMuxerConfig);
		pMuxer->StartMuxing(*pMuxerStreams, *pMuxerFrame, GetFileName());

		AppendOsdFrameIfExist();
	}

	pMuxerFrame->pStream = frame.type == StreamingMediaRecorder::FRAME_TYPE_AUDIO ? pMuxerStreams->pAudio
	                       : frame.type == StreamingMediaRecorder::FRAME_TYPE_VIDEO ? pMuxerStreams->pVideo
	                       : pMuxerStreams->pOther;

	if (pMuxerFrame->pStream == NULL)
	{
		// warning: invalid frame type, DROP it!!
		return false;
	}

	// compose a Muxer::Frame with a StreamingMediaRecorder::Frame
	pMuxerFrame->isKey = frame.isKey;
	pMuxerFrame->timecode = frame.timecode;
	pMuxerFrame->size = frame.size;
	pMuxerFrame->data = frame.data;
	pMuxerFrame->needCopyBuffer = frame.needCopyBuffer;
	pMuxerFrame->pFreeBuffer = frame.pFreeBuffer;
	pMuxerFrame->pFreeBufferParam = frame.pFreeBufferParam;

	if (isWaitingForFirstFrame)
	{
		// start the muxer and append the first frame
		isWaitingForFirstFrame = false;

		pMuxerStreams->dateUTC = frame.timecode / 1000000000ull;
		pMuxerConfig->maxDuration = (double)fileHelper.GetSuggestedDuration(frame.timecode) * 2 / 1000000000.0;
		pMuxer->SetFileConfig(*pMuxerConfig);
		pMuxer->StartMuxing(*pMuxerStreams, *pMuxerFrame, GetFileName());

		AppendOsdFrameIfExist();
	}
	else
	{
		// append one frame
		pMuxer->AppendFrame(*pMuxerFrame);
	}

	return true;
}

void Recorder::MyEventListener::FileClosed(const Muxer::FileClosedEvent &event) const
{
	if (pRecorder->fileHelper.AddMediaFile(event.fileName, event.startTime, event.endTime) == false)
	{
		// error: fail to add the media file into the library
		// FIXME: delete temporal file
	}
}

void Recorder::MyEventListener::SuggestSplitting() const
{
	if (pRecorder->isRecording)
	{
		pRecorder->isSplitting = true;
	}
}

void Recorder::AppendOsdFrameIfExist()
{
	if (osdBufferSize != 0)
	{
		// compose an osd Muxer::Frame
		pMuxerFrame->pStream = pMuxerStreams->pOther;
		pMuxerFrame->isKey = false;
		pMuxerFrame->size = osdBufferSize;
		pMuxerFrame->data = pOsdBuffer;
		pMuxerFrame->needCopyBuffer = true;
		pMuxerFrame->pFreeBuffer = NULL;
		pMuxerFrame->pFreeBufferParam = NULL;

		pMuxer->AppendFrame(*pMuxerFrame);
	}
}

static bool MyFreeBuffer(void *pParam, unsigned char *pBuffer)
{
	if (pBuffer != NULL)
	{
		delete[] pBuffer;
	}

	return true;
}

void Recorder::BackupExtraBuffer(const StreamingMediaRecorder::RecordingConfig &config)
{
	if ((pExtraBuffer != NULL) && (extraBufferSize != 0) && (pFreeExtraBuffer != NULL))
	{
		pFreeExtraBuffer(pFreeExtraBufferParam, pExtraBuffer);
	}

	extraBufferSize = config.extraSize;
	if ((config.extraData == NULL) || (extraBufferSize == 0))
	{
		extraBufferSize = 0;
		pExtraBuffer = NULL;
		pFreeExtraBuffer = NULL;
		pFreeExtraBufferParam = NULL;
	}
	else if (config.needCopyBuffer)
	{
		pExtraBuffer = new unsigned char[extraBufferSize];
		memcpy(pExtraBuffer, config.extraData, extraBufferSize);
		pFreeExtraBuffer = MyFreeBuffer;
		pFreeExtraBufferParam = NULL;

		if (config.pFreeBuffer != NULL)
		{
			config.pFreeBuffer(config.pFreeBufferParam, config.extraData);
		}
	}
	else
	{
		pExtraBuffer = config.extraData;
		pFreeExtraBuffer = config.pFreeBuffer;
		pFreeExtraBufferParam = config.pFreeBufferParam;
	}
}

void Recorder::BackupOsdBuffer(const StreamingMediaRecorder::Frame &frame)
{
	if ((pOsdBuffer != NULL) && (osdBufferSize != 0) && (pFreeOsdBuffer != NULL))
	{
		pFreeOsdBuffer(pFreeOsdBufferParam, pOsdBuffer);
	}

	osdBufferSize = frame.size;
	if ((frame.data == NULL) || (osdBufferSize == 0))
	{
		osdBufferSize = 0;
		pOsdBuffer = NULL;
		pFreeOsdBuffer = NULL;
		pFreeOsdBufferParam = NULL;
	}
	else if (frame.needCopyBuffer)
	{
		pOsdBuffer = new unsigned char[osdBufferSize];
		memcpy(pOsdBuffer, frame.data, osdBufferSize);
		pFreeOsdBuffer = MyFreeBuffer;
		pFreeOsdBufferParam = NULL;

		if (frame.pFreeBuffer != NULL)
		{
			frame.pFreeBuffer(frame.pFreeBufferParam, frame.data);
		}
	}
	else
	{
		pOsdBuffer = frame.data;
		pFreeOsdBuffer = frame.pFreeBuffer;
		pFreeOsdBufferParam = frame.pFreeBufferParam;
	}
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
class IdStreamingMediaRecorder : public StreamingMediaRecorder
{
public:
	enum ACCESSIBILITY
	{
		READ_ONLY,
		READ_WRITE
	};

	// global methods
	static IdStreamingMediaRecorder * GetInstance(ACCESSIBILITY mode = READ_ONLY);

	// destructor
	virtual ~IdStreamingMediaRecorder();

	// recording APIs
	virtual bool StartRecording(int chId, const RecordingConfig &);
	virtual bool StopRecording(int chId);
	virtual bool StopAllChannels();
	virtual bool AppendFrame(int chId, const Frame &);

	// streaming APIs
	virtual int StartStreaming(const StreamingRequest &) { return -1; }  // return reqId
	virtual bool ResetStreaming(int reqId, const StreamingRequest &) { return false; }  // for seeking
	virtual bool StopStreaming(int reqId) { return true; }
	virtual const Frame * GetOneFrame(int reqId) { return NULL; }

private:
	static void Initialize();

	// global settings
	static const char *rootPath;
	static int recorderCount;
	static int maxRetrieverCount;

	// global members
	static bool isInitialized;
	static bool writeLock;
	static StreamingMediaLibrary *pLibrary;
	static Recorder **pRecorders;
	static int **pRetrievers;

	// constructor
	IdStreamingMediaRecorder() {}

	// private members
	ACCESSIBILITY mode;
};

// global settings
const char *IdStreamingMediaRecorder::rootPath;
int IdStreamingMediaRecorder::recorderCount;
int IdStreamingMediaRecorder::maxRetrieverCount;

// global members
bool IdStreamingMediaRecorder::isInitialized = false;
bool IdStreamingMediaRecorder::writeLock = false;
StreamingMediaLibrary *IdStreamingMediaRecorder::pLibrary;
Recorder **IdStreamingMediaRecorder::pRecorders;
int **IdStreamingMediaRecorder::pRetrievers;

// global methods
StreamingMediaRecorder * StreamingMediaRecorder::GetStreamingMediaRecorder()
{
	return IdStreamingMediaRecorder::GetInstance(IdStreamingMediaRecorder::READ_WRITE);
}

StreamingMediaRecorder * StreamingMediaRecorder::GetReadOnlyStreamingMediaRecorder()
{
	return IdStreamingMediaRecorder::GetInstance(IdStreamingMediaRecorder::READ_ONLY);
}

IdStreamingMediaRecorder * IdStreamingMediaRecorder::GetInstance(ACCESSIBILITY _mode)
{
	Initialize();

	IdStreamingMediaRecorder *pStreamingMediaRecorder = new IdStreamingMediaRecorder();
	if ((_mode == READ_WRITE) && !writeLock)
	{
		pStreamingMediaRecorder->mode = READ_WRITE;
		writeLock = true;
	}
	else
	{
		pStreamingMediaRecorder->mode = READ_ONLY;
	}
	return pStreamingMediaRecorder;
}

void IdStreamingMediaRecorder::Initialize()
{
	if (!isInitialized)
	{
		int i;

		rootPath = "123";

		pLibrary = new StreamingMediaLibrary();

		recorderCount = 8;
		pRecorders = new Recorder *[recorderCount];
		for (i = 0; i < recorderCount; i++)
		{
			pRecorders[i] = new Recorder(pLibrary->CreateChannelHelper(i+1));
		}

		maxRetrieverCount = 8;
		pRetrievers = new int *[maxRetrieverCount];
		for (i = 0; i < maxRetrieverCount; i++)
		{
			pRetrievers[i] = new int();
		}

		isInitialized = true;
	}
}

// destructor
IdStreamingMediaRecorder::~IdStreamingMediaRecorder()
{
	if (mode == READ_WRITE)
	{
		StopAllChannels();
		writeLock = false;
	}
}

// recording APIs
bool IdStreamingMediaRecorder::StartRecording(int chId, const RecordingConfig &config)
{
	if ((mode == READ_ONLY) || (chId <= 0) || (chId > recorderCount) || (pRecorders == NULL) || (pRecorders[chId - 1] == NULL))
	{
		return false;
	}

	return pRecorders[chId - 1]->StartRecording(config);
}

bool IdStreamingMediaRecorder::StopRecording(int chId)
{
	if ((mode == READ_ONLY) || (chId <= 0) || (chId > recorderCount) || (pRecorders == NULL) || (pRecorders[chId - 1] == NULL))
	{
		return false;
	}

	return pRecorders[chId - 1]->StopRecording();
}

bool IdStreamingMediaRecorder::StopAllChannels()
{
	if ((mode == READ_ONLY) || (pRecorders == NULL))
	{
		return false;
	}

	for (int i = 0; i < recorderCount; i++)
	{
		if (pRecorders[i] != NULL)
		{
			pRecorders[i]->StopRecording();
		}
	}

	delete pLibrary;  // FIXME: just for debug
	return true;
}

bool IdStreamingMediaRecorder::AppendFrame(int chId, const Frame & frame)
{
	if ((mode == READ_ONLY) || (chId <= 0) || (chId > recorderCount) || (pRecorders == NULL) || (pRecorders[chId - 1] == NULL))
	{
		return false;
	}

	return pRecorders[chId - 1]->AppendFrame(frame);
}
