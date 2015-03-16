
#if !defined(TIMESTAMP_CPP__CECE0FA8_966A_9632_BB99_C265C067D814__)
#define TIMESTAMP_CPP__CECE0FA8_966A_9632_BB99_C265C067D814__

#include <timestamp.hpp>
#include <assert.h>

using namespace instek;

//////////////////////////////////////////////////////////////////////
// global functions and variables
//////////////////////////////////////////////////////////////////////

/**
 * Static constant representing `zero-time'.
 * Note: this object requires static construction.
 */
const CTimeValue CTimeValue::zero = CTimeValue(0, 0);

/**
 * Constant for maximum time representable.  Note that this time
 * is not intended for use with select () or other calls that may
 * have *their own* implementation-specific maximum time representations.
 * Its primary use is in time computations such as those used by the
 * dynamic subpriority strategies in the ACE_Dynamic_Message_Queue class.
 * Note: this object requires static construction.
 */
const CTimeValue CTimeValue::max_time (LONG_MAX, UTIL_ONE_SECOND_IN_USECS - 1);

/**
 * Static constant to remove time skew between FILETIME and POSIX
 * time.  POSIX and Win32 use different epochs (Jan. 1, 1970 v.s.
 * Jan. 1, 1601).  The following constant defines the difference
 * in 100ns ticks.
 * 
 * In the beginning (Jan. 1, 1601), there was no time and no computer.
 * And Bill said: "Let there be time," and there was time....
 */
#if defined (WIN32)
#include<functional>
#include<algorithm>
const DWORDLONG CTimeValue::FILETIME_to_timval_skew =
	UTIL_INT64_LITERAL(0x19db1ded53e8000);
#endif


//////////////////////////////////////////////////////////////////////
// class CTimeValue
//////////////////////////////////////////////////////////////////////

CTimeValue::CTimeValue(void)
{
	this->set(0, 0);
}

CTimeValue::CTimeValue(long sec, long usec)
{
	this->set(sec, usec);
}

CTimeValue::CTimeValue(const CTimeValue &tv)
{
	this->tv_ = tv.tv_;
}

#if defined (WIN32)
//Initializes the CTimeValue object from a Win32 FILETIME
CTimeValue::CTimeValue(const FILETIME &file_time)
{
	this->set(file_time);
}

CTimeValue::CTimeValue(const SYSTEMTIME &st)
{
	this->set(st);
}
#endif

CTimeValue::CTimeValue(const struct timeval &tv)
{
	this->set(tv);
}

void CTimeValue::set(long sec, long usec)
{
	this->tv_.tv_sec = sec;
	this->tv_.tv_usec = usec;
	this->normalize ();
	
	if (*this > CTimeValue::max_time || *this < CTimeValue::zero) {
		this->tv_.tv_sec = 0;
		this->tv_.tv_usec = 0;		
	}
}

void CTimeValue::set(double d)
{
	long l = (long) d;
	this->tv_.tv_sec = l;
	this->tv_.tv_usec = (long) ((d - (double) l) * UTIL_ONE_SECOND_IN_USECS + .5);
	this->normalize ();
}

void CTimeValue::set(const timeval &tv)
{
	this->tv_.tv_sec = tv.tv_sec;
	this->tv_.tv_usec = tv.tv_usec;
	this->normalize ();

	if (*this > CTimeValue::max_time || *this < CTimeValue::zero) {
		this->tv_.tv_sec = 0;
		this->tv_.tv_usec = 0;		
	}
}

#if defined (WIN32)
void CTimeValue::set(const time_t &t)
{
	FILETIME file_time;
    LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
    file_time.dwLowDateTime = (DWORD) ll;
    file_time.dwHighDateTime = (DWORD) (ll >>32);
	this->set(file_time);
}

void CTimeValue::set(const FILETIME &file_time)
{
	//Initializes the CTimeValue object from a Win32 FILETIME
	//Don't use a struct initializer, gcc don't like it.
	ULARGE_INTEGER _100ns;
	_100ns.LowPart = file_time.dwLowDateTime;
	_100ns.HighPart = file_time.dwHighDateTime;
	_100ns.QuadPart -= CTimeValue::FILETIME_to_timval_skew;

	//Convert 100ns units to seconds;
	this->tv_.tv_sec = (long) (_100ns.QuadPart / (10000 * 1000));

	//Convert remainder to microseconds;
	this->tv_.tv_usec = (long) ((_100ns.QuadPart % (10000 * 1000)) / 10);

	this->normalize ();
}

void CTimeValue::set(const SYSTEMTIME &t)
{
	FILETIME file_time;

	if( ::SystemTimeToFileTime(&t, &file_time)==TRUE )
		this->set(file_time);
}
#endif

// Converts from CTimeValue format into milli-seconds format.
unsigned long CTimeValue::msec(void) const
{
	return this->tv_.tv_sec * 1000 + this->tv_.tv_usec / 1000;
}

