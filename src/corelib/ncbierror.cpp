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
#if defined(NCBI_OS_MSWIN)
#  include <Winsock2.h>
#endif

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

CNcbiError::ECode CNcbiError::Code(void) const
{
#if defined(NCBI_OS_MSWIN)
    if (m_Code == eNotSet) {
//        if (e->m_Category == eMsWindows)
        switch (m_Native) {
        case ERROR_ACCESS_DENIED:       m_Code = ePermissionDenied;              break;
        case ERROR_ALREADY_EXISTS:      m_Code = eFileExists;                    break;
        case ERROR_BAD_ARGUMENTS:       m_Code = eInvalidArgument;               break;
        case ERROR_BAD_EXE_FORMAT:      m_Code = eExecutableFormatError;         break;
        case ERROR_BROKEN_PIPE:         m_Code = eBrokenPipe;                    break;
        case ERROR_DIR_NOT_EMPTY:       m_Code = eDirectoryNotEmpty;             break;
        case ERROR_DISK_FULL:           m_Code = eNoSpaceOnDevice;               break;
        case ERROR_FILE_EXISTS:         m_Code = eFileExists;                    break;
        case ERROR_FILE_NOT_FOUND:      m_Code = eNoSuchFileOrDirectory;         break;
        case ERROR_NOT_ENOUGH_MEMORY:   m_Code = eNotEnoughMemory;               break;
        case ERROR_NOT_SUPPORTED:       m_Code = eNotSupported;                  break;
        case ERROR_PATH_NOT_FOUND:      m_Code = eNoSuchFileOrDirectory;         break;
        case ERROR_TOO_MANY_OPEN_FILES: m_Code = eTooManyFilesOpen;              break;

        case WSAEADDRINUSE:             m_Code = eAddressInUse;                  break; //EADDRINUSE
        case WSAEADDRNOTAVAIL:          m_Code = eAddressNotAvailable;           break; //ADDRNOTAVAIL
        case WSAEAFNOSUPPORT:           m_Code = eAddressFamilyNotSupported;     break; //EAFNOSUPPORT
        case WSAEALREADY:               m_Code = eConnectionAlreadyInProgress;   break; //EALREADY
        case WSAECONNABORTED:           m_Code = eConnectionAborted;             break; //ECONNABORTED
        case WSAECONNREFUSED:           m_Code = eConnectionRefused;             break; //ECONNREFUSED
        case WSAECONNRESET:             m_Code = eConnectionReset;               break; //ECONNRESET
        case WSAEDESTADDRREQ:           m_Code = eDestinationAddressRequired;    break; //EDESTADDRREQ
        case WSAEHOSTUNREACH:           m_Code = eHostUnreachable;               break; //EHOSTUNREACH
        case WSAEINPROGRESS:            m_Code = eOperationInProgress;           break; //EINPROGRESS
        case WSAEINTR:                  m_Code = eInterrupted;                   break; //EINTR
        case WSAEISCONN:                m_Code = eAlreadyConnected;              break; //EISCONN
        case WSAELOOP:                  m_Code = eTooManySymbolicLinkLevels;     break; //ELOOP 
        case WSAEMSGSIZE:               m_Code = eMessageSize;                   break; //EMSGSIZE
        case WSAENAMETOOLONG:           m_Code = eFilenameTooLong;               break; //ENAMETOOLONG
        case WSAENETDOWN:               m_Code = eNetworkDown;                   break; //ENETDOWN
        case WSAENETRESET:              m_Code = eNetworkReset;                  break; //ENETRESET
        case WSAENETUNREACH:            m_Code = eNetworkUnreachable;            break; //ENETUNREACH
        case WSAENOBUFS:                m_Code = eNoBufferSpace;                 break; //ENOBUFS
        case WSAENOPROTOOPT:            m_Code = eNoProtocolOption;              break; //ENOPROTOOPT
        case WSAENOTCONN:               m_Code = eNotConnected;                  break; //ENOTCONN
        case WSAENOTEMPTY:              m_Code = eDirectoryNotEmpty;             break; //ENOTEMPTY
        case WSAENOTSOCK:               m_Code = eNotASocket;                    break; //ENOTSOCK
        case WSAEOPNOTSUPP:             m_Code = eOperationNotSupported;         break; //EOPNOTSUPP
        case WSAEPROTONOSUPPORT:        m_Code = eProtocolNotSupported;          break; //EPROTONOSUPPORT
        case WSAEPROTOTYPE:             m_Code = eWrongProtocolType;             break; //EPROTOTYPE
        case WSAESHUTDOWN:              m_Code = eBrokenPipe;                    break; //     EPIPE ?
        case WSAETIMEDOUT:              m_Code = eTimedOut;                      break; //ETIMEDOUT
        case WSAEWOULDBLOCK:            m_Code = eOperationWouldBlock;           break; //EWOULDBLOCK 

        default:
            m_Code     = eUnknown;
            break;
        }
    }
#endif
    return m_Code;
}

void  CNcbiError::Set(ECode code, const CTempString& extra)
{
    CNcbiError* e = NcbiError_GetOrCreate();
    e->m_Code     = code;
    e->m_Category = code < eUnknown ? eGeneric : eNcbi;
    e->m_Native   = code;
    e->m_Extra    = extra;
}

void  CNcbiError::SetErrno(int native_err_code, const CTempString& extra)
{
    CNcbiError* e = NcbiError_GetOrCreate();
    e->m_Code     = ECode(native_err_code);
    e->m_Category = e->m_Code < eUnknown ? eGeneric : eNcbi;
    e->m_Native   = native_err_code;
    e->m_Extra    = extra;
}
void  CNcbiError::SetFromErrno(const CTempString& extra)
{
    SetErrno(errno,extra);
}

#if defined(NCBI_OS_MSWIN)
void  CNcbiError::SetWindowsError( int native_err_code, const CTempString& extra)
{
    CNcbiError* e = NcbiError_GetOrCreate();
    e->m_Code     = eNotSet;
    e->m_Category = eMsWindows;
    e->m_Native   = native_err_code;
    e->m_Extra    = extra;
}

void  CNcbiError::SetFromWindowsError( const CTempString& extra)
{
    SetWindowsError( GetLastError(), extra );
}
#endif

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
