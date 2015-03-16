
#include <sys/time.h>
#include <time.h>

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

static void GenerateRandomBytes(void * buffer_, size_t size)
{
	unsigned char * buffer = (unsigned char *) buffer_;

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

static bool isOpenVideoInputFile = false;
static bool isCloseVideoInputFile = false;
static FILE * pInputFile = NULL;
static const int VIDEO_FILE_HEADER_SIZE = 1 + 1 + 2 + 2 + 4;
static const int VIDEO_FRAME_HEADER_SIZE = 1 + 4 + 2 + 4;
static char fileHeaderBuffer[VIDEO_FILE_HEADER_SIZE];
static char frameHeaderBuffer[VIDEO_FRAME_HEADER_SIZE];

static const int MAX_CODEC_PRIVATE_SIZE = 120;
static unsigned char H264VideoTrackCodecPrivate[MAX_CODEC_PRIVATE_SIZE];
static const unsigned char H264_VIDEO_TRACK_DEFAULT_CODEC_PRIVATE[] =
{
		0x01, 0x42, 0xE0, 0x1E,  0xFF, 0xE1, 0x00, 0x00,  0x01, 0x00, 0x00
};

static const unsigned char G711_ULAW_AUDIO_TRACK_CODEC_PRIVATE[] =
{
		0x07, 0x00, 0x01, 0x00,  0x40, 0x1F, 0x00, 0x00,
		0x40, 0x1F, 0x00, 0x00,  0x01, 0x00, 0x08, 0x00,
		0x00, 0x00
};
static const unsigned char G711_ALAW_AUDIO_TRACK_CODEC_PRIVATE[] =
{
		0x06, 0x00, 0x01, 0x00,  0x40, 0x1F, 0x00, 0x00,
		0x40, 0x1F, 0x00, 0x00,  0x01, 0x00, 0x08, 0x00,
		0x00, 0x00
};
/*static const unsigned char ADPCM_AUDIO_TRACK_CODEC_PRIVATE[] =
{
		0x02, 0x00, 0x01, 0x00,  0x40, 0x1F, 0x00, 0x00,
		0xA0, 0x0F, 0x00, 0x00,  0x00, 0x04, 0x04, 0x00,
		0x00, 0x00
};*/
static const unsigned char ADPCM_AUDIO_TRACK_CODEC_PRIVATE[] =
{
		0x45, 0x00, 0x01, 0x00,  0x40, 0x1F, 0x00, 0x00,
		0xA0, 0x0F, 0x00, 0x00,  0x01, 0x00, 0x04, 0x00,
		0x02, 0x00, 0x02, 0x00
};
static const unsigned char AMR_AUDIO_TRACK_CODEC_PRIVATE[] =
{
		0x36, 0x01, 0x01, 0x00,  0x40, 0x1F, 0x00, 0x00,
		0xA0, 0x0F, 0x00, 0x00,  0x00, 0x04, 0x04, 0x00,
		0x00, 0x00
};

static bool PeekFirstFrame()
{
	// end of file
	if (isCloseVideoInputFile)
	{
		return false;
	}

	// open file
	if (!isOpenVideoInputFile)
	{
		pInputFile = fopen(INPUT_FILE_NAME, "rb");

		// fail to open file
		if (pInputFile == NULL)
		{
			isCloseVideoInputFile = true;
			cout << "fail to open file: " << INPUT_FILE_NAME << endl;
			return false;
		}

		isOpenVideoInputFile = true;

		fread(fileHeaderBuffer, VIDEO_FILE_HEADER_SIZE, 1, pInputFile);

		switch (fileHeaderBuffer[1])
		{
			case 0xFE:  // NO audio track
				HasAudioTrack = false;
				break;

			case 1:  // G.711 u-law, (1 channel, 8 bits, 8000 Hz)
				HasAudioTrack = true;
				pAudioTrackCodec           = "A_MS/ACM";
				pAudioTrackCodecPrivate    = G711_ULAW_AUDIO_TRACK_CODEC_PRIVATE;
				AudioTrackCodecPrivateSize = sizeof(G711_ULAW_AUDIO_TRACK_CODEC_PRIVATE) / sizeof(unsigned char);
				AudioTrackSamplingRate     = 8000.0;
				AudioTrackChannels         = 1;
				AudioTrackBitDepth         = 8;
				break;

			case 7:  // G.711 a-law, (1 channel, 8 bits, 8000 Hz)
				HasAudioTrack = true;
				pAudioTrackCodec           = "A_MS/ACM";
				pAudioTrackCodecPrivate    = G711_ALAW_AUDIO_TRACK_CODEC_PRIVATE;
				AudioTrackCodecPrivateSize = sizeof(G711_ALAW_AUDIO_TRACK_CODEC_PRIVATE) / sizeof(unsigned char);
				AudioTrackSamplingRate     = 8000.0;
				AudioTrackChannels         = 1;
				AudioTrackBitDepth         = 8;
				break;

			case 4:  // AMR
				HasAudioTrack = true;
				pAudioTrackCodec           = "A_AMR/NB";
				//pAudioTrackCodecPrivate    = AMR_AUDIO_TRACK_CODEC_PRIVATE;
				//AudioTrackCodecPrivateSize = sizeof(AMR_AUDIO_TRACK_CODEC_PRIVATE) / sizeof(unsigned char);
				AudioTrackSamplingRate     = 8000.0;
				AudioTrackChannels         = 1;
				AudioTrackBitDepth         = 8;
				break;

			case 2:  // PCM, 1 channel, 16 bits, 8000 Hz, little endian
				HasAudioTrack = true;
				pAudioTrackCodec           = "A_PCM/INT/LIT";
				AudioTrackSamplingRate     = 8000.0;
				AudioTrackChannels         = 1;
				AudioTrackBitDepth         = 16;
				break;

			case 0:  // PCM, 1 channel, 16 bits, 8000 Hz, big endian
				HasAudioTrack = true;
				pAudioTrackCodec           = "A_PCM/INT/BIG";
				AudioTrackSamplingRate     = 8000.0;
				AudioTrackChannels         = 1;
				AudioTrackBitDepth         = 16;
				break;

			case 3:  // ADPCM, (1 channel, 4 bits, 8000 Hz)
				HasAudioTrack = true;
				pAudioTrackCodec           = "A_MS/ACM";
				pAudioTrackCodecPrivate    = ADPCM_AUDIO_TRACK_CODEC_PRIVATE;
				AudioTrackCodecPrivateSize = sizeof(ADPCM_AUDIO_TRACK_CODEC_PRIVATE) / sizeof(unsigned char);
				AudioTrackSamplingRate     = 8000.0;
				AudioTrackChannels         = 1;
				AudioTrackBitDepth         = 4;
				break;

			case 5:  // MP2 (MPA)
			case 6:  // MP4
			default:  // unknown audio codec
				HasAudioTrack = true;
				break;
		}

		switch (fileHeaderBuffer[0])
		{
			case 4:  case 5:  // H.264
				VideoTrackCodecId = VIDEO_CODEC_H264;
				pVideoTrackCodec = "V_MPEG4/ISO/AVC";
				break;

			case 3:  // Motion JPEG
				VideoTrackCodecId = VIDEO_CODEC_MJPEG;
				pVideoTrackCodec = "V_MJPEG";
				break;

			case 2:  // MPEG4
				VideoTrackCodecId = VIDEO_CODEC_MPEG4;
				pVideoTrackCodec = "V_MPEG4/ISO/ASP";
				break;

			case 1:  // HikVision
			default:  // raw data
				VideoTrackCodecId = VIDEO_CODEC_RAWDATA;
				pVideoTrackCodec = "V_UNCOMPRESSED";
				break;
		}
		VideoTrackFrameWidth = *(unsigned short *)(fileHeaderBuffer + 1 + 1);
		VideoTrackFrameHeight = *(unsigned short *)(fileHeaderBuffer + 1 + 1 + 2);
		SegmentDate = *(unsigned int *)(fileHeaderBuffer + 1 + 1 + 2 + 2);

		if (VideoTrackCodecId == VIDEO_CODEC_H264)
		{
			// peek file
			size_t size = fread(H264VideoTrackCodecPrivate, sizeof(unsigned char), MAX_CODEC_PRIVATE_SIZE, pInputFile);

			unsigned char * pSPS = NULL;
			unsigned char * pPPS = NULL;
			unsigned char * pBuffer = H264VideoTrackCodecPrivate + VIDEO_FRAME_HEADER_SIZE;
			unsigned char * lastStartCodePosition = NULL;
			size -= VIDEO_FRAME_HEADER_SIZE;

			while (size > 4)
			{
				// find start code: <00 00 00 01>
				// warning: use <00 00 01> for convenient
				if ((pBuffer[0] == 0) && (pBuffer[2] == 1) && (pBuffer[1] == 0))
				{
					// find sps & pps
					if ((pBuffer[3] & 0x1F) == 0x07)
					{
						pSPS = pBuffer;
					}
					else if ((pBuffer[3] & 0x1F) == 0x08)
					{
						pPPS = pBuffer;
					}

					if (lastStartCodePosition != NULL)
					{
						// start code --> the size of last NAL unit
						size_t nalUnitSize = pBuffer - lastStartCodePosition - 4;
						lastStartCodePosition[0] = (nalUnitSize >> 16) & 0xFF;
						lastStartCodePosition[1] = (nalUnitSize >>  8) & 0xFF;
						lastStartCodePosition[2] = (nalUnitSize      ) & 0xFF;
					}
					lastStartCodePosition = pBuffer;
				}
				size--;
				pBuffer++;
			}

			// check sps & pps
			if (pSPS != NULL)
			{
				if ((pSPS[0] == 0) && (pSPS[1] == 0) && (pSPS[2] > 1u))
				{
					pSPS++;
				}
				else
				{
					pSPS = NULL;
				}
			}
			if (pPPS != NULL)
			{
				if ((pPPS[0] == 0) && (pPPS[1] == 0) && (pPPS[2] > 1u))
				{
					pPPS++;
				}
				else
				{
					pPPS = NULL;
				}
			}

			// is it ready for sps & pps?
			if ((pSPS != NULL) && (pPPS != NULL))
			{
				// construct the private data
				size_t SPSSize = (pSPS[0] << 8) + pSPS[1];
				size_t PPSSize = (pPPS[0] << 8) + pPPS[1];

				H264VideoTrackCodecPrivate[0] = 0x01;
				H264VideoTrackCodecPrivate[1] = pSPS[3];
				H264VideoTrackCodecPrivate[2] = pSPS[4];
				H264VideoTrackCodecPrivate[3] = pSPS[5];
				H264VideoTrackCodecPrivate[4] = 0xFF;
				H264VideoTrackCodecPrivate[5] = 0xE1;
				memcpy(H264VideoTrackCodecPrivate + 6, pSPS, 2 + SPSSize);
				H264VideoTrackCodecPrivate[6 + 2 + SPSSize] = 0x01;
				memcpy(H264VideoTrackCodecPrivate + 6 + 2 + SPSSize + 1, pPPS, 2 + PPSSize);

				pVideoTrackCodecPrivate = H264VideoTrackCodecPrivate;
				VideoTrackCodecPrivateSize = 6 + 2 + SPSSize + 1 + 2 + PPSSize;
			}
			else
			{
				// use the default private data
				pVideoTrackCodecPrivate = H264_VIDEO_TRACK_DEFAULT_CODEC_PRIVATE;
				VideoTrackCodecPrivateSize = sizeof(H264_VIDEO_TRACK_DEFAULT_CODEC_PRIVATE);
			}

			// reset file
			fseek(pInputFile, (long)VIDEO_FILE_HEADER_SIZE, SEEK_SET);
		}

		return true;
	}

	return false;
}

static bool GetNextFrame(struct Frame * frame)
{
	// end of file
	if (isCloseVideoInputFile)
	{
		return false;
	}

	// open file
	if (!isOpenVideoInputFile)
	{
		pInputFile = fopen(INPUT_FILE_NAME, "rb");

		// fail to open file
		if (pInputFile == NULL)
		{
			isCloseVideoInputFile = true;
			cout << "fail to open file: " << INPUT_FILE_NAME << endl;
			return false;
		}

		isOpenVideoInputFile = true;

		fseek(pInputFile, (long)VIDEO_FILE_HEADER_SIZE, SEEK_SET);
	}

	// read one frame header
	size_t headerReadSize = fread(frameHeaderBuffer, sizeof(char), sizeof(frameHeaderBuffer)/sizeof(char), pInputFile);  // FIXME: MUST check size
	if (headerReadSize <= 0)
	{
		// close file
		fclose(pInputFile);
		isCloseVideoInputFile = true;
		return false;
	}

	if (VideoTrackTimecodeBase == 0.0)
	{
		VideoTrackTimecodeBase = *(unsigned int *)(frameHeaderBuffer + 1) * 1000000000.0;
	}

	frame->isAudio = (frameHeaderBuffer[0] == 4);
	frame->isKey = (frameHeaderBuffer[0] == 1);
	frame->timecode = (double)*(unsigned int *)(frameHeaderBuffer + 1) * 1000000000.0;
	frame->timecode += (double)*(unsigned short *)(frameHeaderBuffer + 1 + 4) * 1000000.0;
	frame->size = *(int *)(frameHeaderBuffer + 1 + 4 + 2);

	// FIXME: we need better buffer management
	// read one frame from file
	// read frame->size bytes
	frame->data = new char[frame->size];
	char * pBuffer = frame->data;
	size_t size = frame->size;

	if (VideoTrackCodecId == VIDEO_CODEC_H264)
	{
		fgetc(pInputFile);
		size--;
	}

	while (size > 0)
	{
		size_t readSize = fread(pBuffer, sizeof(char), size, pInputFile);
		if (readSize <= 0)
		{
			// close file
			fclose(pInputFile);
			isCloseVideoInputFile = true;
			return false;
		}
		size -= readSize;
		pBuffer += readSize;
	}

	if (VideoTrackCodecId == VIDEO_CODEC_H264)
	{
		pBuffer[0] = '\0';

		// the byte stream format --> the NAL unit stream format
		pBuffer = frame->data;
		size = frame->size;
		int counter = 0;
		char * lastStartCodePosition = NULL;
		while ((size > 3) && (++counter <= 100))
		{
			// find start code: <00 00 00 01>
			// warning: use <00 00 01> for convenient
			if ((pBuffer[0] == 0) && (pBuffer[2] == 1) && (pBuffer[1] == 0))
			{
				if (lastStartCodePosition != NULL)
				{
					// start code --> the size of last NAL unit
					size_t nalUnitSize = pBuffer - lastStartCodePosition - 4;
					lastStartCodePosition[0] = (nalUnitSize >> 16) & 0xFF;
					lastStartCodePosition[1] = (nalUnitSize >>  8) & 0xFF;
					lastStartCodePosition[2] = (nalUnitSize      ) & 0xFF;
				}
				else
				{
					int offset = pBuffer - frame->data;
					frame->data += offset;
					frame->size -= offset;
				}
				lastStartCodePosition = pBuffer;
			}
			size--;
			pBuffer++;
		}
		if (lastStartCodePosition != NULL)
		{
			// start code --> the size of last NAL unit
			size_t nalUnitSize = frame->data + frame->size - lastStartCodePosition - 4;
			lastStartCodePosition[0] = (nalUnitSize >> 16) & 0xFF;
			lastStartCodePosition[1] = (nalUnitSize >>  8) & 0xFF;
			lastStartCodePosition[2] = (nalUnitSize      ) & 0xFF;
		}
	}

	return true;
}

