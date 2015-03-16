
#include "mkvmuxer.hpp"

using namespace std;

static const char *INPUT_FILE_NAME1 = "data.bin.g711.4";
static const char *INPUT_FILE_NAME2 = "data.bin.h264";

static void AudioStreamInitializer_Default(MKVMuxer::AudioStream &audio)
{
	audio.SetCodec(MKVMuxer::AudioStream::CODEC_ID_PCM_LITTLE_ENDIAN);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 8;
}

static void AudioStreamInitializer_Pcm_LE(MKVMuxer::AudioStream &audio)
{
	audio.SetCodec(MKVMuxer::AudioStream::CODEC_ID_PCM_LITTLE_ENDIAN);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 16;
}

static void AudioStreamInitializer_Pcm_BE(MKVMuxer::AudioStream &audio)
{
	audio.SetCodec(MKVMuxer::AudioStream::CODEC_ID_PCM_BIG_ENDIAN);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 16;
}

static void AudioStreamInitializer_G711_Ulaw(MKVMuxer::AudioStream &audio)
{
	audio.SetCodec(MKVMuxer::AudioStream::CODEC_ID_G711_ULAW);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 8;
}

static void AudioStreamInitializer_G711_Alaw(MKVMuxer::AudioStream &audio)
{
	audio.SetCodec(MKVMuxer::AudioStream::CODEC_ID_G711_ALAW);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 8;
}

static void AudioStreamInitializer_ADPCM(MKVMuxer::AudioStream &audio)
{
	audio.SetCodec(MKVMuxer::AudioStream::CODEC_ID_ADPCM);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 4;
}

static void AudioStreamInitializer_AMR(MKVMuxer::AudioStream &audio)
{
	audio.SetCodec(MKVMuxer::AudioStream::CODEC_ID_AMR);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 8;
}

static void (*audioStreamInitializers[])(MKVMuxer::AudioStream &audio) =
{
	AudioStreamInitializer_Pcm_BE,
	AudioStreamInitializer_G711_Ulaw,
	AudioStreamInitializer_Pcm_LE,
	AudioStreamInitializer_ADPCM,
	AudioStreamInitializer_AMR,
	AudioStreamInitializer_Default,
	AudioStreamInitializer_Default,
	AudioStreamInitializer_G711_Alaw
};

static inline void InitializeAudioStream(unsigned int type, MKVMuxer::AudioStream &audio)
{
	if ((type >= 0) && (type <= 7))
	{
		audioStreamInitializers[type](audio);
	}
	else
	{
		AudioStreamInitializer_Default(audio);
	}
}

static FILE *pInputFile = NULL;
static const int VIDEO_FILE_HEADER_SIZE = 1 + 1 + 2 + 2 + 4;
static const int VIDEO_FRAME_HEADER_SIZE = 1 + 4 + 2 + 4;
static char fileHeaderBuffer[VIDEO_FILE_HEADER_SIZE];
static char frameHeaderBuffer[VIDEO_FRAME_HEADER_SIZE];

static bool SetupStreamConfigs(MKVMuxer::Streams &streams, const char *fileName)
{
	pInputFile = fopen(fileName, "rb");

	// fail to open file
	if (pInputFile == NULL)
	{
		cout << "fail to open file: " << fileName << endl;
		return false;
	}

	fread(fileHeaderBuffer, VIDEO_FILE_HEADER_SIZE, 1, pInputFile);

	unsigned char videoType = fileHeaderBuffer[0];
	unsigned char audioType = fileHeaderBuffer[1];
	streams.pVideo->width = *(unsigned short *)(fileHeaderBuffer + 1 + 1);
	streams.pVideo->height = *(unsigned short *)(fileHeaderBuffer + 1 + 1 + 2);
	//streams.dateUTC = *(unsigned int *)(fileHeaderBuffer + 1 + 1 + 2 + 2);

	streams.pVideo->SetCodec(videoType);

	if (audioType == 0xFE)
	{
		// no audio stream
		streams.pAudio = NULL;
	}
	else
	{
		InitializeAudioStream(audioType, *streams.pAudio);
	}

	return true;
}

static bool GetOneFrame(MKVMuxer::Frame *pFrame, const MKVMuxer::Streams &streams)
{
	// read one frame header
	size_t headerReadSize = fread(frameHeaderBuffer, sizeof(char), sizeof(frameHeaderBuffer)/sizeof(char), pInputFile);  // FIXME: MUST check size
	if (headerReadSize <= 0)
	{
		// close file
		fclose(pInputFile);
		return false;
	}

	if (streams.dateUTC == 0)
	{
		((MKVMuxer::Streams)streams).dateUTC = *(unsigned int *)(frameHeaderBuffer + 1);
	}

	pFrame->pStream = (frameHeaderBuffer[0] == 4) ? (MKVMuxer::Stream *)streams.pAudio : (MKVMuxer::Stream *)streams.pVideo;
	if (pFrame->pStream == NULL)
	{
		return false;
	}
	pFrame->isKey = (frameHeaderBuffer[0] == 1);
	pFrame->timecode = (double)*(unsigned int *)(frameHeaderBuffer + 1) * 1000000000.0;
	pFrame->timecode += (double)*(unsigned short *)(frameHeaderBuffer + 1 + 4) * 1000000.0;
	pFrame->size = *(int *)(frameHeaderBuffer + 1 + 4 + 2);

	// FIXME: we need better buffer management
	// read one frame from file
	// read pFrame->size bytes
	pFrame->data = new unsigned char[pFrame->size];
	unsigned char *pBuffer = pFrame->data;
	size_t size = pFrame->size;

	while (size > 0)
	{
		size_t readSize = fread(pBuffer, sizeof(unsigned char), size, pInputFile);
		if (readSize <= 0)
		{
			// close file
			fclose(pInputFile);
			return false;
		}
		size -= readSize;
		pBuffer += readSize;
	}

	return true;
}

int main(int argc, char **argv)
{
	MKVMuxer muxer;

	MKVMuxer::FileConfig config;
	muxer.SetFileConfig(config);

	MKVMuxer::Streams streams;
	MKVMuxer::VideoStream video;
	MKVMuxer::AudioStream audio;
	MKVMuxer::SubtitleStream osd;

	streams.pVideo = &video;
	streams.pAudio = &audio;
	streams.pOther = &osd;
	video.trackNumber = 1;
	audio.trackNumber = 2;
	osd.trackNumber = 3;

	int count = 0;

	do
	{
		if ((count == 0) && !SetupStreamConfigs(streams, INPUT_FILE_NAME1))
		{
			return -1;
		}

		if ((count == 1) && !SetupStreamConfigs(streams, INPUT_FILE_NAME2))
		{
			return -1;
		}

		video.language = "eng";
		audio.language = "eng";
		osd.SetCodec(MKVMuxer::SubtitleStream::CODEC_ID_OSD);
		osd.language = "eng";

		MKVMuxer::Frame myFrame;

		if (!GetOneFrame(&myFrame, streams))
		{
			return -1;
		}

		if (!muxer.StartMuxing(streams, myFrame))
		{
			return -1;
		}

		while (GetOneFrame(&myFrame, streams))//(newFrameArrived)
		{
			muxer.AppendFrame(myFrame);
		}

		muxer.StopMuxing();
	} while (++count < 2);

	return 0;
}

