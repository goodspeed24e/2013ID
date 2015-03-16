
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <boost/thread/thread.hpp>
#include <boost/thread/xtime.hpp>
#include <boost/filesystem.hpp>
#include "streaming_media_library.hpp"

class RootNode;
class ChannelNode;
class YearNode;
class DateNode;
class FileNode;

typedef FileNode StreamingMediaFileImpl;

class RootIndexNode
{
public:
	virtual ~RootIndexNode() {}
	virtual int GetCurrentDuration(int chId) = 0;
	virtual bool SetDefaultDuration(int chId, int duration) = 0;
	virtual bool InsertIndex(StreamingMediaFileImpl *pCurrentNode) = 0;
	virtual StreamingMediaFileImpl * SearchForwardlyAndLoad(int chId, time_t time) = 0;
	virtual StreamingMediaFileImpl * SearchBackwardlyAndLoad(int chId, time_t time) = 0;

	static RootIndexNode * GetRootIndexNode(size_t channelCount);
};

static FILE *pIndexFile;
static int TIMEZONE;

static inline bool IndexFileRead(void *ptr, size_t size)
{
	int value = 0;
	do
	{
		value = fread(ptr, 1, size, pIndexFile);
		size -= value;
	} while ((value > 0) && (size > 0));

	if (value <= 0)
	{
		// error: bad file
		fclose(pIndexFile);
		pIndexFile = NULL;
		return false;
	}

	return true;
}

static inline bool IndexFileWrite(void *ptr, size_t size)
{
	int value = 0;
	do
	{
		value = fwrite(ptr, 1, size, pIndexFile);
		size -= value;
	} while ((value > 0) && (size > 0));

	if (value <= 0)
	{
		// error: bad file
		fclose(pIndexFile);
		pIndexFile = NULL;
		return false;
	}

	return true;
}

class RootNode : public RootIndexNode
{
public:
	RootNode(size_t channelCount);
	virtual ~RootNode();

	bool UpdateIndexFile();
	bool SetBufferContent(int chId, int value);
	ChannelNode * GetChannelNode(int chId, bool isCreate = true);

	virtual int GetCurrentDuration(int chId);
	virtual bool SetDefaultDuration(int chId, int duration);
	virtual bool InsertIndex(FileNode *pCurrentNode);
	virtual FileNode * SearchForwardlyAndLoad(int chId, time_t time);
	virtual FileNode * SearchBackwardlyAndLoad(int chId, time_t time);

protected:
	enum
	{
		DEFAULT_CHANNEL_TABLE_CAPACITY = 255
	};

	size_t channelTableCapacity;
	ChannelNode **pChannelTable;

	int seekBase;
	size_t headerSize;
	size_t bufferSize;
	char *pBuffer;
	bool isDirty;
};

class ChannelNode
{
public:
	ChannelNode(int _chId, int seek = -1);
	virtual ~ChannelNode();

	int chId;
	int GetCurrentDuration();
	void SetDefaultDuration(int duration) { defaultDuration = duration; }
	bool UpdateIndexFile();
	bool SetBufferContent(int year, int value);
	bool ForceReloadBuffer();

	YearNode * GetYearNode(time_t time, bool isCreate = false);
	YearNode * GetYearNodeForwardly(time_t time);
	YearNode * GetYearNodeBackwardly(time_t time);
	YearNode * GetFirstYearNode() { return pFirstYearNode; }
	YearNode * GetLastYearNode()  { return pLastYearNode;  }

	FileNode * GetFirstFileNode() { return pFirstFileNode; }
	FileNode * GetLastFileNode()  { return pLastFileNode;  }

//protected:
	enum
	{
		DEFAULT_YEAR_TABLE_CAPACITY = 8
	};

	unsigned int yearTableBase;
	size_t     yearTableCapacity;
	YearNode **pYearTable;

	YearNode  *pFirstYearNode;
	YearNode  *pLastYearNode;

	RootNode *pParent;
	FileNode  *pFirstFileNode;
	FileNode  *pLastFileNode;

	int seekBase;
	size_t headerSize;
	size_t bufferSize;
	char *pBuffer;
	bool isDirty;
	int defaultDuration;
};

class YearNode
{
public:
	YearNode(int year, int seek = -1);
	virtual ~YearNode();

	time_t startTime;
	time_t endTime;

	bool UpdateIndexFile();
	bool SetBufferContent(time_t time, int value);
	bool ForceReloadBuffer();

	DateNode * GetDateNode(time_t time, bool isCreate = false);
	DateNode * GetDateNodeForwardly(time_t time);
	DateNode * GetDateNodeBackwardly(time_t time);
	DateNode * GetFirstDateNode() { return pFirstDateNode; }
	DateNode * GetLastDateNode()  { return pLastDateNode;  }
	ChannelNode * GetParentNode() { return pParent;        }

//protected:
	size_t dateTableCapacity;

	bool       isLeapYear;
	DateNode **pDateTable;
	DateNode  *pFirstDateNode;
	DateNode  *pLastDateNode;

	ChannelNode *pParent;

	int seekBase;
	size_t headerSize;
	size_t bufferSize;
	char *pBuffer;
	bool isDirty;
};

class DateNode
{
public:
	DateNode(time_t time, int duration, int seek = -1);
	virtual ~DateNode();

	time_t startTime;
	time_t endTime;

	bool UpdateIndexFile();
	bool SetBufferContent(time_t time, FileNode *currentNode);
	bool ForceReloadBuffer();

	int        GetFileDuration()  { return fileDuration;   }  // sec
	FileNode * GetFileNodeForwardly(time_t time);
	FileNode * GetFileNodeBackwardly(time_t time);
	FileNode * GetFirstFileNode() { return pFirstFileNode; }
	FileNode * GetLastFileNode()  { return pLastFileNode;  }

	YearNode * GetParentNode()    { return pParent;        }

//protected:
	int        fileDuration;  // sec
	size_t     fileTableCapacity;
	size_t     extraTableCapacity;
	size_t     extraNodeCount;
	size_t     GetTotalTableSize() { return fileTableCapacity + extraNodeCount; }
	FileNode **pFileTable;
	FileNode  *pFirstFileNode;
	FileNode  *pLastFileNode;

	YearNode *pParent;

	int seekBase;
	size_t headerSize;
	size_t bufferSize;
	char *pBuffer;
	bool isDirty;
};

class FileNode : public StreamingMediaFile
{
public:
	enum FileType
	{
		FILE_TYPE_NULL,
		FILE_TYPE_UNLOADED,
		FILE_TYPE_TEMPORARY,
		FILE_TYPE_NORMAL,
	};

	// config
	enum
	{
		DEFAULT_CHANNEL_COUNT = 16,
		DEFAULT_MEDIA_DURATION = 10,//600,  // sec
	};

	enum
	{
		BUFFER_SIZE = 128,
	};

	FileNode(FileType _type = FILE_TYPE_NULL, int seek = -1);
	virtual ~FileNode();

	const char * GetFileName() const;
	bool RebuildFileName();
	bool RenameFrom(const char *old);
	FileNode * GetNext() { return pNext; }
	FileNode * GetPrev() { return pPrev; }
	FileNode * LoadNext() { return LoadNextFromFileDirectly(); }
	FileNode * LoadPrev() { return LoadPreviousFromFileDirectly(); }

	FileNode * LoadNextFromFileDirectly();
	FileNode * LoadPreviousFromFileDirectly();

	// private members
	int serial;
	int diskId;
	char *fullPath;
	const char *fileName;

	// file node members
	char fileNameBuffer[BUFFER_SIZE];

	FileType type;

	// index tree
	DateNode *pParent;
	FileNode *pPrev;
	FileNode *pNext;

	int seekBase;
	bool isDirty;

	// static const
	static FileNode * const pUnloadedNode;
	static FileNode * const pUnkonwnNode;
};

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
RootIndexNode * RootIndexNode::GetRootIndexNode(size_t channelCount)
{
	return new RootNode(channelCount);
}

// for recording
inline int RootNode::GetCurrentDuration(int chId)
{
	int result = 0;

	ChannelNode *channelNode = GetChannelNode(chId);
	if (channelNode != NULL)
	{
		result = channelNode->GetCurrentDuration();
	}

	if (result == 0)
	{
		result = FileNode::DEFAULT_MEDIA_DURATION;
	}

	return result;
}

