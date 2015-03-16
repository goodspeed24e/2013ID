
#ifndef ID_MKV_PB_READER_H_
#define ID_MKV_PB_READER_H_

#include <stdio.h>
#include <list>
#include "pb_reader.hpp"
#include "streaming_media_library.hpp"
#include "demuxer.hpp"

namespace idmkv
{

class Reader : public PBReader
{
private:
	class MyFrame : public Frame::Header, public Frame
	{
	private:
		friend class Reader;

		instek::PacketType::retval type_;
		instek::CTimeValue         timestamp_;
		unsigned char             *data_;
		size_t                     dataSize_;

	public:
		MyFrame() : type_(instek::PacketType::NONE), timestamp_(0L), data_(NULL), dataSize_(0) {}

		virtual ~MyFrame()
		{
			if (data_ != NULL)
			{
				//delete data_;  // this will be deleted by demuxer.hpp
			}
		}

		virtual instek::PacketType::retval GetFrameType() const
		{
			return type_;
		}

		virtual size_t GetDataSize() const
		{
			return dataSize_;
		}

		virtual instek::CTimeValue GetTimestamp() const
		{
			return timestamp_;
		}

		virtual const Frame::Header & GetFrameHeader() const
		{
			return *this;
		}

		virtual const unsigned char * GetFrameData(size_t &length) const
		{
			length = dataSize_;
			return data_;
		}
	};

private:
	int                        channel_;

	StreamingMediaLibrary     *pLibrary_;
	StreamingMediaChannelHelper *pHelper_;
	PlayDirection::retval      direction_;
	PlaySpeed::retval          speed_;
	CTimeValue                 seek_tv_;

	bool                       isStopped_;
	Demuxer                   *pDemuxer_;
	const Demuxer::Streams    *pStreams_;
	instek::Codec::retval      codec_;

	bool                                   isBackwardFrameListReady_;
	std::list<boost::shared_ptr<MyFrame> > backwardFrameList_;
	std::list<boost::shared_ptr<MyFrame> > backwardOsdList_;
	unsigned char                         *lastBackwardFrameData_;

	#define INPUT_FILE_NAME   "test.mkv"
	char                       filename_[256];

	boost::shared_ptr<MyFrame> nullFrame_;

	size_t                     bufferSize_;
	unsigned char             *pBuffer_;

	unsigned char * GetBuffer(size_t size)
	{
		if (size <= bufferSize_)
		{
			return pBuffer_;
		}
		if (pBuffer_ != NULL)
		{
			delete pBuffer_;
		}
		pBuffer_ = new unsigned char[size];
		bufferSize_ = (pBuffer_ == NULL) ? 0 : size;
		return pBuffer_;
	}

public:
	Reader(int channel)
		: channel_(channel),
		  direction_(PlayDirection::FORWARD), speed_(PlaySpeed::X1), seek_tv_(0),
		  isStopped_(false), pDemuxer_(NULL), pStreams_(NULL), codec_(instek::Codec::NONE),
		  isBackwardFrameListReady_(false), lastBackwardFrameData_(NULL), bufferSize_(0), pBuffer_(NULL)
	{
		pLibrary_ = new StreamingMediaLibrary();
		pHelper_ = &pLibrary_->CreateChannelHelper(channel);
		filename_[0] = '\0';
	}

	virtual ~Reader()
	{
		isStopped_ = true;

		if (pDemuxer_ != NULL)
		{
			delete pDemuxer_;
		}

		if (pBuffer_ != NULL)
		{
			delete pBuffer_;
		}

		while (!backwardFrameList_.empty())
		{
			if (lastBackwardFrameData_ != NULL)
			{
				delete[] lastBackwardFrameData_;
			}
			lastBackwardFrameData_ = backwardFrameList_.back()->data_;
			backwardFrameList_.pop_back();
		}
		backwardFrameList_.clear();
		backwardOsdList_.clear();

		if (lastBackwardFrameData_ != NULL)
		{
			delete[] lastBackwardFrameData_;
		}
	}

	virtual void start() {}
	virtual void stop() {}

	virtual bool seek(const CTimeValue &seek_tv, const PlayDirection::retval direction, const PlaySpeed::retval speed)
	{
		seek_tv_   = seek_tv;
		direction_ = direction;
		speed_     = speed;

		const StreamingMediaFile &media = (direction_ != PlayDirection::BACKWARD) ? pHelper_->LocateMediaFileForwardly(seek_tv_.sec()) : pHelper_->LocateMediaFileBackwardly(seek_tv_.sec());
		if ((media.startTime != 0) && (media.GetFileName() != NULL))
		{
			strcpy(filename_, media.GetFileName());
		}
		else
		{
			strcpy(filename_, INPUT_FILE_NAME);
			return false;
		}

		return true;
	}

