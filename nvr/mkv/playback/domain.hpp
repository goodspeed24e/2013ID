
#if !defined(DOMAIN_HPP__CECE0FA8_966A_9635_B779_C2600963287D814__)
#define DOMAIN_HPP__CECE0FA8_966A_9635_B779_C2600963287D814__

#include "util_config.hpp"
//#include <util/packet.hpp>
namespace instek
{


/**
 * The signal type of the specify channel/camera
 */
class InstekUtil_Export SignalType
{
public:
	typedef char retval;

	static const retval NORMAL;
	static const retval NO_SIGNAL;
	static const retval VLOSS;
	static const retval HARDWARE_ERR;
};


/**
 * The video standard
 */
class InstekUtil_Export VideoStandard
{
public:
	typedef char retval;
	
	static const retval NTSC;
	static const retval PAL;
	static const retval SECAM;
};

/**
 * The destruction reason
 */
class InstekUtil_Export SlotBadType
{
public:
	typedef char retval;

	static const retval SECTOR;
	static const retval FORMAT;	
};

/**
 * The supported vendor
 */
class InstekUtil_Export CameraVendor
{
public:
	typedef char retval;

	static const retval HIK_VISION;
	static const retval MESSOA;
	static const retval AXIS;
	static const retval PIXORD;
};

/**
 * Supported Codec
 */
class InstekUtil_Export Codec
{
public:
	typedef char retval;

	static const retval NONE;
	static const retval HIKVISION;
	static const retval MPEG4;
	static const retval MJPEG;
	static const retval H264;
	static const retval HIK42VISION;
};

/**
 * Supported packet/frame type
 */
class InstekUtil_Export PacketType
{
public:
	typedef char retval;
	
	static const retval NONE;
	static const retval AUDIO;
	static const retval I;
	static const retval P;
	static const retval B;
	static const retval BP;
	static const retval BBP;
	static const retval CSH;
	static const retval OSD;
	//static const retval METADATA;
	static const retval TWINCSH;
	static const retval TWINOSD;
};

/**
 * The play direction
 */
class InstekUtil_Export PlayDirection
{
public:
	typedef char retval;
	
	static const retval FORWARD;
	static const retval BACKWARD;
};

/**
 * Play speed
 */
class InstekUtil_Export PlaySpeed
{
public:
	typedef char retval;

	static const retval X1;
	static const retval X2;
	static const retval X4;
	static const retval X8;
};

/**
 * All kind of return value of Slot related functions.
 */
class InstekUtil_Export SlotRetValue
{
public:
	typedef char retval;

	/// operate successful
	static const retval OK;
	
	/// Front/End of AV-Slot file
	static const retval BOS;
	static const retval EOS;
	
	/// Front/End of play/seek duration
	static const retval BOD;
	static const retval EOD;
	
	/// Front/End of recorded data (multiple AV-Slot files)
	static const retval BOR;
	static const retval EOR;
	
	/// Front/End of <GOF>
	static const retval BOGOF;
	static const retval EOGOF;
	
	/// IO error
	static const retval IO_ERR;
	
	/// Illegal IAV format
	static const retval FORMAT_ERR;
	
	/// Invalid or illegal input argument
	static const retval INVALID;
	
	/// Unknown error
	static const retval UNKNOWN;

	static const retval PREV_SLOT;
	static const retval CODEC_CHANGE;
	/// no data available in sqlite database (metadata)
	static const retval NO_AVAILABLE_DATA;
};

/**
 * To specify the state of AV-Slot file.
 */
class InstekUtil_Export SlotState
{
public:
	typedef unsigned char retval;
	
	/// The AV-Slot file was written fully.
	static const retval FULL;
	
	/// The AV-Slot file have never been used.
	static const retval EMPTY;

	/// Used AV-Slot file.
	static const retval USED;

