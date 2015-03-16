
#ifndef WIN32
#include <signal.h>
#endif

#include <string>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/enable_shared_from_this.hpp>

#if defined(WIN32) && !defined(_WIN32_WINNT)
	#define _WIN32_WINNT 0x0501
#endif

#if (BOOST_VERSION > 104000)
	#include <boost/asio.hpp>
	#define asio boost::asio
	typedef boost::system::error_code asio_error_code;
#else
	#include <asio.hpp>
	typedef asio::error_code asio_error_code;
#endif

#include <domain.hpp>
//#include <hdvr/log/syslog.hpp>
//#include <hdvr/engine.hpp>
//#include <hdvr/sql_proxy/sql_proxy_manager.hpp>
//#include <hdvr/utility/wstring_util.hpp>
//#include <util/packet_buf.hpp>
//#include <hdvr/avfm/channel_storage_reader_wrapper.hpp>
#include <id_mkv_pb_reader.hpp>

#define PLAYBACK_VER 2
#define DEFAULT_PB_HANDLER_NUM 2

#ifdef slog
#undef slog
#endif
#define slog printf

#if (BOOST_VERSION > 104000)
	typedef boost::thread::id thread_id_type;

	static inline thread_id_type get_current_thread_id()
	{
		return boost::this_thread::get_id();
	}

	static inline void detach_current_thread()
	{
	}
#else
	typedef pid_t thread_id_type;

	static inline thread_id_type get_current_thread_id()
	{
		return getpid();
	}

	static inline void detach_current_thread()
	{
		pthread_detach(pthread_self());
	}
#endif

using namespace std;
using asio::ip::tcp;

static asio::io_service PB_IO_SERVICE;
static bool service_running = true;

thread_id_type main_thread_tid;

class pb_handler: public boost::enable_shared_from_this<pb_handler>
{
private:
	
	boost::thread* thread_;

	static boost::mutex create_thread_mutex_;
	static int HANDLER_NUM;
	static int SESSION_NUM;

	void run()
	{
		detach_current_thread();
		asio_error_code ec;
		int try_num = 0;

		for(;;)
		{
			size_t num = 0;
			try {
				num = PB_IO_SERVICE.run(ec);
			}
			catch (const std::out_of_range&)
			{
				if (pb_handler::SESSION_NUM < 2)
					slog("[Playback] session num < 2 and catch exception\n");

				break;
			}
			catch (...)
			{
				slog("[Playback] catch unknown exception in playback handler\n");
			}

			if (!service_running)
				break;

			if (++try_num < 5)
			{
				slog("[Playback] execute tasks: %d, error: %s", num, ec.message().c_str());
			}
			else
			{
				slog("[Playback] Fatal error -- execute tasks: %d, error: %s", num, ec.message().c_str());
				::exit(1);
			}
		}
	}
public:

	static void create_thread_on_demand()
	{
		boost::mutex::scoped_lock lock(create_thread_mutex_);
		++pb_handler::SESSION_NUM;

		if (pb_handler::HANDLER_NUM > 2)
			return;

		if (pb_handler::SESSION_NUM > (pb_handler::HANDLER_NUM * 3))
		{
			++HANDLER_NUM;
			boost::shared_ptr<pb_handler> handler(new pb_handler());
			handler->detach();
		}
	}

	static void destroy_thread_on_demand()
	{
		boost::mutex::scoped_lock lock(create_thread_mutex_);

		if ((--pb_handler::SESSION_NUM) < 0)
			pb_handler::SESSION_NUM = 0;

		if (pb_handler::HANDLER_NUM > DEFAULT_PB_HANDLER_NUM)
		{
			if (main_thread_tid == get_current_thread_id()) //main thread, just ignore it.
				return;

			if (((pb_handler::HANDLER_NUM * 3) > pb_handler::SESSION_NUM))
			{
				--pb_handler::HANDLER_NUM;
				throw std::out_of_range("delete this thread");
			}
		}
	}

	pb_handler():thread_(NULL)
	{
	}

	~pb_handler()
	{
		delete thread_;
	}
	void detach()
	{
		thread_ = new boost::thread(boost::bind(&pb_handler::run, shared_from_this()));
	}
};

