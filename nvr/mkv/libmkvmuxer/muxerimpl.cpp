
#include "muxerimpl.hpp"

namespace MuxerImpl
{

// declare the muxer constructors of your container formats here
Muxer * CreateMkvMuxer();

};

static MuxerImpl::MuxerFactory::Pair __constructorPairs[] =
{
	// associate the muxer constructors with their corresponding container formats
	MuxerImpl::MuxerFactory::Pair(Muxer::CONTAINER_FORMAT_MKV, MuxerImpl::CreateMkvMuxer)
};

MuxerImpl::MuxerFactory::Map MuxerImpl::MuxerFactory::constructors;

Muxer * Muxer::GetInstance(Muxer::ContainerFormat key)
{
	if (MuxerImpl::MuxerFactory::constructors.empty())
	{
		for (unsigned int i = 0; i < sizeof(__constructorPairs) / sizeof(MuxerImpl::MuxerFactory::Pair); i++)
		{
			MuxerImpl::MuxerFactory::constructors.insert(__constructorPairs[i]);
		}
	}

	MuxerImpl::MuxerFactory::Map::iterator it;
	it = MuxerImpl::MuxerFactory::constructors.find(key);
	return it != MuxerImpl::MuxerFactory::constructors.end() ? (*it).second() : NULL;
}

Muxer::FileConfig::FileConfig()
	: videoCueThreshold(DEFAULT_VIDEO_CUE_THRESHOLD), maxDuration(DEFAULT_MAX_DURATION),
	  timecodeScale(1000000ull), applicationName(L"muxer")
{
	pImpl = new MuxerImpl::FileConfig(this);
}

Muxer::FileConfig::~FileConfig()
{
	if (pImpl != NULL)
	{
		delete pImpl;
	}
}

void Muxer::Streams::ClearAllStreams()
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
}

Muxer::Stream::Stream(CodecType type)
	: pNext(NULL), codecType(type), language(NULL)
{
	pImpl = new MuxerImpl::Stream(this);
}

Muxer::Stream::~Stream()
{
	if (pImpl != NULL)
	{
		delete pImpl;
	}
}

void Muxer::Stream::SetCodec(unsigned int id)
{
	pImpl->codecId = id;
	if (pImpl->pCodec != NULL)
	{
		const MuxerImpl::Codec *pLastCodec = pImpl->pCodec;
		pImpl->pCodec = pImpl->pCodec->GetInstance(id);
		if ((pImpl->pCodec != pLastCodec) && pLastCodec->needDelete && (pLastCodec != pImpl->pMyCodec))
		{
			delete pLastCodec;
		}
	}
}

void MuxerImpl::Stream::FixCodecByFrame(const Muxer::Frame &frame)
{
	if ((pCodec != NULL) && pCodec->needFix && (frame.pStream == pPublic))
	{
		Codec *pResultCodec = pCodec->FixByFrame(frame, pMyCodec);
		if (pResultCodec != NULL)
		{
			if ((pMyCodec != NULL) && (pMyCodec != pResultCodec) && pMyCodec->needDelete)
			{
				delete pMyCodec;
			}
			pCodec = pMyCodec = pResultCodec;
		}
	}
}
