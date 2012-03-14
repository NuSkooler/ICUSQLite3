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

//	:TODO: IMPORTANT: all these .data() should be .c_str() from std::string!? (unless toUTF8String() already guarentees this -- needs validation)

#if defined(__GNUG__) && !defined(__APPLE__)
	#pragma implementation "ICUSQLite3.h"
#endif	//	defined(__GNUG__) && !defined(__APPLE__)

#ifdef __BORLANDC__
	#pragma hdrstop
#endif	//	__BORLANDC__

#if defined(__GNUC__)
	#include <string.h>
	#include <stdio.h>
#endif

#include "ICUSQLite3.h"

#include <assert.h>

//	:TODO: clean this up -- the amag version of sqlite3.c contains ICU stuff
//	SQLite3 and/or SQLite3 + ICU extensions
//#if defined(ICUSQLITE_HAVE_ICU_EXTENSIONS)
//	#include "sqliteicu.h"
//#else	//	defined(ICUSQLITE_HAVE_ICU_EXTENSIONS)
//	#include "sqlite3.h"
//#endif	//	!defined(ICUSQLITE_HAVE_ICU_EXTENSIONS)

#include "sqlite3.h"

//	ICU
#include <unicode/utmscale.h>

#if defined(ICUSQLITE3_ANDROID)
	#define SQLITE_SOFT_HEAP_LIMIT (4 * 1024 * 1024)
#endif	//	defined(ICUSQLITE3_ANDROID)

///////////////////////////////////////////////////////////////////////////////
//	Macros, defines, etc.
///////////////////////////////////////////////////////////////////////////////
//	:TODO: update this and related methods to use the sqlite3_compileoption_*() APIs if available.
#if ICUSQLITE_HAVE_CODEC
	#define ICUSQLITE_ENC_FLAG		ICUSQLITE_SUPPORT_ENCRYPTION
#else	//	ICUSQLITE_HAVE_CODEC
	#define ICUSQLITE_ENC_FLAG		ICUSQLITE_SUPPORT_NONE
#endif	//	!ICUSQLITE_HAVE_CODEC

#if ICUSQLITE_HAVE_METADATA
	#define ICUSQLITE_METADATA_FLAG	ICUSQLITE_SUPPORT_METADATA
#else	//	ICUSQLITE_HAVE_METADATA
	#define	ICUSQLITE_METADATA_FLAG	ICUSQLITE_SUPPORT_NONE
#endif	//	!ICUSQLITE_HAVE_METADATA

#if ICUSQLITE_HAVE_LOAD_EXTENSION
	#define ICUSQLITE_LOAD_EXT_FLAG	ICUSQLITE_SUPPORT_LOAD_EXT
#else	//	ICUSQLITE_HAVE_LOAD_EXTENSION
	#define ICUSQLITE_LOAD_EXT_FLAG	ICUSQLITE_SUPPORT_NONE
#endif	//	!ICUSQLITE_HAVE_LOAD_EXTENSION

#if SQLITE_VERSION_NUMBER >= 3004000
	#define ICUSQLITE_INC_BLOB_FLAG	ICUSQLITE_SUPPORT_INC_BLOB
#else	//	SQLITE_VERSION_NUMBER >= 3004000
	#define ICUSQLITE_INC_BLOB_FLAG	ICUSQLITE_SUPPORT_NONE
#endif	//	SQLITE_VERSION_NUMBER < 3004000

#if SQLITE_VERSION_NUMBER >= 3006008
	#define ICUSQLITE_SAVEPOINT_FLAG	ICUSQLITE_SUPPORT_SAVEPOINT
#else	//	SQLITE_VERSION_NUMBER >= 3006008
	#define ICUSQLITE_SAVEPOINT_FLAG	ICUSQLITE_SUPPORT_NONE
#endif	//	SQLITE_VERSION_NUMBER < 3006008

#if SQLITE_VERSION_NUMBER >= 3006011
	#define ICUSQLITE_BACKUP_FLAG	ICUSQLITE_SUPPORT_BACKUP
#else	//	SQLITE_VERSION_NUMBER >= 3006011
	#define ICUSQLITE_BACKUP_FLAG	ICUSQLITE_SUPPORT_NONE
#endif	//	SQLITE_VERSION_NUMBER < 3006011

#define ICUSQLITE_SUPPORTED_FLAGS	(ICUSQLITE_ENC_FLAG | ICUSQLITE_LOAD_EXT_FLAG | ICUSQLITE_INC_BLOB_FLAG | ICUSQLITE_SAVEPOINT_FLAG | ICUSQLITE_BACKUP_FLAG)

///////////////////////////////////////////////////////////////////////////////
//	Utility functions
///////////////////////////////////////////////////////////////////////////////
static double IcuSqlite3Atof(
	const char* psz)
{
	int sign = 1;
	long double v1 = 0.0;
	int sig = 0;
	
	while(isspace(*(unsigned char*)psz)) {
		++psz;
	}
	
	if('-' == *psz) {
		sign = -1;
		++psz;
	} else if('+' == *psz) {
		++psz;
	}

	while('0' == *psz) { ++psz; }

	while(isdigit(*(unsigned char*)psz)) {
		v1 = v1 * 10.0 + (*psz - '0');
		++psz;
		++sig;
	}

	if('.' == *psz) {
		long double div = 1.0;
		++psz;
		if(0 == sig) {
			while('0' == *psz) {
				div *= 10.0;
				++psz;
			}
		}

		while(isdigit(*(unsigned char*)psz)) {
			if(sig < 18) {
				v1 = v1 * 10.0 + (*psz - '0');
				div *= 10.0;
				++sig;
			}
			++psz;
		}
		v1 /= div;
	}

	if('e' == *psz || 'E' == *psz) {
		int esign = 1;
		int eval = 0;
		long double scale = 1.0;
		++psz;

		if('-' == *psz) {
			esign = -1;
			++psz;
		} else if('+' == *psz) {
			++psz;
		}

		while(isdigit(*(unsigned char*)psz)) {
			eval = eval * 10 + *psz - '0';
			++psz;
		}

		while(eval > 64)	{ scale *= 1.0e+64; eval -= 64; }
		while(eval >= 16)	{ scale *= 1.0e+16; eval -= 16; }
		while(eval >=  4)	{ scale *= 1.0e+4;  eval -= 4; }
		while(eval >=  1)	{ scale *= 1.0e+1;  eval -= 1; }

		if(esign < 0) {
			v1 /= scale;
		} else {
			v1 *= scale;
		}
	}

	return (double)((sign < 0) ? -v1 : v1);
}

///////////////////////////////////////////////////////////////////////////////
//	IcuSqlite3StatementBuffer
///////////////////////////////////////////////////////////////////////////////
IcuSqlite3StatementBuffer::IcuSqlite3StatementBuffer()
	: m_buffer(NULL)
{
}

IcuSqlite3StatementBuffer::~IcuSqlite3StatementBuffer()
{
	Clear();
}

void IcuSqlite3StatementBuffer::Clear()
{
	if(NULL != m_buffer) {
		sqlite3_free(m_buffer);
		m_buffer = NULL;
	}
}

const char* IcuSqlite3StatementBuffer::Format(
	const char* format, ...)
{
	Clear();
	va_list va;
	va_start(va, format);
	m_buffer = sqlite3_vmprintf(format, va);
	va_end(va);
	return m_buffer;
}

///////////////////////////////////////////////////////////////////////////////
//	IcuSqlite3ResultSet - public
///////////////////////////////////////////////////////////////////////////////
IcuSqlite3FunctionContext::IcuSqlite3FunctionContext(
	void* ctxt, int argCount, void** args)
	: m_ctxt(ctxt)
	, m_argCount(argCount)
	, m_args(args)
{
}

int IcuSqlite3FunctionContext::GetArgCount() const
{
	return m_argCount;
}

UnicodeString IcuSqlite3FunctionContext::GetArgAsUnicodeString(
	unsigned int n)
{
	return UnicodeString(reinterpret_cast<const UChar*>(
		sqlite3_value_text16((sqlite3_value*)m_args[n])));
}

int64_t IcuSqlite3FunctionContext::GetArgAsInt64(
	unsigned int n)
{
	return sqlite3_value_int64((sqlite3_value*)m_args[n]);
}

double IcuSqlite3FunctionContext::GetArgAsDouble(
	unsigned int n)
{
	return sqlite3_value_double((sqlite3_value*)m_args[n]);
}

void IcuSqlite3FunctionContext::SetResult(
	const UnicodeString& str)
{
#ifdef WORDS_BIGENDIAN
	sqlite3_result_text16be((sqlite3_context*)m_ctxt, str.getBuffer(), 
		str.length() * sizeof(UChar), SQLITE_TRANSIENT);
#else
	sqlite3_result_text16le((sqlite3_context*)m_ctxt, str.getBuffer(), 
		str.length()*sizeof(UChar), SQLITE_TRANSIENT);
#endif
}

void IcuSqlite3FunctionContext::SetResult(
	const char* str, int length)
{
	sqlite3_result_text((sqlite3_context*)m_ctxt, str, length, 
		SQLITE_TRANSIENT);
}

void IcuSqlite3FunctionContext::SetResult(
	int64_t i)
{
	sqlite3_result_int64((sqlite3_context*)m_ctxt, i);
}

void IcuSqlite3FunctionContext::SetResult(
	double d)
{
	sqlite3_result_double((sqlite3_context*)m_ctxt, d);
}