boost::mutex pb_handler::create_thread_mutex_;
int pb_handler::HANDLER_NUM = DEFAULT_PB_HANDLER_NUM;
int pb_handler::SESSION_NUM = 0;

class pb_session : public boost::enable_shared_from_this<pb_session>
{
private:
	tcp::socket socket_;
	char buffer_[256]; // 0-227 for send, 228-255 for read.
	std::vector<asio::const_buffer> packet_buffers_;
	int camera_id_;
	CTimeValue seek_tv_;
	PlayDirection::retval play_direction_;
	PlaySpeed::retval play_speed_;
	boost::scoped_ptr<PBReader> reader_;
	bool seek_packet_;

	pb_session(): socket_(PB_IO_SERVICE), seek_packet_(false)
	{
		packet_buffers_.push_back(asio::buffer(buffer_, 28));
		//trivial buffer. it will be replaced in live_packet().
		packet_buffers_.push_back(asio::buffer(buffer_, 28));
		packet_buffers_.push_back(asio::buffer(buffer_ + 28, 8));
	}

public:
	typedef boost::shared_ptr<pb_session> pointer;

	static pointer create()
	{
		return pointer(new pb_session());
	}

	tcp::socket& socket()
	{
		return socket_;
	}

	void start()
	{
		pb_handler::create_thread_on_demand();

		// set socket keep alives
		asio::socket_base::keep_alive keep_alive_opt(true);
		asio_error_code ec;
		socket_.set_option(keep_alive_opt, ec);
		if(ec)
			slog("[PB] set socket keep-alive err[%d] %s\n", ec.value(), ec.message().c_str());

		// set linger to 0
		asio::socket_base::linger linger_opt(true, 0);
		socket_.set_option(linger_opt, ec);
		if(ec)
			slog("[PB] Set Socket linger Err[%d] %s\n", ec.value(), ec.message().c_str());


		asio::async_read(socket_, asio::buffer(buffer_, 8),
				boost::bind(&pb_session::read_header, shared_from_this(), asio::placeholders::error));
	}

	
	void read_header(const asio_error_code& err)
	{
		if(!err)
		{
			size_t request_type, len;

			memcpy(&request_type, buffer_, 4);
			memcpy(&len, buffer_ + 4, 4);
			if (request_type != PACKET_TYPE_AVT_REQ && len <= 0)
			{
				exit_session();
				return;
			}

			asio::async_read(socket_, asio::buffer(buffer_, len),
							boost::bind(&pb_session::read_rest_header, shared_from_this(), asio::placeholders::error));
		}
		else
		{
			exit_session();
		}
	}

	void read_rest_header(const asio_error_code& err)
	{
		if(!err)
		{
			int nread = 0;
			nread += 4; //protocol_ver
			int user_name_len;
			memcpy(&user_name_len, buffer_ + nread, 4);
			nread += 4;

			size_t inbytes = user_name_len * 2;
			//size_t outbytes = user_name_len * 4;
			//boost::scoped_array<char> outbuf(new char[outbytes]);
			//int convert_len = iconv_convert(const_cast<char*>("UTF-16LE"), buffer_ + nread, inbytes, const_cast<char*>("UTF-8"), outbuf.get(), outbytes);
			//std::string user_name(outbuf.get(), convert_len);
			nread += inbytes;

			int passwd_len;
			memcpy(&passwd_len, buffer_ + nread, 4);
			nread += 4;

			inbytes = passwd_len * 2;
			//outbytes = passwd_len * 4;
			//outbuf.reset(new char[outbytes]);
			//convert_len = iconv_convert(const_cast<char*>("UTF-16LE"), buffer_ + nread, inbytes, const_cast<char*>("UTF-8"), outbuf.get(), outbytes);
			//std::string user_pwd(outbuf.get(), convert_len);
			nread += inbytes;

			//timestamp:8
			nread += 8;

			this->camera_id_ = *((int*)(buffer_ + nread));
			//nread += 12; //camera_id:4, resolution:4 and streaming_mode:4

			int nwrite = 0;
			int var = PACKET_TYPE_AVT_RESP;
			memcpy(buffer_, &var, 4);
			nwrite += 4;

			var = 8; //len
			memcpy(buffer_ + nwrite, &var, 4);
			nwrite += 4;

			var = PLAYBACK_VER; //PLAYBACK_VER
			memcpy(buffer_ + nwrite, &var, 4);
			nwrite += 4;

			bool login_flag;
			if (1)//(this->handle_login(user_name, user_pwd))
			{
				var = AVREQ_RESPONCE_OK;
				login_flag = true;
			}
			else
			{
				var = AVREQ_RESPONCE_AUTH_FAILED;
				login_flag = false;
			}

			memcpy(buffer_ + nwrite, &var, 4);
			nwrite += 4;

			asio::async_write(socket_,
					asio::buffer(buffer_, nwrite),
					boost::bind(&pb_session::handle_login_reponse, shared_from_this(),
						login_flag, asio::placeholders::error));
		}
		else
		{
			exit_session();
		}
	}