// Returns the value of the object as a timeval.
CTimeValue::operator timeval () const
{
	return this->tv_;
}

CTimeValue::operator const timeval * () const
{
	return (const timeval *) &this->tv_;
}

#if defined (WIN32)
// Returns the value of the object as a Win32 FILETIME.
CTimeValue::operator SYSTEMTIME () const
{
	// NOTE:
	// BOOL FileTimeToSystemTime(const FILETIME* lpFileTime, LPSYSTEMTIME lpSystemTime); 
	// The FileTimeToSystemTime function only works with FILETIME values that are 
	// less than 0x8000000000000000. The function fails with values equal to or greater than that. 
	//
	// Nonzero indicates success. Zero indicates failure. To get extended error information, call GetLastError.
	
	SYSTEMTIME sys_time;
	::memset(&sys_time, 0, sizeof(SYSTEMTIME));
	
	FILETIME file_time = (FILETIME)*this;
	if (::FileTimeToSystemTime(&file_time, &sys_time) != 0) {
		return sys_time;
	} else {
		// HELP:
		// If you know how to handle this kind of error in here, please fix me.
		return sys_time;
	}
}

// Returns the value of the object as a Win32 FILETIME.
CTimeValue::operator FILETIME () const
{
	FILETIME file_time;

	ULARGE_INTEGER _100ns;
	_100ns.QuadPart = (((DWORDLONG) this->tv_.tv_sec * (10000 * 1000) +
		this->tv_.tv_usec * 10) +
		CTimeValue::FILETIME_to_timval_skew);

	file_time.dwLowDateTime = _100ns.LowPart;
	file_time.dwHighDateTime = _100ns.HighPart;

	return file_time;
}
#endif

// Returns number of seconds.
long CTimeValue::sec() const
{
	return this->tv_.tv_sec;
}

// Returns number of micro-seconds.
long CTimeValue::usec() const
{
	return this->tv_.tv_usec;
}

CTimeValue & CTimeValue::operator*= (double d)
{
	double time = ((double) this->sec ()) * UTIL_ONE_SECOND_IN_USECS + this->usec ();
	time *= d;
	this->set((long)(time / UTIL_ONE_SECOND_IN_USECS), ((long)time) % UTIL_ONE_SECOND_IN_USECS);
	this->normalize ();
	return *this;
}

// Add TV to this.
CTimeValue & CTimeValue::operator+= (const CTimeValue &tv)
{
	this->set(this->sec () + tv.sec (), this->usec () + tv.usec ());
	this->normalize ();
	return *this;
}

// Subtract TV to this.
CTimeValue & CTimeValue::operator-= (const CTimeValue &tv)
{
	this->set(this->sec() - tv.sec(), this->usec() - tv.usec());
	this->normalize ();
	return *this;
}

void CTimeValue::dump(void) const
{
	::fprintf(stdout, "tv_sec_(%d), tv_usec_(%d)\n", static_cast<int> (this->tv_.tv_sec), 
		static_cast<int> (this->tv_.tv_usec));
}

void CTimeValue::normalize(void)
{
	// From Hans Rohnert...
	if (this->tv_.tv_usec >= UTIL_ONE_SECOND_IN_USECS)
	{
		do
		{
			this->tv_.tv_sec++;
			this->tv_.tv_usec -= UTIL_ONE_SECOND_IN_USECS;
		}
		while (this->tv_.tv_usec >= UTIL_ONE_SECOND_IN_USECS);
	}
	else if (this->tv_.tv_usec <= -UTIL_ONE_SECOND_IN_USECS)
	{
		do
		{
			this->tv_.tv_sec--;
			this->tv_.tv_usec += UTIL_ONE_SECOND_IN_USECS;
		}
		while (this->tv_.tv_usec <= -UTIL_ONE_SECOND_IN_USECS);
	}

	if (this->tv_.tv_sec >= 1 && this->tv_.tv_usec < 0)
	{
		this->tv_.tv_sec--;
		this->tv_.tv_usec += UTIL_ONE_SECOND_IN_USECS;
	}
	else if (this->tv_.tv_sec < 0 && this->tv_.tv_usec > 0)
	{
		this->tv_.tv_sec++;
		this->tv_.tv_usec -= UTIL_ONE_SECOND_IN_USECS;
	}
}



#if 0
//////////////////////////////////////////////////////////////////////
//	class CDateTime
//////////////////////////////////////////////////////////////////////

CDateTime::CDateTime(void)
{
	this->update();
}

CDateTime::CDateTime(const CTimeValue& timevalue)
{
	this->update(timevalue);
}