void IcuSqlite3FunctionContext::SetNullResult()
{
	sqlite3_result_null((sqlite3_context*)m_ctxt);
}


///////////////////////////////////////////////////////////////////////////////
//	IcuSqlite3ResultSet - public
///////////////////////////////////////////////////////////////////////////////
IcuSqlite3ResultSet::IcuSqlite3ResultSet()
	: m_stmt(NULL)
	, m_util(NULL)
	, m_eof(true)
	, m_first(true)
	, m_cols(0)
	, m_ownStmt(false)
{
}

IcuSqlite3ResultSet::IcuSqlite3ResultSet(
	const IcuSqlite3ResultSet& resultSet)
	: m_util(NULL)
{
	m_stmt		= resultSet.m_stmt;
	
	//
	//	only one IcuSqlite3ResultSet can own the statement at once
	//
	const_cast<IcuSqlite3ResultSet&>(resultSet).m_stmt = NULL;
	
	m_eof		= resultSet.m_eof;
	m_first		= resultSet.m_first;
	m_cols		= resultSet.m_cols;
	m_ownStmt	= resultSet.m_ownStmt;
}

IcuSqlite3ResultSet::IcuSqlite3ResultSet(
	void* db, void* stmt, bool eof, bool first, bool ownStmt /*= true*/)
	: m_util(NULL)
{
	m_db		= db;
	m_stmt		= stmt;
	m_eof		= eof;
	m_first		= first;
	m_cols		= sqlite3_column_count((sqlite3_stmt*)m_stmt);
	m_ownStmt	= ownStmt;
}

/*virtual*/
IcuSqlite3ResultSet::~IcuSqlite3ResultSet()
{
	Finalize();
	if(m_util) {
		delete m_util;
	}
}

IcuSqlite3ResultSet& IcuSqlite3ResultSet::operator=(
	const IcuSqlite3ResultSet& resultSet)
{
	if(&resultSet != this) {
		Finalize();

		m_stmt		= resultSet.m_stmt;
		
		//
		//	only one IcuSqlite3ResultSet can own the statement at once
		//
		const_cast<IcuSqlite3ResultSet&>(resultSet).m_stmt = NULL;
		
		m_eof		= resultSet.m_eof;
		m_first		= resultSet.m_first;
		m_cols		= resultSet.m_cols;
		m_ownStmt	= resultSet.m_ownStmt;
	}
	
	return *this;	
}

int IcuSqlite3ResultSet::FindColumnIndex(
	const UnicodeString& colName)
{
	if(NULL == m_stmt || colName.isEmpty()) {
		return ICUSQLITE_COLUMN_IDX_INVALID;
	}
	
	for(int i = 0; i < m_cols; ++i) {
		const UChar* cn = static_cast<const UChar*>(
			sqlite3_column_name16((sqlite3_stmt*)m_stmt, i));
		
		if(0 == colName.compare(cn, -1)) {
			return i;
		}
	}
	
	return ICUSQLITE_COLUMN_IDX_INVALID;
}

UnicodeString IcuSqlite3ResultSet::GetColumnName(
	const int colIdx)
{
	if(NULL == m_stmt || colIdx < 0 || colIdx > m_cols - 1) {
		return "";
	}
	
	const UChar* cn = 
		reinterpret_cast<const UChar*>(sqlite3_column_name16(
			(sqlite3_stmt*)m_stmt, colIdx));
	return UnicodeString(cn);
}

UnicodeString IcuSqlite3ResultSet::GetDeclaredColumnType(
	const int colIdx)
{
	if(NULL == m_stmt || colIdx < 0 || colIdx > m_cols - 1) {
		return "";
	}
	
	const UChar* dt = 
		reinterpret_cast<const UChar*>(sqlite3_column_decltype16(
			(sqlite3_stmt*)m_stmt, colIdx));
	return UnicodeString(dt);
}

EIcuSqlite3ColumnTypes IcuSqlite3ResultSet::GetColumnType(
	const int colIdx)
{
	if(NULL == m_stmt || colIdx < 0 || colIdx > m_cols - 1) {
		return ICUSQLITE_COLUMN_TYPE_INVALID;
	}
	
	return static_cast<EIcuSqlite3ColumnTypes>(
		sqlite3_column_type((sqlite3_stmt*)m_stmt, colIdx));
}

UnicodeString IcuSqlite3ResultSet::GetDatabaseName(
	const int colIdx)
{
	UnicodeString name;

#if ICUSQLITE_HAVE_METADATA
	if(NULL != m_stmt && colIdx >= 0 && colIdx <= m_cols - 1) {
		const UChar* dbName = 
			reinterpret_cast<const UChar*>(sqlite3_column_database_name16(
				(sqlite3_stmt*)m_stmt, colIdx));
		if(dbName) {
			name = UnicodeString(dbName);
		}
	}
#endif	//	!ICUSQLITE_HAVE_METADATA

	return name;
}

UnicodeString IcuSqlite3ResultSet::GetTableName(
	const int colIdx)
{
	UnicodeString name;

#if ICUSQLITE_HAVE_METADATA
	if(NULL != m_stmt && colIdx >= 0 && colIdx <= m_cols - 1) {
		const UChar* tblName = 
			reinterpret_cast<const UChar*>(sqlite3_column_table_name16(
				(sqlite3_stmt*)m_stmt, colIdx));
		if(tblName) {
			name = UnicodeString(tblName);
		}
	}
#endif	//	!ICUSQLITE_HAVE_METADATA

	return name;
}

UnicodeString IcuSqlite3ResultSet::GetOriginName(
	const int colIdx)
{
	UnicodeString name;

#if ICUSQLITE_HAVE_METADATA
	if(NULL != m_stmt && colIdx >= 0 && colIdx <= m_cols - 1) {
		const UChar* originName = 
			reinterpret_cast<const UChar*>(sqlite3_column_origin_name16(
				(sqlite3_stmt*)m_stmt, colIdx));
		if(originName) {
			name = UnicodeString(originName);
		}
	}
#endif	//	!ICUSQLITE_HAVE_METADATA

	return name;
}

int32_t IcuSqlite3ResultSet::GetInt(
	const int colIdx, const int32_t defVal /*= 0*/)
{
	if(NULL == m_stmt || colIdx < 0 || colIdx > m_cols - 1) {
		return defVal;
	}
	return sqlite3_column_int((sqlite3_stmt*)m_stmt, colIdx);
}

int32_t IcuSqlite3ResultSet::GetInt(
	const UnicodeString& colName, const int32_t defVal /*= 0*/)
{
	return GetInt(FindColumnIndex(colName), defVal);
}

int64_t IcuSqlite3ResultSet::GetInt64(
	const int colIdx, const int64_t defVal /*= 0*/)
{
	if(NULL == m_stmt || colIdx < 0 || colIdx > m_cols - 1) {
		return defVal;
	}
	return sqlite3_column_int64((sqlite3_stmt*)m_stmt, colIdx);
}

int64_t IcuSqlite3ResultSet::GetInt64(
	const UnicodeString& colName, const int64_t defVal /*= 0*/)
{
	return GetInt64(FindColumnIndex(colName), defVal);
}

double IcuSqlite3ResultSet::GetDouble(
	const int colIdx, const double defVal /*= 0.0*/)
{
	if(NULL == m_stmt || colIdx < 0 || colIdx > m_cols - 1) {
		return defVal;
	}
	return sqlite3_column_double((sqlite3_stmt*)m_stmt, colIdx);
}

double IcuSqlite3ResultSet::GetDouble(
	const UnicodeString& colName, const double defVal /*= 0.0*/)
{
	return GetDouble(FindColumnIndex(colName), defVal);
}

UnicodeString IcuSqlite3ResultSet::GetString(
	const int colIdx, 
	const UnicodeString& defVal /*= ""*/)
{
	if(NULL == m_stmt || colIdx < 0 || colIdx > m_cols - 1) {
		return defVal;
	}
	return reinterpret_cast<const UChar*>(
		sqlite3_column_text16((sqlite3_stmt*)m_stmt, colIdx));
}

UnicodeString IcuSqlite3ResultSet::GetString(
	const UnicodeString& colName,
	const UnicodeString& defVal /*= ""*/)
{
	return GetString(FindColumnIndex(colName), defVal);
}

std::string IcuSqlite3ResultSet::GetStringUTF8(
	const int colIdx, 
	const std::string& defVal /*= ""*/)
{
	if(NULL == m_stmt || colIdx < 0 || colIdx > m_cols - 1) {
		return defVal;
	}
	const unsigned char* utf8 = sqlite3_column_text((sqlite3_stmt*)m_stmt, colIdx);
	return (NULL == utf8) ? "" : std::string((const char*)utf8);
}

std::string IcuSqlite3ResultSet::GetStringUTF8(
	const UnicodeString& colName,
	const std::string& defVal /*= ""*/)
{
	return GetStringUTF8(FindColumnIndex(colName), defVal);
}

UDate IcuSqlite3ResultSet::GetDateTime(
	const int colIdx, const UDate defVal /*= 0.0*/)
{
	//	:TODO: fill in the beef!
	//
	//	See http://www.sqlite.org/datatype3.html
	//
	switch(GetColumnType(colIdx)) {
		case ICUSQLITE_COLUMN_TYPE_INTEGER :
			break;

		case ICUSQLITE_COLUMN_TYPE_TEXT :
			{
				if(!m_util) {
					m_util = new ICUSQLite3Utility();
				}
				UDate result = defVal;
				if(m_util->Parse(result, GetString(colIdx), DATE_FORMAT_UNKNOWN)) {
					return result;
				}
			}
			break;

		case ICUSQLITE_COLUMN_TYPE_FLOAT :
			break;
	}
	
	return defVal;
}