	void handle_login_reponse(bool login_flag, const asio_error_code& err)
	{
		if (!err)
		{
			if (login_flag)
			{
				//this->reader_.reset(new PBChannelStorageReader(this->camera_id_, "../repos"));
				this->reader_.reset(new idmkv::Reader(this->camera_id_));
				asio::async_read(socket_,
				asio::buffer(buffer_, 28),
				boost::bind(&pb_session::handle_seek_request, shared_from_this(),
					asio::placeholders::error));
			}
			else
				deliver_none_packet(err);
		}
		else
			exit_session();
	}

	void handle_seek_request(const asio_error_code& err)
	{
		if (!err)
		{
			//skip pkt_type:4, pkt_size:4, PB_VER:4
			int* ts_sec = (int*) (buffer_ + 12);
			int* ts_msec = (int*) (buffer_ + 16);
			int* speed = (int*) (buffer_ + 20);
			int* direction = (int*) (buffer_ + 24);
			if (*ts_sec < 0 || *ts_msec < 0) {
				deliver_none_packet(err);
				return;
			}
			else if (*speed < 1 /*PB_SPEED_X1*/ || *speed > 8 /*PB_SPEED_X8*/) {
				deliver_none_packet(err);
				return;
			}
			else if (*direction != 0 /*PB_DIRECT_FORWARD*/ && *direction != 1 /*PB_DIRECT_BACKWARD*/) {
				deliver_none_packet(err);
				return;
			}

			this->seek_tv_ = CTimeValue(*ts_sec, (*ts_msec) * 1000);
			this->play_direction_ = ((*direction) == 0)? PlayDirection::FORWARD:PlayDirection::BACKWARD;
			switch (*speed)
			{
				case 1:
					this->play_speed_ = PlaySpeed::X1;
					break;

				case 2:
					this->play_speed_ = PlaySpeed::X2;
					break;

				case 4:
					this->play_speed_ = PlaySpeed::X4;
					break;

				case 8:
					this->play_speed_ = PlaySpeed::X8;
					break;

				default:
					exit_session();
					return;
			}

			slog("Channel(%d) SeekTime: %d\n", this->camera_id_, *ts_sec);

			bool seek_flag = this->reader_->seek(this->seek_tv_, this->play_direction_, this->play_speed_);
			if (seek_flag)
			{
				this->reader_->start();
			}

			int packet_type = PACKET_TYPE_AVT_SEEK_RESPONSE;
			memcpy(buffer_, &packet_type, 4);
			int payload_length = 4;
			memcpy(buffer_ + 4, &payload_length, 4);
			int protocol_ver = PLAYBACK_VER;
			memcpy(buffer_ + 8, &protocol_ver, 4);

			asio::async_write(socket_,
				asio::buffer(buffer_, 12),
				boost::bind(&pb_session::handle_seek_response, shared_from_this(), seek_flag,
					asio::placeholders::error));
		}
		else
			exit_session();
	}

	void handle_seek_response(bool seek_flag, const asio_error_code& err)
	{
		if (!err)
		{
			if(seek_flag)
			{
				asio::async_read(socket_,
					asio::buffer(buffer_ + 228, 28),
					boost::bind(&pb_session::receive_seek_packet, shared_from_this(),
						asio::placeholders::error));
				this->handle_data_msg(err);
			}
			else
			{
				this->deliver_fake_setup_msg();
			}
		}
		else
			exit_session();
	}
	