// Constructor with init values, no check for validy
CDateTime::CDateTime(
	int day,
	int month,
	int year,
	int hour,
	int minute,
	int second,
	int microsec,
	int wday,
	int dst)
	: 	day_ (day),
		month_ (month),
		year_ (year),
		hour_ (hour),
		minute_ (minute),
		second_ (second),
		microsec_ (microsec),
		wday_ (wday),
		isdst_ (dst)
{
	assert(year > 0 && "year <= 0 must NOT occur.");
	assert(month > 0 && "month <= 0 must NOT occur.");
	assert(day > 0 && "day <= 0 must NOT occur.");
	assert(hour >= 0 && "hour < 0 must NOT occur.");
	assert(minute >= 0 && "hour < 0 must NOT occur.");
	assert(second >= 0 && "second < 0 must NOT occur.");
	assert(microsec >=0 && "microsec < 0 must NOT occur.");
	assert(wday >=0 && "wday < 0 must NOT occur.");
}



//////////////////////////////////////////////////////////////////////
//	class CCountdownTimer
//////////////////////////////////////////////////////////////////////

CCountdownTimer::CCountdownTimer (CTimeValue *max_wait_time)
	:	max_wait_time_ (max_wait_time),
    stopped_ (0)
{
  	this->start();
}

CCountdownTimer::~CCountdownTimer(void)
{
	this->stop();
}

int CCountdownTimer::start(void)
{
	if (this->max_wait_time_ != 0) {
		this->start_time_ = instek::gettimeofday();
		this->stopped_ = 0;
	}
	return 0;
}

int CCountdownTimer::stop(void)
{
	if (this->max_wait_time_ != 0 && this->stopped_ == 0) {
		CTimeValue elapsed_time = instek::gettimeofday() - this->start_time_;
		
		if (*this->max_wait_time_ > elapsed_time) {
			*this->max_wait_time_ -= elapsed_time;
		} else {
			// Used all of timeout.
			*this->max_wait_time_ = CTimeValue::zero;
			// errno = ETIME;
		}
		this->stopped_ = 1;
	}
  	return 0;
}





//////////////////////////////////////////////////////////////////////
//	class CTimeKeeper
//////////////////////////////////////////////////////////////////////

CTimeKeeper::CTimeKeeper(void)
	:	items_ ()
{
}

CTimeKeeper::~CTimeKeeper(void)
{
	std::for_each(this->items_.begin(), this->items_.end(), CTimeKeeper::cleanup_timer());
}

void CTimeKeeper::mark(int item_idx, const char *src_file, int src_line)
{
	timer_item *ele = new timer_item;
	ele->item_tv_ = instek::gettimeofday();
	ele->src_line_ = src_line;
	ele->src_file_.assign(src_file);

	ele->item_name_.resize(20);
	::sprintf(&ele->item_name_[0], "%08d", item_idx);
	
	this->items_.push_back(ele);
}

void CTimeKeeper::mark(const std::string &item_name, const char *src_file, int src_line)
{
	timer_item *ele = new timer_item;
	ele->item_tv_ = instek::gettimeofday();
	ele->src_line_ = src_line;
	ele->item_name_.assign(item_name);
	ele->src_file_.assign(src_file);
	
	this->items_.push_back(ele);
}

void CTimeKeeper::dump(bool verbose) const
{
	CTimeValue last_tv(CTimeValue::zero);
	
	for (std::vector<timer_item *>::const_iterator iter = this->items_.begin();
		 iter != this->items_.end();
		 ++iter)
	{
		CDateTime dt((*iter)->item_tv_);
		
		if (verbose && iter == this->items_.begin())
			::printf("mark(%s) => datetime(%04d-%02d-%02d %02d:%02d:%02d.%06d), delay(%09d), src(%s:%d)\n", 
				(*iter)->item_name_.c_str(), dt.year(), dt.month(), dt.day(), 
				dt.hour(), dt.minute(), dt.second(), dt.microsec(), 0,
				(*iter)->src_file_.c_str(), (*iter)->src_line_);
				
		else if (verbose && iter != this->items_.begin())
			::printf("mark(%s) => datetime(%04d-%02d-%02d %02d:%02d:%02d.%06d), delay(%09d), src(%s:%d)\n", 
				(*iter)->item_name_.c_str(), dt.year(), dt.month(), dt.day(), 
				dt.hour(), dt.minute(), dt.second(), dt.microsec(), (int)((*iter)->item_tv_ - last_tv).msec(),
				(*iter)->src_file_.c_str(), (*iter)->src_line_);
				
		else if (!verbose && iter == this->items_.begin())
			::printf("mark(%s) => datetime(%02d:%02d:%02d.%06d), delay(%09d)\n", 
				(*iter)->item_name_.c_str(),  
				dt.hour(), dt.minute(), dt.second(), dt.microsec(), 0);
				
		else
			::printf("mark(%s) => datetime(%02d:%02d:%02d.%06d), delay(%09d)\n", 
				(*iter)->item_name_.c_str(),  
				dt.hour(), dt.minute(), dt.second(), dt.microsec(), 
				(int)((*iter)->item_tv_ - last_tv).msec());
		
		last_tv = (*iter)->item_tv_;
	}
}
#endif
#endif // TIMESTAMP_CPP__CECE0FA8_966A_9632_BB99_C265C067D814__