	/// Bad AV-Slot file.
	static const retval BAD;
};

/**
 * The version number of IAV file
 */
class InstekUtil_Export IAVFVersion
{
public:
	typedef unsigned char retval;

	static const retval V1;
	static const retval V2;
	static const retval WINDOWS_V3;
	static const retval LINUX_V3;
	static const retval LINUX_V4;
	static const retval WINDOWS_V4;
	static const retval NEXTGEN_V1;
};


/**
 * The recording type (the type of recording request)
 */
class InstekUtil_Export RecordingType
{
public:
	typedef char retval;
	
	/// schedule recording
	static const retval RT_SR;

	/// video loss alarm
	static const retval RT_VL;

	/// DI alarm
	static const retval RT_DI;

	/// motion detection -- start recording
	static const retval RT_MB;

	/// motion detection -- end recording
	static const retval RT_ME;

	/// manual recording
	static const retval RT_MR;
};


/**
 * The bitrate control mode
 */
class InstekUtil_Export BitRateControl
{
public:
	typedef char retval;

	static const retval VBR;
	static const retval CBR;
};

/**
 * A kind of control flag, by using this flag to enable/disable something
 */
class InstekUtil_Export ControlFlag
{
public:
	typedef char retval;

	static const retval ENABLE;
	static const retval DISABLE;
};

/**
 * A flag used to recognize either Alarm or Event type.
 */
class InstekUtil_Export TriggerType
{
public:
	typedef char retval;
	
	/// Alarm type
	static const retval ALARM;

	/// Event type
	static const retval EVENT;
};

/**
 * To specify the type of AV-Clip
 */
class InstekUtil_Export ClipType
{
public:
	typedef char retval;
	
	/// Alarm recording (VL, DI, Motion)
	static const retval ALARM;

	/// Event recording (VL, DI, Motion)
	static const retval EVENT;
	
	/// Manual recording
	static const retval MANUAL;
	
	/// Schedule recording
	static const retval SCHEDULE;
	
	/// SDK recording
	static const retval SDK;
	
	/// No Signal (special clip-type)
	static const retval NO_SIGNAL;	
};

/**
 * The resolution of video
 */
class InstekUtil_Export CommonResolution
{
public:
	typedef char retval;
	
	static const retval D1;
	static const retval CIF;
	static const retval DCIF;
	static const retval QCIF;
};


/**
 * The version number of CMD message (XML message).
 */
class InstekUtil_Export CMDMsgVersion
{
public:
	typedef unsigned char retval;

	/// request message in ACTIVE command path
	static const retval V1;
	
	/// response message in ACTIVE command path
	static const retval V2;
};


/**
 * Indicate the type of command.
 */
class InstekUtil_Export CommandType
{
public:
	typedef unsigned char retval;

	/// Unknown command type
	static const retval UNKNOWN;
	
	/// request message in ACTIVE command path
	static const retval ACTIVE_REQ;
	
	/// response message in ACTIVE command path
	static const retval ACTIVE_RES;
	
	/// request message in PASSIVE command path
	static const retval PASSIVE_REQ;
	
	/// response message in PASSIVE command path
	static const retval PASSIVE_RES;
	
	/// acknowledgement (ACK) message in PASSIVE command path
	static const retval PASSIVE_ACK;
};



/**
 * The version number of Command protocol
 */
class InstekUtil_Export CMDProtocolVersion
{
public:
	typedef unsigned char retval;

	/// request message in ACTIVE command path
	static const retval V1;

	/// response message in ACTIVE command path
	static const retval V2;
};


/**
 * Indicate the type of notification message (in PASSIVE path).
 */
class InstekUtil_Export NotifyType
{
public:
	typedef unsigned int retval;

	/// video loss, motion, DI alarm
	static const retval ALARM;

	/// configuration changed
	static const retval CONFIG;

	/// camera status (enable, disable, vl, access right, recording)
	static const retval CAM_STATUS;
	