	void deliver_setup_msg(PBReader::Frame* csh_packet)
	{
		//CDecodeInfo decode_info(this->reader_->decode_info());
		int payload_length = 52 + csh_packet->GetFrameHeader().GetDataSize();//decode_info.GetCSHSize();

		int packet_type = PACKET_TYPE_AVT_STREAM_SETUP;
		memcpy(buffer_, &packet_type, 4);

		memcpy(buffer_ + 4, &payload_length, 4);

		int protocol_ver = PLAYBACK_VER;//ProtocolMsgHandler::PLAYBACK_VER;
		memcpy(buffer_ + 8, &protocol_ver, 4);

		int signal_type = SIGNAL_TYPE_NORMAL;
		memcpy(buffer_ + 20, &signal_type, 4);

		// Ignore field
		int resolution = 0;
		memcpy(buffer_ + 24, &resolution, 4);

		int play_mode;
		if (this->play_direction_ == PlayDirection::FORWARD) {
			play_mode = 0; //PLAY_MODE_FORWARD;
		}
		else if (this->play_direction_ == PlayDirection::BACKWARD) {
			play_mode = 1; //PLAY_MODE_BACKWARD;
		}
		else {
			deliver_none_packet(asio_error_code());
			return;
		}
		memcpy(buffer_ + 32, &play_mode, 4);

		int mode_option = 0;
		memcpy(buffer_ + 36, &mode_option, 4);

		int mode_option_value = 0;
		memcpy(buffer_ + 40, &mode_option_value, 4);

		//CTimeValue seek_start_tv(this->cached_frame_->GetFrameHeader().GetTimestamp());
		int tv_sec = csh_packet->GetFrameHeader().GetTimestamp().sec();//decode_info.GetCSHTimestamp();;
		int tv_msec = 0;

		memcpy(buffer_ + 44, &tv_sec, 4);

		memcpy(buffer_ + 48, &tv_msec, 4);

		size_t data_length;
		const unsigned char* data_buf = csh_packet->GetFrameData(data_length);

		int codec = this->reader_->get_current_codec();// = decode_info.GetCodecType();

		memcpy(buffer_ + 52, &codec, 4);

		//int cssh_hdr_len = decode_info.GetCSHSize();
		memcpy(buffer_ + 56, &data_length, 4);

		memcpy(buffer_ + 60, data_buf, data_length);
		asio::async_write(socket_,
			asio::buffer(buffer_, payload_length + 8),
			boost::bind(&pb_session::handle_data_msg, shared_from_this(),
			asio::placeholders::error));
	}

	void handle_data_msg(const asio_error_code& err)
	{
		if (!err)
		{
			if (seek_packet_)
			{
				seek_packet_ = false;
				this->reader_->stop();
				handle_seek_request(err);
				return;
			}

			boost::shared_ptr<PBReader::Frame> frame_ptr = this->reader_->next(); //use frame itself to judge the return condition
			if (frame_ptr.get())
			{
				if(frame_ptr->GetFrameHeader().GetFrameType() == PacketType::CSH)
				{
					this->deliver_setup_msg(frame_ptr.get());
					return;
				}

				int payload_length = 28 + frame_ptr->GetFrameHeader().GetDataSize();

				//segment 1: start offset: 0 size: 28
				int packet_type = PACKET_TYPE_AVT_STREAM_DATA;
				memcpy(buffer_, &packet_type, 4);
				memcpy(buffer_ + 4, &payload_length, 4);
				int protocol_ver = PLAYBACK_VER;//ProtocolMsgHandler::PLAYBACK_VER;
				memcpy(buffer_ + 8, &protocol_ver, 4);
				int frame_delay = 0;
				memcpy(buffer_ + 12, &frame_delay, 4);

				int frame_type;
				PacketType::retval pkt_type = frame_ptr->GetFrameHeader().GetFrameType();

				frame_type = instek::translatePacketType(pkt_type);
				if(frame_type == PACKET_TYPE_NONE)
				{
					deliver_none_packet(err);
					return;
				}


				memcpy(buffer_ + 16, &frame_type, 4);
				size_t data_length;
				const unsigned char* data_buf = frame_ptr->GetFrameData(data_length);
				memcpy(buffer_ + 24, &data_length, 4);

				//segment 2.
				packet_buffers_[1] = asio::buffer(data_buf, data_length);

				//segment 3 start offset: 28 size: 8
				CTimeValue frame_tv(frame_ptr->GetFrameHeader().GetTimestamp());
				int frame_tv_sec = frame_tv.sec();
				int frame_tv_msec = frame_tv.usec() / 1000;

				memcpy(buffer_ + 28, &frame_tv_sec, 4);
				memcpy(buffer_ + 32, &frame_tv_msec, 4);
				asio::async_write(socket_, packet_buffers_,
						boost::bind(&pb_session::handle_data_msg, shared_from_this(),
							asio::placeholders::error));
			}
			else
			{
				this->deliver_none_packet(asio_error_code());
			}
		}
		else
			exit_session();
	}