// for recording
inline bool RootNode::SetDefaultDuration(int chId, int duration)
{
	ChannelNode *channelNode = GetChannelNode(chId);
	if (channelNode == NULL)
	{
		return false;
	}

	channelNode->SetDefaultDuration(duration);
	return true;
}

// for indexing
FileNode * RootNode::SearchForwardlyAndLoad(int chId, time_t time)
{
	// force to reload file
	if (pIndexFile != NULL)
	{
		fclose(pIndexFile);
	}
	pIndexFile = fopen(".index", "rb+");

	ChannelNode *pChannelNode = GetChannelNode(chId);
	YearNode *pYearNode;
	DateNode *pDateNode;
	FileNode *pFileNode;

	if (((pYearNode = pChannelNode->GetYearNodeForwardly(time)) != NULL)
	    && ((pDateNode = pYearNode->GetDateNodeForwardly(time)) != NULL)
	    && ((pFileNode = pDateNode->GetFileNodeForwardly(time)) != NULL))
	{
		return pFileNode;
	}

	return NULL;
}

// for indexing
FileNode * RootNode::SearchBackwardlyAndLoad(int chId, time_t time)
{
	// force to reload file
	if (pIndexFile != NULL)
	{
		fclose(pIndexFile);
	}
	pIndexFile = fopen(".index", "rb+");

	ChannelNode *pChannelNode = GetChannelNode(chId);
	YearNode *pYearNode;
	DateNode *pDateNode;
	FileNode *pFileNode;

	if (((pYearNode = pChannelNode->GetYearNodeBackwardly(time)) != NULL)
	    && ((pDateNode = pYearNode->GetDateNodeBackwardly(time)) != NULL)
	    && ((pFileNode = pDateNode->GetFileNodeBackwardly(time)) != NULL))
	{
		return pFileNode;
	}

	return NULL;
}

// for recording
bool RootNode::InsertIndex(FileNode *pCurrentNode)
{
	ChannelNode *pChannelNode = GetChannelNode(pCurrentNode->chId);
	if (pChannelNode == NULL)
	{
		return false;
	}

	FileNode *pLastNode = pChannelNode->pLastFileNode;
	if (pLastNode == NULL)
	{
		// This is the first node of the current channel
		pCurrentNode->pPrev = NULL;
		pCurrentNode->pNext = NULL;
		pChannelNode->pLastFileNode = pCurrentNode;

		YearNode *pYearNode = pChannelNode->GetYearNode(pCurrentNode->startTime, true);
		if (pYearNode != NULL)
		{
			pYearNode->pParent = pChannelNode;

			DateNode *pDateNode = pYearNode->GetDateNode(pCurrentNode->startTime, true);
			if (pDateNode != NULL)
			{
				pDateNode->pParent = pYearNode;
				pCurrentNode->pParent = pDateNode;

				pDateNode->SetBufferContent(pCurrentNode->startTime, pCurrentNode);
				pDateNode->UpdateIndexFile();
			}
			else
			{
				// error:
			}

			pYearNode->UpdateIndexFile();
		}
		else
		{
			// error:
		}

		pChannelNode->UpdateIndexFile();
		UpdateIndexFile();
	}
	else
	{
		pLastNode->pNext = pCurrentNode;
		pCurrentNode->pParent = pLastNode->pParent;
		pCurrentNode->pPrev = pLastNode;
		pCurrentNode->pNext = NULL;
		pChannelNode->pLastFileNode = pCurrentNode;

		if ((pCurrentNode->pParent != NULL) || (pCurrentNode->startTime >= pCurrentNode->pParent->endTime))
		{
			YearNode *pYearNode = pChannelNode->GetYearNode(pCurrentNode->startTime, true);
			if (pYearNode != NULL)
			{
				pCurrentNode->pParent = pYearNode->GetDateNode(pCurrentNode->startTime, true);
			}
		}

		if (pCurrentNode->pParent != NULL)
		{
			pCurrentNode->pParent->SetBufferContent(pCurrentNode->startTime, pCurrentNode);
		}

		if (pLastNode->pParent != NULL)
		{
			pLastNode->pParent->SetBufferContent(pLastNode->startTime, pLastNode);
			if (pLastNode->pParent != pCurrentNode->pParent)
			{
				pLastNode->pParent->UpdateIndexFile();
				if ((pLastNode->pParent->pParent != NULL) && (pCurrentNode->pParent != NULL) && (pLastNode->pParent->pParent != pCurrentNode->pParent->pParent))
				{
					pLastNode->pParent->pParent->UpdateIndexFile();
				}
			}
		}

		if (pCurrentNode->pParent != NULL)
		{
			pCurrentNode->pParent->UpdateIndexFile();
			if (pCurrentNode->pParent->pParent != NULL)
			{
				pCurrentNode->pParent->pParent->UpdateIndexFile();
			}
		}

		pChannelNode->UpdateIndexFile();
		UpdateIndexFile();

		if (pLastNode->pPrev != NULL)
		{
			if ((pLastNode->pPrev->pParent != NULL) && (pLastNode->pPrev->pParent != pLastNode->pParent))
			{
				if ((pLastNode->pPrev->pParent->pParent != NULL) && (pLastNode->pParent) && (pLastNode->pPrev->pParent->pParent != pLastNode->pParent->pParent))
				{
					delete pLastNode->pPrev->pParent->pParent;
					pLastNode->pPrev->pParent->pParent = NULL;
				}
				delete pLastNode->pPrev->pParent;
				pLastNode->pPrev->pParent = NULL;
			}
			delete pLastNode->pPrev;
			pLastNode->pPrev = NULL;
		}
	}

	return true;
}

RootNode::RootNode(size_t channelCount)
	: seekBase(0), headerSize(4), isDirty(false)
{
	pIndexFile = fopen(".index", "rb+");
	if ((pIndexFile != NULL)
	    && (fread(&channelTableCapacity, 1, 4, pIndexFile) == 4))
	{
		if (channelCount > channelTableCapacity)
		{
			// FIXME: MUST extend root node in the index file
		}
		bufferSize = headerSize + channelTableCapacity * 4;
		pBuffer = new char[bufferSize];

		*(int *)pBuffer = channelTableCapacity;
		if (IndexFileRead(pBuffer + 4, bufferSize - 4) == false)
		{
			// error:
			memset(pBuffer + 4, 0, bufferSize - 4);
			isDirty = true;
		}
	}
	else
	{
		// error:
		if (pIndexFile != NULL)
		{
			fclose(pIndexFile);
			pIndexFile = NULL;
		}

		// create new file
		pIndexFile = fopen(".index", "wb+");
		channelTableCapacity = channelCount < DEFAULT_CHANNEL_TABLE_CAPACITY ? DEFAULT_CHANNEL_TABLE_CAPACITY : channelCount;

		bufferSize = headerSize + channelTableCapacity * 4;
		pBuffer = new char[bufferSize];
		memset(pBuffer, 0, bufferSize);
		*(int *)pBuffer = channelTableCapacity;
		isDirty = true;

		UpdateIndexFile();
	}

	pChannelTable = new ChannelNode *[channelTableCapacity];
	for (size_t i = 0; i < channelTableCapacity; i++)
	{
		pChannelTable[i] = NULL;
	}
}

RootNode::~RootNode()
{
	if (pChannelTable != NULL)
	{
		for (size_t i = 0; i < channelTableCapacity; i++)
		{
			if (pChannelTable[i] != NULL)
			{
				delete pChannelTable[i];
			}
		}
		delete[] pChannelTable;
	}
}

