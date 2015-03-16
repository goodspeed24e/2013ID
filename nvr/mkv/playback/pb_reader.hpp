
#ifndef PB_READER_H_
#define PB_READER_H_

#include <boost/shared_ptr.hpp>
#include <domain.hpp>
#include <timestamp.hpp>
//#include <hdvr/sql_proxy/sql_proxy_manager.hpp>

using namespace instek;

class PBReader
{
public:
	class Frame;
	virtual ~PBReader() {}
	virtual void start() {}
	virtual void stop() {}
	virtual bool seek(const CTimeValue &, const PlayDirection::retval, const PlaySpeed::retval) = 0;
	virtual instek::Codec::retval get_current_codec() = 0;
	virtual boost::shared_ptr<Frame> next() = 0;

	class Frame
	{
	public:
		class Header;
		virtual ~Frame() {}
		virtual const Header & GetFrameHeader() const = 0;
		virtual const unsigned char * GetFrameData(size_t &) const = 0;

		class Header
		{
		public:
			virtual ~Header() {}
			virtual instek::PacketType::retval GetFrameType() const = 0;
			virtual size_t GetDataSize() const = 0;
			virtual instek::CTimeValue GetTimestamp() const = 0;
		};
	};
};

#endif