	virtual instek::Codec::retval get_current_codec()
	{
		return codec_;
	}

	virtual boost::shared_ptr<Frame> next()
	{
		// end of file
		if (isStopped_)
		{
			return nullFrame_;
		}

		// open file
		if (pDemuxer_ == NULL)
		{
			pDemuxer_ = DemuxerUtilities::CreateMkvDemuxer();
			if (pDemuxer_ == NULL)
			{
				// error: fail to create demuxer
				isStopped_ = true;
				return nullFrame_;
			}

restart:
			pDemuxer_->StopDemuxing();
			pStreams_ = pDemuxer_->StartDemuxing(filename_, direction_ == PlayDirection::BACKWARD ? 0ull : (uint64_t)seek_tv_.sec() * 1000000000ull + (uint64_t)seek_tv_.usec() * 1000ull);
			if ((pStreams_ == NULL) || !pStreams_->HasVideo())
			{
				// error: fail to open file or bad mkv file format
				isStopped_ = true;
				delete pDemuxer_;
				pDemuxer_ = NULL;
				return nullFrame_;
			}

			size_t spsSize = 0;
			size_t ppsSize = 0;
			size_t extraSize = 0;

			// extract sps & pps from codec private for h.264
			if ((pStreams_->pVideo->codec == 4)
			    && (pStreams_->pVideo->pCodecPrivate != NULL)
				&& (pStreams_->pVideo->codecPrivateSize >= 11))
			{
				spsSize = (pStreams_->pVideo->pCodecPrivate[6] << 8)
				          + pStreams_->pVideo->pCodecPrivate[7];

				if (pStreams_->pVideo->codecPrivateSize >= spsSize + 11)
				{
					ppsSize = (pStreams_->pVideo->pCodecPrivate[spsSize + 9] << 8)
					          + pStreams_->pVideo->pCodecPrivate[spsSize + 10];

					if (pStreams_->pVideo->codecPrivateSize < spsSize + ppsSize + 11)
					{
						// something wrong
						ppsSize = 0;
					}
				}
				else
				{
					// something wrong
					spsSize = 0;
				}
			}

			extraSize = (spsSize ? spsSize + 4 : 0) + (ppsSize ? ppsSize + 4 : 0);

			// initialize the result object
			boost::shared_ptr<MyFrame> frame(new MyFrame());

			frame->type_       = instek::PacketType::CSH;
			frame->timestamp_.set((double)pStreams_->dateUTC + (direction_ == PlayDirection::BACKWARD ? pStreams_->duration : 0.0));
			frame->dataSize_   = (pStreams_->HasAudio() ? 50 : 49)
			                     + (extraSize > 39 ? extraSize : 0);
			frame->data_       = new unsigned char[frame->dataSize_];

			// compose sps & pps for h.264
			if (extraSize > 0)
			{
				frame->data_[0] = extraSize;

				unsigned char *ptr = frame->data_
				                     + (frame->data_[0] <= 39 ? 1
				                        : pStreams_->HasAudio() ? 50 : 49);

				if (spsSize)
				{
					ptr[0] = 0;
					ptr[1] = 0;
					ptr[2] = 0;
					ptr[3] = 1;
					memcpy(ptr + 4, pStreams_->pVideo->pCodecPrivate + 8, spsSize);
					ptr += spsSize + 4;
				}

				if (ppsSize)
				{
					ptr[0] = 0;
					ptr[1] = 0;
					ptr[2] = 0;
					ptr[3] = 1;
					memcpy(ptr + 4, pStreams_->pVideo->pCodecPrivate + spsSize + 11, ppsSize);
				}
			}

			// compose a csh packet
			unsigned char *ptr = frame->data_ + 40;
			*(int *)ptr = instek::CTH_VIDEO;
			if (pStreams_->HasAudio())
			{
				*(int *)ptr |= instek::CTH_AUDIO;
				ptr[4] = pStreams_->pAudio->codec;
				ptr++;
			}
			ptr += 4;
			*(int *)ptr = 1;
			ptr += 4;
			*ptr = pStreams_->pVideo->codec;

			// translate codec id
			switch (pStreams_->pVideo->codec)
			{
			case 4:
				codec_ = instek::Codec::H264;
				break;
			case 3:
				codec_ = instek::Codec::MJPEG;
				break;
			case 2:
				codec_ = instek::Codec::MPEG4;
				break;
			default:
				codec_ = instek::Codec::NONE;
				break;
			}

			return frame;
		}

		if ((direction_ != PlayDirection::BACKWARD) || !isBackwardFrameListReady_)
		{
			// get one frame by demuxer
			const Demuxer::Frame *pFrame;
			while ((pFrame = pDemuxer_->GetOneFrame()) != NULL)
			{
				// initialize the result object
				boost::shared_ptr<MyFrame> frame(new MyFrame());

				frame->type_ = (pFrame->pStream->codecType == Demuxer::Stream::CODEC_TYPE_AUDIO) ? instek::PacketType::AUDIO
				               : (pFrame->pStream->codecType == Demuxer::Stream::CODEC_TYPE_SUBTITLE) ? instek::PacketType::OSD
				               : (pFrame->isKey) ? instek::PacketType::I
							   : instek::PacketType::P;
				frame->timestamp_.set(pFrame->timecode / 1000000000ull, pFrame->timecode / 1000ull % 1000000ull);
				if (frame->type_ != instek::PacketType::OSD)
				{
					frame->dataSize_ = pFrame->size;
					frame->data_     = pFrame->data;
				}
				else
				{
					// compose an osd packet
					frame->dataSize_ = pFrame->size + 5;
					frame->data_     = GetBuffer(frame->dataSize_);
					if (frame->data_ == NULL)
					{
						// error: fail to allocate memory
						isStopped_ = true;
						return nullFrame_;
					}
					frame->data_[0] = pStreams_->HasAudio() ? 0x03 : 0x01;     // show byte (1)
					frame->data_[1] = 0x00;                                    // presentation byte (1)
					*(unsigned short *)(frame->data_ + 2) = pFrame->size + 1;  // name length (2)
					memcpy(frame->data_ + 4, pFrame->data, pFrame->size);      // data
					frame->data_[frame->dataSize_ - 1] = '\0';
				}

				if (direction_ != PlayDirection::BACKWARD)
				{
					/*if ((frame->timestamp_ < seek_tv_) && (frame->type_ != instek::PacketType::OSD))
					{
						// skip current frame
						continue;
					}*/

					return frame;
				}
				else
				{
					if (frame->timestamp_ >= seek_tv_)
					{
						// skip all following frames
						break;
					}

					unsigned char *newBuffer = new unsigned char[frame->dataSize_];
					memcpy(newBuffer, frame->data_, frame->dataSize_);
					frame->data_ = newBuffer;
					backwardFrameList_.push_back(frame);
					if (frame->type_ == instek::PacketType::OSD)
					{
						backwardOsdList_.push_back(frame);
					}
				}
			}

			if (direction_ != PlayDirection::BACKWARD)
			{
				// something wrong
				// try to seek to the next file
				const StreamingMediaFile &media = pHelper_->LocateNextMediaFile();
				if ((media.startTime != 0) && (media.GetFileName() != NULL))
				{
					strcpy(filename_, media.GetFileName());
					seek_tv_.set(media.startTime, 0);
					goto restart;
				}
				else
				{
					strcpy(filename_, INPUT_FILE_NAME);
				}

				return nullFrame_;
			}
			else
			{
				isBackwardFrameListReady_ = true;
				if (!backwardOsdList_.empty())
				{
					boost::shared_ptr<MyFrame> frame = backwardOsdList_.back();
					backwardOsdList_.pop_back();
					return frame;
				}
			}
		}

		while (!backwardFrameList_.empty())
		{
			boost::shared_ptr<MyFrame> frame = backwardFrameList_.back();
			backwardFrameList_.pop_back();

			if (lastBackwardFrameData_ != NULL)
			{
				delete[] lastBackwardFrameData_;
			}
			lastBackwardFrameData_ = frame->data_;

			if (frame->type_ != instek::PacketType::OSD)
			{
				return frame;
			}
			else if (!backwardOsdList_.empty())
			{
				frame = backwardOsdList_.back();
				backwardOsdList_.pop_back();
				return frame;
			}
		}
		backwardFrameList_.clear();
		backwardOsdList_.clear();

		// try to seek to the next file
		const StreamingMediaFile &media = pHelper_->LocatePreviousMediaFile();
		if ((media.startTime != 0) && (media.GetFileName() != NULL))
		{
			strcpy(filename_, media.GetFileName());
			seek_tv_.set(media.endTime, 0);
			isBackwardFrameListReady_ = false;
			goto restart;
		}
		else
		{
			strcpy(filename_, INPUT_FILE_NAME);
		}

		return nullFrame_;
	}

	#undef INPUT_FILE_NAME
};

};

#endif