bool RootNode::UpdateIndexFile()
{
	if (!isDirty || (pBuffer == NULL) || (bufferSize == 0))
	{
		// there is nothing to do
		return true;
	}
	isDirty = false;

	if (pIndexFile == NULL)
	{
		pIndexFile = fopen(".index", "rb+");
		if (pIndexFile != NULL)
		{
			// error: file already exist
			fclose(pIndexFile);
			pIndexFile = NULL;
			return false;
		}

		// create new file
		pIndexFile = fopen(".index", "wb+");
		if (pIndexFile == NULL)
		{
			// error:
			return false;
		}
	}

	if (seekBase >= 0)
	{
		if (fseek(pIndexFile, seekBase, SEEK_SET) != 0)
		{
			// error:
			fclose(pIndexFile);
			pIndexFile = NULL;
			return false;
		}
	}
	else
	{
		if (fseek(pIndexFile, 0, SEEK_END) != 0)
		{
			// error:
			fclose(pIndexFile);
			pIndexFile = NULL;
			return false;
		}
		seekBase = ftell(pIndexFile);
	}

	if (IndexFileWrite(pBuffer, bufferSize) == false)
	{
		// error:
		return false;
	}

	return true;
}

bool RootNode::SetBufferContent(int chId, int value)
{
	if ((chId <= 0) || ((size_t)chId > channelTableCapacity) || (pBuffer == NULL))
	{
		// error: wrong id or wrong buffer
		return false;
	}

	*((int *)(pBuffer + headerSize) + chId - 1) = value;
	isDirty = true;
	return true;
}

