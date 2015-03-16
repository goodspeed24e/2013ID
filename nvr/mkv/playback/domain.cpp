
#include <domain.hpp>

namespace instek
{

//////////////////////////////////////////////////////////////////////
// class SignalType
//////////////////////////////////////////////////////////////////////
	
const SignalType::retval SignalType::NORMAL			= 0x01;
const SignalType::retval SignalType::NO_SIGNAL		= 0x02;
const SignalType::retval SignalType::VLOSS			= 0x03;
const SignalType::retval SignalType::HARDWARE_ERR	= 0x04;



//////////////////////////////////////////////////////////////////////
// class VideoStandard
//////////////////////////////////////////////////////////////////////
	
const VideoStandard::retval VideoStandard::NTSC		= 'N';
const VideoStandard::retval VideoStandard::PAL		= 'P';
const VideoStandard::retval VideoStandard::SECAM	= 'S';



//////////////////////////////////////////////////////////////////////
// class SlotBadType
//////////////////////////////////////////////////////////////////////
	
const SlotBadType::retval SlotBadType::SECTOR	= 0x01;
const SlotBadType::retval SlotBadType::FORMAT	= 0x02;



//////////////////////////////////////////////////////////////////////
// class CameraVendor
//////////////////////////////////////////////////////////////////////

const CameraVendor::retval CameraVendor::HIK_VISION	= 0x01;
const CameraVendor::retval CameraVendor::MESSOA		= 0x02;
const CameraVendor::retval CameraVendor::AXIS		= 0x03;
const CameraVendor::retval CameraVendor::PIXORD		= 0x04;



//////////////////////////////////////////////////////////////////////
// class Codec
//////////////////////////////////////////////////////////////////////

const Codec::retval Codec::NONE		= 0x00;
const Codec::retval Codec::HIKVISION 	= 0x01;
const Codec::retval Codec::MPEG4 	= 0x02;
const Codec::retval Codec::MJPEG	= 0x03;
const Codec::retval Codec::H264		= 0x04;
const Codec::retval Codec::HIK42VISION	= 0x05;


//////////////////////////////////////////////////////////////////////
// class PacketType
//////////////////////////////////////////////////////////////////////

const PacketType::retval PacketType::NONE	= 0x01;
const PacketType::retval PacketType::AUDIO	= 0x02;
const PacketType::retval PacketType::I		= 0x03;
const PacketType::retval PacketType::P		= 0x04;
const PacketType::retval PacketType::B		= 0x05;
const PacketType::retval PacketType::BP		= 0x06;
const PacketType::retval PacketType::BBP	= 0x07;
const PacketType::retval PacketType::CSH	= 0x08;
const PacketType::retval PacketType::OSD	= 0x09;
//const PacketType::retval PacketType::METADATA	= 0x0a;
const PacketType::retval PacketType::TWINCSH	= 0x80;
const PacketType::retval PacketType::TWINOSD	= 0x81;



//////////////////////////////////////////////////////////////////////
// class PlayDirection
//////////////////////////////////////////////////////////////////////

const PlayDirection::retval PlayDirection::FORWARD 	= 'F';
const PlayDirection::retval PlayDirection::BACKWARD	= 'B';



//////////////////////////////////////////////////////////////////////
// class PlaySpeed
//////////////////////////////////////////////////////////////////////

const PlaySpeed::retval PlaySpeed::X1 = '1';
const PlaySpeed::retval PlaySpeed::X2 = '2';
const PlaySpeed::retval PlaySpeed::X4 = '4';
const PlaySpeed::retval PlaySpeed::X8 = '8';



//////////////////////////////////////////////////////////////////////
// class SlotRetValue
//////////////////////////////////////////////////////////////////////

const SlotRetValue::retval SlotRetValue::OK			= 0x00;

const SlotRetValue::retval SlotRetValue::BOS		= 0x01;
const SlotRetValue::retval SlotRetValue::EOS		= 0x01;

const SlotRetValue::retval SlotRetValue::BOD		= 0x02;
const SlotRetValue::retval SlotRetValue::EOD		= 0x02;

const SlotRetValue::retval SlotRetValue::BOR		= 0x03;
const SlotRetValue::retval SlotRetValue::EOR		= 0x03;

const SlotRetValue::retval SlotRetValue::BOGOF		= 0x04;
const SlotRetValue::retval SlotRetValue::EOGOF		= 0x04;

const SlotRetValue::retval SlotRetValue::IO_ERR		= 0x05;

const SlotRetValue::retval SlotRetValue::FORMAT_ERR	= 0x06;

const SlotRetValue::retval SlotRetValue::INVALID	= 0x07;

const SlotRetValue::retval SlotRetValue::UNKNOWN	= 0x08;

const SlotRetValue::retval SlotRetValue::PREV_SLOT	= 0x09;
const SlotRetValue::retval SlotRetValue::CODEC_CHANGE	=0x0A;
const SlotRetValue::retval SlotRetValue::NO_AVAILABLE_DATA =0x0B;


//////////////////////////////////////////////////////////////////////
// class SlotState
//////////////////////////////////////////////////////////////////////

const SlotState::retval SlotState::FULL		= 'F';
const SlotState::retval SlotState::EMPTY	= 'E';
const SlotState::retval SlotState::USED		= 'U';
const SlotState::retval SlotState::BAD		= 'B';




//////////////////////////////////////////////////////////////////////
// class IAVFVersion
//////////////////////////////////////////////////////////////////////

const IAVFVersion::retval IAVFVersion::V1 = 0x01;
const IAVFVersion::retval IAVFVersion::V2 = 0x02;
const IAVFVersion::retval IAVFVersion::WINDOWS_V3 = 0x03;
const IAVFVersion::retval IAVFVersion::LINUX_V3 = 0x04;
const IAVFVersion::retval IAVFVersion::LINUX_V4 = 0x05;
const IAVFVersion::retval IAVFVersion::WINDOWS_V4 = 0x06;
const IAVFVersion::retval IAVFVersion::NEXTGEN_V1 = 0x80;



//////////////////////////////////////////////////////////////////////
// class RecordingType
//////////////////////////////////////////////////////////////////////

const RecordingType::retval RecordingType::RT_SR = 0x01;
const RecordingType::retval RecordingType::RT_VL = 0x02;
const RecordingType::retval RecordingType::RT_DI = 0x03;
const RecordingType::retval RecordingType::RT_MB = 0x04;
const RecordingType::retval RecordingType::RT_ME = 0x05;
const RecordingType::retval RecordingType::RT_MR = 0x06;



//////////////////////////////////////////////////////////////////////
// class BitRateControl
//////////////////////////////////////////////////////////////////////

const BitRateControl::retval BitRateControl::VBR	= 'V';
const BitRateControl::retval BitRateControl::CBR	= 'C';



//////////////////////////////////////////////////////////////////////
// class ControlFlag
//////////////////////////////////////////////////////////////////////

const ControlFlag::retval ControlFlag::ENABLE	= 'Y';
const ControlFlag::retval ControlFlag::DISABLE	= 'N';



//////////////////////////////////////////////////////////////////////
// class TriggerType
//////////////////////////////////////////////////////////////////////

const TriggerType::retval TriggerType::ALARM	= 'A';
const TriggerType::retval TriggerType::EVENT	= 'E';


//////////////////////////////////////////////////////////////////////
// class ClipType
//////////////////////////////////////////////////////////////////////

const ClipType::retval ClipType::ALARM		= 'A';
const ClipType::retval ClipType::EVENT		= 'E';
const ClipType::retval ClipType::MANUAL		= 'M';
const ClipType::retval ClipType::SCHEDULE	= 'S';
const ClipType::retval ClipType::SDK		= 'D';
const ClipType::retval ClipType::NO_SIGNAL	= 'N';	



//////////////////////////////////////////////////////////////////////
// class CommonResolution
//////////////////////////////////////////////////////////////////////

const CommonResolution::retval CommonResolution::D1  = 0x01;
const CommonResolution::retval CommonResolution::CIF = 0x02;
const CommonResolution::retval CommonResolution::DCIF= 0x03;
const CommonResolution::retval CommonResolution::QCIF= 0x04;


//////////////////////////////////////////////////////////////////////
// class CMDMsgVersion
//////////////////////////////////////////////////////////////////////

const CMDMsgVersion::retval CMDMsgVersion::V1	= 0x01;
const CMDMsgVersion::retval CMDMsgVersion::V2	= 0x02;


//////////////////////////////////////////////////////////////////////
// class CommandType
//////////////////////////////////////////////////////////////////////

const CommandType::retval CommandType::UNKNOWN		= 0x00;
const CommandType::retval CommandType::ACTIVE_REQ	= 0x01;
const CommandType::retval CommandType::ACTIVE_RES	= 0x02;
const CommandType::retval CommandType::PASSIVE_REQ	= 0x03;
const CommandType::retval CommandType::PASSIVE_RES	= 0x04;
const CommandType::retval CommandType::PASSIVE_ACK	= 0x05;


//////////////////////////////////////////////////////////////////////
// class CMDProtocolVersion
//////////////////////////////////////////////////////////////////////

const CMDProtocolVersion::retval CMDProtocolVersion::V1 	= 0x01;
const CMDProtocolVersion::retval CMDProtocolVersion::V2		= 0x02;


//////////////////////////////////////////////////////////////////////
// class NotifyType
//////////////////////////////////////////////////////////////////////

const NotifyType::retval NotifyType::ALARM			= 1 << 0;
const NotifyType::retval NotifyType::CONFIG			= 1 << 1;
const NotifyType::retval NotifyType::CAM_STATUS		= 1 << 2;
const NotifyType::retval NotifyType::ALL			= NotifyType::ALARM | 
	NotifyType::CONFIG | 
	NotifyType::CAM_STATUS;


//////////////////////////////////////////////////////////////////////
// class AuthInfo
//////////////////////////////////////////////////////////////////////

const AuthInfo::retval AuthInfo::LOGIN_OK			= 0x01;
const AuthInfo::retval AuthInfo::INEXISTENT_USER	= 0x02;
const AuthInfo::retval AuthInfo::PWD_INCORRECT		= 0x03;
const AuthInfo::retval AuthInfo::ACC_DISABLED		= 0x04;


//////////////////////////////////////////////////////////////////////
// class UserGroupType
//////////////////////////////////////////////////////////////////////

const UserGroupType::retval UserGroupType::UNKNOWN = -1;
const UserGroupType::retval UserGroupType::ADMIN = 0;
const UserGroupType::retval UserGroupType::MANAGER = 1;
const UserGroupType::retval UserGroupType::OPERATOR = 2;
const UserGroupType::retval UserGroupType::LIVESTREAM = 1000;
const UserGroupType::retval UserGroupType::PLAYBACK = 1000;
const UserGroupType::retval UserGroupType::ARCHIVE = 1000;
const UserGroupType::retval UserGroupType::PTZ = 1000;
const UserGroupType::retval UserGroupType::DO = 1000;

PacketType::retval translatePacketType(int pctType)
{
	if (pctType == PACKET_TYPE_AUDIO)
		return PacketType::AUDIO;
	else if (pctType == PACKET_TYPE_I)
		return PacketType::I;
	else if (pctType == PACKET_TYPE_P)
		return PacketType::P;
	else if (pctType == PACKET_TYPE_B)
		return PacketType::B;
	else if (pctType == PACKET_TYPE_BBP)
		return PacketType::BBP;
	else if (pctType == PACKET_TYPE_BP)
		return PacketType::BP;
	else if ( pctType == PACKET_TYPE_CSSH )
		return PacketType::CSH;
	else if ( pctType == PACKET_TYPE_TWINCSH )
		return PacketType::TWINCSH;
	else if ( pctType == PACKET_TYPE_OSD )
		return PacketType::OSD;
	else if ( pctType == PACKET_TYPE_TWINOSD )
		return PacketType::TWINOSD;
	/*else if ( pctType == PACKET_TYPE_METADATA)
		return PacketType::METADATA;*/
	else 
		return PacketType::NONE;
}

CPacketType translatePacketType(char pctType)
{
	if (pctType == PacketType::AUDIO)
		return PACKET_TYPE_AUDIO;
	else if (pctType == PacketType::I)
		return PACKET_TYPE_I;
	else if (pctType == PacketType::P)
		return PACKET_TYPE_P;
	else if (pctType == PacketType::B)
		return PACKET_TYPE_B;
	else if (pctType == PacketType::BBP)
		return PACKET_TYPE_BBP;
	else if (pctType == PacketType::BP)
		return PACKET_TYPE_BP;
	else if ( pctType == PacketType::CSH )
		return PACKET_TYPE_CSSH;
	else if ( pctType == PacketType::TWINCSH )
		return PACKET_TYPE_TWINCSH;
	else if ( pctType == PacketType::OSD )
		return PACKET_TYPE_OSD;
	else if ( pctType == PacketType::TWINOSD )
		return PACKET_TYPE_TWINOSD;
	/*else if ( pctType == PacketType::METADATA )
		return PACKET_TYPE_METADATA;*/
	else 
		return PACKET_TYPE_NONE;
}


};	//-- namespace instek



