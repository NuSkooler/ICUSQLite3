#ifndef __ICU_SQLITE3_DEF_H__
#define __ICU_SQLITE3_DEF_H__

#ifndef WIN32
	#define __declspec(x)
#endif 

#if defined(ICUSQLITE_MAKINGLIB)
  #define ICUSQLITE_DLLIMPEXP
#elif defined(ICUSQLITE_MAKINGDLL)
  #define ICUSQLITE_DLLIMPEXP	__declspec(dllexport)
#elif defined(ICUSQLITE_USINGDLL)
  #define ICUSQLITE_DLLIMPEXP	__declspec(dllimport) 
#else // not making nor using DLL
  #define ICUSQLITE_DLLIMPEXP
#endif

#endif	//	!__ICU_SQLITE3_DEF_H__
