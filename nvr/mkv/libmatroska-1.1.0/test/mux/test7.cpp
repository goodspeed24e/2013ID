
#ifdef _MSC_VER
#include <windows.h>      // for min/max
#endif // _MSC_VER

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

using namespace LIBMATROSKA_NAMESPACE;
using namespace std;

// the data structure for all kind of frames
struct Frame
{
	bool isAudio;
	bool isKey;
	uint64 timecode;
	size_t size;
	char * data;
};

// a list of all video codec id
enum VideoCodecId
{
	VIDEO_CODEC_DEFAULT   = 0,
	VIDEO_CODEC_RAWDATA   = 0,
	VIDEO_CODEC_HIKVISION = 1,
	VIDEO_CODEC_MPEG4     = 2,
	VIDEO_CODEC_MJPEG     = 3,
	VIDEO_CODEC_H264      = 4
};

// file parameters
static const char          *  OUTPUT_FILE_NAME           = "test.mkv";
static const char          *  INPUT_FILE_NAME            = "test.bin";
static const uint64           TIMECODE_SCALE             = 1000000u;  // 1,000,000 means all timecodes in the segment are expressed in milliseconds
static const int              META_SEEK_ELEMENT_SIZE     = 300;  // 300 octets
static const double           SEGMENT_DEFAULT_DURATION   = 0.0;
static const wchar_t       *  APPLICATION_NAME           = L"test7";
static const bool            bWriteDefaultValues         = false;
static unsigned int           SegmentDate                = 0;

// cueing data element parameters
static const double           VIDEO_CUE_THRESHOLD        = 5.0 * 1000000000.0;  // 5 seconds
static const double           SEGMENT_MAX_DURATION       = 600.0;  // 600 sec
static const int              CUEING_DATA_ELEMENT_SIZE   = ((int)(SEGMENT_MAX_DURATION / (VIDEO_CUE_THRESHOLD / 1000000000.0)) + 1) * 20 + 200;
static const uint64           BLOCK_MAX_DURATION         = (unsigned long long)SEGMENT_MAX_DURATION * 1000000000ull;

// default tracks parameters
static const unsigned int     VIDEO_TRACK_NUMBER         = 1;
static const unsigned int     AUDIO_TRACK_NUMBER         = 2;
static const unsigned int     SUBTITLE_TRACK_NUMBER      = 3;
static const bool             HasVideoTrack              = true;  // always true
static bool                   HasAudioTrack              = false;
static bool                   HasSubtitleTrack           = false;
static const char          *  TRACK_LANGUAGE             = "eng";

// default video track parameters
static double                 VideoTrackTimecodeBase     = 0.0;
static VideoCodecId           VideoTrackCodecId          = VIDEO_CODEC_DEFAULT;
static const char          * pVideoTrackCodec            = "V_UNCOMPRESSED";
static const unsigned char * pVideoTrackCodecPrivate     = NULL;
static size_t                 VideoTrackCodecPrivateSize = 0;
//static unsigned int           VideoTrackFrameRate        = 30;
//static uint64                 VideoTrackFrameDuration    = 1000000000u / VideoTrackFrameRate;
static unsigned int           VideoTrackFrameWidth       = 640;
static unsigned int           VideoTrackFrameHeight      = 480;

// default audio track parameters
static const char          * pAudioTrackCodec            = "A_PCM/INT/LIT";
static const unsigned char * pAudioTrackCodecPrivate     = NULL;
static size_t                 AudioTrackCodecPrivateSize = 0;
//static unsigned int           AudioTrackFrameRate        = 30;
//static uint64                 AudioTrackFrameDuration    = 1000000000u / AudioTrackFrameRate;
static double                 AudioTrackSamplingRate     = 8000.0;
static unsigned int           AudioTrackChannels         = 1;
static unsigned int           AudioTrackBitDepth         = 8;

// default subtitle track parameters
static const char          * pSubtitleTrackCodec         = "S_TEXT/UTF8";

#include "my_tools.cc"

