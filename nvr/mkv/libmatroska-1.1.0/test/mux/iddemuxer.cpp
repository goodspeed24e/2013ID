
#include "demuxer.hpp"

int main(int argc, char **argv)
{
	Demuxer *pDemuxer = DemuxerUtilities::CreateMkvDemuxer();

	if (pDemuxer->StartDemuxing(argc >= 2 ? argv[1] : "test.mkv") == NULL)
	{
		return -1;
	}

	while (pDemuxer->GetOneFrame(NULL) != NULL)
	{
	}

	pDemuxer->StopDemuxing();

	return 0;
}
