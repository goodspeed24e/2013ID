
#include <stdio.h>
#include <iostream>
#include "muxer.hpp"

using namespace std;

static const char *INPUT_FILE_NAME1 = "data.bin.g711.4";
static const char *INPUT_FILE_NAME2 = "data.bin.h264";

static void AudioStreamInitializer_Default(Muxer::AudioStream &audio)
{
	audio.SetCodec(Muxer::AudioStream::CODEC_ID_PCM_LITTLE_ENDIAN);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 8;
}

static void AudioStreamInitializer_Pcm_LE(Muxer::AudioStream &audio)
{
	audio.SetCodec(Muxer::AudioStream::CODEC_ID_PCM_LITTLE_ENDIAN);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 16;
}

static void AudioStreamInitializer_Pcm_BE(Muxer::AudioStream &audio)
{
	audio.SetCodec(Muxer::AudioStream::CODEC_ID_PCM_BIG_ENDIAN);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 16;
}

static void AudioStreamInitializer_G711_Ulaw(Muxer::AudioStream &audio)
{
	audio.SetCodec(Muxer::AudioStream::CODEC_ID_G711_ULAW);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 8;
}

static void AudioStreamInitializer_G711_Alaw(Muxer::AudioStream &audio)
{
	audio.SetCodec(Muxer::AudioStream::CODEC_ID_G711_ALAW);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 8;
}

static void AudioStreamInitializer_ADPCM(Muxer::AudioStream &audio)
{
	audio.SetCodec(Muxer::AudioStream::CODEC_ID_ADPCM);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 4;
}

static void AudioStreamInitializer_AMR(Muxer::AudioStream &audio)
{
	audio.SetCodec(Muxer::AudioStream::CODEC_ID_AMR);
	audio.samplingRate = 8000.0;
	audio.channelCount = 1;
	audio.bitDepth = 8;
}

static void (*audioStreamInitializers[])(Muxer::AudioStream &audio) =
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

static inline void InitializeAudioStream(unsigned int type, Muxer::AudioStream &audio)
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

static bool SetupStreamConfigs(Muxer::Streams &streams, const char *fileName)
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

static bool MyFreeBuffer(void *pParam, unsigned char *pBuffer)
{
	if (pBuffer != NULL)
	{
		delete[] pBuffer;
	}
	return true;
}

static bool GetOneFrame(Muxer::Frame *pFrame, const Muxer::Streams &streams)
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
		const_cast<Muxer::Streams &>(streams).dateUTC = *(unsigned int *)(frameHeaderBuffer + 1);
	}

	pFrame->pStream = (frameHeaderBuffer[0] == 4) ? (Muxer::Stream *)streams.pAudio : (Muxer::Stream *)streams.pVideo;
	if (pFrame->pStream == NULL)
	{
		return false;
	}
	pFrame->isKey = (frameHeaderBuffer[0] == 1);
	pFrame->timecode = (double)*(unsigned int *)(frameHeaderBuffer + 1) * 1000000000.0;
	pFrame->timecode += (double)*(unsigned short *)(frameHeaderBuffer + 1 + 4) * 1000000.0;
	pFrame->size = *(int *)(frameHeaderBuffer + 1 + 4 + 2);
	pFrame->pFreeBuffer = MyFreeBuffer;

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
	Muxer &muxer = *Muxer::GetInstance(Muxer::CONTAINER_FORMAT_MKV);

	Muxer::FileConfig config;
	muxer.SetFileConfig(config);

	Muxer::Streams streams;
	Muxer::VideoStream video;
	Muxer::AudioStream audio;
	Muxer::SubtitleStream osd;

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
		osd.SetCodec(Muxer::SubtitleStream::CODEC_ID_OSD);
		osd.language = "eng";

		Muxer::Frame myFrame;

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