int main(int argc, char **argv)
{
    cout << "Creating \"" << OUTPUT_FILE_NAME << "\"" << endl;

    try
	{
		// write the head of the file (with everything already configured)
		StdIOCallback out_file(OUTPUT_FILE_NAME, MODE_CREATE);

		// Writing EBML test
		{
			EbmlHead FileHead;

			*static_cast<EbmlString *>(&GetChild<EDocType>(FileHead)) = "matroska";
			*static_cast<EbmlUInteger *>(&GetChild<EDocTypeVersion>(FileHead)) = MATROSKA_VERSION;
			*static_cast<EbmlUInteger *>(&GetChild<EDocTypeReadVersion>(FileHead)) = MATROSKA_VERSION;

			FileHead.Render(out_file, true);
		}

		KaxSegment FileSegment;

		// size is unknown and will always be, we can render it right away
		// 5 octets can represent 2^35, it is wide enough for almost any file
		filepos_t SegmentSize = FileSegment.WriteHead(out_file, 5, bWriteDefaultValues);
		filepos_t MetaSeekSize;
		filepos_t InfoSize;
		filepos_t TrackSize;
		filepos_t CueSize;
		filepos_t ClusterSize = 0;

		// reserve some space for the Meta Seek writen at the end
		KaxSeekHead MetaSeek;
		EbmlVoid MetaSeekDummy;
		{
			MetaSeekDummy.SetSize(META_SEEK_ELEMENT_SIZE);
			MetaSeekDummy.Render(out_file, bWriteDefaultValues);
		}

		PeekFirstFrame();

		// fill the mandatory Info section
		KaxInfo & MyInfos = GetChild<KaxInfo>(FileSegment);
		{
			*static_cast<EbmlUInteger *>(&GetChild<KaxTimecodeScale>(MyInfos)) = TIMECODE_SCALE;
			*static_cast<EbmlFloat *>(&GetChild<KaxDuration>(MyInfos)) = SEGMENT_DEFAULT_DURATION;

			std::string MuxingAppString = "libebml v";
			MuxingAppString += EbmlCodeVersion.c_str();
			MuxingAppString += " + libmatroska v";
			MuxingAppString += KaxCodeVersion.c_str();
			UTFstring MuxingAppUTFstring;
			MuxingAppUTFstring.SetUTF8(MuxingAppString);
			*(EbmlUnicodeString *)&GetChild<KaxMuxingApp>(MyInfos)  = MuxingAppUTFstring;
			*(EbmlUnicodeString *)&GetChild<KaxWritingApp>(MyInfos) = APPLICATION_NAME;

			//GetChild<KaxDateUTC>(MyInfos).SetEpochDate(time(NULL));  // get current time
			GetChild<KaxDateUTC>(MyInfos).SetEpochDate(SegmentDate);  // must call PeekFirstFrame() first

			unsigned char SegmentUIDBuffer[128 / 8];
			GenerateRandomBytes(SegmentUIDBuffer, 128 / 8);
			GetChild<KaxSegmentUID>(MyInfos).CopyBuffer(SegmentUIDBuffer, 128 / 8);

			InfoSize = MyInfos.Render(out_file, true);
			MetaSeek.IndexThis(MyInfos, FileSegment);
		}

		KaxTracks & AllTracks = GetChild<KaxTracks>(FileSegment);

		// fill video track params
		KaxTrackEntry & VideoTrack = GetChild<KaxTrackEntry>(AllTracks);
		{
			VideoTrack.SetGlobalTimecodeScale(TIMECODE_SCALE);

			*static_cast<EbmlUInteger *>(&GetChild<KaxTrackNumber>(VideoTrack)) = VIDEO_TRACK_NUMBER;
			*static_cast<EbmlUInteger *>(&GetChild<KaxTrackUID>(VideoTrack)) = GenerateRandomUInteger();
			*static_cast<EbmlUInteger *>(&GetChild<KaxTrackType>(VideoTrack)) = track_video;

			//*static_cast<EbmlUInteger *>(&GetChild<KaxTrackMinCache>(VideoTrack)) = 1;
			//*static_cast<EbmlUInteger *>(&GetChild<KaxTrackDefaultDuration>(VideoTrack)) = VideoTrackFrameDuration;
			*static_cast<EbmlString *>(&GetChild<KaxTrackLanguage>(VideoTrack)) = TRACK_LANGUAGE;

			*static_cast<EbmlString *>(&GetChild<KaxCodecID>(VideoTrack)) = pVideoTrackCodec;
			if (pVideoTrackCodecPrivate != NULL)
			{
				GetChild<KaxCodecPrivate>(VideoTrack).CopyBuffer(pVideoTrackCodecPrivate, VideoTrackCodecPrivateSize);
			}

			VideoTrack.EnableLacing(false);

			// video specific params
			KaxTrackVideo & VideoTrackParameters = GetChild<KaxTrackVideo>(VideoTrack);
			*static_cast<EbmlUInteger *>(&GetChild<KaxVideoPixelWidth>(VideoTrackParameters)) = VideoTrackFrameWidth;
			*static_cast<EbmlUInteger *>(&GetChild<KaxVideoPixelHeight>(VideoTrackParameters)) = VideoTrackFrameHeight;

			// content encoding params
			if (VideoTrackCodecId == VIDEO_CODEC_H264)
			{
				KaxContentEncodings & VideoTrackContentEncodings = GetChild<KaxContentEncodings>(VideoTrack);
				KaxContentEncoding & VideoTrackContentEncoding = GetChild<KaxContentEncoding>(VideoTrackContentEncodings);
				KaxContentCompression & VideoTrackContentComp = GetChild<KaxContentCompression>(VideoTrackContentEncoding);

				unsigned char VideoTrackContentCompSettings[] = { 0 };
				*static_cast<EbmlUInteger *>(&GetChild<KaxContentCompAlgo>(VideoTrackContentComp)) = 3;
				GetChild<KaxContentCompSettings>(VideoTrackContentComp).CopyBuffer(VideoTrackContentCompSettings, 1);
			}
		}

		// fill audio track params
		KaxTrackEntry & AudioTrack = HasAudioTrack ? GetNextChild<KaxTrackEntry>(AllTracks, VideoTrack) : VideoTrack;
		if (HasAudioTrack)
		{
			AudioTrack.SetGlobalTimecodeScale(TIMECODE_SCALE);

			*static_cast<EbmlUInteger *>(&GetChild<KaxTrackNumber>(AudioTrack)) = AUDIO_TRACK_NUMBER;
			*static_cast<EbmlUInteger *>(&GetChild<KaxTrackUID>(AudioTrack)) = GenerateRandomUInteger();
			*static_cast<EbmlUInteger *>(&GetChild<KaxTrackType>(AudioTrack)) = track_audio;

			//*static_cast<EbmlUInteger *>(&GetChild<KaxTrackDefaultDuration>(AudioTrack)) = AudioTrackFrameDuration;
			*static_cast<EbmlString *>(&GetChild<KaxTrackLanguage>(AudioTrack)) = TRACK_LANGUAGE;

			*static_cast<EbmlString *>(&GetChild<KaxCodecID>(AudioTrack)) = pAudioTrackCodec;
			if (pAudioTrackCodecPrivate != NULL)
			{
				GetChild<KaxCodecPrivate>(AudioTrack).CopyBuffer(pAudioTrackCodecPrivate, AudioTrackCodecPrivateSize);
			}

			AudioTrack.EnableLacing(true);

			// audio specific params
			KaxTrackAudio & AudioTrackParameters = GetChild<KaxTrackAudio>(AudioTrack);
			*static_cast<EbmlFloat *>(&GetChild<KaxAudioSamplingFreq>(AudioTrackParameters)) = AudioTrackSamplingRate;
			*static_cast<EbmlUInteger *>(&GetChild<KaxAudioChannels>(AudioTrackParameters)) = AudioTrackChannels;
			*static_cast<EbmlUInteger *>(&GetChild<KaxAudioBitDepth>(AudioTrackParameters)) = AudioTrackBitDepth;
		}

		// fill subtitle track params
		HasSubtitleTrack = true;
		KaxTrackEntry & SubtitleTrack = HasSubtitleTrack ? GetNextChild<KaxTrackEntry>(AllTracks, AudioTrack) : AudioTrack;
		if (HasSubtitleTrack)
		{
			SubtitleTrack.SetGlobalTimecodeScale(TIMECODE_SCALE);

			*static_cast<EbmlUInteger *>(&GetChild<KaxTrackNumber>(SubtitleTrack)) = SUBTITLE_TRACK_NUMBER;
			*static_cast<EbmlUInteger *>(&GetChild<KaxTrackUID>(SubtitleTrack)) = GenerateRandomUInteger();
			*static_cast<EbmlUInteger *>(&GetChild<KaxTrackType>(SubtitleTrack)) = track_subtitle;

			*static_cast<EbmlString *>(&GetChild<KaxTrackLanguage>(SubtitleTrack)) = TRACK_LANGUAGE;

			*static_cast<EbmlString *>(&GetChild<KaxCodecID>(SubtitleTrack)) = pSubtitleTrackCodec;

			SubtitleTrack.EnableLacing(false);
		}

		TrackSize = AllTracks.Render(out_file, bWriteDefaultValues);
		MetaSeek.IndexThis(AllTracks, FileSegment);

		KaxCues AllCues;
		AllCues.SetGlobalTimecodeScale(TIMECODE_SCALE);

		EbmlVoid AllCuesDummy;
		{
			AllCuesDummy.SetSize(CUEING_DATA_ELEMENT_SIZE);
			AllCuesDummy.Render(out_file, bWriteDefaultValues);
		}

		// filling all clusters
		{
			bool isFirstCluster = true;
			KaxCluster * pCluster = NULL;
			KaxBlockBlob * pBlockBlob = NULL, * pLastBlockBlob = NULL, * pCueCandidate = NULL;
			struct Frame MyFrame;
			DataBuffer * pLastSubtitleData = NULL;
			uint64 lastSubtitleTimecode = 0ull;
			double lastCueTimecode = 0;  // force the first cluster to be a cue

			while (GetNextFrame(&MyFrame))
			{
				if (!HasAudioTrack && MyFrame.isAudio)
				{
					// skip this frame
					continue;
				}

				// use a new cluster if this is a key frame
				if ((MyFrame.isKey) || (pCluster == NULL))
				{
					// release the last cluster
					if (pCluster != NULL)
					{
						ClusterSize += pCluster->Render(out_file, AllCues, bWriteDefaultValues);
						pCluster->ReleaseFrames();
						if (isFirstCluster)
						{
							isFirstCluster = false;
							MetaSeek.IndexThis(*pCluster, FileSegment);
						}
						delete pCluster;
						pCluster = NULL;
					}

					// allocate a new cluster
					if (pCluster == NULL)
					{
						pCluster = new KaxCluster();
					}
					pCluster->SetParent(FileSegment); // mandatory to store references in this Cluster
					pCluster->InitTimecode(MyFrame.timecode / TIMECODE_SCALE, TIMECODE_SCALE);
					//pCluster->EnableChecksum();

					pLastBlockBlob = NULL;
				}

				// prepare a frame
				DataBuffer * pMyData = new DataBuffer((binary *)MyFrame.data, MyFrame.size);

				// allocate current blob
				pBlockBlob = new KaxBlockBlob(BLOCK_BLOB_ALWAYS_SIMPLE);
				pCluster->AddBlockBlob(pBlockBlob);
				pBlockBlob->SetParent(*pCluster);

				// add a frame
				pBlockBlob->AddFrameAuto(MyFrame.isAudio ? AudioTrack : VideoTrack, MyFrame.timecode, *pMyData, LACING_AUTO, pLastBlockBlob);

				// add a cue for any new cluster
				if (pLastBlockBlob == NULL)
				{
					if (HasSubtitleTrack && (lastCueTimecode == 0))
					{
						// add the 1st OSD : FIXME
						DataBuffer * pSubtitleData = new DataBuffer((binary *)"Camera 1", 8);

						pLastSubtitleData = pSubtitleData;
						lastSubtitleTimecode = MyFrame.timecode;
					}
					else if (HasSubtitleTrack && (MyFrame.timecode - lastCueTimecode >= VIDEO_CUE_THRESHOLD))
					{
						// add OSD : FIXME
						DataBuffer * pSubtitleData = new DataBuffer((binary *)"ALERT: *@#$!^%#@#&", 18);

						KaxBlockBlob * pb = new KaxBlockBlob(BLOCK_BLOB_NO_SIMPLE);
						pCluster->AddBlockBlob(pb);
						pb->SetParent(*pCluster);
						pb->AddFrameAuto(SubtitleTrack, lastSubtitleTimecode, *pLastSubtitleData, LACING_AUTO, NULL);
						pb->SetBlockDuration(MyFrame.timecode - lastSubtitleTimecode);

						pLastSubtitleData = pSubtitleData;
						lastSubtitleTimecode = MyFrame.timecode;
					}

					if ((lastCueTimecode == 0) || (MyFrame.timecode - lastCueTimecode >= VIDEO_CUE_THRESHOLD))
					{
						lastCueTimecode = MyFrame.timecode;
						AllCues.AddBlockBlob(*pBlockBlob);
						pCueCandidate = NULL;
					}
					else
					{
						pCueCandidate = pBlockBlob;
					}
				}

				pLastBlockBlob = pBlockBlob;
			}

			if (pCluster != NULL)
			{
				if (HasSubtitleTrack && (pLastSubtitleData != NULL))
				{
					// add OSD : FIXME
					KaxBlockBlob * pb = new KaxBlockBlob(BLOCK_BLOB_NO_SIMPLE);
					pCluster->AddBlockBlob(pb);
					pb->SetParent(*pCluster);
					pb->AddFrameAuto(SubtitleTrack, lastSubtitleTimecode, *pLastSubtitleData, LACING_AUTO, NULL);
					pb->SetBlockDuration(BLOCK_MAX_DURATION);
				}

				// add a cue for the last cluster
				if (pCueCandidate != NULL)
				{
					AllCues.AddBlockBlob(*pCueCandidate);
				}

				// release the last cluster
				ClusterSize += pCluster->Render(out_file, AllCues, bWriteDefaultValues);
				pCluster->ReleaseFrames();

				// re-calculate SegDuration
				KaxDuration & SegDuration = GetChild<KaxDuration>(MyInfos);
				*static_cast<EbmlFloat *>(&SegDuration) = (MyFrame.timecode - VideoTrackTimecodeBase) / TIMECODE_SCALE;

				// correct SegDuration in the out_file
				uint64 CurrentPosition = out_file.getFilePointer();
				out_file.setFilePointer(SegDuration.GetElementPosition());
				SegDuration.Render(out_file, false, true, true);
				out_file.setFilePointer(CurrentPosition);
			}
		}

		//CueSize = AllCues.Render(out_file, bWriteDefaultValues);
		CueSize = AllCuesDummy.ReplaceWith(AllCues, out_file, bWriteDefaultValues);
		MetaSeek.IndexThis(AllCues, FileSegment);

		MetaSeekSize = MetaSeekDummy.ReplaceWith(MetaSeek, out_file, bWriteDefaultValues);

		// let's assume we know the size of the Segment element
		// the size of the FileSegment is also computed because mandatory elements we don't write ourself exist
		if (FileSegment.ForceSize(SegmentSize - FileSegment.HeadSize() + MetaSeekSize + InfoSize + TrackSize + CueSize + ClusterSize))
		{
			FileSegment.OverwriteHead(out_file);
		}

		out_file.close();
    }
    catch (exception & Ex)
    {
		cout << Ex.what() << endl;
    }

	cout << "... ok!" << endl;
    return 0;
}
