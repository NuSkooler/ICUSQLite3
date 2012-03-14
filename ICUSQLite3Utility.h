/*
 Copyright (c) 2010 Bryan Ashby

 This software is provided 'as-is', without any express or implied
 warranty. In no event will the authors be held liable for any damages
 arising from the use of this software.

 Permission is granted to anyone to use this software for any purpose,
 including commercial applications, and to alter it and redistribute it
 freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

    3. This notice may not be removed or altered from any source
    distribution.
*/

#include <unicode/smpdtfmt.h>

#ifndef __ICU_SQLITE3_UTILITY_H__
#define __ICU_SQLITE3_UTILITY_H__

enum EIcuSqlite3DTStorageTypes {
	//
	//	from sqlite:
	//	TEXT as ISO8601 strings ("YYYY-MM-DD HH:MM:SS.SSS").
	//
	ICUSQLITE_DATETIME_ISO8601,

	//
	//	ISO8601 date only
	//
	ICUSQLITE_DATETIME_ISO8601_DATE,

	//
	//	ISO8601 time only
	//
	ICUSQLITE_DATETIME_ISO8601_TIME,

	//
	//	From sqlite:
	//	REAL as Julian day numbers, the number of days since noon in 
	//	Greenwich on November 24, 4714 B.C. according to the proleptic 
	//	Gregorian calendar.
	//
	ICUSQLITE_DATETIME_JULIAN,

	//
	//	From sqlite:
	//	INTEGER as Unix Time, the number of seconds since 
	//	1970-01-01 00:00:00 UTC. 
	//
	ICUSQLITE_DATETIME_UNIX,

	//
	//	ICU UTC int64_t
	//	http://userguide.icu-project.org/datetime/universaltimescale
	//	
	ICUSQLITE_DATETIME_ICU_UTC,
};

enum EIcuSqlite3FormatIndex {
	DATE_FORMAT_UNKNOWN				= -1,
	DATE_FORMAT_ISO8601_DATETIME	= ICUSQLITE_DATETIME_ISO8601,
	DATE_FORMAT_ISO8601_DATE		= ICUSQLITE_DATETIME_ISO8601_DATE,
	DATE_FORMAT_ISO8601_TIME		= ICUSQLITE_DATETIME_ISO8601_TIME,
	DATE_FORMAT_JULIAN				= ICUSQLITE_DATETIME_JULIAN,
	DATE_FORMAT_UNIX				= ICUSQLITE_DATETIME_UNIX,
	DATE_FORMAT_ICU_UTC				= ICUSQLITE_DATETIME_ICU_UTC,
	DATE_FORMAT_ISO8601_DATETIME_MILLISECONDS,
	DATE_FORMAT_ISO8601_TIME_MILLISECONDS,
};

enum EIcuSqlite3FormatIndex;

class ICUSQLite3Utility
{
public:
	ICUSQLite3Utility();
	virtual ~ICUSQLite3Utility();

	bool Parse(UDate& result, const UnicodeString& dateTimeStr,
		const int type = DATE_FORMAT_UNKNOWN);

	UnicodeString Format(const UDate& dateTime,
		const EIcuSqlite3DTStorageTypes type = ICUSQLITE_DATETIME_ISO8601);

private:
	void SetFormat(const EIcuSqlite3FormatIndex index);

	EIcuSqlite3FormatIndex	m_index;
	SimpleDateFormat*		m_dtFormat;
};

#endif	//	!__ICU_SQLITE3_UTILITY_H__
