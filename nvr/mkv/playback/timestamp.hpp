
#if !defined(TIMESTAMP_HPP__CECE0FA8_966A_4192_BB99_C265C067D814__)
#define TIMESTAMP_HPP__CECE0FA8_966A_4192_BB99_C265C067D814__

#include "util_config.hpp"

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#if defined (WIN32)
	#include <functional>
	#include <WinSock2.h>
#else
	#include <sys/time.h>
#endif

#include <memory>
#include <string>
#include <vector>

using namespace std;

#define UTIL_ONE_SECOND_IN_MSECS 1000L
#define UTIL_ONE_SECOND_IN_USECS 1000000L
#define UTIL_ONE_SECOND_IN_NSECS 1000000000L

#define UTIL_INT64_LITERAL(n) 			n ## i64
#define TIME_KEEPER_MARK(keeper, item)	(keeper).mark(item, __FILE__, __LINE__)


namespace instek
{
/// Predefine classes
class CTimeValue;
//class CDateTime;
//class CCountdownTimer;
//class CTimeKeeper;


/** 
 * This class encapsulate timeval object, and provide every function that
 * timeval have.
 * 
 * @version 1.0, 21/05/2004
 * @author	clifford <cliffordmooi@instekdigital.com>
 */
class InstekUtil_Export CTimeValue
{
public:
	CTimeValue(void);
	CTimeValue(const CTimeValue &tv);
	explicit CTimeValue(long sec, long usec = 0);
	explicit CTimeValue(const struct timeval &t);

#if defined (WIN32)
	explicit CTimeValue(const SYSTEMTIME &t);
	explicit CTimeValue(const FILETIME &ft);
#endif

	/// Initializes the CTimeValue from two longs.
	void set(long sec, long usec);
	void set(double d);
	void set(const timeval &t);

#if defined (WIN32)
	void set(const time_t &t);
	void set(const SYSTEMTIME &t);
	void set(const FILETIME &ft);
#endif

	operator timeval (void) const;
	operator const timeval * (void) const;
	
#if defined (WIN32)
	operator SYSTEMTIME (void) const;
	operator FILETIME (void) const;
#endif

	long sec() const;
	unsigned long msec() const;
	long usec() const;

	void dump(void) const;

	// = The following arithmetic methods operate on CTimeValue's.
	CTimeValue & operator+= (const CTimeValue &tv);
	CTimeValue & operator-= (const CTimeValue &tv);
	CTimeValue & operator*= (double d);

	friend CTimeValue operator + (const CTimeValue &tv1, const CTimeValue &tv2);
	friend CTimeValue operator - (const CTimeValue &tv1, const CTimeValue &tv2);
	friend int operator < (const CTimeValue &tv1, const CTimeValue &tv2);
	friend int operator > (const CTimeValue &tv1, const CTimeValue &tv2);
	friend int operator <= (const CTimeValue &tv1, const CTimeValue &tv2);
	friend int operator >= (const CTimeValue &tv1, const CTimeValue &tv2);
	friend int operator == (const CTimeValue &tv1, const CTimeValue &tv2);
	friend int operator != (const CTimeValue &tv1, const CTimeValue &tv2);
	friend CTimeValue operator * (double d, const CTimeValue &tv);
	friend CTimeValue operator * (const CTimeValue &tv, double d);

public:
	///Constant "0".
	static const CTimeValue zero;

	/// Constant for maximum time representable.  Note that this time is
	/// not intended for use with <select> or other calls that may have
	/// *their own* implementation-specific maximum time representations.
	/// Its primary use is in time computations such as those used by the
	/// dynamic subpriority strategies in the ACE_Dynamic_Message_Queue
	/// class.
	static const CTimeValue max_time;

#if defined (WIN32)
	static const DWORDLONG FILETIME_to_timval_skew;
#endif

private:
	/// Put the timevalue into a canonical form.
	void normalize(void);

	/// The timeval structure is used to specify time values. It is associated 
	/// with the Berkeley Software Distribution (BSD) file Time.h.
	timeval tv_;
	
};


#if 0
/** 
 * The Date class provides a set of system independant date handling methods.  
 * The class can be used for any purpose. It is used as a parameter and return  
 * value in various PKI-Plus methods. 
 *  
 * @version 1.0, 21/05/2004
 * @author	clifford <cliffordmooi@instekdigital.com>
 */
class InstekUtil_Export CDateTime
{
public:
	/// Constructor initializes current time/date info.
	CDateTime(void);
	
