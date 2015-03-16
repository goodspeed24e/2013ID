#ifndef __UtilConfig_h__
#define __UtilConfig_h__

#ifdef WIN32
	#ifdef SV_CODE_BASE
		#ifdef So_InstekUtil
			#define InstekUtil_Export		__declspec(dllexport)
		#else
			#define InstekUtil_Export		__declspec(dllimport)
		#endif
	#else
		#define InstekUtil_Export
	#endif
#else
	#define InstekUtil_Export
#endif

#endif
