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
* Author: Eugene Vasilchenko
*
* File Description:
*   Standard serialization exceptions
*/

#include <ncbi_pch.hpp>
#include <serial/exception.hpp>

BEGIN_NCBI_SCOPE

void CSerialException::AddFrameInfo(string frame_info)
{
    m_FrameStack = frame_info + m_FrameStack;
}

void CSerialException::ReportExtra(ostream& out) const
{
    if ( !m_FrameStack.empty() ) {
        out << " at " << m_FrameStack;
    }
}

const char* CInvalidChoiceSelection::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eFail:  return "eFail";
    default:     return CException::GetErrCodeString();
    }
}

const char* CInvalidChoiceSelection::GetName(
    size_t index, const char* const names[], size_t namesCount)
{
    if ( index > namesCount )
        return "?unknown?";
    return names[index];
    
}

CInvalidChoiceSelection::CInvalidChoiceSelection(
    const CDiagCompileInfo& diag_info,
    size_t currentIndex, size_t mustBeIndex,
    const char* const names[], size_t namesCount, 
    EDiagSev severity)
        : CSerialException(diag_info, 0,
          (CSerialException::EErrCode) CException::eInvalid,"")
{
    x_Init(diag_info,
           string("Invalid choice selection: ")+
           GetName(currentIndex, names, namesCount)+". "
           "Expected: "+
           GetName(mustBeIndex, names, namesCount),0, severity);
    x_InitErrCode((CException::EErrCode)(CInvalidChoiceSelection::eFail));
}

CInvalidChoiceSelection::CInvalidChoiceSelection(
    const char* file, int line,
    size_t currentIndex, size_t mustBeIndex,
    const char* const names[], size_t namesCount,
    EDiagSev severity)
        : CSerialException(CDiagCompileInfo(file, line), 0,
          (CSerialException::EErrCode) CException::eInvalid,"")
{
    x_Init(CDiagCompileInfo(file, line),
           string("Invalid choice selection: ")+
           GetName(currentIndex, names, namesCount)+". "
           "Expected: "+
           GetName(mustBeIndex, names, namesCount),0, severity);
    x_InitErrCode((CException::EErrCode)(CInvalidChoiceSelection::eFail));
}

CInvalidChoiceSelection::CInvalidChoiceSelection(
    size_t currentIndex, size_t mustBeIndex,
    const char* const names[], size_t namesCount,
    EDiagSev severity)
        : CSerialException(CDiagCompileInfo("unknown", 0), 0,
          (CSerialException::EErrCode) CException::eInvalid,"")
{
    x_Init(CDiagCompileInfo("unknown", 0),
           string("Invalid choice selection: ")+
           GetName(currentIndex, names, namesCount)+". "
           "Expected: "+
           GetName(mustBeIndex, names, namesCount),0, severity);
    x_InitErrCode((CException::EErrCode)(CInvalidChoiceSelection::eFail));
}

CInvalidChoiceSelection::CInvalidChoiceSelection(
    const CInvalidChoiceSelection& other)
    : CSerialException(other)
{
    x_Assign(other);
}

CInvalidChoiceSelection::~CInvalidChoiceSelection(void) throw()
{
}

const char* CInvalidChoiceSelection::GetType(void) const
{
    return "CInvalidChoiceSelection";
}

CInvalidChoiceSelection::TErrCode CInvalidChoiceSelection::GetErrCode(void) const
{
    return typeid(*this) == typeid(CInvalidChoiceSelection) ?
        (CInvalidChoiceSelection::TErrCode) x_GetErrCode() :
        (CInvalidChoiceSelection::TErrCode) CException::eInvalid;
}

CInvalidChoiceSelection::CInvalidChoiceSelection(void)
{
}

const CException* CInvalidChoiceSelection::x_Clone(void) const
{
    return new CInvalidChoiceSelection(*this);
}

const char* CSerialException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eNotImplemented: return "eNotImplemented";
    case eEOF:            return "eEOF";
    case eIoError:        return "eIoError";
    case eFormatError:    return "eFormatError";
    case eOverflow:       return "eOverflow";
    case eInvalidData:    return "eInvalidData";
    case eIllegalCall:    return "eIllegalCall";
    case eFail:           return "eFail";
    case eNotOpen:        return "eNotOpen";
    case eMissingValue:   return "eMissingValue";
    default:              return CException::GetErrCodeString();
    }
}

const char* CUnassignedMember::GetErrCodeString(void) const
{
#if 0
    switch ( GetErrCode() ) {
        case eGet:            return "eGet";
        case eWrite:          return "eWrite";
        case eUnknownMember:  return "eUnknownMember";
        default:              return CException::GetErrCodeString();
        }
#else
        // At least with ICC 9.0 on 64-bit Linux in Debug and MT mode
    // there is an apparent bug that causes the above "switch" based
    // variant of this function to misbehave and crash with SEGV...
    TErrCode e = GetErrCode();
    if (e == eGet)           {return "eGet";}
    if (e == eWrite)         {return "eWrite";}
    if (e == eUnknownMember) {return "eUnknownMember";}
    return CException::GetErrCodeString();
#endif
}


END_NCBI_SCOPE

/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2006/12/04 14:53:29  gouriano
* Moved GetErrCodeString method into src
*
* Revision 1.17  2006/01/18 19:45:23  ssikorsk
* Added an extra argument to CException::x_Init
*
* Revision 1.16  2004/09/24 22:28:35  vakatov
* CInvalidChoiceSelection -- added pre-DIAG_COMPILE_INFO constructor
*
* Revision 1.15  2004/09/22 13:32:17  kononenk
* "Diagnostic Message Filtering" functionality added.
* Added function SetDiagFilter()
* Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
* Module, class and function attribute added to CNcbiDiag and CException
* Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
* 	CDiagCompileInfo + fixes on derived classes and their usage
* Macro NCBI_MODULE can be used to set default module name in cpp files
*
* Revision 1.14  2004/07/04 19:11:24  vakatov
* Do not use "throw()" specification after constructors and assignment
* operators of exception classes inherited from "std::exception" -- as it
* causes ICC 8.0 generated code to abort in Release mode.
*
* Revision 1.13  2004/05/17 21:03:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.12  2004/05/11 15:56:36  gouriano
* Change GetErrCode method prototype to return TErrCode - to be able to
* safely cast EErrCode to an eInvalid
*
* Revision 1.11  2003/10/27 19:18:03  grichenk
* Reformatted object stream error messages
*
* Revision 1.10  2003/03/11 17:59:39  gouriano
* reimplement CInvalidChoiceSelection exception
*
* Revision 1.9  2003/03/10 18:54:25  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.8  2002/12/23 18:59:52  dicuccio
* Fixed an 80-chr boundary violation.  Log to end.
*
* Revision 1.7  2002/12/23 18:41:19  dicuccio
* Added previously unimplemented class ctors / dtors (classes declared but not
* defined)
*
* Revision 1.6  2001/04/17 03:58:39  vakatov
* CSerialNotImplemented:: -- constructor and destructor added
*
* Revision 1.5  2001/01/05 20:10:50  vasilche
* CByteSource, CIStrBuffer, COStrBuffer, CLightString, CChecksum, CWeakMap
* were moved to util.
*
* Revision 1.4  2000/07/05 13:20:22  vasilche
* Fixed bug on 64 bit compilers.
*
* Revision 1.3  2000/07/03 20:47:22  vasilche
* Removed unused variables/functions.
*
* Revision 1.2  2000/07/03 18:42:43  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.1  2000/02/17 20:02:43  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* ===========================================================================
*/
