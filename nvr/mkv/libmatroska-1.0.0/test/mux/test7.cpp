/****************************************************************************
** libmatroska : parse Matroska files, see http://www.matroska.org/
**
** <file/class description>
**
** Copyright (C) 2002-2004 Steve Lhomme.  All rights reserved.
**
** This file is part of libmatroska.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
** 
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
** 
** You should have received a copy of the GNU Lesser General Public
** License along with this library; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
** See http://www.matroska.org/license/lgpl/ for LGPL licensing information.**
** Contact license@matroska.org if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

/*!
    \file
    \version \$Id: test7.cpp 1078 2005-03-03 13:13:04Z robux4 $
    \brief Test muxing two tracks into valid clusters/blocks/frames
    \author Steve Lhomme     <robux4 @ users.sf.net>
*/

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

const uint64 TIMECODE_SCALE = 1000000;  // 1,000,000 means all timecodes in the segment are expressed in milliseconds
const int META_SEEK_ELEMENT_SIZE = 300;  // 300 octets
const wchar_t * APPLICATION_NAME = L"test7";

const bool bWriteDefaultValues = false;
//const bool bWriteDefaultValues = true;

int main(int argc, char **argv)
{
    cout << "Creating \"test.mkv\"" << endl;

    try {
		// write the head of the file (with everything already configured)
		StdIOCallback out_file("test.mkv", MODE_CREATE);

		///// Writing EBML test
		EbmlHead FileHead;

		EDocType & MyDocType = GetChild<EDocType>(FileHead);
		*static_cast<EbmlString *>(&MyDocType) = "matroska";

		EDocTypeVersion & MyDocTypeVer = GetChild<EDocTypeVersion>(FileHead);
		*(static_cast<EbmlUInteger *>(&MyDocTypeVer)) = MATROSKA_VERSION;

		EDocTypeReadVersion & MyDocTypeReadVer = GetChild<EDocTypeReadVersion>(FileHead);
		*(static_cast<EbmlUInteger *>(&MyDocTypeReadVer)) = MATROSKA_VERSION;

		FileHead.Render(out_file, true);

		KaxSegment FileSegment;

		// size is unknown and will always be, we can render it right away
		// 5 octets can represent 2^35, it is wide enough for almost any file
		uint64 SegmentSize = FileSegment.WriteHead(out_file, 5, bWriteDefaultValues);
		
		KaxTracks & MyTracks = GetChild<KaxTracks>(FileSegment);

		// reserve some space for the Meta Seek writen at the end		
		EbmlVoid Dummy;
		Dummy.SetSize(META_SEEK_ELEMENT_SIZE);
		Dummy.Render(out_file, bWriteDefaultValues);

		KaxSeekHead MetaSeek;

		// fill the mandatory Info section
		KaxInfo & MyInfos = GetChild<KaxInfo>(FileSegment);
		KaxTimecodeScale & TimeScale = GetChild<KaxTimecodeScale>(MyInfos);
		*(static_cast<EbmlUInteger *>(&TimeScale)) = TIMECODE_SCALE;

		KaxDuration & SegDuration = GetChild<KaxDuration>(MyInfos);
		*(static_cast<EbmlFloat *>(&SegDuration)) = 0.0;  // FIXME: Oh, my god!! How to predict it!!

		std::string MuxingAppString = "libebml v";
		MuxingAppString += EbmlCodeVersion.c_str();
		MuxingAppString += " + libmatroska v";
		MuxingAppString += KaxCodeVersion.c_str();
		UTFstring MuxingAppUTFstring;
		MuxingAppUTFstring.SetUTF8(MuxingAppString);
		*((EbmlUnicodeString *)&GetChild<KaxMuxingApp>(MyInfos))  = MuxingAppUTFstring;
		*((EbmlUnicodeString *)&GetChild<KaxWritingApp>(MyInfos)) = APPLICATION_NAME;
		GetChild<KaxWritingApp>(MyInfos).SetDefaultSize(25);

		KaxDateUTC & SegDateUTC = GetChild<KaxDateUTC>(MyInfos);
		SegDateUTC.SetEpochDate(time(NULL));  // get current time

		// FIXME: How to generate SegmentUID?

		uint32 InfoSize = MyInfos.Render(out_file, true);
		MetaSeek.IndexThis(MyInfos, FileSegment);

		// fill track 1 params
		KaxTrackEntry & MyTrack1 = GetChild<KaxTrackEntry>(MyTracks);
		MyTrack1.SetGlobalTimecodeScale(TIMECODE_SCALE);

		KaxTrackNumber & MyTrack1Number = GetChild<KaxTrackNumber>(MyTrack1);
		*(static_cast<EbmlUInteger *>(&MyTrack1Number)) = 1;

		KaxTrackUID & MyTrack1UID = GetChild<KaxTrackUID>(MyTrack1);
		*(static_cast<EbmlUInteger *>(&MyTrack1UID)) = 7;

		*(static_cast<EbmlUInteger *>(&GetChild<KaxTrackType>(MyTrack1))) = track_video;

		KaxCodecID & MyTrack1CodecID = GetChild<KaxCodecID>(MyTrack1);
		*static_cast<EbmlString *>(&MyTrack1CodecID) = "V_MPEG4/ISO/AVC";

		MyTrack1.EnableLacing(true);

		// video specific params
		KaxTrackVideo & MyTrack1Video = GetChild<KaxTrackVideo>(MyTrack1);

		KaxVideoPixelHeight & MyTrack1PHeight = GetChild<KaxVideoPixelHeight>(MyTrack1Video);
		*(static_cast<EbmlUInteger *>(&MyTrack1PHeight)) = 200;

		KaxVideoPixelWidth & MyTrack1PWidth = GetChild<KaxVideoPixelWidth>(MyTrack1Video);
		*(static_cast<EbmlUInteger *>(&MyTrack1PWidth)) = 320;

		uint64 TrackSize = MyTracks.Render(out_file, bWriteDefaultValues);
		MetaSeek.IndexThis(MyTracks, FileSegment);


		// "manual" filling of a cluster"
		/// \todo whenever a BlockGroup is created, we should memorize it's position
		KaxCues AllCues;
		AllCues.SetGlobalTimecodeScale(TIMECODE_SCALE);

		KaxCluster Clust1;
		Clust1.SetParent(FileSegment); // mandatory to store references in this Cluster
		Clust1.SetPreviousTimecode(0, TIMECODE_SCALE); // the first timecode here
		Clust1.EnableChecksum();

		// automatic filling of a Cluster
		// simple frame
		KaxBlockGroup *MyNewBlock, *MyLastBlockTrk1 = NULL, *MyLastBlockTrk2 = NULL, *MyNewBlock2;
		DataBuffer *data7 = new DataBuffer((binary *)"tototototo", countof("tototototo"));
		Clust1.AddFrame(MyTrack1, 250 * TIMECODE_SCALE, *data7, MyNewBlock, LACING_EBML);
		if (MyNewBlock != NULL)
			MyLastBlockTrk1 = MyNewBlock;
		DataBuffer *data0 = new DataBuffer((binary *)"TOTOTOTO", countof("TOTOTOTO"));
		Clust1.AddFrame(MyTrack1, 260 * TIMECODE_SCALE, *data0, MyNewBlock); // to test EBML lacing
		if (MyNewBlock != NULL)
			MyLastBlockTrk1 = MyNewBlock;
		DataBuffer *data6 = new DataBuffer((binary *)"tototototo", countof("tototototo"));
		Clust1.AddFrame(MyTrack1, 270 * TIMECODE_SCALE, *data6, MyNewBlock); // to test lacing
		if (MyNewBlock != NULL) {
			MyLastBlockTrk1 = MyNewBlock;
		} else {
			MyLastBlockTrk1->SetBlockDuration(50 * TIMECODE_SCALE);
		}

		DataBuffer *data5 = new DataBuffer((binary *)"tototototo", countof("tototototo"));
		//Clust1.AddFrame(MyTrack2, 23 * TIMECODE_SCALE, *data5, MyNewBlock); // to test with another track
		Clust1.AddFrame(MyTrack1, 23 * TIMECODE_SCALE, *data5, MyNewBlock); // to test with another track

		// add the "real" block to the cue entries
		AllCues.AddBlockGroup(*MyLastBlockTrk1);

		// frame for Track 2
		DataBuffer *data8 = new DataBuffer((binary *)"tttyyy", countof("tttyyy"));
		//Clust1.AddFrame(MyTrack2, 107 * TIMECODE_SCALE, *data8, MyNewBlock, *MyLastBlockTrk2);
		Clust1.AddFrame(MyTrack1, 107 * TIMECODE_SCALE, *data8, MyNewBlock, *MyLastBlockTrk1);

		AllCues.AddBlockGroup(*MyNewBlock);

		// frame with a past reference
		DataBuffer *data4 = new DataBuffer((binary *)"tttyyy", countof("tttyyy"));
		Clust1.AddFrame(MyTrack1, 300 * TIMECODE_SCALE, *data4, MyNewBlock, *MyLastBlockTrk1);

		// frame with a past & future reference
		if (MyNewBlock != NULL) {
			DataBuffer *data3 = new DataBuffer((binary *)"tttyyy", countof("tttyyy"));
			if (Clust1.AddFrame(MyTrack1, 280 * TIMECODE_SCALE, *data3, MyNewBlock2, *MyLastBlockTrk1, *MyNewBlock)) {
				MyNewBlock2->SetBlockDuration(20 * TIMECODE_SCALE);
				MyLastBlockTrk1 = MyNewBlock2;
			} else {
				printf("Error adding a frame !!!");
			}
		}

		AllCues.AddBlockGroup(*MyLastBlockTrk1);
		//AllCues.UpdateSize();

		// simulate the writing of the stream :
		// - write an empty element with enough size for the cue entry
		// - write the cluster(s)
		// - seek back in the file and write the cue entry over the empty element

		uint64 ClusterSize = Clust1.Render(out_file, AllCues, bWriteDefaultValues);
		Clust1.ReleaseFrames();
		MetaSeek.IndexThis(Clust1, FileSegment);

		KaxCluster Clust2;
		Clust2.SetParent(FileSegment); // mandatory to store references in this Cluster
		Clust2.SetPreviousTimecode(300 * TIMECODE_SCALE, TIMECODE_SCALE); // the first timecode here
		Clust2.EnableChecksum();

		DataBuffer *data2 = new DataBuffer((binary *)"tttyyy", countof("tttyyy"));
		Clust2.AddFrame(MyTrack1, 350 * TIMECODE_SCALE, *data2, MyNewBlock, *MyLastBlockTrk1);
		
		AllCues.AddBlockGroup(*MyNewBlock);

		ClusterSize += Clust2.Render(out_file, AllCues, bWriteDefaultValues);
		Clust2.ReleaseFrames();

// older version, write at the end		AllCues.Render(out_file);
		uint32 CueSize = AllCues.Render(out_file, bWriteDefaultValues);
		MetaSeek.IndexThis(AllCues, FileSegment);

		// \todo put it just before the Cue Entries
		uint32 MetaSeekSize = Dummy.ReplaceWith(MetaSeek, out_file, bWriteDefaultValues);

#ifdef VOID_TEST
		MyInfos.VoidMe(out_file);
#endif // VOID_TEST

		// let's assume we know the size of the Segment element
		// the size of the FileSegment is also computed because mandatory elements we don't write ourself exist
		if (FileSegment.ForceSize(SegmentSize - FileSegment.HeadSize() + MetaSeekSize
			                      + TrackSize + ClusterSize + CueSize + InfoSize)) {
			FileSegment.OverwriteHead(out_file);
		}

#if 0
		delete[] buf_bin;
		delete[] buf_txt;
#endif // 0

#ifdef OLD
		MuxedFile.Close(1000); // 1000 ms
#endif // OLD
		out_file.close();
    }
    catch (exception & Ex)
    {
		cout << Ex.what() << endl;
    }

    return 0;
}
