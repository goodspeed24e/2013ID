

//////////////////////////////////////////////////////////////////////
// global functions
//////////////////////////////////////////////////////////////////////

inline CTimeValue 
gettimeofday(void)
{
#if defined (WIN32)
	FILETIME tfile;
	::GetSystemTimeAsFileTime(&tfile);
	return CTimeValue(tfile);
#else
	// NOTE:
	// int gettimeofday(struct timeval *tv, struct timezone *tz);
	// ::gettimeofday() and ::settimeofday() return 0 for success, or -1  for  failure
	// (in which case errno is set appropriately).
	//
	timeval tv;
	return ::gettimeofday(&tv, 0) == -1 ? CTimeValue(-1) : CTimeValue(tv);
#endif
}

inline CTimeValue 
operator + (const CTimeValue &tv1, const CTimeValue &tv2)
{
	CTimeValue sum (tv1.sec () + tv2.sec (),
		tv1.usec () + tv2.usec ());

	sum.normalize();
	return sum;
}

inline CTimeValue 
operator - (const CTimeValue &tv1, const CTimeValue &tv2)
{
	CTimeValue delta (tv1.sec () - tv2.sec (),
		tv1.usec () - tv2.usec ());
	delta.normalize();
	return delta;
}

inline CTimeValue 
operator * (double d, const CTimeValue &tv)
{
	return CTimeValue (tv) *= d;
}

inline CTimeValue 
operator * (const CTimeValue &tv, double d)
{
	return CTimeValue (tv) *= d;
}

// True if tv1 > tv2.
inline int 
operator > (const CTimeValue &tv1, const CTimeValue &tv2)
{
	if (tv1.sec () > tv2.sec ())
		return 1;
	else if (tv1.sec () == tv2.sec ()
		&& tv1.usec () > tv2.usec ())
		return 1;
	else
		return 0;
}

// True if tv1 >= tv2.
inline int 
operator >= (const CTimeValue &tv1, const CTimeValue &tv2)
{
	if (tv1.sec () > tv2.sec ())
		return 1;
	else if (tv1.sec () == tv2.sec ()
		&& tv1.usec () >= tv2.usec ())
		return 1;
	else
		return 0;
}

// True if tv1 < tv2.
inline int 
operator < (const CTimeValue &tv1, const CTimeValue &tv2)
{
	return tv2 > tv1;
}

// True if tv1 >= tv2.
inline int 
operator <= (const CTimeValue &tv1, const CTimeValue &tv2)
{
	return tv2 >= tv1;
}

// True if tv1 == tv2.
inline int 
operator == (const CTimeValue &tv1, const CTimeValue &tv2)
{
	return tv1.sec () == tv2.sec ()
		&& tv1.usec () == tv2.usec ();
}

// True if tv1 != tv2.
inline int 
operator != (const CTimeValue &tv1, const CTimeValue &tv2)
{
	return !(tv1 == tv2);
}



#if 0
//////////////////////////////////////////////////////////////////////
//	class CDateTime
//////////////////////////////////////////////////////////////////////

inline void
CDateTime::update(const CTimeValue& timevalue)
{
	time_t utc_time = timevalue.sec();
	struct tm tm_time;
	::gmtime_r(&utc_time, &tm_time);
	this->day_ = tm_time.tm_mday;
	this->month_ = tm_time.tm_mon + 1;    //months are 0-11
	this->year_ = tm_time.tm_year + 1900; //reports years since 1900
	this->hour_ = tm_time.tm_hour;
	this->minute_ = tm_time.tm_min;
	this->second_ = tm_time.tm_sec;
	this->microsec_ = timevalue.usec();
	this->wday_ = tm_time.tm_wday;
	struct tm local_tm;
	const time_t ts = this->second_;
	localtime_r(&ts, &local_tm);
	this->isdst_ = local_tm.tm_isdst;
}

inline void
CDateTime::update(void)
{
	this->update(instek::gettimeofday());
}

// get day
inline int
CDateTime::day(void) const
{
	return day_;
}

// set day
inline void
CDateTime::day(int day)
{
	day_ = day;
}

// get month
inline int
CDateTime::month(void) const
{
	return month_;
}

// set month
inline void
CDateTime::month(int month)
{
	month_ = month;
}

// get year
inline int
CDateTime::year(void) const
{
	return year_;
}

// set year
inline void
CDateTime::year(int year)
{
	year_ = year;
}

// get hour
inline int
CDateTime::hour(void) const
{
	return hour_;
}

// set hour
inline void
CDateTime::hour(int hour)
{
	hour_ = hour;
}

// get minute
inline int
CDateTime::minute(void) const
{
	return minute_;
}

// set minute
inline void
CDateTime::minute(int minute)
{
	minute_ = minute;
}

// get second
inline int
CDateTime::second(void) const
{
	return second_;
}

// set second
inline void
CDateTime::second(int second)
{
	second_ = second;
}

// get microsec
inline int
CDateTime::microsec(void) const
{
	return microsec_;
}

// set microsec
inline void
CDateTime::microsec(int microsec)
{
	microsec_ = microsec;
}

// get wday
inline int
CDateTime::weekday(void) const
{
	return wday_;
}

// set wday
inline void
CDateTime::weekday(int wday)
{
	wday_ = wday;
}

// get isdst
inline int
CDateTime::isdst(void) const
{
	return isdst_;
}

// set isdst
inline void
CDateTime::isdst(int dst)
{
	isdst_ = dst;
}



//////////////////////////////////////////////////////////////////////
//	class CCountdownTimer
//////////////////////////////////////////////////////////////////////

inline int 
CCountdownTimer::stopped(void) const
{
	return stopped_;
}

inline int 
CCountdownTimer::update(void)
{
	return this->stop() == 0 && this->start();
}


#endif