UDate IcuSqlite3ResultSet::GetDateTime(
	const UnicodeString& colName, const UDate defVal /*= 0.0*/)
{
	return GetDateTime(FindColumnIndex(colName), defVal);
}

bool IcuSqlite3ResultSet::GetBool(
	const int colIdx, const bool defVal /*= false*/)
{
	return 0 != GetInt(colIdx, (defVal) ? 1 : 0);
}

bool IcuSqlite3ResultSet::GetBool(
	const UnicodeString& colName, const bool defVal /*= false*/)
{
	return GetBool(FindColumnIndex(colName), defVal);
}

const unsigned char* IcuSqlite3ResultSet::GetBlob(
	const int colIdx, int& len)
{
	if(NULL == m_stmt || colIdx < 0 || colIdx > m_cols - 1) {
		len = 0;
		return NULL;
	}

	len = sqlite3_column_bytes((sqlite3_stmt*)m_stmt, colIdx);
	return reinterpret_cast<const unsigned char*>(
		sqlite3_column_blob((sqlite3_stmt*)m_stmt, colIdx));
}

const unsigned char* IcuSqlite3ResultSet::GetBlob(
	const UnicodeString& colName, int& len)
{
	return GetBlob(FindColumnIndex(colName), len);
}

bool IcuSqlite3ResultSet::IsNull(
	const int colIdx)
{
	return (ICUSQLITE_COLUMN_TYPE_NULL == GetColumnType(colIdx));
}

bool IcuSqlite3ResultSet::IsNull(
	const UnicodeString& colName)
{
	return IsNull(FindColumnIndex(colName));
}

bool IcuSqlite3ResultSet::NextRow()
{
	if(NULL == m_stmt) {
		return false;
	}
	
	int rc;
	if(m_first) {
		m_first = false;
		rc = (m_eof) ? SQLITE_DONE : SQLITE_ROW;
	} else {
		rc = sqlite3_step((sqlite3_stmt*)m_stmt);
	}
	
	switch(rc) {
		case SQLITE_ROW : 
			return true;
		
		case SQLITE_DONE :
			m_eof = true;
			return false;
	}
	
	//	:TODO: should this just call Finalize()? what if !m_ownStmt ?
	sqlite3_finalize((sqlite3_stmt*)m_stmt);
	m_stmt = NULL;
	return false;
}

void IcuSqlite3ResultSet::Finalize()
{
	if(m_stmt && m_ownStmt) {
		sqlite3_finalize((sqlite3_stmt*)m_stmt);
		m_stmt = NULL;
	}
}

const char* IcuSqlite3ResultSet::GetRawSQL()
{
	if(NULL == m_stmt) {
		return NULL;
	}
	return sqlite3_sql((sqlite3_stmt*)m_stmt);
}

UnicodeString IcuSqlite3ResultSet::GetSQL()
{
	UnicodeString sql;
	const char* sqlUtf8 = GetRawSQL();
	if(sqlUtf8) {
		sql = UnicodeString::fromUTF8(sqlUtf8);
	}
	return sql;
}

//	:TODO: missing all the GetXXXX()!

///////////////////////////////////////////////////////////////////////////////
//	IcuSqlite3Table
///////////////////////////////////////////////////////////////////////////////

IcuSqlite3Table::IcuSqlite3Table()
	: m_results(NULL)
	, m_util(NULL)
	, m_rows(0)
	, m_cols(0)
	, m_currentRow(0)
{
}

IcuSqlite3Table::IcuSqlite3Table(
	const IcuSqlite3Table& table)
	: m_util(NULL)
{
	m_results		= table.m_results;
	
	//
	//	Only one IcuSqlite3Table can own the results
	//
	const_cast<IcuSqlite3Table&>(table).m_results = NULL;
	m_rows			= table.m_rows;
	m_cols			= table.m_cols;
	m_currentRow	= table.m_currentRow;
}

IcuSqlite3Table::IcuSqlite3Table(
	char** results, int rows, int cols)
	: m_util(NULL)
{
	m_results		= results;
	m_rows			= rows;
	m_cols			= cols;
	m_currentRow	= 0;
}

/*virtual*/
IcuSqlite3Table::~IcuSqlite3Table()
{
	Finalize();
	if(m_util) {
		delete m_util;
	}
}

IcuSqlite3Table& IcuSqlite3Table::operator=(
	const IcuSqlite3Table& table)
{
	if(&table != this) {
		Finalize();
		
		m_results		= table.m_results;
	
		//
		//	Only one IcuSqlite3Table can own the results
		//
		const_cast<IcuSqlite3Table&>(table).m_results = NULL;
		m_rows			= table.m_rows;
		m_cols			= table.m_cols;
		m_currentRow	= table.m_currentRow;		
	}
	
	return *this;
}

int IcuSqlite3Table::FindColumnIndex(
	const char* utf8ColName)
{
	if(NULL == m_results || NULL == utf8ColName ||
		'\0' == utf8ColName[0])
	{
		return ICUSQLITE_COLUMN_IDX_INVALID;
	}
	
	for(int i = 0; i < m_cols; ++i) {
		if(0 == strcmp(utf8ColName, m_results[i])) {
			return i;
		}
	}
	
	return ICUSQLITE_COLUMN_IDX_INVALID;
}

int IcuSqlite3Table::FindColumnIndex(
	const UnicodeString& colName)
{	
	//
	//	m_results always contains UTF-8 char*'s so we'll need to convert
	//	colName -> UTF-8 for lookup
	//
	std::string utf8ColName;
	return FindColumnIndex(colName.toUTF8String(utf8ColName).data());
}

UnicodeString IcuSqlite3Table::GetColumnName(
	const int colIdx)
{
	if(NULL == m_results || colIdx < 0 || colIdx > m_cols - 1) {
		return "";
	}
	const char* name = m_results[colIdx];
	return UnicodeString::fromUTF8(name);
}

/*
UnicodeString IcuSqlite3Table::GetAsString(
	const int colIdx)
{
	const char* val = GetValue(colIdx);
	return (NULL == val) ? UNICODE_STRING_SIMPLE :
		UnicodeString::fromUTF8(val);
}

UnicodeString IcuSqlite3Table::GetAsString(
	const UnicodeString& colName)
{
	return GetAsString(FindColumnIndex(colName));
}
*/

//	:TODO: we're already using STL, just use a to_string<> instead of scanf() on these:
int IcuSqlite3Table::GetInt(
	const int colIdx, const int defVal /*= 0*/)
{
	int result = defVal;
	const char* val = GetValue(colIdx);
	if(NULL != val) {	//	:TODO: does sscanf() do a NULL check?
		sscanf(val, "%d", &result);
	}
	return result;
}

int IcuSqlite3Table::GetInt(
	const UnicodeString& colName, const int defVal /*= 0*/)
{
	return GetInt(FindColumnIndex(colName), defVal);
}

int64_t IcuSqlite3Table::GetInt64(
	const int colIdx, const int64_t defVal /*= 0*/)
{
	int64_t result = defVal;
	const char* val = GetValue(colIdx);
	if(NULL != val) {	//	:TODO: sscanf() does this?
		sscanf(val, "%lld", &result);
	}
	return result;
}

int64_t IcuSqlite3Table::GetInt64(
	const UnicodeString& colName, const int64_t defVal /*= 0*/)
{
	return GetInt64(FindColumnIndex(colName), defVal);
}

double IcuSqlite3Table::GetDouble(
	const int colIdx, const double defVal /*= 0.0*/)
{
	double result = defVal;
	const char* val = GetValue(colIdx);
	if(NULL != val) {
		result = IcuSqlite3Atof(val);
	}
	return result;
}

double IcuSqlite3Table::GetDouble(
	const UnicodeString& colName, const double defVal /*= 0.0*/)
{
	return GetDouble(FindColumnIndex(colName), defVal);
}

UnicodeString IcuSqlite3Table::GetString(
	const int colIdx, const UnicodeString& defVal /*= ""*/)
{
	const char* val = GetValue(colIdx);
	return (NULL != val) ? UnicodeString::fromUTF8(val) : defVal;
}

UnicodeString IcuSqlite3Table::GetString(
	const UnicodeString& colName, 
	const UnicodeString& defVal /*= ""*/)
{
	return GetString(FindColumnIndex(colName), defVal);
}

std::string IcuSqlite3Table::GetStringUTF8(
	const int colIdx, const std::string& defVal /*= ""*/)
{
	const char* val = GetValue(colIdx);
	return (NULL != val) ? val : defVal;
}

std::string IcuSqlite3Table::GetStringUTF8(
	const UnicodeString& colName, 
	const std::string& defVal /*= ""*/)
{
	return GetStringUTF8(FindColumnIndex(colName), defVal);
}