ChannelNode * RootNode::GetChannelNode(int chId, bool isCreate)
{
	if ((chId <= 0) || ((size_t)chId > channelTableCapacity))
	{
		// error: wrong id
		return NULL;
	}

	if (pChannelTable[chId - 1] == NULL)
	{
		// try to load the specific channel node
		if ((pIndexFile != NULL) && (pBuffer != NULL))
		{
			if (*((int *)(pBuffer + headerSize) + chId - 1) != 0)
			{
				pChannelTable[chId - 1] = new ChannelNode(chId, *((int *)(pBuffer + headerSize) + chId - 1));
				pChannelTable[chId - 1]->pParent = this;

				if (!pChannelTable[chId - 1]->isDirty)
				{
					// try to load the last node
					ChannelNode *pChannalNode = pChannelTable[chId - 1];
					int yearIndex;
					int yearSeekBase;
					if ((pChannalNode->yearTableBase != 0)
					    && ((yearIndex = *((int *)pChannalNode->pBuffer + 3)) != -1)
					    && (yearIndex >= 0) && ((size_t)yearIndex < pChannalNode->yearTableCapacity)
					    && ((yearSeekBase = *((int *)(pChannalNode->pBuffer + pChannalNode->headerSize) + yearIndex)) != 0))
					{
						int year = pChannalNode->yearTableBase + yearIndex;
						pChannalNode->pYearTable[yearIndex] = new YearNode(year, yearSeekBase);
						pChannalNode->pYearTable[yearIndex]->pParent = pChannalNode;

						if (!pChannalNode->pYearTable[yearIndex]->isDirty)
						{
							YearNode *pYearNode = pChannalNode->pYearTable[yearIndex];
							int dateIndex;
							int dateSeekBase;
							if (((dateIndex = *((int *)pYearNode->pBuffer + 2)) != -1)
							    && (dateIndex >= 0) && ((size_t)dateIndex < pYearNode->dateTableCapacity)
								&& (pYearNode->isLeapYear || ((size_t)dateIndex != pYearNode->dateTableCapacity - 1))
								&& ((dateSeekBase = *((int *)(pYearNode->pBuffer + pYearNode->headerSize) + dateIndex)) != 0))
							{
								struct tm t;  // local time
								t.tm_year = year - 1900;
								t.tm_mon  = 0;
								t.tm_mday = 1;
								t.tm_hour = 0;
								t.tm_min  = 0;
								t.tm_sec  = 0;
								time_t dateStartTime = mktime(&t);
								dateStartTime += dateIndex * 24 * 60 * 60;
								pYearNode->pDateTable[dateIndex] = new DateNode(dateStartTime, 0, dateSeekBase);
								pYearNode->pDateTable[dateIndex]->pParent = pYearNode;

								if (!pYearNode->pDateTable[dateIndex]->isDirty)
								{
									DateNode *pDateNode = pYearNode->pDateTable[dateIndex];
									int fileSeekBase = *((int *)pDateNode->pBuffer + 3);
									if (fileSeekBase != 0)
									{
										FileNode *pFileNode = new FileNode(FileNode::FILE_TYPE_NORMAL, fileSeekBase);
										if (!pFileNode->isDirty)
										{
											//pDateNode->pFileTable[index] = pFileNode;
											pFileNode->pParent = pDateNode;
											pChannelTable[chId - 1]->pLastFileNode = pFileNode;
										}
										else
										{
											// error
											if (!isCreate)
											{
												delete pFileNode;
												//pDateNode->pFileTable[index] = NULL;
											}
										}
									}
								}
								else
								{
									// error
									if (!isCreate)
									{
										delete pYearNode->pDateTable[dateIndex];
										pYearNode->pDateTable[dateIndex] = NULL;
									}
								}
							}
						}
						else
						{
							// error
							if (!isCreate)
							{
								delete pChannalNode->pYearTable[yearIndex];
								pChannalNode->pYearTable[yearIndex] = NULL;
							}
						}
					}
				}
				else
				{
					// error
					if (!isCreate)
					{
						delete pChannelTable[chId - 1];
						pChannelTable[chId - 1] = NULL;
					}
				}
			}
		}

		// if there exist no specific channel node, create a new one
		if ((pChannelTable[chId - 1] == NULL) && isCreate)
		{
			pChannelTable[chId - 1] = new ChannelNode(chId);
			pChannelTable[chId - 1]->pParent = this;
			pChannelTable[chId - 1]->UpdateIndexFile();
			UpdateIndexFile();

			// initialize channel node
		}
	}

	return pChannelTable[chId - 1];
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
ChannelNode::ChannelNode(int _chId, int seek)
	: chId(_chId),
	  yearTableBase(0), yearTableCapacity(DEFAULT_YEAR_TABLE_CAPACITY),
	  pFirstYearNode(NULL), pLastYearNode(NULL),
	  pParent(NULL), pFirstFileNode(NULL), pLastFileNode(NULL),
	  seekBase(seek), headerSize(16), isDirty(false), defaultDuration(0)
{
	if ((pIndexFile != NULL) && (seekBase > 0)
	    && (fseek(pIndexFile, seekBase, SEEK_SET) == 0)
	    && (fread(&yearTableCapacity, 1, 4, pIndexFile) == 4))
	{
		bufferSize = headerSize + yearTableCapacity * 4;
		pBuffer = new char[bufferSize];

		*(int *)pBuffer = yearTableCapacity;
		if (fread(&yearTableBase, 1, 4, pIndexFile) != 4)
		{
			yearTableBase = 0;
			isDirty = true;
		}
		*((int *)pBuffer + 1) = yearTableBase;
		if (IndexFileRead(pBuffer + 8, bufferSize - 8) == false)
		{
			memset(pBuffer + 8, 0, bufferSize - 8);  // FIXME: can not set all zero
			*((int *)pBuffer + 2) = -1;
			*((int *)pBuffer + 3) = -1;
			isDirty = true;
		}
	}
	else
	{
		// error
		if (pIndexFile != NULL)
		{
			//fclose(pIndexFile);
			//pIndexFile = NULL;
		}
		yearTableCapacity = DEFAULT_YEAR_TABLE_CAPACITY;

		bufferSize = headerSize + yearTableCapacity * 4;
		pBuffer = new char[bufferSize];
		memset(pBuffer, 0, bufferSize);
		*(int *)pBuffer = yearTableCapacity;
		*((int *)pBuffer + 2) = -1;
		*((int *)pBuffer + 3) = -1;
		isDirty = true;
	}

	pYearTable = new YearNode *[yearTableCapacity];
	for (size_t i = 0; i < yearTableCapacity; i++)
	{
		pYearTable[i] = NULL;
	}
}

ChannelNode::~ChannelNode()
{
	if (pYearTable != NULL)
	{
		for (size_t i = 0; i < yearTableCapacity; i++)
		{
			if (pYearTable[i] != NULL)
			{
				delete pYearTable[i];
			}
		}
		delete[] pYearTable;
	}
}

bool ChannelNode::ForceReloadBuffer()
{
	bool result = true;
	size_t reloadedYearTableCapacity;
	size_t reloadedBufferSize;

	if ((pIndexFile != NULL) && (seekBase > 0)
	    && (fseek(pIndexFile, seekBase, SEEK_SET) == 0)
	    && (fread(&reloadedYearTableCapacity, 1, 4, pIndexFile) == 4))
	{
		if (reloadedYearTableCapacity != yearTableCapacity)
		{
			result = false;

			// recalculate bufferSize
			reloadedBufferSize = headerSize + reloadedYearTableCapacity * 4;
			if (reloadedBufferSize > bufferSize)
			{
				reloadedBufferSize = bufferSize;
			}
		}
		else
		{
			reloadedBufferSize = bufferSize;
		}

		if (IndexFileRead(pBuffer + 4, reloadedBufferSize - 4) == false)
		{
			result = false;
		}

		if ((size_t)*((int *)pBuffer + 1) != yearTableBase)
		{
			result = false;
		}
	}
	else
	{
		result = false;
	}

	return result;
}

bool ChannelNode::UpdateIndexFile()
{
	if (!isDirty || (pBuffer == NULL) || (bufferSize == 0) || (pIndexFile == NULL))
	{
		// there is nothing to do
		return true;
	}
	isDirty = false;

	if (seekBase > 0)
	{
		if (fseek(pIndexFile, seekBase, SEEK_SET) != 0)
		{
			// error:
			fclose(pIndexFile);
			pIndexFile = NULL;
			return false;
		}
	}
	else
	{
		if (fseek(pIndexFile, 0, SEEK_END) != 0)
		{
			// error:
			fclose(pIndexFile);
			pIndexFile = NULL;
			return false;
		}
		seekBase = ftell(pIndexFile);
		pParent->SetBufferContent(chId, seekBase);
	}

	if (IndexFileWrite(pBuffer, bufferSize) == false)
	{
		// error:
		return false;
	}

	return true;
}

bool ChannelNode::SetBufferContent(int year, int value)
{
	if (pBuffer == NULL)
	{
		// error: wrong buffer
		return false;
	}

	if (yearTableBase == 0)
	{
		*((int *)pBuffer + 1) = yearTableBase = year;
		isDirty = true;
	}

	int index = year - yearTableBase;

	if ((index < 0) || ((size_t)index >= yearTableCapacity))
	{
		// error: wrong year
		return false;
	}

	*((int *)(pBuffer + headerSize) + index) = value;

	int firstIndex = *((int *)pBuffer + 2);
	int lastIndex  = *((int *)pBuffer + 3);
	if ((firstIndex == -1) || (index < firstIndex))
	{
		*((int *)pBuffer + 2) = index;
	}
	if ((lastIndex == -1) || (index > lastIndex))
	{
		*((int *)pBuffer + 3) = index;
	}

	isDirty = true;
	return true;
}

YearNode * ChannelNode::GetYearNode(time_t time, bool isCreate)
{
	struct tm *t;
	t = localtime(&time);
	int year = 1900 + t->tm_year;

	if ((yearTableBase == 0) && (seekBase > 0))
	{
		yearTableBase = *((int *)pBuffer + 1);
	}

	if (yearTableBase == 0)
	{
		if (!isCreate)
		{
			return NULL;
		}
		else
		{
			*((int *)pBuffer + 1) = yearTableBase = year;
			isDirty = true;
		}
	}

	int index = year - yearTableBase;

	if ((index < 0) || ((size_t)index >= yearTableCapacity))
	{
		// error: wrong year
		return NULL;
	}

	if (pYearTable[index] == NULL)
	{
		// try to load the specific year node
		if ((pIndexFile != NULL) && (pBuffer != NULL))
		{
			if (*((int *)(pBuffer + headerSize) + index) != 0)
			{
				pYearTable[index] = new YearNode(year, *((int *)(pBuffer + headerSize) + index));
				pYearTable[index]->pParent = this;

				if (pYearTable[index]->isDirty)
				{
					// error
					if (!isCreate)
					{
						delete pYearTable[index];
						pYearTable[index] = NULL;
					}
				}
			}
		}

		// if there exist no specific year node, create a new one
		if ((pYearTable[index] == NULL) && isCreate)
		{
			pYearTable[index] = new YearNode(year);
			pYearTable[index]->pParent = this;
		}
	}

	return pYearTable[index];
}

YearNode * ChannelNode::GetYearNodeForwardly(time_t time)
{
	ForceReloadBuffer();

	struct tm *t;
	t = localtime(&time);
	int year = 1900 + t->tm_year;

	if ((yearTableBase == 0) && (seekBase > 0))
	{
		yearTableBase = *((int *)pBuffer + 1);
	}

	if (yearTableBase == 0)
	{
		return NULL;
	}

	int index = (unsigned int)year <= yearTableBase ? 0 : year - yearTableBase;
	int lastIndex = *((int *)pBuffer + 3);

	if ((index < 0) || ((size_t)index >= yearTableCapacity)
	    || (lastIndex < 0)
		|| (index > lastIndex))
	{
		// error: wrong year
		return NULL;
	}

	for ( ; ((size_t)index < yearTableCapacity) && (index <= lastIndex); index++)
	{
		if (pYearTable[index] == NULL)
		{
			// try to load the specific year node
			if ((pIndexFile != NULL) && (pBuffer != NULL))
			{
				int yearSeekBase;
				if ((yearSeekBase = *((int *)(pBuffer + headerSize) + index)) != 0)
				{
					pYearTable[index] = new YearNode(year, yearSeekBase);
					pYearTable[index]->pParent = this;

					if (pYearTable[index]->isDirty)
					{
						// error
						delete pYearTable[index];
						pYearTable[index] = NULL;
					}
				}
			}
		}

		if (pYearTable[index] != NULL)
		{
			return pYearTable[index];
		}
	}

	return NULL;
}

YearNode * ChannelNode::GetYearNodeBackwardly(time_t time)
{
	ForceReloadBuffer();

	struct tm *t;
	t = localtime(&time);
	int year = 1900 + t->tm_year;

	if ((yearTableBase == 0) && (seekBase > 0))
	{
		yearTableBase = *((int *)pBuffer + 1);
	}

	if (yearTableBase == 0)
	{
		return NULL;
	}

	int index = year - yearTableBase;
	int firstIndex = *((int *)pBuffer + 2);

	if ((size_t)index >= yearTableCapacity)
	{
		index = yearTableCapacity - 1;
	}
	if ((index < 0) || ((size_t)index >= yearTableCapacity)
	    || ((size_t)firstIndex >= yearTableCapacity)
		|| (index < firstIndex))
	{
		// error: wrong year
		return NULL;
	}

	for ( ; (index >= 0) && (index >= firstIndex); index--)
	{
		if (pYearTable[index] == NULL)
		{
			// try to load the specific year node
			if ((pIndexFile != NULL) && (pBuffer != NULL))
			{
				int yearSeekBase;
				if ((yearSeekBase = *((int *)(pBuffer + headerSize) + index)) != 0)
				{
					pYearTable[index] = new YearNode(year, yearSeekBase);
					pYearTable[index]->pParent = this;

					if (pYearTable[index]->isDirty)
					{
						// error
						delete pYearTable[index];
						pYearTable[index] = NULL;
					}
				}
			}
		}

		if (pYearTable[index] != NULL)
		{
			return pYearTable[index];
		}
	}

	return NULL;
}

inline int ChannelNode::GetCurrentDuration()
{
	if (defaultDuration == 0)
	{
		if ((pLastFileNode != NULL) && (pLastFileNode->pParent != NULL))
		{
			defaultDuration = pLastFileNode->pParent->GetFileDuration();
		}
		else
		{
			defaultDuration = FileNode::DEFAULT_MEDIA_DURATION;
		}
	}

	return defaultDuration;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
YearNode::YearNode(int year, int seek)
	: pFirstDateNode(NULL), pLastDateNode(NULL), pParent(NULL),
	  seekBase(seek), headerSize(12), isDirty(false)
{
	if (year < 1970)
	{
		startTime = endTime = 0;
	}
	else
	{
		struct tm t;  // local time
		t.tm_year = year - 1900;
		t.tm_mon  = 0;
		t.tm_mday = 1;
		t.tm_hour = 0;
		t.tm_min  = 0;
		t.tm_sec  = 0;
		startTime = mktime(&t);

		t.tm_year = year + 1 - 1900;
		t.tm_mon  = 0;
		t.tm_mday = 1;
		t.tm_hour = 0;
		t.tm_min  = 0;
		t.tm_sec  = 0;
		endTime   = mktime(&t);
	}

	isLeapYear = year % 400 == 0 ? true
	             : year % 100 == 0 ? false
	             : year %   4 == 0 ? true : false;

	dateTableCapacity = 366;

	int valueIsLeapYear = 0;
	if ((pIndexFile != NULL) && (seekBase > 0)
	    && (fseek(pIndexFile, seekBase, SEEK_SET) == 0)
	    && (fread(&valueIsLeapYear, 1, 4, pIndexFile) == 4))
	{
		if (isLeapYear == !valueIsLeapYear)
		{
			// FIXME: something wrong
		}
		bufferSize = headerSize + dateTableCapacity * 4;
		pBuffer = new char[bufferSize];

		*(int *)pBuffer = isLeapYear ? -1 : 0;
		if (IndexFileRead(pBuffer + 4, bufferSize - 4) == false)
		{
			*((int *)pBuffer + 1) = -1;
			*((int *)pBuffer + 2) = -1;
			memset(pBuffer + 12, 0, bufferSize - 12);
			isDirty = true;
		}
	}
	else
	{
		// error
		if (pIndexFile != NULL)
		{
			//fclose(pIndexFile);
			//pIndexFile = NULL;
		}

		bufferSize = headerSize + dateTableCapacity * 4;
		pBuffer = new char[bufferSize];
		memset(pBuffer, 0, bufferSize);
		*(int *)pBuffer = isLeapYear ? -1 : 0;
		*((int *)pBuffer + 1) = -1;
		*((int *)pBuffer + 2) = -1;
		isDirty = true;
	}

	pDateTable = new DateNode *[dateTableCapacity];
	for (size_t i = 0; i < dateTableCapacity; i++)
	{
		pDateTable[i] = NULL;
	}
}

YearNode::~YearNode()
{
	if (pDateTable != NULL)
	{
		for (size_t i = 0; i < dateTableCapacity; i++)
		{
			if (pDateTable[i] != NULL)
			{
				delete pDateTable[i];
			}
		}
		delete[] pDateTable;
	}
}

bool YearNode::ForceReloadBuffer()
{
	bool result = true;
	size_t reloadedDateTableCapacity = 366;
	size_t reloadedBufferSize = headerSize + reloadedDateTableCapacity * 4;

	if ((pIndexFile != NULL) && (seekBase > 0)
	    && (fseek(pIndexFile, seekBase, SEEK_SET) == 0))
	{
		if (reloadedBufferSize != bufferSize)
		{
			result = false;

			if (reloadedBufferSize > bufferSize)
			{
				reloadedBufferSize = bufferSize;
			}
		}

		if (IndexFileRead(pBuffer, reloadedBufferSize) == false)
		{
			result = false;
		}
	}
	else
	{
		result = false;
	}

	return result;
}

bool YearNode::UpdateIndexFile()
{
	if (!isDirty || (pBuffer == NULL) || (bufferSize == 0) || (pIndexFile == NULL))
	{
		// there is nothing to do
		return true;
	}
	isDirty = false;

	if (seekBase > 0)
	{
		if (fseek(pIndexFile, seekBase, SEEK_SET) != 0)
		{
			// error:
			fclose(pIndexFile);
			pIndexFile = NULL;
			return false;
		}
	}
	else
	{
		if (fseek(pIndexFile, 0, SEEK_END) != 0)
		{
			// error:
			fclose(pIndexFile);
			pIndexFile = NULL;
			return false;
		}
		seekBase = ftell(pIndexFile);
		struct tm *t;
		t = localtime(&startTime);
		pParent->SetBufferContent(1900 + t->tm_year, seekBase);
	}

	if (IndexFileWrite(pBuffer, bufferSize) == false)
	{
		// error:
		return false;
	}

	return true;
}

bool YearNode::SetBufferContent(time_t time, int value)
{
	if ((time < startTime) || (time >= endTime) || (pBuffer == NULL))
	{
		// error: wrong time or wrong buffer
		return false;
	}

	int index = (time - startTime) / (24 * 60 * 60);

	if ((index < 0) || ((size_t)index >= dateTableCapacity)
	    || (!isLeapYear && ((size_t)index == dateTableCapacity - 1)))
	{
		// error: wrong index
		return false;
	}

	*((int *)(pBuffer + headerSize) + index) = value;

	int firstIndex = *((int *)pBuffer + 1);
	int lastIndex  = *((int *)pBuffer + 2);
	if ((firstIndex == -1) || (index < firstIndex))
	{
		*((int *)pBuffer + 1) = index;
	}
	if ((lastIndex == -1) || (index > lastIndex))
	{
		*((int *)pBuffer + 2) = index;
	}

	isDirty = true;
	return true;
}

DateNode * YearNode::GetDateNode(time_t time, bool isCreate)
{
	if ((time < startTime) || (time >= endTime))
	{
		// error: wrong time
		return NULL;
	}

	int index = (time - startTime) / (24 * 60 * 60);

	if ((index < 0) || ((size_t)index >= dateTableCapacity)
	    || (!isLeapYear && ((size_t)index == dateTableCapacity - 1)))
	{
		// error: wrong index
		return NULL;
	}

	if (pDateTable[index] == NULL)
	{
		// try to load the specific date node
		if ((pIndexFile != NULL) && (pBuffer != NULL))
		{
			if (*((int *)(pBuffer + headerSize) + index) != 0)
			{
				pDateTable[index] = new DateNode(time, 0, *((int *)(pBuffer + headerSize) + index));
				pDateTable[index]->pParent = this;

				if (pDateTable[index]->isDirty)
				{
					// error
					if (!isCreate)
					{
						delete pDateTable[index];
						pDateTable[index] = NULL;
					}
				}
			}
		}

		// if there exist no specific date node, create a new one
		if ((pDateTable[index] == NULL) && (isCreate))
		{
			pDateTable[index] = new DateNode(time, pParent->GetCurrentDuration());
			pDateTable[index]->pParent = this;
			pDateTable[index]->UpdateIndexFile();
		}
	}

	return pDateTable[index];
}

DateNode * YearNode::GetDateNodeForwardly(time_t time)
{
	ForceReloadBuffer();

	if (time >= endTime)
	{
		// error: wrong time
		return NULL;
	}

	int index = time <= startTime ? 0 : (time - startTime) / (24 * 60 * 60);
	int lastIndex = *((int *)pBuffer + 2);

	if ((index < 0) || ((size_t)index >= dateTableCapacity)
	    || (!isLeapYear && ((size_t)index == dateTableCapacity - 1))
	    || (lastIndex < 0)
		|| (index > lastIndex))
	{
		// error: wrong index
		return NULL;
	}

	for ( ; ((size_t)index < dateTableCapacity) && (index <= lastIndex); index++)
	{
		if (pDateTable[index] == NULL)
		{
			// try to load the specific date node
			if ((pIndexFile != NULL) && (pBuffer != NULL))
			{
				int dateSeekBase;
				if ((dateSeekBase = *((int *)(pBuffer + headerSize) + index)) != 0)
				{
					pDateTable[index] = new DateNode(time, 0, dateSeekBase);
					pDateTable[index]->pParent = this;

					if (pDateTable[index]->isDirty)
					{
						// error
						delete pDateTable[index];
						pDateTable[index] = NULL;
					}
				}
			}
		}

		if (pDateTable[index] != NULL)
		{
			return pDateTable[index];
		}
	}

	return NULL;
}

DateNode * YearNode::GetDateNodeBackwardly(time_t time)
{
	ForceReloadBuffer();

	if (time < startTime)
	{
		// error: wrong time
		return NULL;
	}

	int index = ((time >= endTime ? endTime - 1 : time) - startTime) / (24 * 60 * 60);
	int firstIndex = *((int *)pBuffer + 1);

	if ((index < 0) || ((size_t)index >= dateTableCapacity)
	    || (!isLeapYear && ((size_t)index == dateTableCapacity - 1))
	    || ((size_t)firstIndex >= dateTableCapacity)
		|| (index < firstIndex))
	{
		// error: wrong index
		return NULL;
	}

	for ( ; (index >= 0) && (index >= firstIndex); index--)
	{
		if (pDateTable[index] == NULL)
		{
			// try to load the specific date node
			if ((pIndexFile != NULL) && (pBuffer != NULL))
			{
				int dateSeekBase;
				if ((dateSeekBase = *((int *)(pBuffer + headerSize) + index)) != 0)
				{
					pDateTable[index] = new DateNode(time, 0, dateSeekBase);
					pDateTable[index]->pParent = this;

					if (pDateTable[index]->isDirty)
					{
						// error
						delete pDateTable[index];
						pDateTable[index] = NULL;
					}
				}
			}
		}

		if (pDateTable[index] != NULL)
		{
			return pDateTable[index];
		}
	}

	return NULL;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
DateNode::DateNode(time_t time, int duration, int seek)
	: extraNodeCount(0),
	  pFirstFileNode(NULL), pLastFileNode(NULL), pParent(NULL),
	  seekBase(seek), headerSize(16), isDirty(false)
{
	size_t totalCapacity;

	startTime = (time - TIMEZONE) / (24 * 60 * 60) * (24 * 60 * 60) + TIMEZONE;
	endTime = startTime + 24 * 60 * 60;

	if ((pIndexFile != NULL) && (seekBase > 0)
	    && (fseek(pIndexFile, seekBase, SEEK_SET) == 0)
	    && (fread(&totalCapacity, 1, 4, pIndexFile) == 4)
	    && (fread(&duration, 1, 4, pIndexFile) == 4))
	{
		fileDuration = duration < 1 ? 1
		               : duration > 60 * 60 ? 60 * 60 : duration;

		fileTableCapacity = (24 * 60 * 60 + fileDuration - 1) / fileDuration;

		if (fileTableCapacity > totalCapacity)
		{
			// FIXME: something wrong
			totalCapacity = fileTableCapacity;
		}
		extraTableCapacity = totalCapacity - fileTableCapacity;

		bufferSize = headerSize + totalCapacity * 4 * 4;
		pBuffer = new char[bufferSize];

		*(int *)pBuffer = totalCapacity;
		*((int *)pBuffer + 1) = fileDuration;
		if (IndexFileRead(pBuffer + 8, bufferSize - 8) == false)
		{
			memset(pBuffer + 8, 0, bufferSize - 8);
			isDirty = true;
		}
	}
	else
	{
		// error
		if (pIndexFile != NULL)
		{
			//fclose(pIndexFile);
			//pIndexFile = NULL;
		}

		fileDuration = duration < 1 ? 1
		               : duration > 60 * 60 ? 60 * 60 : duration;

		fileTableCapacity = (24 * 60 * 60 + fileDuration - 1) / fileDuration;
		extraTableCapacity = fileTableCapacity > 24 * 60 ? fileTableCapacity : 24 * 60;
		totalCapacity = fileTableCapacity + extraTableCapacity;

		bufferSize = headerSize + totalCapacity * 4 * 4;
		pBuffer = new char[bufferSize];
		memset(pBuffer, 0, bufferSize);
		*(int *)pBuffer = totalCapacity;
		*((int *)pBuffer + 1) = fileDuration;
		isDirty = true;
	}

	pFileTable = new FileNode *[totalCapacity];
	for (size_t i = 0; i < totalCapacity; i++)
	{
		pFileTable[i] = NULL;
	}
}

DateNode::~DateNode()
{
	if (pFileTable != NULL)
	{
		for (size_t i = 0; i < fileTableCapacity; i++)
		{
			if (pFileTable[i] != NULL)
			{
				delete pFileTable[i];
			}
		}
		delete[] pFileTable;
	}
}

bool DateNode::ForceReloadBuffer()
{
	bool result = true;
	size_t reloadedTotalCapacity;
	size_t reloadedFileDuration;
	size_t reloadedBufferSize;

	if ((pIndexFile != NULL) && (seekBase > 0)
	    && (fseek(pIndexFile, seekBase, SEEK_SET) == 0)
	    && (fread(&reloadedTotalCapacity, 1, 4, pIndexFile) == 4)
	    && (fread(&reloadedFileDuration, 1, 4, pIndexFile) == 4))
	{
		reloadedFileDuration = reloadedFileDuration < 1 ? 1
		               : reloadedFileDuration > 60 * 60 ? 60 * 60 : reloadedFileDuration;

		if (reloadedFileDuration != (size_t)fileDuration)
		{
			result = false;
		}

		if (reloadedTotalCapacity != fileTableCapacity + extraTableCapacity)
		{
			result = false;

			if (reloadedTotalCapacity > fileTableCapacity + extraTableCapacity)
			{
				reloadedTotalCapacity = fileTableCapacity + extraTableCapacity;
			}
		}

		// recalculate bufferSize
		reloadedBufferSize = headerSize + reloadedTotalCapacity * 4 * 4;
		if (reloadedBufferSize > bufferSize)
		{
			reloadedBufferSize = bufferSize;
		}

		if (IndexFileRead(pBuffer + 8, reloadedBufferSize - 8) == false)
		{
			result = false;
		}
	}
	else
	{
		result = false;
	}

	return result;
}

bool DateNode::UpdateIndexFile()
{
	if (!isDirty || (pBuffer == NULL) || (bufferSize == 0) || (pIndexFile == NULL))
	{
		// there is nothing to do
		return true;
	}
	isDirty = false;

	if (seekBase > 0)
	{
		if (fseek(pIndexFile, seekBase, SEEK_SET) != 0)
		{
			// error:
			fclose(pIndexFile);
			pIndexFile = NULL;
			return false;
		}
	}
	else
	{
		if (fseek(pIndexFile, 0, SEEK_END) != 0)
		{
			// error:
			fclose(pIndexFile);
			pIndexFile = NULL;
			return false;
		}
		seekBase = ftell(pIndexFile);
		pParent->SetBufferContent(startTime, seekBase);
	}

	if (IndexFileWrite(pBuffer, bufferSize) == false)
	{
		// error:
		return false;
	}

	return true;
}

bool DateNode::SetBufferContent(time_t time, FileNode *currentNode)
{
	if ((time < startTime) || (time >= endTime) || (pBuffer == NULL))
	{
		// error: wrong time or wrong buffer
		return false;
	}

	int index = (time - startTime) / fileDuration;

	if ((index < 0) || ((size_t)index >= fileTableCapacity))
	{
		// error: wrong index
		return false;
	}

	if (seekBase > 0)
	{
		// FIXME: collision list
		currentNode->seekBase = seekBase + headerSize + index * 4 * 4;
	}

	*((int *)(pBuffer + headerSize) + index * 4) = currentNode->startTime;
	*((int *)(pBuffer + headerSize) + index * 4 + 1) = currentNode->endTime;
	if ((currentNode->pPrev != NULL) && (currentNode->pPrev->seekBase > 0))
	{
		*((int *)(pBuffer + headerSize) + index * 4 + 2) = currentNode->pPrev->seekBase;
	}
	else
	{
		//*((int *)(pBuffer + headerSize) + index * 4 + 2) = 0;
	}
	if ((currentNode->pNext != NULL) && (currentNode->pNext->seekBase > 0))
	{
		*((int *)(pBuffer + headerSize) + index * 4 + 3) = currentNode->pNext->seekBase;
	}
	else
	{
		//*((int *)(pBuffer + headerSize) + index * 4 + 3) = 0;
	}

	if (currentNode->seekBase > 0)
	{
		int firstSeekBase = *((int *)pBuffer + 2);
		int lastSeekBase  = *((int *)pBuffer + 3);
		if ((firstSeekBase == 0) || (currentNode->seekBase < firstSeekBase))
		{
			*((int *)pBuffer + 2) = currentNode->seekBase;
		}
		if ((lastSeekBase == 0) || (currentNode->seekBase > lastSeekBase))
		{
			*((int *)pBuffer + 3) = currentNode->seekBase;
		}
	}

	isDirty = true;
	return true;
}

FileNode * DateNode::GetFileNodeForwardly(time_t time)
{
	ForceReloadBuffer();

	if (time >= endTime)
	{
		// error: wrong time
		return NULL;
	}

	int index = time <= startTime ? 0 : (time - startTime) / fileDuration;
	int lastSeekBase;
	time_t lastStartTime;
	int lastIndex = fileTableCapacity - 1;

	if (((lastSeekBase = *((int *)pBuffer + 3)) - seekBase >= (int)headerSize)
	    && ((lastStartTime = *(int *)(pBuffer + lastSeekBase - seekBase)) > startTime))
	{
		lastIndex = (lastStartTime - startTime) / fileDuration;
	}

	if ((index < 0) || ((size_t)index >= fileTableCapacity) || (index > lastIndex))
	{
		// error: wrong index
		return NULL;
	}

	// while (valid index && (startTime == 0))
	for ( ; ((size_t)index < fileTableCapacity) && (index <= lastIndex) && (*(int *)(pBuffer + headerSize + index * 4 * 4) == 0); index++)
	{
	}

	if (((size_t)index >= fileTableCapacity) || (index > lastIndex))
	{
		// error: no index
		return NULL;
	}

	FileNode *node = pFileTable[index];
	time_t upperBound = startTime + (index + 1) * fileDuration;

	if (node == NULL)
	{
		// try to load the specific file node
		if ((pIndexFile != NULL) && (pBuffer != NULL))
		{
			int offset = headerSize + index * 4 * 4;

			// if (prev->startTime != 0) then go through the while-loop from prev
			if ((*(int *)(pBuffer + offset) != 0)
			    && (*((int *)(pBuffer + offset) + 2) != 0)
			    && (*((int *)(pBuffer + offset) + 2) - seekBase >= (int)headerSize)
			    && (*(int *)(pBuffer + *((int *)(pBuffer + offset) + 2) - seekBase) != 0))
			{
				offset = *((int *)(pBuffer + offset) + 2) - seekBase;
			}

			// if (startTime != 0)
			while ((offset >= (int)headerSize) && (*(int *)(pBuffer + offset) != 0) && (*(int *)(pBuffer + offset) < upperBound))
			{
				// if (time < endTime)
				if (time < *((int *)(pBuffer + offset) + 1))
				{
					// bingo!! load it
					// FIXME: incorrect list nodes
					//pFileTable[index] = new FileNode(FileNode::FILE_TYPE_NORMAL, seekBase + headerSize + index * 4 * 4);
					//pFileTable[index]->pParent = this;
					//return pFileTable[index];
					return new FileNode(FileNode::FILE_TYPE_NORMAL, seekBase + offset);
				}
				offset = *((int *)(pBuffer + offset) + 3) - seekBase;
			}
		}
	}

	return node;
}

FileNode * DateNode::GetFileNodeBackwardly(time_t time)
{
	ForceReloadBuffer();

	if (time < startTime)
	{
		// error: wrong time
		return NULL;
	}

	int index = ((time >= endTime ? endTime - 1 : time) - startTime) / fileDuration;
	int firstSeekBase;
	time_t firstStartTime;
	int firstIndex = 0;

	if (((firstSeekBase = *((int *)pBuffer + 2)) - seekBase >= (int)headerSize)
	    && ((firstStartTime = *(int *)(pBuffer + firstSeekBase - seekBase)) > startTime))
	{
		firstIndex = (firstStartTime - startTime) / fileDuration;
	}

	if ((index < 0) || ((size_t)index >= fileTableCapacity) || (index < firstIndex))
	{
		// error: wrong index
		return NULL;
	}

	// while (valid index && (startTime == 0))
	for ( ; (index >= 0) && (index >= firstIndex) && (*(int *)(pBuffer + headerSize + index * 4 * 4) == 0); index--)
	{
	}

	if ((index < 0) || (index < firstIndex))
	{
		// error: no index
		return NULL;
	}

	FileNode *node = pFileTable[index];
	time_t lowerBound = startTime + index * fileDuration;

	if (node == NULL)
	{
		// try to load the specific file node
		if ((pIndexFile != NULL) && (pBuffer != NULL))
		{
			int offset = headerSize + index * 4 * 4;

			// if (next->startTime != 0) then go through the while-loop from next
			if ((*((int *)(pBuffer + offset) + 1) != 0)
			    && (*((int *)(pBuffer + offset) + 3) != 0)
			    && (*((int *)(pBuffer + offset) + 3) - seekBase < (int)bufferSize)
			    && (*(int *)(pBuffer + *((int *)(pBuffer + offset) + 3) - seekBase) != 0))
			{
				offset = *((int *)(pBuffer + offset) + 3) - seekBase;
			}

			// if (endTime != 0)
			while ((offset < (int)bufferSize) && (*((int *)(pBuffer + offset) + 1) != 0) && (*((int *)(pBuffer + offset) + 1) >= lowerBound))
			{
				// if (time >= startTime)
				if (time >= *(int *)(pBuffer + offset))
				{
					// bingo!! load it
					// FIXME: incorrect list nodes
					//pFileTable[index] = new FileNode(FileNode::FILE_TYPE_NORMAL, seekBase + headerSize + index * 4 * 4);
					//pFileTable[index]->pParent = this;
					//return pFileTable[index];
					return new FileNode(FileNode::FILE_TYPE_NORMAL, seekBase + offset);
				}
				offset = *((int *)(pBuffer + offset) + 2) - seekBase;
			}
		}
	}

	return node;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
FileNode * const FileNode::pUnloadedNode = new FileNode(FILE_TYPE_UNLOADED);
FileNode * const FileNode::pUnkonwnNode  = new FileNode(FILE_TYPE_UNLOADED);

FileNode::FileNode(FileType _type, int seek)
	: type(_type), pParent(NULL), pPrev(NULL), pNext(NULL),
	  seekBase(seek), isDirty(false)
{
	startTime = endTime = 0;
	if ((pIndexFile != NULL) && (seekBase > 0)
	    && (fseek(pIndexFile, seekBase, SEEK_SET) == 0)
	    && (fread(&startTime, 1, 4, pIndexFile) == 4)
	    && (fread(&endTime, 1, 4, pIndexFile) == 4))
	{
	}
	else
	{
		// error
		if (pIndexFile != NULL)
		{
			//fclose(pIndexFile);
			//pIndexFile = NULL;
		}
		isDirty = true;
	}
}

const char * FileNode::GetFileName() const
{
	return fileName;
}

FileNode::~FileNode()
{
}

bool FileNode::RebuildFileName()
{
	struct tm *time;
	time = localtime(&startTime);

	// chxx/yyyy/MMdd/hhmmss_hhmmss.mkv
#if 1
	sprintf(fileNameBuffer, "ch%02d/%d/%02d%02d/%02d%02d%02d_",
#else
	sprintf(fileNameBuffer, "ch%02d_%d_%02d%02d_%02d%02d%02d_",
#endif
	        chId, 1900+time->tm_year, 1+time->tm_mon, time->tm_mday,
	        time->tm_hour, time->tm_min, time->tm_sec);

	time = localtime(&endTime);

	// chxx/yyyy/MMdd/hhmmss_hhmmss.mkv
	sprintf(fileNameBuffer + 22, "%02d%02d%02d.mkv",
	        time->tm_hour, time->tm_min, time->tm_sec);

	fileName = fileNameBuffer;
	return true;
}

bool FileNode::RenameFrom(const char *old)
{
	RebuildFileName();

#if 1
	fileNameBuffer[14] = '\0';
	boost::filesystem::path dir(fileNameBuffer, boost::filesystem::native);
	fileNameBuffer[14] = '/';
	// test dir
	if (!boost::filesystem::exists(dir))
	{
		// create dir
		if (!boost::filesystem::create_directories(dir))
		{
			// error: fail to create dir
			return false;
		}
	}
#endif

	return rename(old, fileNameBuffer) == 0;
}

// for indexing
FileNode * FileNode::LoadNextFromFileDirectly()
{
	if (pNext != NULL)
	{
		return pNext;
	}

	// force to reload file
	if (pIndexFile != NULL)
	{
		fclose(pIndexFile);
	}
	pIndexFile = fopen(".index", "rb+");

	if ((pIndexFile == NULL) || (seekBase <= 0))
	{
		return NULL;
	}

	int nextSeekBase;
	if ((fseek(pIndexFile, seekBase + 12, SEEK_SET) == 0)
	    && (fread(&nextSeekBase, 1, 4, pIndexFile) == 4)
		&& (nextSeekBase > 0))
	{
		pNext = new FileNode(FileNode::FILE_TYPE_NORMAL, nextSeekBase);
		pNext->pPrev = this;
		if (!pNext->isDirty)
		{
			return pNext;
		}
	}

	return NULL;
}

// for indexing
FileNode * FileNode::LoadPreviousFromFileDirectly()
{
	if (pPrev != NULL)
	{
		return pPrev;
	}

	// force to reload file
	if (pIndexFile != NULL)
	{
		fclose(pIndexFile);
	}
	pIndexFile = fopen(".index", "rb+");

	if ((pIndexFile == NULL) || (seekBase <= 0))
	{
		return NULL;
	}

	int prevSeekBase;
	if ((fseek(pIndexFile, seekBase + 8, SEEK_SET) == 0)
	    && (fread(&prevSeekBase, 1, 4, pIndexFile) == 4)
		&& (prevSeekBase > 0))
	{
		pPrev = new FileNode(FileNode::FILE_TYPE_NORMAL, prevSeekBase);
		pPrev->pNext = this;
		if (!pPrev->isDirty)
		{
			return pPrev;
		}
	}

	return NULL;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
class StreamingMediaLibraryImpl
{
public:
	enum
	{
		TEMPORARY_FILE_NAME_COUNT = 128,
	};

	StreamingMediaLibraryImpl();
	virtual ~StreamingMediaLibraryImpl();
	bool Initialize();

	StreamingMediaLibrary *pPublic;

	static bool isInitialized;
	static RootIndexNode *pIndexRoot;
	static unsigned int temporaryFileNameIndex;
	static char **temporaryFileNames;
};

bool StreamingMediaLibraryImpl::isInitialized = false;
RootIndexNode *StreamingMediaLibraryImpl::pIndexRoot;
unsigned int StreamingMediaLibraryImpl::temporaryFileNameIndex = -1;
char **StreamingMediaLibraryImpl::temporaryFileNames;

StreamingMediaLibraryImpl::StreamingMediaLibraryImpl()
{
	Initialize();
}

StreamingMediaLibraryImpl::~StreamingMediaLibraryImpl()
{
}

bool StreamingMediaLibraryImpl::Initialize()
{
	if (!isInitialized)
	{
		isInitialized = true;

		struct tm t;
		t.tm_year = 1970 - 1900;
		t.tm_mon  = 0;
		t.tm_mday = 2;
		t.tm_hour = 0;
		t.tm_min  = 0;
		t.tm_sec  = 0;
		TIMEZONE = mktime(&t) - 24 * 60 * 60;

		pIndexRoot = RootIndexNode::GetRootIndexNode(FileNode::DEFAULT_CHANNEL_COUNT);  // FIXME: MUST check the status

		// initialize root node

		temporaryFileNames = new char *[TEMPORARY_FILE_NAME_COUNT];
		for (int i = 0; i < TEMPORARY_FILE_NAME_COUNT; i++)
		{
			temporaryFileNames[i] = new char[8];
			sprintf(temporaryFileNames[i], "%03d.mkv", i);
		}
	}
	return true;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
StreamingMediaChannelHelper::StreamingMediaChannelHelper(StreamingMediaLibrary &_library, int chId)
	: library(_library)
{
	pRecordingMediaFile = new StreamingMediaFileImpl();
	pLocatingMediaFile  = new StreamingMediaFileImpl();
	pRecordingMediaFile->chId = chId;
	pLocatingMediaFile->chId  = chId;
}

StreamingMediaChannelHelper::~StreamingMediaChannelHelper()
{
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
StreamingMediaLibrary::StreamingMediaLibrary()
{
	pImpl = new StreamingMediaLibraryImpl();
	pImpl->pPublic = this;
}

StreamingMediaLibrary::~StreamingMediaLibrary()
{
	if (pImpl != NULL)
	{
		delete pImpl;
	}
}

// params: file, ch id, start time, end time
bool StreamingMediaLibrary::AddMediaFile(const char *fileName, StreamingMediaFile &mediaFile)
{
	StreamingMediaFileImpl *pFileNode = dynamic_cast<StreamingMediaFileImpl *>(&mediaFile);
	if (pFileNode == NULL)
	{
		return false;
	}

	if (pFileNode->RenameFrom(fileName))
	{
		// create file node
		FileNode *pCurrentNode = new FileNode(FileNode::FILE_TYPE_NORMAL);
		*pCurrentNode = *pFileNode;
		pCurrentNode->fileName = pCurrentNode->fileNameBuffer;

		return pImpl->pIndexRoot->InsertIndex(pCurrentNode);
	}
	else
	{
		// error: fail to rename the file
		// FIXME:
		pFileNode->fileName = NULL;
		return false;
	}
}

// params: ch id, start time
bool StreamingMediaLibrary::AllocateRecordingFile(StreamingMediaFile &mediaFile)
{
	StreamingMediaFileImpl *pFileNode = dynamic_cast<StreamingMediaFileImpl *>(&mediaFile);
	if (pFileNode == NULL)
	{
		return false;
	}

	// cyclically reuse these file names
	if (++pImpl->temporaryFileNameIndex >= pImpl->TEMPORARY_FILE_NAME_COUNT)
	{
		pImpl->temporaryFileNameIndex = 0;
	}

	pFileNode->fileName = pImpl->temporaryFileNames[pImpl->temporaryFileNameIndex];
	return true;
}

// params: ch id, time
bool StreamingMediaLibrary::ExtendRecordingClip(int chId, time_t)
{
	return true;
}

uint64_t StreamingMediaLibrary::GetSuggestedDuration(int chId, uint64_t startTime)
{
	time_t duration = pImpl->pIndexRoot->GetCurrentDuration(chId);

	time_t _start = startTime / 1000000000ull;
	struct tm *start;
	start = localtime(&_start);

	return (duration - (start->tm_min * 60 + start->tm_sec) % duration) * 1000000000ull - startTime % 1000000000ull;
}

// params: ch id, time, or MediaFile
StreamingMediaFile & StreamingMediaLibrary::LocateMediaFile(StreamingMediaFile &fileNode, LocatingOption option)
{
	switch (option)
	{
	case NEXT_ONE:
		{
			StreamingMediaFileImpl *pFileNode = dynamic_cast<StreamingMediaFileImpl *>(&fileNode);
			if ((pFileNode != NULL)
			    && ((pFileNode->GetNext() != NULL)
			        || (pFileNode->LoadNext() != NULL)))
			{
				pFileNode->GetNext()->chId = fileNode.chId;
				pFileNode->GetNext()->RebuildFileName();
				return *pFileNode->GetNext();
			}
		}
		break;

	case PREVIOUS_ONE:
		{
			StreamingMediaFileImpl *pFileNode = dynamic_cast<StreamingMediaFileImpl *>(&fileNode);
			if ((pFileNode != NULL)
			    && ((pFileNode->GetPrev() != NULL)
			        || (pFileNode->LoadPrev() != NULL)))
			{
				pFileNode->GetPrev()->chId = fileNode.chId;
				pFileNode->GetPrev()->RebuildFileName();
				return *pFileNode->GetPrev();
			}
		}
		break;

	default:  // assume to use FORWARD_SEARCH
	case FORWARD_SEARCH:
		{
			FileNode *pFileNode = pImpl->pIndexRoot->SearchForwardlyAndLoad(fileNode.chId, fileNode.startTime);

			if (pFileNode != NULL)
			{
				pFileNode->chId = fileNode.chId;
				pFileNode->RebuildFileName();
				return *pFileNode;
			}
		}
		break;

	case BACKWARD_SEARCH:
		{
			FileNode *pFileNode = pImpl->pIndexRoot->SearchBackwardlyAndLoad(fileNode.chId, fileNode.startTime);

			if (pFileNode != NULL)
			{
				pFileNode->chId = fileNode.chId;
				pFileNode->RebuildFileName();
				return *pFileNode;
			}
		}
		break;
	}

	// error:
	fileNode.startTime = fileNode.endTime = 0;
	return fileNode;
}
