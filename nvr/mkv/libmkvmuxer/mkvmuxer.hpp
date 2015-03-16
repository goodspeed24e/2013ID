
#ifndef MKV_MUXER_HPP
#define MKV_MUXER_HPP

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

#include "muxerimpl.hpp"

using namespace LIBMATROSKA_NAMESPACE;

class MkvMuxer : public Muxer
{
public:
	MkvMuxer();
	virtual ~MkvMuxer();
	virtual bool SetFileConfig(const FileConfig &);

public:
	// interface for event listeners
	virtual bool AddEventListener(const EventListener &);
	virtual bool RemoveEventListener(const EventListener &);

public:
	// interface for muxing A/V streams
	virtual bool StartMuxing(const Streams &, const Frame &, const char * = NULL);
	virtual bool StopMuxing();
	virtual bool AppendFrame(const Frame &);

public:
	// other public inner classes
	class MkvCodec : public MuxerImpl::Codec
	{
	public:
		enum
		{
			MAX_PRIVATE_DATA_SIZE = 120
		};

		MkvCodec(unsigned int _id, const char *_identifier, const unsigned char *_privateData, size_t _privateDataSize, bool _needFix = false, bool _needDelete = false)
			: Codec(_id), identifier(_identifier), privateData(_privateData), privateDataSize(_privateDataSize)
		{
			needFix = _needFix;
			needDelete = _needDelete;
		}

		virtual ~MkvCodec()
		{
			if (needDelete && (privateData != NULL))
			{
				delete[] privateData;
			}
		}

		const char          *identifier;
		const unsigned char *privateData;
		size_t               privateDataSize;
	};

	class MkvVideoCodec : public MkvCodec
	{
	public:
		MkvVideoCodec(unsigned int _id, const char *_identifier, const unsigned char *_privateData, size_t _privateDataSize, bool _needDelete = false) : MkvCodec(_id, _identifier, _privateData, _privateDataSize, _id == VideoStream::CODEC_ID_H264, _needDelete) {}
		virtual ~MkvVideoCodec() {}

		virtual Codec *         FixByFrame(const Frame &, Codec *) const;
		virtual unsigned char * FixFrameData(Frame &) const;
		virtual const Codec *   GetInstance(unsigned int id) const { return MkvVideoCodec::GetNewInstance(id); }

		static const Codec * GetNewInstance(unsigned int id) { if (id > VideoStream::CODEC_ID_MAX) id = VideoStream::CODEC_ID_DEFAULT;  return CODECS[id]; }
		static const Codec * const CODECS[];
	};

	class MkvAudioCodec : public MkvCodec
	{
	public:
		MkvAudioCodec(unsigned int _id, const char *_identifier, const unsigned char *_privateData, size_t _privateDataSize) : MkvCodec(_id, _identifier, _privateData, _privateDataSize) {}
		virtual ~MkvAudioCodec() {}

		virtual const Codec * GetInstance(unsigned int id) const { return MkvAudioCodec::GetNewInstance(id); }

		static const Codec * GetNewInstance(unsigned int id) { if (id > AudioStream::CODEC_ID_MAX) id = AudioStream::CODEC_ID_DEFAULT;  return CODECS[id]; }
		static const Codec * const CODECS[];
	};

	class MkvSubtitleCodec : public MkvCodec
	{
	public:
		MkvSubtitleCodec(unsigned int _id, const char *_identifier, const unsigned char *_privateData, size_t _privateDataSize) : MkvCodec(_id, _identifier, _privateData, _privateDataSize) {}
		virtual ~MkvSubtitleCodec() {}

		virtual const Codec * GetInstance(unsigned int id) const { return MkvSubtitleCodec::GetNewInstance(id); }

		static const Codec * GetNewInstance(unsigned int id) { if (id > SubtitleStream::CODEC_ID_MAX) id = SubtitleStream::CODEC_ID_DEFAULT;  return CODECS[id]; }
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
	State                state;
	const char          *pOutFileName;
	char                 myOutFileName[9];
	const EventListener *pListener;
	const Streams       *pStreams;

protected:
	// protected temporary members
	IOCallback    *pMKVFile;
	EbmlHead      *pMKVHead;
	KaxSegment    *pSegment;
	filepos_t      segmentSize;

	KaxSeekHead   *pMetaSeek;
	EbmlVoid      *pMetaSeekDummy;
	KaxCues       *pAllCues;
	EbmlVoid      *pAllCuesDummy;

	KaxTrackEntry *pVideoTrack;
	KaxTrackEntry *pAudioTrack;
	KaxTrackEntry *pSubtitleTrack;

	bool           isSuggest;
	bool           isFirstCluster;
	KaxCluster    *pCluster;
	KaxBlockBlob  *pBlockBlob;
	KaxBlockBlob  *pLastVideoBlockBlob;
	uint64         clusterMinTimecode;
	uint64         firstFrameTimecode;
	uint64         lastFrameTimecode;

	KaxBlockGroup *pLastSubtitleBlockGroup;
	uint64         lastSubtitleTimecode;
	uint64         lastSubtitlePosition;
	unsigned char *pSubtitleData;
	size_t         subtitleDataSize;
};

#endif  // MKV_MUXER_HPP
