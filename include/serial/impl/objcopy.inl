#if defined(OBJCOPY__HPP)  &&  !defined(OBJCOPY__INL)
#define OBJCOPY__INL

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*/

inline
CObjectIStream& CObjectStreamCopier::In(void) const
{
    return m_In;
}

inline
CObjectOStream& CObjectStreamCopier::Out(void) const
{
    return m_Out;
}

inline
void CObjectStreamCopier::CopyObject(TTypeInfo type)
{
    Out().CopyObject(type, *this);
}

inline
void CObjectStreamCopier::ThrowError1(const CDiagCompileInfo& diag_info,
                                      TFailFlags flags,
                                      const char* message)
{
    Out().SetFailFlagsNoError(CObjectOStream::fInvalidData);
    In().ThrowError1(diag_info, flags, message);
}

inline
void CObjectStreamCopier::ThrowError1(const CDiagCompileInfo& diag_info,
                                      TFailFlags flags,
                                      const string& message)
{
    Out().SetFailFlagsNoError(CObjectOStream::fInvalidData);
    In().ThrowError1(diag_info, flags, message);
}

inline
void CObjectStreamCopier::ExpectedMember(const CMemberInfo* memberInfo)
{
    bool was_set = (Out().GetFailFlags() & CObjectOStream::fInvalidData) != 0;
    Out().SetFailFlagsNoError(CObjectOStream::fInvalidData);
    if (!In().ExpectedMember(memberInfo) && !was_set) {
        Out().ClearFailFlags(CObjectOStream::fInvalidData);
    }
}

inline
void CObjectStreamCopier::DuplicatedMember(const CMemberInfo* memberInfo)
{
    Out().SetFailFlagsNoError(CObjectOStream::fInvalidData);
    In().DuplicatedMember(memberInfo);
}

inline
void CObjectStreamCopier::CopyExternalObject(TTypeInfo type)
{
    In().RegisterObject(type);
    Out().RegisterObject(type);
    CopyObject(type);
}

inline
void CObjectStreamCopier::CopyString(void)
{
    Out().CopyString(In());
}

inline
void CObjectStreamCopier::CopyStringStore(void)
{
    Out().CopyStringStore(In());
}

inline
void CObjectStreamCopier::CopyAnyContentObject(void)
{
    Out().CopyAnyContentObject(In());
}

inline
void CObjectStreamCopier::CopyNamedType(TTypeInfo namedTypeInfo,
                                        TTypeInfo objectType)
{
    Out().CopyNamedType(namedTypeInfo, objectType, *this);
}

inline
void CObjectStreamCopier::CopyContainer(const CContainerTypeInfo* cType)
{
    Out().CopyContainer(cType, *this);
}

inline
void CObjectStreamCopier::CopyClassRandom(const CClassTypeInfo* classType)
{
    Out().CopyClassRandom(classType, *this);
}

inline
void CObjectStreamCopier::CopyClassSequential(const CClassTypeInfo* classType)
{
    Out().CopyClassSequential(classType, *this);
}

inline
void CObjectStreamCopier::CopyChoice(const CChoiceTypeInfo* choiceType)
{
    Out().CopyChoice(choiceType, *this);
}

inline
void CObjectStreamCopier::CopyAlias(const CAliasTypeInfo* aliasType)
{
    Out().CopyAlias(aliasType, *this);
}

#endif /* def OBJCOPY__HPP  &&  ndef OBJCOPY__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2006/10/05 19:23:37  gouriano
* Moved from parent folder
*
* Revision 1.11  2004/09/22 13:32:17  kononenk
* "Diagnostic Message Filtering" functionality added.
* Added function SetDiagFilter()
* Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
* Module, class and function attribute added to CNcbiDiag and CException
* Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
* 	CDiagCompileInfo + fixes on derived classes and their usage
* Macro NCBI_MODULE can be used to set default module name in cpp files
*
* Revision 1.10  2003/10/21 21:08:45  grichenk
* Fixed aliases-related bug in XML stream
*
* Revision 1.9  2003/08/13 15:47:02  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.8  2003/03/10 18:52:37  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.7  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.6  2002/10/25 14:49:29  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.5  2001/05/17 14:57:00  lavr
* Typos corrected
*
* Revision 1.4  2000/10/20 15:51:23  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.3  2000/09/29 16:18:13  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.2  2000/09/18 20:00:04  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.1  2000/09/01 13:15:59  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* ===========================================================================
*/
