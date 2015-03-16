
#ifndef STREAMING_MEDIA_LIBRARY_HPP
#define STREAMING_MEDIA_LIBRARY_HPP

#include <stdlib.h>
#include <time.h>

typedef unsigned long long uint64_t;

class StreamingMediaLibraryImpl;
class StreamingMediaLibrary;

class StreamingMediaFile
{
public:
	StreamingMediaFile() : startTime(0), endTime(0) {}
	virtual ~StreamingMediaFile() {}
	virtual const char * GetFileName() const = 0;

	int    chId;
	time_t startTime;
	time_t endTime;
};

class StreamingMediaChannelHelper
{
public:
	virtual ~StreamingMediaChannelHelper();

	// for recorders
	bool                       AddMediaFile(const char *fileName, time_t startTime, time_t endTime);
	const StreamingMediaFile & AllocateRecordingFile(time_t time);
	bool                       ExtendRecordingClip(time_t time);
	uint64_t                   GetSuggestedDuration(uint64_t startTime);

	// for retrievers
	const StreamingMediaFile & LocateMediaFileExactly(time_t time);
	const StreamingMediaFile & LocateMediaFileForwardly(time_t time);
	const StreamingMediaFile & LocateMediaFileBackwardly(time_t time);
	const StreamingMediaFile & LocateNextMediaFile();
	const StreamingMediaFile & LocatePreviousMediaFile();

private:
	StreamingMediaLibrary &library;
	StreamingMediaFile    *pRecordingMediaFile;
	StreamingMediaFile    *pLocatingMediaFile;

	friend class StreamingMediaLibrary;  // for StreamingMediaLibrary::CreateChannelHelper(chId)
	StreamingMediaChannelHelper(StreamingMediaLibrary &_library, int chId);
};

class StreamingMediaLibrary
{
public:
	StreamingMediaLibrary();
	virtual ~StreamingMediaLibrary();
	StreamingMediaChannelHelper & CreateChannelHelper(int chId);

protected:
	friend class StreamingMediaChannelHelper;

	enum LocatingOption
	{
		NEXT_ONE,
		PREVIOUS_ONE,
		EXACTLY_MATCH,
		FORWARD_SEARCH,
		BACKWARD_SEARCH
	};

	// for recorders
	virtual bool AddMediaFile(const char *, StreamingMediaFile &);
	virtual bool AllocateRecordingFile(StreamingMediaFile &);
	virtual bool ExtendRecordingClip(int chId, time_t);
	virtual uint64_t GetSuggestedDuration(int chId, uint64_t);

	// for retrievers
	virtual StreamingMediaFile & LocateMediaFile(StreamingMediaFile &, LocatingOption);

	StreamingMediaLibraryImpl *pImpl;
};

inline StreamingMediaChannelHelper & StreamingMediaLibrary::CreateChannelHelper(int chId)
{
	return *new StreamingMediaChannelHelper(*this, chId);
}

inline bool StreamingMediaChannelHelper::AddMediaFile(const char *fileName, time_t startTime, time_t endTime)
{
	pRecordingMediaFile->startTime = startTime;
	pRecordingMediaFile->endTime   = endTime;
	return library.AddMediaFile(fileName, *pRecordingMediaFile);
}

inline const StreamingMediaFile & StreamingMediaChannelHelper::AllocateRecordingFile(time_t time)
{
	pRecordingMediaFile->startTime = pRecordingMediaFile->endTime = time;
	library.AllocateRecordingFile(*pRecordingMediaFile);
	return *pRecordingMediaFile;
}

inline bool StreamingMediaChannelHelper::ExtendRecordingClip(time_t time)
{
	return library.ExtendRecordingClip(pRecordingMediaFile->chId, time);
}

inline uint64_t StreamingMediaChannelHelper::GetSuggestedDuration(uint64_t startTime)
{
	return library.GetSuggestedDuration(pRecordingMediaFile->chId, startTime);
}

inline const StreamingMediaFile & StreamingMediaChannelHelper::LocateMediaFileExactly(time_t time)
{
	pLocatingMediaFile->startTime = pLocatingMediaFile->endTime = time;
	pLocatingMediaFile = &library.LocateMediaFile(*pLocatingMediaFile, StreamingMediaLibrary::EXACTLY_MATCH);
	return *pLocatingMediaFile;
}

inline const StreamingMediaFile & StreamingMediaChannelHelper::LocateMediaFileForwardly(time_t time)
{
	pLocatingMediaFile->startTime = pLocatingMediaFile->endTime = time;
	pLocatingMediaFile = &library.LocateMediaFile(*pLocatingMediaFile, StreamingMediaLibrary::FORWARD_SEARCH);
	return *pLocatingMediaFile;
}

inline const StreamingMediaFile & StreamingMediaChannelHelper::LocateMediaFileBackwardly(time_t time)
{
	pLocatingMediaFile->startTime = pLocatingMediaFile->endTime = time;
	pLocatingMediaFile = &library.LocateMediaFile(*pLocatingMediaFile, StreamingMediaLibrary::BACKWARD_SEARCH);
	return *pLocatingMediaFile;
}

inline const StreamingMediaFile & StreamingMediaChannelHelper::LocateNextMediaFile()
{
	pLocatingMediaFile = &library.LocateMediaFile(*pLocatingMediaFile, StreamingMediaLibrary::NEXT_ONE);
	return *pLocatingMediaFile;
}

inline const StreamingMediaFile & StreamingMediaChannelHelper::LocatePreviousMediaFile()
{
	pLocatingMediaFile = &library.LocateMediaFile(*pLocatingMediaFile, StreamingMediaLibrary::PREVIOUS_ONE);
	return *pLocatingMediaFile;
}

#endif  // STREAMING_MEDIA_LIBRARY_HPP
