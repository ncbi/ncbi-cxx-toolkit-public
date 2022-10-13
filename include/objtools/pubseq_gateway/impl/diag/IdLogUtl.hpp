/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  useful utilities
 *
 */

#ifndef _IDLOGUTL_HPP_
#define _IDLOGUTL_HPP_

#include <csignal>
#include <sys/time.h>
#include <map>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbimtx.hpp>

BEGIN_SCOPE(IdLogUtil)

USING_NCBI_SCOPE;

#define DISALLOW_COPY_AND_ASSIGN(TypeName)	\
	TypeName(const TypeName&);				\
	TypeName& operator=(const TypeName&)

class
NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted. Replace IdLogUtil::EError with project/library specific class")
EError: public CException {
public:
    enum EErrCode {
        eUnknown = 0x10000,
		eFatal,
		eGeneric,
		eUserError,
        eSeqFailed,
		eBlobError,
		eSybDriver,
		eMemory,
		eNoPoolThreads,
		eUserCancelled,
        eConfiguration,
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
			case eUnknown:			return "eUnknown";
			case eFatal:			return "eFatal";
			case eGeneric:			return "eGeneric";
			case eUserError:		return "eUserError";
        	case eSeqFailed:		return "eSeqFailed";
			case eBlobError:		return "eBlobError";
			case eSybDriver:		return "eSybDriver";
			case eMemory:			return "eMemory";
			case eNoPoolThreads:	return "eNoPoolThreads";
		    case eUserCancelled:	return "eUserCancelled";
            case eConfiguration:    return "eConfiguration";
			default:				return CException::GetErrCodeString();
	    }
    }
    NCBI_EXCEPTION_DEFAULT(EError, CException);
};

#define RAISE_ERROR(errc, comm)                     \
    do {                                            \
        NCBI_THROW(IdLogUtil::EError, errc, comm);  \
    } while (0)

NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted.")
void DumpBinBuffer(const unsigned char* buf, size_t len);
NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted.")
void DumpBinBuffer(const unsigned char* buf, size_t len, const string& filename);
NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted.")
void StringReplace(string& where, const string& what, const string& replace);
NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted.")
string StringFetch(string& src, char delim);
NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted.")
void StringFromFile(const string& filename, string& str);
NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted.")
void StringToFile(const string& filename, const string& str);
NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted. Replace gettime() call with gettime() in psg_cassandra (cass_util.hpp) library.")
int64_t gettime(void);
NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted.")
int64_t gettime_ns(void);
NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted.")
string Int64ToDt(int64_t dt, bool inUTC);
NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted. Use ncbistr.hpp")
string TrimRight(const string& s, const string& delimiters = " \f\n\r\t\v");
NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted. Use ncbistr.hpp")
string TrimLeft(const string& s, const string& delimiters = " \f\n\r\t\v");
NCBI_STD_DEPRECATED("psg_diag library is deprecated and will be deleted. Use ncbistr.hpp")
string Trim(const string& s, const string& delimiters = " \f\n\r\t\v" );

END_SCOPE(IdLogUtil)

#endif  // _IDLOGUTL_HPP_