	void receive_seek_packet(const asio_error_code& err)
	{
		if (!err)
			seek_packet_ = true;
	}

	void deliver_fake_setup_msg()
	{
		int payload_length = 52;
		//buffer_.reset(new char[payload_length + 8]);

		int packet_type = PACKET_TYPE_AVT_STREAM_SETUP;
		memcpy(buffer_, &packet_type, 4);
		memcpy(buffer_ + 4, &payload_length, 4);
		int protocol_ver = PLAYBACK_VER;//ProtocolMsgHandler::PLAYBACK_VER;
		memcpy(buffer_ + 8, &protocol_ver, 4);
		int frame_height = 0;
		memcpy(buffer_ + 12, &frame_height, 4);
		int frame_width = 0;
		memcpy(buffer_ + 16, &frame_width, 4);

		int signal_type = SIGNAL_TYPE_NO_DATA;
		memcpy(buffer_ + 20, &signal_type, 4);

		// Ignore field
		int resolution = 0;
		memcpy(buffer_ + 24, &resolution, 4);

		int frame_rate = 0;
		memcpy(buffer_ + 28, &frame_rate, 4);

		int play_mode = 0;//PLAY_MODE_FORWARD;
		memcpy(buffer_ + 32, &play_mode, 4);

		int mode_option = 0;
		memcpy(buffer_ + 36, &mode_option, 4);

		int mode_option_value = 0;
		memcpy(buffer_ + 40, &mode_option_value, 4);

		CTimeValue seek_start_tv(instek::gettimeofday());
		int tv_sec = seek_start_tv.sec();
		int tv_msec = seek_start_tv.usec() / 1000;
		memcpy(buffer_ + 44, &tv_sec, 4);
		memcpy(buffer_ + 48, &tv_msec, 4);

		int codec = Codec::HIKVISION;
		memcpy(buffer_ + 52, &codec, 4);

		int cssh_hdr_len = 0;
		memcpy(buffer_ + 56, &cssh_hdr_len, 4);

		asio::async_write(socket_,
			asio::buffer(buffer_, 60),
			boost::bind(&pb_session::deliver_none_packet, shared_from_this(),
			asio::placeholders::error));
	}
	
	void deliver_none_packet(const asio_error_code& err)
	{
		if (!err)
		{
			int payload_length = 28;
			//buffer_.reset(new char[payload_length + 8]);

			int packet_type = PACKET_TYPE_AVT_STREAM_DATA;
			memcpy(buffer_, &packet_type, 4);
			memcpy(buffer_ + 4, &payload_length, 4);
			int protocol_ver = PLAYBACK_VER;//ProtocolMsgHandler::PLAYBACK_VER;
			memcpy(buffer_ + 8, &protocol_ver, 4);
			int frame_delay = 0;
			memcpy(buffer_ + 12, &frame_delay, 4);
			int frame_type = PACKET_TYPE_NONE;
			memcpy(buffer_ + 16, &frame_type, 4);
			//int sub_channel = FRAME_SUBCHANNEL_VIDEO;
			//::memcpy(buffer_.get() + 20, &sub_channel, 4);
			int video_length = 0;
			memcpy(buffer_ + 24, &video_length, 4);

			// no frame data should be sent

			CTimeValue frame_tv(instek::gettimeofday());
			int frame_tv_sec = frame_tv.sec();
			int frame_tv_msec = frame_tv.usec() / 1000;
			memcpy(buffer_ +  28, &frame_tv_sec, 4);
			memcpy(buffer_ + 32, &frame_tv_msec, 4);
			asio::async_write(socket_,
					asio::buffer(buffer_, 36),
					boost::bind(&pb_session::deliver_none_packet, shared_from_this(),
					asio::placeholders::error));
		}
		else
			exit_session();
	}

