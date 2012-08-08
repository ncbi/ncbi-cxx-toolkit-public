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
 * Author:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbierror.hpp>

#define NCBI_USE_ERRCODE_X   Corelib_Diag

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////

class CNcbiError_Int : public CNcbiError
{
public:
    CNcbiError_Int(void) {
    }
};

static
void NcbiError_Cleanup(CNcbiError* e, void*)
{
    delete e;
}

static 
CStaticTls<CNcbiError> s_Last;

static
CNcbiError* NcbiError_GetOrCreate(void)
{
    CNcbiError* e = s_Last.GetValue();
    if (!e) {
        s_Last.SetValue(e = new CNcbiError_Int(), NcbiError_Cleanup);
    }
    return e;
}

/////////////////////////////////////////////////////////////////////////////

CNcbiError::CNcbiError(void)
    : m_Code(eSuccess), m_Category(eGeneric), m_Native(0)
{
}


const CNcbiError& CNcbiError::GetLast(void)
{
    return *NcbiError_GetOrCreate();
}

void  CNcbiError::Set(ECode code, const string& extra)
{
    CNcbiError* e = NcbiError_GetOrCreate();
    e->m_Code     = code;
    e->m_Category = code < eUnknown ? eGeneric : eNcbi;
    e->m_Native   = code;
    e->m_Extra    = extra;
}

void  CNcbiError::SetErrno(int native_err_code, const string& extra)
{
    CNcbiError* e = NcbiError_GetOrCreate();
    e->m_Code     = (ECode)native_err_code;
    e->m_Category = e->m_Code < eUnknown ? eGeneric : eNcbi;
    e->m_Native   = native_err_code;
    e->m_Extra    = extra;
}
void  CNcbiError::SetFromErrno(const string& extra)
{
    SetErrno(errno,extra);
}
void  CNcbiError::SetWindowsError( int native_err_code, const string& extra)
{
    CNcbiError* e = NcbiError_GetOrCreate();
//TODO: make translation!
#if NCBI_OS_MSWIN
    switch (native_err_code) {
    case ERROR_FILE_NOT_FOUND:      e->m_Code = no_such_file_or_directory; break;
    case ERROR_PATH_NOT_FOUND:      e->m_Code = no_such_file_or_directory; break;
    case ERROR_TOO_MANY_OPEN_FILES: e->m_Code = too_many_files_open;       break;
    case ERROR_ACCESS_DENIED:       e->m_Code = permission_denied;         break;
    case ERROR_NOT_ENOUGH_MEMORY:   e->m_Code = not_enough_memory;         break;
    case ERROR_NOT_SUPPORTED:       e->m_Code = not_supported;             break;
    case ERROR_FILE_EXISTS:         e->m_Code = file_exists;               break;
    case ERROR_BROKEN_PIPE:         e->m_Code = broken_pipe;               break;
    case ERROR_DISK_FULL:           e->m_Code = no_space_on_device;        break;
    case ERROR_DIR_NOT_EMPTY:       e->m_Code = directory_not_empty;       break;
    case ERROR_BAD_ARGUMENTS:       e->m_Code = invalid_argument;          break;
    case ERROR_ALREADY_EXISTS:      e->m_Code = file_exists;               break;
    case ERROR_BAD_EXE_FORMAT:      e->m_Code = executable_format_error;   break;
    default:
        e->m_Code     = eUnknown;
        break;
    }
#else
        e->m_Code     = eUnknown;
#endif

    e->m_Category = eMsWindows;
    e->m_Native   = native_err_code;
    e->m_Extra    = extra;
}
void  CNcbiError::SetFromWindowsError(                  const string& extra)
{
#if defined(NCBI_OS_MSWIN)
    SetWindowsError(GetLastError(),extra);
#else
    Set(eUnknown,extra);
#endif
}


/////////////////////////////////////////////////////////////////////////////

CNcbiOstream& operator<< (CNcbiOstream& str, const CNcbiError& err)
{
    if (err.Category() == CNcbiError::eGeneric) {
        str << err.Code() << ": " << Ncbi_strerror(err.Code());
    }
#if NCBI_OS_MSWIN
    else if (err.Category() == CNcbiError::eMsWindows) {
        str << err.Native() << ": " << CLastErrorAdapt::GetErrCodeString(err.Native());
    }
#endif
    else {
        str << err.Code();
    }
    if (!err.Extra().empty()) {
        str << ": " << err.Extra();
    }
    return str;
}


END_NCBI_SCOPE