	static const retval ALL;
};


/**
 * The result of system log in (authentication)
 */
class InstekUtil_Export AuthInfo
{
public:
	typedef unsigned char retval;

	/// login succeed
	static const retval LOGIN_OK;

	/// login failed, because of inexistent user
	static const retval INEXISTENT_USER;

	/// login failed, because of incorrect password
	static const retval PWD_INCORRECT;

	/// login failed, because the account have been disabled
	static const retval ACC_DISABLED;
};


/**
 * The group type of user (role)
 */
class InstekUtil_Export UserGroupType
{
public:
	typedef int retval;

	/// unknown group type
	static const retval UNKNOWN;

	/// administrator
	static const retval ADMIN;

	/// manager
	static const retval MANAGER;

	/// operator
	static const retval OPERATOR;

	static const retval LIVESTREAM;
	static const retval PLAYBACK;
	static const retval ARCHIVE;
	static const retval PTZ;
	static const retval DO;
};

//content type header
enum CTHType{
	CTH_DISABLE 	= 0x00000000,
	CTH_AUDIO 	= 0x00000001,
	CTH_OSD 	= 0x00000002,
	CTH_VIDEO	= 0x00000004	//codec id
	//new CTHType must be the power of 2
	//CTH_NEW 	= 0x00000004
};

enum AudioCodecType
{
	AUDIO_CODEC_PCM = 0x00,
	AUDIO_CODEC_PCMU = 0x01,
	AUDIO_CODEC_L16 = 0x02,
	AUDIO_CODEC_HUNT_ADPCM = 0x03,
	AUDIO_CODEC_AMR = 0x04,
	AUDIO_CODEC_MPA = 0x05,
	AUDIO_CODEC_MP4 = 0x06,
	AUDIO_CODEC_PCMA = 0x07,
	AUDIO_CODEC_DEFAULT = 0xFF
};

enum CPacketType
{
        PACKET_TYPE_NONE = 0,
        PACKET_TYPE_AUDIO,
        PACKET_TYPE_I,
        PACKET_TYPE_P,
        PACKET_TYPE_B,
        PACKET_TYPE_BBP,
        PACKET_TYPE_BP,
        PACKET_TYPE_CSSH,
	PACKET_TYPE_OSD,
	PACKET_TYPE_METADATA,
	PACKET_TYPE_TWINCSH = 128,
	PACKET_TYPE_TWINOSD
};

enum CSignalType
{
	SIGNAL_TYPE_NORMAL,
	SIGNAL_TYPE_NO_SIGNAL,
	SIGNAL_TYPE_DISABLED,
	SIGNAL_TYPE_NETWORK_DATA_LOSS,
	SIGNAL_TYPE_HARDWARE_ERROR,
	SIGNAL_TYPE_NO_DATA
};

enum CAVTPacketType
{
	PACKET_TYPE_AVT_REQ,
	PACKET_TYPE_AVT_RESP,
	PACKET_TYPE_AVT_SEEK,
	PACKET_TYPE_AVT_STREAM_SETUP,
	PACKET_TYPE_AVT_STREAM_DATA,
	PACKET_TYPE_AVT_SET_PLAY_MODE,
	PACKET_TYPE_AVT_SET_AUDIO_ENABLED,
	PACKET_TYPE_AVT_STATUS_UPDATE_SUBSCRIBE,
	PACKET_TYPE_AVT_STATUS_UPDATE,
	PACKET_TYPE_AVT_QUERY_STATUS_UPDATE,
	PACKET_TYPE_AVT_SEEK_RESPONSE
};

enum CRespStatus
{
	AVREQ_RESPONCE_OK,
	AVREQ_RESPONCE_AUTH_FAILED
};

PacketType::retval InstekUtil_Export translatePacketType(int pctType);
CPacketType InstekUtil_Export translatePacketType(char pctType);

};	//-- namespace instek

#endif // DOMAIN_HPP__CECE0FA8_966A_9635_B779_C2600963287D814__