	void exit_session()
	{
		asio_error_code ec;
		socket_.close(ec);
		if(ec)
			slog("[PB] Socket Close Forcibly Err[%d] %s\n", ec.value(), ec.message().c_str());

		pb_handler::destroy_thread_on_demand();
	}

	bool handle_login(const std::string& name, const std::string& pwd)
	{
		//CSQLProxy dbconn;
		//dbconn.Open();
		//return dbconn.AuthenticateUserByResourceID(name, pwd, "Playback", this->camera_id_);
		return true;
	}
};


class pb_server
{
public:
	pb_server()
	    : acceptor_(PB_IO_SERVICE)
	{
		tcp::endpoint listen_endpoint(tcp::v4(), 60006);
		acceptor_.open(listen_endpoint.protocol());
		asio::socket_base::enable_connection_aborted enable_opt(true);
		acceptor_.set_option(enable_opt);
		acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
		acceptor_.bind(listen_endpoint);
		acceptor_.listen();

		start_accept();
	}

private:
	void start_accept()
	{
		
		pb_session::pointer new_connection =
			pb_session::create();
		
		acceptor_.async_accept(new_connection->socket(),
			boost::bind(&pb_server::handle_accept, this, new_connection,
				asio::placeholders::error));
	}

	void handle_accept(pb_session::pointer new_connection,
		const asio_error_code& error)
	{
		if (!error)
			new_connection->start();
		else
			slog("[PB] acceptor err[%d] %s\n", error.value(), error.message().c_str());

		start_accept();
	}

	tcp::acceptor acceptor_;
};

#ifndef WIN32
void SignalHandler(int signum)
{
	printf("Receiver an unix signal, signum (%d).\n", signum);
	service_running = false;
	PB_IO_SERVICE.stop();
}

static void CatchSignal(int sigNum)
{
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));

        sa.sa_handler = SignalHandler;
        if (sigaction(sigNum, &sa, NULL))
                ::exit(1);
}
#endif

int main(int argc,char *argv[])
{
	//SystemLog::open("pb_server");
	slog("playback server start\n");

#ifndef WIN32
	::setlocale(LC_CTYPE, "en_US");
	::system("rm -rf /dev/shm/*.idb");
#endif

	try
	{
#ifndef WIN32
		CatchSignal(SIGTERM);
		CatchSignal(SIGINT);
#endif

		main_thread_tid = get_current_thread_id();

		pb_server server;

		{
			boost::shared_ptr<pb_handler> handler(new pb_handler());
			handler->detach();
		}

		asio_error_code ec;
		int try_num = 0;

		for(;;)
		{
			size_t num = 0;
			try {
				num = PB_IO_SERVICE.run(ec);
			}
			catch (...)
			{
				slog("[Playback] catch unknown exception in playback handler\n");
			}

			if (!service_running)
				break;

			if (++try_num < 5)
			{
				slog("[Playback] execute tasks: %d, error: %s", num, ec.message().c_str());
			}
			else
			{
				slog("[Playback] Fatal error -- execute tasks: %d, error: %s", num, ec.message().c_str());
				::exit(1);
			}
		}
	}
	catch (std::exception& e)
	{
		slog("Catch An Exception: %s\n", e.what());
	}

	//wait a while, to make sure all threads are terminated.
	boost::xtime xt;
	boost::xtime_get(&xt, boost::TIME_UTC);
	xt.sec += 1;
	boost::thread::sleep(xt);

#ifndef WIN32
	::system("rm -rf /dev/shm/*.idb");
#endif
	slog("playback server stop\n");
	//SystemLog::close();

	return 0;
}
