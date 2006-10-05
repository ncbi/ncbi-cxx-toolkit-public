#if defined(VARIANT__HPP)  &&  !defined(VARIANT__INL)
#define VARIANT__INL

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
const CChoiceTypeInfo* CVariantInfo::GetChoiceType(void) const
{
    return m_ChoiceType;
}

inline
CVariantInfo::EVariantType CVariantInfo::GetVariantType(void) const
{
    return m_VariantType;
}

inline
CVariantInfo* CVariantInfo::SetNoPrefix(void)
{
    GetId().SetNoPrefix();
    return this;
}

inline
CVariantInfo* CVariantInfo::SetNotag(void)
{
    GetId().SetNotag();
    return this;
}

inline
CVariantInfo* CVariantInfo::SetCompressed(void)
{
    GetId().SetCompressed();
    return this;
}

inline
bool CVariantInfo::IsInline(void) const
{
    return GetVariantType() == eInlineVariant;
}

inline
bool CVariantInfo::IsNonObjectPointer(void) const
{
    return GetVariantType() == eNonObjectPointerVariant;
}

inline
bool CVariantInfo::IsObjectPointer(void) const
{
    return GetVariantType() == eObjectPointerVariant;
}

inline
bool CVariantInfo::IsSubClass(void) const
{
    return GetVariantType() == eSubClassVariant;
}

inline
bool CVariantInfo::IsNotPointer(void) const
{
    return (GetVariantType() & ePointerFlag) == 0;
}

inline
bool CVariantInfo::IsPointer(void) const
{
    return (GetVariantType() & ePointerFlag) != 0;
}

inline
bool CVariantInfo::IsNotObject(void) const
{
    return (GetVariantType() & eObjectFlag) == 0;
}

inline
bool CVariantInfo::IsObject(void) const
{
    return (GetVariantType() & eObjectFlag) != 0;
}

inline
bool CVariantInfo::CanBeDelayed(void) const
{
    return m_DelayOffset != eNoOffset;
}

inline
CDelayBuffer& CVariantInfo::GetDelayBuffer(TObjectPtr object) const
{
    return CTypeConverter<CDelayBuffer>::Get(CRawPointer::Add(object, m_DelayOffset));
}

inline
const CDelayBuffer& CVariantInfo::GetDelayBuffer(TConstObjectPtr object) const
{
    return CTypeConverter<const CDelayBuffer>::Get(CRawPointer::Add(object, m_DelayOffset));
}

inline
TConstObjectPtr CVariantInfo::GetVariantPtr(TConstObjectPtr choicePtr) const
{
    return m_GetConstFunction(this, choicePtr);
}

inline
TObjectPtr CVariantInfo::GetVariantPtr(TObjectPtr choicePtr) const
{
    return m_GetFunction(this, choicePtr);
}

inline
void CVariantInfo::ReadVariant(CObjectIStream& stream,
                               TObjectPtr choicePtr) const
{
    m_ReadHookData.GetCurrentFunction()(stream, this, choicePtr);
}

inline
void CVariantInfo::WriteVariant(CObjectOStream& stream,
                                TConstObjectPtr choicePtr) const
{
    m_WriteHookData.GetCurrentFunction()(stream, this, choicePtr);
}

inline
void CVariantInfo::SkipVariant(CObjectIStream& stream) const
{
    m_SkipHookData.GetCurrentFunction()(stream, this);
}

inline
void CVariantInfo::CopyVariant(CObjectStreamCopier& stream) const
{
    m_CopyHookData.GetCurrentFunction()(stream, this);
}

inline
void CVariantInfo::DefaultReadVariant(CObjectIStream& stream,
                                      TObjectPtr choicePtr) const
{
    m_ReadHookData.GetDefaultFunction()(stream, this, choicePtr);
}

inline
void CVariantInfo::DefaultWriteVariant(CObjectOStream& stream,
                                TConstObjectPtr choicePtr) const
{
    m_WriteHookData.GetDefaultFunction()(stream, this, choicePtr);
}

inline
void CVariantInfo::DefaultSkipVariant(CObjectIStream& stream) const
{
    m_SkipHookData.GetDefaultFunction()(stream, this);
}

inline
void CVariantInfo::DefaultCopyVariant(CObjectStreamCopier& stream) const
{
    m_CopyHookData.GetDefaultFunction()(stream, this);
}

#endif /* def VARIANT__HPP  &&  ndef VARIANT__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2006/10/05 19:23:37  gouriano
* Moved from parent folder
*
* Revision 1.13  2006/01/19 18:22:34  gouriano
* Added possibility to save bit string data in compressed format
*
* Revision 1.12  2003/12/01 19:04:23  grichenk
* Moved Add and Sub from serialutil to ncbimisc, made them methods
* of CRawPointer class.
*
* Revision 1.11  2003/07/29 18:47:47  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.10  2002/12/23 18:38:52  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.9  2002/11/20 21:18:21  gouriano
* corrected SetNotag method
*
* Revision 1.8  2002/11/19 19:45:55  gouriano
* added SetNotag method
*
* Revision 1.7  2002/10/15 13:39:06  gouriano
* added "noprefix" flag
*
* Revision 1.6  2002/09/09 18:14:00  grichenk
* Added CObjectHookGuard class.
* Added methods to be used by hooks for data
* reading and skipping.
*
* Revision 1.5  2000/10/13 20:22:47  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.4  2000/09/29 16:18:15  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.3  2000/09/26 19:24:54  vasilche
* Added user interface for setting read/write/copy hooks.
*
* Revision 1.2  2000/09/26 17:38:08  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.1  2000/09/18 20:00:12  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/