int64_t IcuSqlite3Table::GetDateTime64(
	const int colIdx, const int64_t& defVal /*= 0*/,
	const EIcuSqlite3DTStorageTypes storedAs /*= ICUSQLITE_DATETIME_ISO8601*/)
{
	int64_t result = defVal;
	const char* val = GetValue(colIdx);
	if(NULL != val) {
		//
		//	Since we're using the table API, we don't know what the column type
		//	is here, so we'll have to guess. SQLite gives back dates in the 
		//	following formats (from http://www.sqlite.org/datatype3.html):
		//	
		//	* TEXT as ISO8601 strings ("YYYY-MM-DD HH:MM:SS.SSS").
		//	* REAL as Julian day numbers, the number of days since noon in 
		//	Greenwich on November 24, 4714 B.C. according to the proleptic 
		//	Gregorian calendar.
		//	* INTEGER as Unix Time, the number of seconds since 
		//	1970-01-01 00:00:00 UTC. 
		//
		if(NULL != strchr(val, '-')) {
			//
			//	TEXT - handler is ignored here
			//
			if(!m_util) {
				m_util = new ICUSQLite3Utility();
			}
			UDate d = 0;
			if(m_util->Parse(d, UnicodeString::fromUTF8(val))) {
				UErrorCode ec = U_ZERO_ERROR;
				int64_t r64 = utmscale_fromInt64(static_cast<int64_t>(d), UDTS_ICU4C_TIME, &ec);
				if(U_SUCCESS(ec)) {
					result = r64;
				}
			}
		} else if (NULL != strchr(val, '.')) {
			//
			//	REAL - handler is ignored here
			//			
			result = static_cast<int64_t>(IcuSqlite3Atof(val));
		} else {
			switch(storedAs) {
				case ICUSQLITE_DATETIME_UNIX :
					//	:TODO: unix ts -> UDate
					break;
				
				case ICUSQLITE_DATETIME_ICU_UTC :
					int64_t utc = 0;
					if(1 == sscanf(val, "%lld", &utc)) {
						//	:TODO: int64_t UTC -> UDate
					}
					break;
			}
		}
	}
	return result;
}

int64_t IcuSqlite3Table::GetDateTime64(
	const UnicodeString& colName, const int64_t& defVal /*= 0*/,
	const EIcuSqlite3DTStorageTypes storedAs /*= ICUSQLITE_DATETIME_ISO8601*/)
{
	return GetDateTime64(FindColumnIndex(colName), defVal, storedAs);
}

UDate IcuSqlite3Table::GetDateTimeUDate(
	const int colIdx, const UDate& defVal /*= 0.0*/, 
	const EIcuSqlite3DTStorageTypes storedAs /*= ICUSQLITE_DATETIME_ISO8601*/)
{
	int64_t v64 = static_cast<int64_t>(defVal);
	UErrorCode ec = U_ZERO_ERROR;
	v64 = utmscale_toInt64(v64, UDTS_ICU4C_TIME, &ec);
	if(U_FAILURE(ec)) {
		return defVal;
	}

	v64 = GetDateTime64(colIdx, v64, storedAs);

	UDate result = static_cast<UDate>(utmscale_toInt64(v64, UDTS_ICU4C_TIME, &ec));
	if(U_FAILURE(ec)) {
		return defVal;
	}
	return result;
	

	//UDate result = defVal;
	//const char* val = GetValue(colIdx);
	//if(NULL != val) {
	//	//
	//	//	Since we're using the table API, we don't know what the column type
	//	//	is here, so we'll have to guess. SQLite gives back dates in the 
	//	//	following formats (from http://www.sqlite.org/datatype3.html):
	//	//	
	//	//	* TEXT as ISO8601 strings ("YYYY-MM-DD HH:MM:SS.SSS").
	//	//	* REAL as Julian day numbers, the number of days since noon in 
	//	//	Greenwich on November 24, 4714 B.C. according to the proleptic 
	//	//	Gregorian calendar.
	//	//	* INTEGER as Unix Time, the number of seconds since 
	//	//	1970-01-01 00:00:00 UTC. 
	//	//
	//	if(NULL != strchr(val, '-')) {
	//		//
	//		//	TEXT - handler is ignored here
	//		//
	//		UErrorCode ec = U_ZERO_ERROR;
	//		SimpleDateFormat sdf(UNICODE_STRING_SIMPLE("yyyy-MM-dd HH:mm:ss"), ec);

	//		TimeZone* tz = TimeZone::createTimeZone("UTC");
	//		//	:TODO: does tz need freed? how is this handled?!
	//		sdf.setTimeZone(*tz);
	//		
	//		if(U_SUCCESS(ec)) {
	//			sdf.setLenient(TRUE);
	//			UDate d = sdf.parse(UnicodeString::fromUTF8(val), ec);
	//			if(U_SUCCESS(ec)) {
	//				result = d;
	//			}
	//		}
	//	} else if (NULL != strchr(val, '.')) {
	//		//
	//		//	REAL - handler is ignored here
	//		//			
	//		result = IcuSqlite3Atof(val);
	//	} else {
	//		switch(handler) {
	//			case ICUSQLITE_DATETIME_UNIX :
	//				//	:TODO: unix ts -> UDate
	//				break;
	//			
	//			case ICUSQLITE_DATETIME_ICU_UTC :
	//				int64_t utc = 0;
	//				if(1 == sscanf(val, "%lld", &utc)) {
	//					//	:TODO: int64_t UTC -> UDate
	//				}
	//				break;
	//		}
	//	}
	//}
	//return result;
}

UDate IcuSqlite3Table::GetDateTimeUDate(
	const UnicodeString& colName, const UDate& defValue /*= 0.0*/, 
	const EIcuSqlite3DTStorageTypes storedAs /*= ICUSQLITE_DATETIME_ISO8601*/)
{
	return GetDateTimeUDate(FindColumnIndex(colName), defValue, storedAs);
}

bool IcuSqlite3Table::GetBool(
	const int colIdx)
{
	return 0 != GetInt(colIdx, 0);
}

bool IcuSqlite3Table::GetBool(
	const UnicodeString& colName)
{
	return 0 != GetInt(FindColumnIndex(colName), 0);
}

bool IcuSqlite3Table::IsNull(
	const int colIdx)
{
	const char* val = GetValue(colIdx);
	return (NULL == val);
}

bool IcuSqlite3Table::IsNull(
	const UnicodeString& colName)
{
	return IsNull(FindColumnIndex(colName));
}

