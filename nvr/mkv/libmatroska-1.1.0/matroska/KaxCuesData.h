/****************************************************************************
** libmatroska : parse Matroska files, see http://www.matroska.org/
**
** <file/class description>
**
** Copyright (C) 2002-2010 Steve Lhomme.  All rights reserved.
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
	\version \$Id: KaxCuesData.h,v 1.8 2004/04/14 23:26:17 robux4 Exp $
	\author Steve Lhomme     <robux4 @ users.sf.net>
*/
#ifndef LIBMATROSKA_CUES_DATA_H
#define LIBMATROSKA_CUES_DATA_H

#include "matroska/KaxTypes.h"
#include "ebml/EbmlUInteger.h"
#include "ebml/EbmlMaster.h"
#include "matroska/KaxDefines.h"

using namespace LIBEBML_NAMESPACE;

START_LIBMATROSKA_NAMESPACE

class KaxBlockGroup;
class KaxBlockBlob;
class KaxCueTrackPositions;
class KaxInternalBlock;

DECLARE_MKX_MASTER(KaxCuePoint)
	public:
		void PositionSet(const KaxBlockGroup & BlockReference, uint64 GlobalTimecodeScale);
		void PositionSet(const KaxBlockBlob & BlobReference, uint64 GlobalTimecodeScale);

		virtual bool IsSmallerThan(const EbmlElement *Cmp) const;

		const KaxCueTrackPositions * GetSeekPosition() const;
		bool Timecode(uint64 & aTimecode, uint64 GlobalTimecodeScale) const;
};

DECLARE_MKX_UINTEGER(KaxCueTime)
};

DECLARE_MKX_MASTER(KaxCueTrackPositions)
	public:
		uint64 ClusterPosition() const;
		uint16 TrackNumber() const;
};

DECLARE_MKX_UINTEGER(KaxCueTrack)
};

DECLARE_MKX_UINTEGER(KaxCueClusterPosition)
};

DECLARE_MKX_UINTEGER(KaxCueBlockNumber)
};

#if MATROSKA_VERSION >= 2
DECLARE_MKX_UINTEGER(KaxCueCodecState)
};

DECLARE_MKX_MASTER(KaxCueReference)
	public:
		void AddReference(const KaxBlockGroup & BlockReferenced, uint64 GlobalTimecodeScale);
		void AddReference(const KaxBlockBlob & BlockReferenced, uint64 GlobalTimecodeScale);
};

DECLARE_MKX_UINTEGER(KaxCueRefTime)
};

DECLARE_MKX_UINTEGER(KaxCueRefCluster)
};

DECLARE_MKX_UINTEGER(KaxCueRefNumber)
};

DECLARE_MKX_UINTEGER(KaxCueRefCodecState)
};
#endif // MATROSKA_VERSION

END_LIBMATROSKA_NAMESPACE

#endif // LIBMATROSKA_CUES_DATA_H
