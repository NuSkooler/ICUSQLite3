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

#include "ICUSQLite3Utility.h"
#include "ICUSQLite3.h"

//	:TODO: const these so they don't have to go through ctor's whenever used
#define DATETIME_MILLI_PATTERN UNICODE_STRING_SIMPLE("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'")
#define DATETIME_PATTERN       UNICODE_STRING_SIMPLE("yyyy-MM-dd'T'HH:mm:ss'Z'")
#define DATE_PATTERN           UNICODE_STRING_SIMPLE("yyyy-MM-dd")
#define TIME_MILLI_PATTERN     UNICODE_STRING_SIMPLE("HH:mm:ss.SSS'Z'")
#define TIME_PATTERN           UNICODE_STRING_SIMPLE("HH:mm:ss'Z'")


ICUSQLite3Utility::ICUSQLite3Utility()
	: m_index(DATE_FORMAT_ISO8601_DATETIME_MILLISECONDS)
{
	UErrorCode ec = U_ZERO_ERROR;
	m_dtFormat = new SimpleDateFormat(DATETIME_MILLI_PATTERN, ec);
	if(U_SUCCESS(ec)) {
		m_dtFormat->adoptTimeZone(TimeZone::createTimeZone("UTC"));
	} else {
		delete m_dtFormat;
		m_dtFormat = NULL;
	}
}

ICUSQLite3Utility::~ICUSQLite3Utility()
{
	delete m_dtFormat;
}

void ICUSQLite3Utility::SetFormat(
	const EIcuSqlite3FormatIndex index)
{
	if(index != m_index) {
		m_index = index;
		switch(m_index) {
			case DATE_FORMAT_ISO8601_DATETIME:
				m_dtFormat->applyPattern(DATETIME_PATTERN);
				break;
			case DATE_FORMAT_ISO8601_DATE:
				m_dtFormat->applyPattern(DATE_PATTERN);
				break;
			case DATE_FORMAT_ISO8601_TIME:
				m_dtFormat->applyPattern(TIME_PATTERN);
				break;
			case DATE_FORMAT_JULIAN:
				// :TODO: NOSHIP - Make sure this works
				m_dtFormat->applyPattern("g");
				break;
			case DATE_FORMAT_UNIX:
				// :TODO: NOSHIP - IMPLEMENT ME!
			case DATE_FORMAT_ICU_UTC:
				// :TODO: NOSHIP - IMPLEMENT ME!
				break;
			case DATE_FORMAT_ISO8601_DATETIME_MILLISECONDS:
				m_dtFormat->applyPattern(DATETIME_MILLI_PATTERN);
				break;
			case DATE_FORMAT_ISO8601_TIME_MILLISECONDS:
				m_dtFormat->applyPattern(TIME_MILLI_PATTERN);
				break;
		}
	}
}

bool ICUSQLite3Utility::Parse(
	UDate& result, const UnicodeString& dateTimeStr, const int type)
{
	if(!m_dtFormat) {
		return false;
	}
	UErrorCode ec = U_ZERO_ERROR;
	if(DATE_FORMAT_UNKNOWN == type) {
		//	:TODO: make a inlined version of this so it's debuggable
#define TRY_FORMAT(type)	\
		SetFormat(type);	\
		result = m_dtFormat->parse(dateTimeStr, ec);	\
		if(U_SUCCESS(ec)) {	\
			return true;	\
		}	\
		ec = U_ZERO_ERROR;

		//Try various formats in order of precedence.
		TRY_FORMAT(DATE_FORMAT_ISO8601_DATETIME_MILLISECONDS);
		TRY_FORMAT(DATE_FORMAT_ISO8601_DATETIME);
		TRY_FORMAT(DATE_FORMAT_ISO8601_DATE);
		TRY_FORMAT(DATE_FORMAT_ISO8601_TIME_MILLISECONDS);
		TRY_FORMAT(DATE_FORMAT_ISO8601_TIME);
		TRY_FORMAT(DATE_FORMAT_ICU_UTC);
		TRY_FORMAT(DATE_FORMAT_UNIX);
		TRY_FORMAT(DATE_FORMAT_JULIAN);
	} else {
		switch(type) {
			case DATE_FORMAT_ISO8601_DATETIME_MILLISECONDS:
			case ICUSQLITE_DATETIME_ISO8601:
				//First try in millisecond format.
				SetFormat(DATE_FORMAT_ISO8601_DATETIME_MILLISECONDS);
				result = m_dtFormat->parse(dateTimeStr, ec);
				if(U_SUCCESS(ec)) {
					//That worked!
					return true;
				}
				//That didn't work. Try regular format.
				SetFormat(DATE_FORMAT_ISO8601_DATETIME);
				result = m_dtFormat->parse(dateTimeStr, ec);
				return (U_ZERO_ERROR >= ec);
				
			case DATE_FORMAT_ISO8601_TIME_MILLISECONDS:
			case ICUSQLITE_DATETIME_ISO8601_TIME:
				//First try in millisecond format.
				SetFormat(DATE_FORMAT_ISO8601_TIME_MILLISECONDS);
				result = m_dtFormat->parse(dateTimeStr, ec);
				if(U_SUCCESS(ec)) {
					//That worked!
					return true;
				}
				//That didn't work. Try regular format.
				SetFormat(DATE_FORMAT_ISO8601_TIME);
				result = m_dtFormat->parse(dateTimeStr, ec);
				return (U_ZERO_ERROR >= ec);
				
			case ICUSQLITE_DATETIME_ISO8601_DATE:
			case ICUSQLITE_DATETIME_JULIAN:
			case ICUSQLITE_DATETIME_UNIX:
			case ICUSQLITE_DATETIME_ICU_UTC:
				SetFormat(static_cast<EIcuSqlite3FormatIndex>(type));
				result = m_dtFormat->parse(dateTimeStr, ec);
				return (U_ZERO_ERROR >= ec);
				
			default:
				//FAIL
				return false;
		}
	}
	return false;
}


UnicodeString ICUSQLite3Utility::Format(
	const UDate& dateTime, const EIcuSqlite3DTStorageTypes type)
{
	if(!m_dtFormat) {
		return UNICODE_STRING_SIMPLE("");
	}
	if(ICUSQLITE_DATETIME_ISO8601 == type) {
		//Format as full date/time.  See if we have milliseconds and
		//	format accordingly.
		if(0 == (static_cast<int64_t>(dateTime) % 1000)) {
			SetFormat(DATE_FORMAT_ISO8601_DATETIME);
		} else {
			SetFormat(DATE_FORMAT_ISO8601_DATETIME_MILLISECONDS);
		}
	} else if(ICUSQLITE_DATETIME_ISO8601_TIME == type) {
		//Format as time only. See if we have milliseconds and
		//	format accordingly.
		if(0 == (static_cast<int64_t>(dateTime) % 1000)) {
			SetFormat(DATE_FORMAT_ISO8601_TIME);
		} else {
			SetFormat(DATE_FORMAT_ISO8601_TIME_MILLISECONDS);
		}
	} else {
		SetFormat(static_cast<EIcuSqlite3FormatIndex>(type));
	}
	UnicodeString result;
	return m_dtFormat->format(dateTime, result);
}