void IcuSqlite3Table::Finalize()
{
	if(NULL != m_results) {
		sqlite3_free_table(m_results);
		m_results = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
//	IcuSqlite3Statement
///////////////////////////////////////////////////////////////////////////////
IcuSqlite3Statement::IcuSqlite3Statement()
	: m_db(NULL)
	, m_stmt(NULL)
	, m_util(NULL)
{
}

IcuSqlite3Statement::IcuSqlite3Statement(
	const IcuSqlite3Statement& stmt)
	: m_util(NULL)
{
	m_db	= stmt.m_db;
	m_stmt	= stmt.m_stmt;

	//	only one obj can own a statement
	const_cast<IcuSqlite3Statement&>(stmt).m_stmt = NULL;
}

IcuSqlite3Statement::IcuSqlite3Statement(
	void* db, void* stmt)
	: m_db(db)
	, m_stmt(stmt)
	, m_util(NULL)
{
}

/*virtual*/
IcuSqlite3Statement::~IcuSqlite3Statement()
{
	Finalize();
	if(m_util) {
		delete m_util;
	}
}

IcuSqlite3Statement& IcuSqlite3Statement::operator=(
	const IcuSqlite3Statement& stmt)
{
	if(&stmt != this) {
		Finalize();

		m_db	= stmt.m_db;
		m_stmt	= stmt.m_stmt;
		
		//	only one obj can own a statement
		const_cast<IcuSqlite3Statement&>(stmt).m_stmt = NULL;
	}

	return *this;
}

int IcuSqlite3Statement::ExecuteUpdate()
{
	if(NULL == m_db || NULL == m_stmt) {
		return -1;
	}

	int rowsChanged = -1;

	if(SQLITE_DONE == sqlite3_step((sqlite3_stmt*)m_stmt)) {
		rowsChanged = sqlite3_changes((sqlite3*)m_db);
	}
	
	sqlite3_reset((sqlite3_stmt*)m_stmt);

	return rowsChanged;
}

IcuSqlite3ResultSet IcuSqlite3Statement::ExecuteQuery()
{
	if(NULL == m_db || NULL == m_stmt) {
		return IcuSqlite3ResultSet(NULL, NULL, true);
	}

	switch(sqlite3_step((sqlite3_stmt*)m_stmt)) {
		case SQLITE_DONE : 
			return IcuSqlite3ResultSet(m_db, m_stmt, true, true, false);
		
		case SQLITE_ROW :
			return IcuSqlite3ResultSet(m_db, m_stmt, false, true, false);
	}

	//	something went wrong
	sqlite3_reset((sqlite3_stmt*)m_stmt);
	return IcuSqlite3ResultSet(NULL, NULL, true);
}

int IcuSqlite3Statement::GetParamCount()
{
	return (NULL == m_stmt) ? 0 : 
		sqlite3_bind_parameter_count((sqlite3_stmt*)m_stmt);
}

int IcuSqlite3Statement::GetParamIndex(
	const UnicodeString& paramName)
{
	if(NULL == m_stmt) {
		return 0;
	}
	
	std::string utf8ParamName;
	return sqlite3_bind_parameter_index((sqlite3_stmt*)m_stmt, 
		paramName.toUTF8String(utf8ParamName).data());
}

UnicodeString IcuSqlite3Statement::GetParamName(
	const int paramIdx)
{
	return (NULL == m_stmt) ? "" : 
		UnicodeString::fromUTF8(
			sqlite3_bind_parameter_name((sqlite3_stmt*)m_stmt, paramIdx));
}

bool IcuSqlite3Statement::Bind(
	const int paramIdx, const UnicodeString& paramValue)
{
	return (NULL != m_stmt && 
		SQLITE_OK == sqlite3_bind_text16((sqlite3_stmt*)m_stmt, paramIdx, 
			(const void*)paramValue.getBuffer(), 
			paramValue.length() * sizeof(UChar), SQLITE_TRANSIENT));
}

bool IcuSqlite3Statement::Bind(
	const int paramIdx, const int paramValue)
{
	return (
		NULL != m_stmt &&
		SQLITE_OK == sqlite3_bind_int((sqlite3_stmt*)m_stmt, paramIdx, 
			paramValue));
}

bool IcuSqlite3Statement::Bind(
	const int paramIdx, const int64_t& paramValue)
{
	return (
		NULL != m_stmt &&
		SQLITE_OK == sqlite3_bind_int64((sqlite3_stmt*)m_stmt, paramIdx, 
			paramValue));
}

bool IcuSqlite3Statement::Bind(
	const int paramIdx, const double& paramValue)
{
	return (
		NULL != m_stmt &&
		SQLITE_OK == sqlite3_bind_double((sqlite3_stmt*)m_stmt, paramIdx, 
			paramValue));
}

bool IcuSqlite3Statement::Bind(
	const int paramIdx, const char* paramValue)
{
	return (
		NULL != m_stmt &&
		SQLITE_OK == sqlite3_bind_text((sqlite3_stmt*)m_stmt, paramIdx,
			paramValue, -1, SQLITE_TRANSIENT));
}

bool IcuSqlite3Statement::Bind(
	const int paramIdx, const std::string& paramValue)
{
	return Bind(paramIdx, paramValue.c_str());
}

bool IcuSqlite3Statement::Bind(
	const int paramIdx, const unsigned char* blobValue, const int blobLen)
{
	return (
		NULL != m_stmt &&
		SQLITE_OK == sqlite3_bind_blob((sqlite3_stmt*)m_stmt, paramIdx, 
			(const void*)blobValue, blobLen, SQLITE_TRANSIENT));
}

bool IcuSqlite3Statement::BindDateTime64(
	const int paramIdx, const int64_t& paramValue,
	const EIcuSqlite3DTStorageTypes storeAs /*= ICUSQLITE_DATETIME_ISO8601*/)
{
	//	:TODO: int64_t -> UDate -> BindDateTimeUDate()
	return false;
}

bool IcuSqlite3Statement::BindDateTimeUDate(
	const int paramIdx, const UDate& paramValue,
	const EIcuSqlite3DTStorageTypes storeAs /*= ICUSQLITE_DATETIME_ISO8601*/)
{
	switch(storeAs) {
		case ICUSQLITE_DATETIME_ISO8601 :
		case ICUSQLITE_DATETIME_ISO8601_DATE :
		case ICUSQLITE_DATETIME_ISO8601_TIME :
			{
				UnicodeString dateTime;
				if(!m_util) {
					m_util = new ICUSQLite3Utility();
				}
				return Bind(paramIdx, m_util->Format(paramValue, storeAs));
			}
			break;

		case ICUSQLITE_DATETIME_JULIAN :
			{
				//	:TODO: .... hrm.
				
			}
			break;

		case ICUSQLITE_DATETIME_UNIX :
			{
				UErrorCode ec = U_ZERO_ERROR;
				int64_t dt64 = utmscale_toInt64(
					static_cast<int64_t>(paramValue), UDTS_UNIX_TIME, &ec);
				if(U_SUCCESS(ec)) {
					return Bind(paramIdx, dt64);
				}
			}
			break;

		case ICUSQLITE_DATETIME_ICU_UTC :
			{				
				UErrorCode ec = U_ZERO_ERROR;
				int64_t dt64 = utmscale_toInt64(
					static_cast<int64_t>(paramValue), UDTS_ICU4C_TIME, &ec);
				if(U_SUCCESS(ec)) {
					return Bind(paramIdx, dt64);
				}
			}
			break;
			
	}

	/*int64_t v64 = static_cast<int64_t>(defVal);
	UErrorCode ec = U_ZERO_ERROR;
	v64 = utmscale_toInt64(v64, UDTS_ICU4C_TIME, &ec);
	if(U_FAILURE(ec)) {
		return defVal;
	}

	v64 = GetDateTime64(colIdx, v64, storedAs);

	UDate result = static_cast<UDate>(utmscale_fromInt64(v64, UDTS_ICU4C_TIME, &ec));
	if(U_FAILURE(ec)) {
		return defVal;
	}
	return result;*/

	return false;
}

bool IcuSqlite3Statement::BindBool(
	const int paramIdx, const bool value)
{
	return Bind(paramIdx, (value) ? 1 : 0);
}

bool IcuSqlite3Statement::BindNull(
	const int paramIdx)
{
	return (
		NULL != m_stmt &&
		SQLITE_OK == sqlite3_bind_null((sqlite3_stmt*)m_stmt, paramIdx));
}

bool IcuSqlite3Statement::BindZeroBlob(
	const int paramIdx, const int blobSize)
{
#if SQLITE_VERSION_NUMBER >= 3004000
	return (
		NULL != m_stmt &&
		SQLITE_OK == sqlite3_bind_zeroblob((sqlite3_stmt*)m_stmt, paramIdx, 
			blobSize));
#else	//	SQLITE_VERSION_NUMBER >= 3004000
	return false;
#endif	//	SQLITE_VERSION_NUMBER < 3004000
}

void IcuSqlite3Statement::ClearBindings()
{
	if(NULL != m_stmt) {
		sqlite3_clear_bindings((sqlite3_stmt*)m_stmt);
	}
}

UnicodeString IcuSqlite3Statement::GetSQL()
{
	UnicodeString sql;
#if SQLITE_VERSION_NUMBER >= 3005003
	if(NULL != m_stmt) {
		const char* utf8Sql = sqlite3_sql((sqlite3_stmt*)m_stmt);
		if(NULL != utf8Sql) {
			sql = UnicodeString::fromUTF8(utf8Sql);
		}
	}
#endif	//	SQLITE_VERSION_NUMBER >= 3005003

	return sql;
}

void IcuSqlite3Statement::Reset()
{
	if(NULL != m_stmt) {
		sqlite3_reset((sqlite3_stmt*)m_stmt);
	}
}

// ...

void IcuSqlite3Statement::Finalize()
{
	if(NULL != m_stmt) {
		sqlite3_finalize((sqlite3_stmt*)m_stmt);
		m_stmt = NULL;
	}
}


///////////////////////////////////////////////////////////////////////////////
//	IcuSqlite3Database
///////////////////////////////////////////////////////////////////////////////
IcuSqlite3Database::IcuSqlite3Database()
	: m_db(NULL)
	, m_busyTimeout(60000)	//	60 sec
	, m_encrypted(false)
{
}

IcuSqlite3Database::IcuSqlite3Database(
	const IcuSqlite3Database& db)
{
	m_db			= db.m_db;
	m_busyTimeout	= db.m_busyTimeout;
	m_encrypted		= db.m_encrypted;
}

/*virtual*/
IcuSqlite3Database::~IcuSqlite3Database()
{
	Close();
}

IcuSqlite3Database& IcuSqlite3Database::operator=(
	const IcuSqlite3Database& db)
{
	if(&db != this) {
		if(NULL == m_db) {
			m_db			= db.m_db;
			m_busyTimeout	= db.m_busyTimeout;
			m_encrypted		= db.m_encrypted;
		} else {
			assert(false);
		}
	}
	return *this;
}

bool IcuSqlite3Database::Open(
	const UnicodeString& filename, 
	const UnicodeString& key,
	const int flags /*= ICUSQLITE_OPEN_READWRITE | ICUSQLITE_OPEN_CREATE*/,
	const int extFlags /*= ICUSQLITE_EXT_OPEN_DEFAULT*/)
{
	std::string utf8Key;
	key.toUTF8String(utf8Key);
	int keyLen = static_cast<int>(utf8Key.length());

	return Open(filename, flags, extFlags, 
		(keyLen > 0) ? reinterpret_cast<const unsigned char*>(utf8Key.data()) : NULL, 
		keyLen);
}

bool IcuSqlite3Database::Open(
	const UnicodeString& filename, 
	const int flags /*= ICUSQLITE_OPEN_READWRITE | ICUSQLITE_OPEN_CREATE*/,
	const int extFlags /*= ICUSQLITE_EXT_OPEN_DEFAULT*/,
	const unsigned char* key /*= NULL*/,
	const int keyLen /*= 0*/)
{
	std::string utf8Filename;
	filename.toUTF8String(utf8Filename);

	int rc = sqlite3_open_v2(utf8Filename.data(),
		(sqlite3**)&m_db, flags, NULL);
	
	if(SQLITE_OK != rc) {
		Close();
		return false;
	}
	
#if defined(ICUSQLITE3_ANDROID)
	//	:TODO: expose this API & call wrapper
	sqlite3_soft_heap_limit(SQLITE_SOFT_HEAP_LIMIT);
#endif	//	defined(ICUSQLITE3_ANDROID)

	//
	//	Always enable extended result codes
	//
	//	:TODO: expose this API & call wrapper?
	rc = sqlite3_extended_result_codes((sqlite3*)m_db, 1);
	//	:TODO: validate rc!

	//
	//	If ICUSQLITE_EXT_OPEN_UTF16 is set (the default), and this is a new
	//	database [file], we'll need to PRAGMA UTF-16 t

#if ICUSQLITE_HAVE_CODEC
	if(NULL != key && keyLen > 0) {
		rc = sqlite3_key((sqlite3*)m_db, key, keyLen);
		if(SQLITE_OK != rc) {
			Close();
			return false;
		}
		m_encrypted = true;
	}
#endif	//	ICUSQLITE_HAVE_CODEC

	SetBusyTimeout(m_busyTimeout);

	//
	//	If this was a newly created database, set the encoding to
	//	UTF-16 if asked
	//
	int schemaVersion = 0;
	if(!ExecuteScalar("PRAGMA schema_version;", schemaVersion)) {
		Close();
		return false;
	}

	if((extFlags & ICUSQLITE_EXT_OPEN_UTF16) && 0 == schemaVersion)	{
		ExecuteUpdate("PRAGMA encoding=\"UTF-16\";");
	}
	
	//	:TODO: integrity check if requested
	
	//	:TODO: implement optional integrity check, see http://www.netmite.com/android/mydroid/frameworks/base/core/jni/android_database_SQLiteDatabase.cpp

/*
#if defined(ICUSQLITE_HAVE_ICU_EXTENSIONS)
	if((extFlags & ICUSQLITE_EXT_OPEN_ICUEXT)) {
		sqlite3IcuInit((sqlite3*)m_db);
	}
#endif	//	defined(ICUSQLITE_HAVE_ICU_EXTENSIONS
*/

	return true;
}

void IcuSqlite3Database::Close()
{
	if(NULL != m_db) {
	
#if SQLITE_VERSION_NUMBER >= 3006000
		//
		//	Finalize any unfished prepared statements
		//
		sqlite3_stmt* stmt;
		while(0 != (stmt = sqlite3_next_stmt((sqlite3*)m_db, 0))) {
			sqlite3_finalize(stmt);
		}
#endif	//	SQLITE_VERSION_NUMBER >= 3006000
		sqlite3_close((sqlite3*)m_db);
		m_db = NULL;
		m_encrypted = false;
	}
}

bool IcuSqlite3Database::Backup(
	const UnicodeString& targetFilename, const UnicodeString& targetKey,
	const UnicodeString& sourceDatabase /*= "main"*/)
{
	std::string utf8TargetKey;
	targetKey.toUTF8String(utf8TargetKey);
	int targetKeyLen = static_cast<int>(utf8TargetKey.length());
	const unsigned char* tk = (targetKeyLen > 0) ? 
		reinterpret_cast<const unsigned char*>(utf8TargetKey.data()) : NULL;
	return Backup(targetFilename, tk, targetKeyLen, sourceDatabase);

}

bool IcuSqlite3Database::Backup(
	const UnicodeString& targetFilename, const unsigned char* targetKey /*= NULL*/, 
	const int keyLen /*= 0*/, 
	const UnicodeString& sourceDatabase /*= "main"*/)
{
#if SQLITE_VERSION_NUMBER >= 3006011
	if(NULL == m_db) {
		return false;
	}

	std::string utf8TargetFilename;
	targetFilename.toUTF8String(utf8TargetFilename);

	sqlite3* dest;
	int rc = sqlite3_open(utf8TargetFilename.data(), &dest);
	if(SQLITE_OK != rc) {
		sqlite3_close(dest);
		return false;
	}

#if ICUSQLITE_HAVE_CODEC
	if(NULL != targetKey && keyLen > 0) {
		rc = sqlite3_key(dest, targetKey, keyLen);
		if(SQLITE_OK != rc) {
			sqlite3_close(dest);
			return false;
		}
	}
#endif	//	ICUSQLITE_HAVE_CODEC

	std::string utf8SourceDatabase;
	sourceDatabase.toUTF8String(utf8SourceDatabase);

	sqlite3_backup* backup = sqlite3_backup_init(dest, "main", (sqlite3*)m_db,
		utf8SourceDatabase.data());
	
	if(NULL == backup) {
		sqlite3_close(dest);
		return false;
	}

	while(SQLITE_OK == (rc = sqlite3_backup_step(backup, 128))) {
		/* nop */
	}
	
	sqlite3_backup_finish(backup);
	sqlite3_close(dest);

	if(SQLITE_DONE == rc) {
		return true;	
	}
#endif	//	SQLITE_VERSION_NUMBER >= 3006011

	return false;
}

EIcuSqlite3RestoreResults IcuSqlite3Database::Restore(
	const UnicodeString& sourceFilename, const UnicodeString& sourceKey,
	const UnicodeString& targetDatabase /*= "main"*/)
{
	std::string utf8SourceKey;
	sourceKey.toUTF8String(utf8SourceKey);
	int sourceKeyLen = static_cast<int>(utf8SourceKey.length());
	const unsigned char* sk = 
		reinterpret_cast<const unsigned char*>(utf8SourceKey.data());
	return Restore(sourceFilename, (sourceKeyLen > 0) ? sk : NULL, 
		sourceKeyLen, targetDatabase);
}

EIcuSqlite3RestoreResults IcuSqlite3Database::Restore(
	const UnicodeString& sourceFilename, const unsigned char* sourceKey/* = NULL*/, 
	const int keyLen /*= 0*/, 
	const UnicodeString& targetDatabase /*= "main"*/)
{
#if SQLITE_VERSION_NUMBER >= 3006011
	if(NULL == m_db) {
		return ICUSQLITE_RESTORE_FAILED;
	}

	std::string utf8SourceFilename;
	sourceFilename.toUTF8String(utf8SourceFilename);

	sqlite3* src;
	int rc = sqlite3_open(utf8SourceFilename.data(), &src);
	if(SQLITE_OK != rc) {
		sqlite3_close(src);
		return ICUSQLITE_RESTORE_FAILED;
	}

#if ICUSQLITE_HAVE_CODEC
	if(NULL != sourceKey && keyLen > 0) {
		rc = sqlite3_key(src, sourceKey, keyLen);
		if(SQLITE_OK != rc) {
			sqlite3_close(src);
			return ICUSQLITE_RESTORE_FAILED;
		}
	}
#endif	//	ICUSQLITE_HAVE_CODEC

	std::string utf8TargetDatabase;
	targetDatabase.toUTF8String(utf8TargetDatabase);

	sqlite3_backup* backup = sqlite3_backup_init((sqlite3*)m_db, 
		utf8TargetDatabase.data(), src, "main");
	
	if(NULL == backup) {
		sqlite3_close(src);
		return ICUSQLITE_RESTORE_FAILED;
	}

	int timeout = 0;
	while(SQLITE_OK == (rc = sqlite3_backup_step(backup, 128)) || 
		SQLITE_BUSY == rc)
	{
		if(SQLITE_BUSY == rc) {
			if(timeout++ >= 3) {
				break;
			}
			sqlite3_sleep(100);
		}
	}
	
	sqlite3_backup_finish(backup);
	sqlite3_close(src);

	if(SQLITE_DONE == rc) {
		return ICUSQLITE_RESTORE_SUCCESS;
	} else if(SQLITE_BUSY == rc || SQLITE_LOCKED == rc) {
		return ICUSQLITE_RESTORE_BUSY;
	}

#endif	//	SQLITE_VERSION_NUMBER >= 3006011
	return ICUSQLITE_RESTORE_FAILED;
}

bool IcuSqlite3Database::Begin(
	const EIcuSqlite3TransTypes type /*= ICUSQLITE_TRANSACTION_DEFAULT*/)
{
	switch(type) {
		case ICUSQLITE_TRANSACTION_DEFERRED :
			return (-1 != ExecuteUpdate("BEGIN DEFFERED TRANSACTION;"));

		case ICUSQLITE_TRANSACTION_IMMEDIATE :
			return (-1 != ExecuteUpdate("BEGIN IMMEDIATE TRANSACTION;"));
	
		case ICUSQLITE_TRANSACTION_EXCLUSIVE :
			return (-1 != ExecuteUpdate("BEGIN EXCLUSIVE TRANSACTION;"));

		default :
			return (-1 != ExecuteUpdate("BEGIN TRANSACTION;"));
	}
}

bool IcuSqlite3Database::Commit()
{
	return (-1 != ExecuteUpdate("COMMIT TRANSACTION;"));
}

bool IcuSqlite3Database::Rollback(
	const UnicodeString& savepointName /*= ""*/)
{
	std::string sql = "ROLLBACK TRANSACTION";

#if SQLITE_VERSION_NUMBER >= 3006008	
	if(!savepointName.isEmpty()) {
		std::string utf8SavepointName;
		sql += " TO ";
		sql += savepointName.toUTF8String(utf8SavepointName);	
	}
#endif	//	SQLITE_VERSION_NUMBER >= 3006008

	sql += ";";
	
	return (-1 != ExecuteUpdate(sql.data()));
}

bool IcuSqlite3Database::IsAutoCommitMode()
{
	return (NULL != m_db &&
		0 != sqlite3_get_autocommit((sqlite3*)m_db));
}

bool IcuSqlite3Database::Savepoint(
	const UnicodeString& savepointName)
{
#if SQLITE_VERSION_NUMBER >= 3006008
	std::string sql = "SAVEPOINT ";
	std::string utf8SavepointName;
	sql += savepointName.toUTF8String(utf8SavepointName);
	sql += ";";
	return (-1 != ExecuteUpdate(sql.data()));
#endif	//	SQLITE_VERSION_NUMBER >= 3006008
	return false;
}

bool IcuSqlite3Database::ReleaseSavepoint(
	const UnicodeString& savepointName)
{
#if SQLITE_VERSION_NUMBER >= 3006008
	std::string sql = "RELEASE SAVEPOINT ";
	std::string utf8SavepointName;
	sql += savepointName.toUTF8String(utf8SavepointName);
	sql += ";";
	return (-1 != ExecuteUpdate(sql.data()));
#endif	//	SQLITE_VERSION_NUMBER >= 3006008
	return false;
}

// ...

bool IcuSqlite3Database::TableExists(
	const UnicodeString& tableName, 
	const UnicodeString& dbName /*= ""*/)
{
	int result = 0;
	const char* sqlBuf;
	IcuSqlite3StatementBuffer sql;
	std::string tableNameBuf;

	if(dbName.isEmpty()) {
		sqlBuf = sql.Format(
			"SELECT COUNT(*)\
			 FROM sqlite_master \
			 WHERE type='table' AND name LIKE '%s';",
			tableName.toUTF8String(tableNameBuf).c_str());
	} else {
		std::string dbNameBuf;
		sqlBuf = sql.Format(
			"SELECT COUNT(*)\
			 FROM %s.sqlite_master \
			 WHERE type='table' AND name LIKE '%s';",
			dbName.toUTF8String(dbNameBuf).c_str(), 
			tableName.toUTF8String(tableNameBuf).c_str());
	}

	return ExecuteScalar(sqlBuf, result) && result > 0;
}

bool IcuSqlite3Database::TableExists(
	const char* tableName, const char* dbName /*= NULL*/)
{
	return TableExists(UnicodeString::fromUTF8(tableName),
		UnicodeString::fromUTF8(dbName));
}

void IcuSqlite3Database::GetDatabaseNames(
	std::set<std::string>& dbNames, 
	std::set<std::string>& dbFiles)
{
	dbNames.clear();
	dbFiles.clear();

	//	buffers for uni -> utf-8 names
	std::string utf8DbName;
	std::string utf8DbFile;

	IcuSqlite3ResultSet results = ExecuteQuery("PRAGMA database_list;");
	while(results.NextRow()) {
		dbNames.insert(results.GetString(1).toUTF8String(utf8DbName));
		dbFiles.insert(results.GetString(2).toUTF8String(utf8DbFile));
	}
}


int IcuSqlite3Database::ExecuteUpdate(
	const UnicodeString& sql)
{
	std::string utf8Sql;
	return ExecuteUpdate(sql.toUTF8String(utf8Sql).data());
}

int IcuSqlite3Database::ExecuteUpdate(
	const char* sql)
{
	if(NULL == m_db) {
		return -1;
	}

	char* err = NULL;
	if(SQLITE_OK == sqlite3_exec((sqlite3*)m_db, sql, 0, 0, &err)) {
		return sqlite3_changes((sqlite3*)m_db);
	}

	return -1;
}

int IcuSqlite3Database::ExecuteUpdate(
	const IcuSqlite3StatementBuffer& sql)
{
	return ExecuteUpdate(static_cast<const char*>(sql));
}

// ...
IcuSqlite3ResultSet IcuSqlite3Database::ExecuteQuery(
	const UnicodeString& sql)
{
	sqlite3_stmt* stmt = 
		static_cast<sqlite3_stmt*>(Prepare(sql.getBuffer(), 
			sql.length() * sizeof(UChar)));

	if(NULL == stmt) {
		return IcuSqlite3ResultSet(m_db, stmt, true);
	}

	switch(sqlite3_step(stmt)) {
		case SQLITE_DONE :	return IcuSqlite3ResultSet(m_db, stmt, true);
		case SQLITE_ROW :	return IcuSqlite3ResultSet(m_db, stmt, false);
	}

	//	something went wrong
	sqlite3_finalize(stmt);
	return IcuSqlite3ResultSet(m_db, NULL, true);
}

IcuSqlite3ResultSet IcuSqlite3Database::ExecuteQuery(
	const char* sql)
{
	return ExecuteQuery(UnicodeString::fromUTF8(sql));
}

IcuSqlite3ResultSet IcuSqlite3Database::ExecuteQuery(
	const IcuSqlite3StatementBuffer& sql)
{
	return ExecuteQuery(UnicodeString::fromUTF8(static_cast<const char*>(sql)));
}

// ...

bool IcuSqlite3Database::ExecuteScalar(
	const UnicodeString& sql, UnicodeString& result)
{
	IcuSqlite3ResultSet r = ExecuteQuery(sql);
	if(!r.Eof() && r.GetColumnCount() > 0) {
		result = r.GetString(0, "");
		return true;
	}
	return false;
}

bool IcuSqlite3Database::ExecuteScalar(
	const char* sql, UnicodeString& result)
{
	return ExecuteScalar(UnicodeString::fromUTF8(sql), result);
}

bool IcuSqlite3Database::ExecuteScalar(
	const UnicodeString& sql, int32_t& result)
{
	IcuSqlite3ResultSet r = ExecuteQuery(sql);
	if(!r.Eof() && r.GetColumnCount() > 0) {
		result = r.GetInt(0);
		return true;
	}
	return false;
}

bool IcuSqlite3Database::ExecuteScalar(
	const char* sql, int32_t& result)
{
	return ExecuteScalar(UnicodeString::fromUTF8(sql), result);
}

bool IcuSqlite3Database::ExecuteScalar(
	const UnicodeString& sql, int64_t& result)
{
	IcuSqlite3ResultSet r = ExecuteQuery(sql);
	if(!r.Eof() && r.GetColumnCount() > 0) {
		result = r.GetInt64(0);
		return true;
	}
	return false;
}

bool IcuSqlite3Database::ExecuteScalar(
	const char* sql, int64_t& result)
{
	return ExecuteScalar(UnicodeString::fromUTF8(sql), result);
}

bool IcuSqlite3Database::ExecuteScalar(
	const UnicodeString& sql, double& result)
{
	IcuSqlite3ResultSet r = ExecuteQuery(sql);
	if(!r.Eof() && r.GetColumnCount() > 0) {
		result = r.GetDouble(0);
		return true;
	}
	return false;
}

bool IcuSqlite3Database::ExecuteScalar(
	const char* sql, double& result)
{
	return ExecuteScalar(UnicodeString::fromUTF8(sql), result);
}

bool IcuSqlite3Database::ExecuteScalar(
	const UnicodeString& sql, bool& result)
{
	IcuSqlite3ResultSet r = ExecuteQuery(sql);
	if(!r.Eof() && r.GetColumnCount() > 0) {
		result = r.GetBool(0);
		return true;
	}
	return false;
}

bool IcuSqlite3Database::ExecuteScalar(
	const char* sql, bool& result)
{
	return ExecuteScalar(UnicodeString::fromUTF8(sql), result);
}

bool IcuSqlite3Database::ExecuteScalar(
	const UnicodeString& sql, std::string& result)
{
	IcuSqlite3ResultSet r = ExecuteQuery(sql);
	if(!r.Eof() && r.GetColumnCount() > 0) {
		result = r.GetStringUTF8(0, "");
		return true;
	}
	return false;
}

bool IcuSqlite3Database::ExecuteScalar(
	const char* sql, std::string& result)
{
	return ExecuteScalar(UnicodeString::fromUTF8(sql), result);
}

bool IcuSqlite3Database::ExecuteScalarDateTime(
	const UnicodeString& sql, UDate& result)
{
	IcuSqlite3ResultSet r = ExecuteQuery(sql);
	if(!r.Eof() && r.GetColumnCount() > 0) {
		result = r.GetDateTime(0);
		return true;
	}
	return false;
}

bool IcuSqlite3Database::ExecuteScalarDateTime(
	const char* sql, UDate& result)
{
	return ExecuteScalarDateTime(UnicodeString::fromUTF8(sql), result);
}

IcuSqlite3Table IcuSqlite3Database::GetTable(
	const UnicodeString& sql)
{
	std::string utf8SqlBuf;
	return GetTable(
		static_cast<const char*>(sql.toUTF8String(utf8SqlBuf).data()));
}

IcuSqlite3Table IcuSqlite3Database::GetTable(
	const char* sql)
{
	if(NULL == m_db) {
		return IcuSqlite3Table(NULL, 0, 0);
	}

	char** results = NULL;
	char* err = NULL;
	int rows = 0;
	int cols = 0;

	if(SQLITE_OK == sqlite3_get_table((sqlite3*)m_db, sql, &results, &rows, 
		&cols, &err)) 
	{
		return IcuSqlite3Table(results, rows, cols);
	}

	return IcuSqlite3Table(NULL, 0, 0);
}

IcuSqlite3Table IcuSqlite3Database::GetTable(
	const IcuSqlite3StatementBuffer& sql)
{
	return GetTable(static_cast<const char*>(sql));
}

IcuSqlite3Statement IcuSqlite3Database::PrepareStatement(
	const UnicodeString& sql)
{
	if(NULL == m_db) {
		return IcuSqlite3Statement(NULL, NULL);
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)Prepare(sql.getBuffer(), 
		sql.length() * sizeof(UChar));
	return IcuSqlite3Statement(m_db, stmt);
}

IcuSqlite3Statement IcuSqlite3Database::PrepareStatement(
	const char* sql)
{
	return PrepareStatement(UnicodeString::fromUTF8(sql));
}

IcuSqlite3Statement IcuSqlite3Database::PrepareStatement(
	const IcuSqlite3StatementBuffer& sql)
{
	return PrepareStatement(static_cast<const char*>(sql));
}

int64_t IcuSqlite3Database::GetLastRowId()
{
	return sqlite3_last_insert_rowid((sqlite3*)m_db);
}

bool IcuSqlite3Database::SetBusyTimeout(
	const int ms)
{
	if(NULL != m_db &&
		(SQLITE_OK == sqlite3_busy_timeout((sqlite3*)m_db, ms)))
	{
		m_busyTimeout = ms;
		return true;
	}
	return false;
}

bool IcuSqlite3Database::CreateScalarFunction(
	const char* funcName, const int args, IcuSqlite3ScalarFunction* func)
{	
	typedef void (*XFUNC)(sqlite3_context*, int, sqlite3_value**);
	return (NULL != func && NULL != m_db && 
		(SQLITE_OK == sqlite3_create_function(
			(sqlite3*)m_db,
			funcName,
			args,
#ifdef WORDS_BIGENDIAN
			SQLITE_UTF16BE,
#else
			SQLITE_UTF16LE,
#endif
			func,
			(XFUNC)xFunc,
			NULL,
			NULL)));
}

bool IcuSqlite3Database::CreateAggregateFunction(
	const char* funcName, const int args, IcuSqlite3AggregateFunction* func)
{
	typedef void (*XSTEP)(sqlite3_context*, int, sqlite3_value**);
	typedef void (*XFINAL)(sqlite3_context*);

	return (NULL != func && NULL != m_db && 
		(SQLITE_OK == sqlite3_create_function(
			(sqlite3*)m_db,
			funcName,
			args,
#ifdef WORDS_BIGENDIAN
			SQLITE_UTF16BE,
#else
			SQLITE_UTF16LE,
#endif
			func,
			NULL,
			(XSTEP)xStep,
			(XFINAL)xFinalize)));
}

// ...

int IcuSqlite3Database::GetLastErrorCode(
	const bool extended /*= false*/)
{
	if(NULL == m_db) {
		return 0;
	}
	
	return (extended) ? 
		sqlite3_extended_errcode((sqlite3*)m_db) :
		sqlite3_errcode((sqlite3*)m_db);
}

UnicodeString IcuSqlite3Database::GetLastErrorMessage()
{
	if(NULL == m_db) {
		return UNICODE_STRING_SIMPLE("");
	}
	
	const UChar* errMsg = (const UChar*)(sqlite3_errmsg16((sqlite3*)m_db));
	return UnicodeString(errMsg, -1);
}

//...

///////////////////////////////////////////////////////////////////////////////
//	IcuSqlite3Database - private
///////////////////////////////////////////////////////////////////////////////
void* IcuSqlite3Database::Prepare(
	const UChar* sql, const int32_t sqlLen /*= -1*/)
{
	if(NULL == m_db) {
		return NULL;
	}

	const UChar* tail = NULL;
	sqlite3_stmt* stmt;

	if(SQLITE_OK != sqlite3_prepare16_v2((sqlite3*)m_db, (void*)sql, sqlLen, 
		&stmt, (const void**)&tail)) 
	{
		return NULL;
	}

	return stmt;
}

/*static*/
void IcuSqlite3Database::xFunc(
	void* ctxt, int argCount, void** args)
{
	IcuSqlite3ScalarFunction* sqlFunc = 
		reinterpret_cast<IcuSqlite3ScalarFunction*>(sqlite3_user_data(
			(sqlite3_context*)ctxt));
	if(NULL != sqlFunc) {
		IcuSqlite3FunctionContext context(ctxt, argCount, args);
		sqlFunc->Scalar(&context);
	}
}

/*static*/
void IcuSqlite3Database::xStep(
	void* ctxt, int argCount, void** args)
{
	IcuSqlite3AggregateFunction* sqlFunc = 
		reinterpret_cast<IcuSqlite3AggregateFunction*>(sqlite3_user_data(
			(sqlite3_context*)ctxt));
	if(NULL != sqlFunc) {
		IcuSqlite3FunctionContext context(ctxt, argCount, args);
		sqlFunc->Step(&context);
	}
}

/*static*/
void IcuSqlite3Database::xFinalize(
	void* ctxt)
{
	IcuSqlite3AggregateFunction* sqlFunc = 
		reinterpret_cast<IcuSqlite3AggregateFunction*>(sqlite3_user_data(
			(sqlite3_context*)ctxt));
	if(NULL != sqlFunc) {
		IcuSqlite3FunctionContext context(ctxt, 0, NULL);
		sqlFunc->Finalize(&context);
	}
}

///////////////////////////////////////////////////////////////////////////////
//	IcuSqlite3Database - static
///////////////////////////////////////////////////////////////////////////////

#if !defined(SQLITE_OMIT_SHARED_CACHE)
bool IcuSqlite3Database::ms_sharedCacheEnabled	= false;
#endif

int	IcuSqlite3Database::ms_supportFlags			= ICUSQLITE_SUPPORTED_FLAGS;

/*static*/
bool IcuSqlite3Database::InitializeSQLite()
{
#if SQLITE_VERSION_NUMBER >= 3006000
	return (SQLITE_OK == sqlite3_initialize());
#else	//	SQLITE_VERSION_NUMBER >= 3006000
	return true;
#endif	//	SQLITE_VERSION_NUMBER < 3006000
}

/*static*/
bool IcuSqlite3Database::ShutdownSQLite()
{
#if SQLITE_VERSION_NUMBER >= 3006000
	return (SQLITE_OK == sqlite3_shutdown());
#else	//	SQLITE_VERSION_NUMBER >= 3006000
	return true;
#endif	//	SQLITE_VERSION_NUMBER < 3006000
}

/*static*/
bool IcuSqlite3Database::Randomness(
	unsigned char* randBuf, const int bufLen)
{
#if SQLITE_VERSION_NUMBER >= 3005008
	if(bufLen > 0) {
		sqlite3_randomness(bufLen, (void*)randBuf);
		return true;
	}
#endif	//	SQLITE_VERSION_NUMBER >= 3005008

	return false;
}

/*static*/
bool IcuSqlite3Database::EnableSharedCache(
	const bool enable)
{
#if !defined(SQLITE_OMIT_SHARED_CACHE)
	int flag = (enable) ? 1 : 0;
	if(SQLITE_OK == sqlite3_enable_shared_cache(flag)) {
		ms_sharedCacheEnabled = enable;
		return true;
	}
	return false;
#else
	return false;
#endif
}

/*static*/
UnicodeString IcuSqlite3Database::GetVersion()
{
	return UnicodeString::fromUTF8(sqlite3_libversion());
}

/*static*/
UnicodeString IcuSqlite3Database::GetSourceId()
{
#if SQLITE_VERSION_NUMBER >= 3006018
	return UnicodeString::fromUTF8(sqlite3_sourceid());
#else	//	SQLITE_VERSION_NUMBER >= 3006018
	return "";
#endif	//	SQLITE_VERSION_NUMBER < 3006018
}

/*static*/
bool IcuSqlite3Database::HasSupport(
	const EIcuSqlite3SupportFlags supportFor)
{
	return (ms_supportFlags & supportFor) ? true : false;
}

#if defined(ICUSQLITE3_ANDROID)
/*static*/
void IcuSqlite3Database::ReleaseMemory()
{
	sqlite3_release_memory(SQLITE_SOFT_HEAP_LIMIT);
}
#endif	//	defined(ICUSQLITE3_ANDROID)

///////////////////////////////////////////////////////////////////////////////
//	IcuSqlite3Transaction
///////////////////////////////////////////////////////////////////////////////
IcuSqlite3Transaction::IcuSqlite3Transaction(
	IcuSqlite3Database* db, const EIcuSqlite3TransTypes type /*= ICUSQLITE_TRANSACTION_DEFAULT*/)
	: m_db(db)
	, m_transOpen(false)
{
		//	:TODO: assert(NULL != db);

	Open(type);
}

IcuSqlite3Transaction::~IcuSqlite3Transaction()
{
	Flush(false);
}

bool IcuSqlite3Transaction::Open(
	const EIcuSqlite3TransTypes type /*= ICUSQLITE_TRANSACTION_DEFAULT*/)
{
	if(m_transOpen && !Flush(false)) {
		return false;
	}

	m_transOpen = m_db->Begin(type);
	m_transOk = m_transOpen;
	return m_transOpen;
}

bool IcuSqlite3Transaction::Flush(
	const bool reOpen /*= false*/)
{
	bool ret = true;
	if(m_transOpen) {
		if(ret = ((m_transOk) ? m_db->Commit() : m_db->Rollback())) {
			m_transOpen = false;
		}
	}
	if(ret && reOpen) {
		ret = Open();
	}
	return ret;
}

bool IcuSqlite3Transaction::Rollback(
	const bool reOpen /*= false*/)
{
	m_transOk = false;
	return Flush(reOpen);
}

bool IcuSqlite3Transaction::Execute(
	const UnicodeString& sql)
{
	if(!m_transOk) {
		return false;
	}
	m_transOk = (-1 != m_db->ExecuteUpdate(sql));
	return m_transOk;
}

bool IcuSqlite3Transaction::Execute(
	const char* sql)
{
	if(!m_transOk) {
		return false;
	}
	m_transOk = (-1 != m_db->ExecuteUpdate(sql));
	return m_transOk;
}

bool IcuSqlite3Transaction::Execute(
	const IcuSqlite3StatementBuffer& sql)
{
	if(!m_transOk) {
		return false;
	}
	m_transOk = (-1 != m_db->ExecuteUpdate(sql));
	return m_transOk;
}