	/// Constructor initializes with the given ACE_Time_Value
	explicit CDateTime(const CTimeValue& timevalue);
	
	/// Constructor with init values, no check for validy
	/// Set/get portions of CDateTime, no check for validity.
	CDateTime(int day, int month = 0, int year = 0,
		int hour = 0, int minute = 0, int second = 0,
		int microsec = 0, int wday = 0, int dst = 0);
	
	/// Update to the current time/date.
	void update(void);
	
	/// Update to the given ACE_Time_Value
	void update(const CTimeValue& timevalue);
	
	/// Get day.
	int day(void) const;
	
	/// Set day.
	void day(int day);
	
	/// Get month.
	int month(void) const;
	
	/// Set month.
	void month(int month);
	
	/// Get year.
	int year(void) const;
	
	/// Set year.
	void year(int year);
	
	/// Get hour.
	int hour(void) const;
	
	/// Set hour.
	void hour(int hour);
	
	/// Get minute.
	int minute(void) const;
	
	/// Set minute.
	void minute(int minute);
	
	/// Get second.
	int second(void) const;
	
	/// Set second.
	void second(int second);
	
	/// Get microsec.
	int microsec(void) const;
	
	/// Set microsec.
	void microsec(int microsec);
	
	/// Get weekday.
	int weekday(void) const;
	
	/// Set weekday.
	void weekday(int wday);

	/// Get isDST
	int isdst(void) const;

	/// Set isDst
	void isdst(int dst);

private:
	int day_;
	int month_;
	int year_;
	int hour_;
	int minute_;
	int second_;
	int microsec_;
	int wday_;
	int isdst_;
	
	
};




/**
 * Keeps track of the amount of elapsed time.
 *
 * This class has a side-effect on the <max_wait_time> -- every
 * time the <stop> method is called the <max_wait_time> is
 * updated.
 * 
 * @version 1.0, 21/05/2004
 * @author	clifford <cliffordmooi@instekdigital.com>
 */
class InstekUtil_Export CCountdownTimer
{
public:
	// = Initialization and termination methods.
	/// Cache the <max_wait_time> and call <start>.
	explicit CCountdownTimer(CTimeValue *max_wait_time);
	
	/// Call <stop>.
	~CCountdownTimer(void);
	
	/// Cache the current time and enter a start state.
	int start(void);
	
	/// Subtract the elapsed time from max_wait_time_ and enter a stopped
	/// state.
	int stop(void);
	
	/// Calls stop and then start.  max_wait_time_ is modified by the
	/// call to stop.
	int update(void);
	
	/// Returns 1 if we've already been stopped, else 0.
	int stopped(void) const;

private:
	/// Maximum time we were willing to wait.
	CTimeValue 	*max_wait_time_;
	
	/// Beginning of the start time.
	CTimeValue 	start_time_;
	
	/// Keeps track of whether we've already been stopped.
	int 		stopped_;
};




/**
 * 
 * @version 1.0, 21/05/2004
 * @author	clifford <cliffordmooi@instekdigital.com>
 */
class InstekUtil_Export CTimeKeeper
{
	typedef struct {
		std::string	item_name_;
		CTimeValue	item_tv_;
		int			src_line_;
		std::string	src_file_;
	} timer_item;
	
	struct cleanup_timer : public unary_function<timer_item *, void>
	{
	public:
		void operator() (timer_item *item) const {
			delete item;
			item = NULL;
		}	
	};
	
public:
	CTimeKeeper(void);
	virtual ~CTimeKeeper(void);
	
	void mark(int item_idx, const char *src_file, int src_line);
	void mark(const std::string &item_name, const char *src_file, int src_line);
	void dump(bool verbose = true) const;
	
protected:
	std::vector<timer_item *>	items_;

};
#endif

/**
 * The functions gettimeofday can get the time as well as a timezone.
 * 
 * @return	the time as well as a timezone. (in Time_Value representation)
 */
InstekUtil_Export CTimeValue gettimeofday(void);

#ifdef WIN32
	#define idtext(x)      L ## x
#else
	#define 	idtext(x)	x
#endif

#include <timestamp.inl>

};	//-- namespace instek

#endif // TIMESTAMP_HPP__CECE0FA8_966A_4192_BB99_C265C067D814__
