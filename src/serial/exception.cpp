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
    const char* file,int line,
    size_t currentIndex, size_t mustBeIndex,
    const char* const names[], size_t namesCount) throw()
        : CSerialException(file, line, 0,
          (CSerialException::EErrCode) CException::eInvalid,"")
{
    x_Init(file,line,
           string("Invalid choice selection: ")+
           GetName(currentIndex, names, namesCount)+". "
           "Expected: "+
           GetName(mustBeIndex, names, namesCount),0);
    x_InitErrCode((CException::EErrCode)(CInvalidChoiceSelection::eFail));
}

CInvalidChoiceSelection::CInvalidChoiceSelection(
    size_t currentIndex, size_t mustBeIndex,
    const char* const names[], size_t namesCount) throw()
        : CSerialException("unknown", 0, 0,
          (CSerialException::EErrCode) CException::eInvalid,"")
{
    x_Init("unknown", 0,
           string("Invalid choice selection: ")+
           GetName(currentIndex, names, namesCount)+". "
           "Expected: "+
           GetName(mustBeIndex, names, namesCount),0);
    x_InitErrCode((CException::EErrCode)(CInvalidChoiceSelection::eFail));
}

CInvalidChoiceSelection::CInvalidChoiceSelection(
    const CInvalidChoiceSelection& other) throw()
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

CInvalidChoiceSelection::CInvalidChoiceSelection(void) throw()
{
}

const CException* CInvalidChoiceSelection::x_Clone(void) const
{
    return new CInvalidChoiceSelection(*this);
}


END_NCBI_SCOPE

/* ---------------------------------------------------------------------------
* $Log$
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
