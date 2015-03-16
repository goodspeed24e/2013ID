
#ifndef MUXER_IMPL_HPP
#define MUXER_IMPL_HPP

#include <map>
#include "muxer.hpp"

namespace MuxerImpl
{

class MuxerFactory
{
public:
	typedef Muxer * (*PConstructor)();
	typedef std::map<Muxer::ContainerFormat, PConstructor> Map;
	typedef std::pair<Muxer::ContainerFormat, PConstructor> Pair;
	static Map constructors;
};

class FileConfig
{
public:
	FileConfig(Muxer::FileConfig *_pPublic) : pPublic(_pPublic) {}

	int      GetMetaSeekElementSize() const { return 300; }
	uint64_t GetVideoCueTimecodeThreshold() const { uint64_t v1 = (uint64_t)pPublic->videoCueThreshold * 1000000000ull;  uint64_t v2 = GetMaxClusterDuration() / 2;  return v1 < v2 ? v1 : v2; }
	int      GetCueingDataElementSize() const { return ((int)(GetMaxSegmentDuration() / GetVideoCueTimecodeThreshold()) + 1) * 20 + 200; }
	uint64_t GetMaxSegmentDuration() const { return (uint64_t)(pPublic->maxDuration * 1000000000.0); }
	uint64_t GetMaxClusterDuration() const { return 0x7FFF * pPublic->timecodeScale; }

	Muxer::FileConfig *pPublic;
};

class Codec
{
public:
	Codec(unsigned int _id) : id(_id), needFix(false), needDelete(false) {}
	virtual ~Codec() {}

protected:
	friend class Stream;
	friend class Muxer::Stream;

	virtual Codec *         FixByFrame(const Muxer::Frame &, Codec *) const { return NULL; }
	virtual unsigned char * FixFrameData(Muxer::Frame &frame) const { return frame.data; }
	virtual const Codec *   GetInstance(unsigned int) const = 0;

	unsigned int id;
	bool         needFix;
	bool         needDelete;
};

class Stream
{
public:
	Stream(Muxer::Stream *_pPublic)
		: pPublic(_pPublic), pCodec(NULL), pMyCodec(NULL)
	{
	}

	virtual ~Stream()
	{
		if ((pMyCodec != NULL) && pMyCodec->needDelete)
		{
			delete pMyCodec;
		}
	}

	void FixCodecByFrame(const Muxer::Frame &frame);

	unsigned char * FixFrameData(Muxer::Frame &frame) const
	{
		return (pCodec != NULL) ? pCodec->FixFrameData(frame) : frame.data;
	}

	template <class T>
	bool CheckCodecInstance()
	{
		if (pCodec == NULL)
		{
			pCodec = T::GetNewInstance(codecId);
		}
		return true;
	}

	Muxer::Stream *pPublic;
	unsigned int   codecId;
	const Codec   *pCodec;
	Codec         *pMyCodec;
};

};  // namespace MuxerImpl

inline unsigned char * Muxer::Frame::FixData()
{
	return pStream->pImpl->FixFrameData(*this);
}

inline unsigned int Muxer::Stream::GetCodec() const
{
	return (pImpl->pCodec != NULL) ? pImpl->pCodec->id : pImpl->codecId;
}

#endif  // MUXER_IMPL_HPP
