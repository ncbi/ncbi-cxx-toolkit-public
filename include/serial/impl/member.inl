#if defined(MEMBER__HPP)  &&  !defined(MEMBER__INL)
#define MEMBER__INL

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
const CClassTypeInfoBase* CMemberInfo::GetClassType(void) const
{
    return m_ClassType;
}

inline
bool CMemberInfo::Optional(void) const
{
//    return m_Optional;
// this is to provide backward compatibility:
//for ASN objects, having default means being optional
//for XML objects, these two properties are unrelated
// after a while, when generated code (ie, datatool) is truly in sync with serial library,
// this can be reversed to simple check above
    return GetId().HaveNoPrefix() ? m_Optional : (m_Optional || m_Default);
}
inline
bool CMemberInfo::Nillable(void) const
{
    return GetId().IsNillable();
}

inline
TConstObjectPtr CMemberInfo::GetDefault(void) const
{
    return m_Default;
}

inline
bool CMemberInfo::HaveSetFlag(void) const
{
    return m_SetFlagOffset != eNoOffset;
}

inline
bool CMemberInfo::CanBeDelayed(void) const
{
    return m_DelayOffset != eNoOffset;
}

inline
CDelayBuffer& CMemberInfo::GetDelayBuffer(TObjectPtr object) const
{
    return CTypeConverter<CDelayBuffer>::Get(CRawPointer::Add(object, m_DelayOffset));
}

inline
const CDelayBuffer& CMemberInfo::GetDelayBuffer(TConstObjectPtr object) const
{
    return CTypeConverter<const CDelayBuffer>::Get(CRawPointer::Add(object, m_DelayOffset));
}

inline
TConstObjectPtr CMemberInfo::GetMemberPtr(TConstObjectPtr classPtr) const
{
    return m_GetConstFunction(this, classPtr);
}

inline
TObjectPtr CMemberInfo::GetMemberPtr(TObjectPtr classPtr) const
{
    return m_GetFunction(this, classPtr);
}

inline
void CMemberInfo::ReadMember(CObjectIStream& stream,
                             TObjectPtr classPtr) const
{
    m_ReadHookData.GetCurrentFunction().m_Main(stream, this, classPtr);
}

inline
void CMemberInfo::ReadMissingMember(CObjectIStream& stream,
                                    TObjectPtr classPtr) const
{
    m_ReadHookData.GetCurrentFunction().m_Missing(stream, this, classPtr);
}

inline
void CMemberInfo::WriteMember(CObjectOStream& stream,
                              TConstObjectPtr classPtr) const
{
    m_WriteHookData.GetCurrentFunction()(stream, this, classPtr);
}

inline
void CMemberInfo::SkipMember(CObjectIStream& stream) const
{
    m_SkipHookData.GetCurrentFunction().m_Main(stream, this);
}

inline
void CMemberInfo::SkipMissingMember(CObjectIStream& stream) const
{
    m_SkipHookData.GetCurrentFunction().m_Missing(stream, this);
}

inline
void CMemberInfo::CopyMember(CObjectStreamCopier& stream) const
{
    m_CopyHookData.GetCurrentFunction().m_Main(stream, this);
}

inline
void CMemberInfo::CopyMissingMember(CObjectStreamCopier& stream) const
{
    m_CopyHookData.GetCurrentFunction().m_Missing(stream, this);
}

inline
void CMemberInfo::DefaultReadMember(CObjectIStream& stream,
                                    TObjectPtr classPtr) const
{
    m_ReadHookData.GetDefaultFunction().m_Main(stream, this, classPtr);
}

inline
void CMemberInfo::DefaultReadMissingMember(CObjectIStream& stream,
                                           TObjectPtr classPtr) const
{
    m_ReadHookData.GetDefaultFunction().m_Missing(stream, this, classPtr);
}

inline
void CMemberInfo::DefaultWriteMember(CObjectOStream& stream,
                                     TConstObjectPtr classPtr) const
{
    m_WriteHookData.GetDefaultFunction()(stream, this, classPtr);
}

inline
void CMemberInfo::DefaultSkipMember(CObjectIStream& stream) const
{
    m_SkipHookData.GetDefaultFunction().m_Main(stream, this);
}

inline
void CMemberInfo::DefaultSkipMissingMember(CObjectIStream& stream) const
{
    m_SkipHookData.GetDefaultFunction().m_Missing(stream, this);
}

inline
void CMemberInfo::DefaultCopyMember(CObjectStreamCopier& stream) const
{
    m_CopyHookData.GetDefaultFunction().m_Main(stream, this);
}

inline
void CMemberInfo::DefaultCopyMissingMember(CObjectStreamCopier& stream) const
{
    m_CopyHookData.GetDefaultFunction().m_Missing(stream, this);
}


inline
CMemberInfo::ESetFlag CMemberInfo::GetSetFlag(TConstObjectPtr object) const
{
    _ASSERT(HaveSetFlag());
    if (Uint4 mask = m_BitSetMask) {
        const Uint4* bitsPtr =
            CTypeConverter<Uint4>::SafeCast(CRawPointer::Add(object, m_SetFlagOffset));
        Uint4 res = (*bitsPtr & mask);
        if ( res ) res = res == mask? 3: 1;
        return ESetFlag(res);
    } else {
        return CTypeConverter<bool>::Get(CRawPointer::Add(object,
            m_SetFlagOffset)) ? eSetYes : eSetNo;
    }
}


inline
bool CMemberInfo::GetSetFlagNo(TConstObjectPtr object) const
{
    _ASSERT(HaveSetFlag());
    if (Uint4 mask = m_BitSetMask) {
        const Uint4* bitsPtr =
            CTypeConverter<Uint4>::SafeCast(CRawPointer::Add(object, m_SetFlagOffset));
        Uint4 res = (*bitsPtr & mask);
        return res == 0;
    } else {
        return !CTypeConverter<bool>::Get(CRawPointer::Add(object, m_SetFlagOffset));
    }
}

inline
bool CMemberInfo::GetSetFlagYes(TConstObjectPtr object) const
{
    _ASSERT(HaveSetFlag());
    if (Uint4 mask = m_BitSetMask) {
        const Uint4* bitsPtr =
            CTypeConverter<Uint4>::SafeCast(CRawPointer::Add(object, m_SetFlagOffset));
        Uint4 res = (*bitsPtr & mask);
        return res != 0;
    } else {
        return CTypeConverter<bool>::Get(CRawPointer::Add(object, m_SetFlagOffset));
    }
}


inline
void CMemberInfo::UpdateSetFlag(TObjectPtr object, ESetFlag value) const
{
    TPointerOffsetType setFlagOffset = m_SetFlagOffset;
    if ( setFlagOffset != eNoOffset ) {
        if (Uint4 mask = m_BitSetMask) {
            Uint4* bitsPtr =
                CTypeConverter<Uint4>::SafeCast(CRawPointer::Add(object, m_SetFlagOffset));
            Uint4 set = !value? 0: value == eSetYes? mask: mask&mask/2;
            Uint4& bits = *bitsPtr;
            bits = (bits & ~mask) | set;
        } else {
            CTypeConverter<bool>::Get(CRawPointer::Add(object, setFlagOffset)) = 
                (value != eSetNo);
        }
    }
}


inline
void CMemberInfo::UpdateSetFlag(TObjectPtr object, TConstObjectPtr src) const
{
    TPointerOffsetType setFlagOffset = m_SetFlagOffset;
    if ( setFlagOffset != eNoOffset ) {
        if (Uint4 mask = m_BitSetMask) {
            Uint4* bitsPtr =
                CTypeConverter<Uint4>::SafeCast(CRawPointer::Add(object, setFlagOffset));
            const Uint4* srcBitsPtr =
                CTypeConverter<Uint4>::SafeCast(CRawPointer::Add(src, setFlagOffset));
            Uint4& bits = *bitsPtr;
            bits = (bits & ~mask) | (*srcBitsPtr & mask);
        } else {
            CTypeConverter<bool>::Get(CRawPointer::Add(object, setFlagOffset)) = 
                CTypeConverter<bool>::Get(CRawPointer::Add(src, setFlagOffset));
        }
    }
}


inline
void CMemberInfo::UpdateSetFlagYes(TObjectPtr object) const
{
    TPointerOffsetType setFlagOffset = m_SetFlagOffset;
    if ( setFlagOffset != eNoOffset ) {
        if (Uint4 mask = m_BitSetMask) {
            Uint4* bitsPtr =
                CTypeConverter<Uint4>::SafeCast(CRawPointer::Add(object, m_SetFlagOffset));
            *bitsPtr |= mask;
        } else {
            CTypeConverter<bool>::Get(CRawPointer::Add(object, setFlagOffset))= true;
        }
    }
}

inline
void CMemberInfo::UpdateSetFlagMaybe(TObjectPtr object) const
{
    TPointerOffsetType setFlagOffset = m_SetFlagOffset;
    if ( setFlagOffset != eNoOffset ) {
        if (Uint4 mask = m_BitSetMask) {
            Uint4* bitsPtr =
                CTypeConverter<Uint4>::SafeCast(CRawPointer::Add(object, m_SetFlagOffset));
            *bitsPtr |= mask&mask/2;
        } else {
            CTypeConverter<bool>::Get(CRawPointer::Add(object, setFlagOffset))= true;
        }
    }
}

inline
bool CMemberInfo::UpdateSetFlagNo(TObjectPtr object) const
{
    TPointerOffsetType setFlagOffset = m_SetFlagOffset;
    if ( setFlagOffset != eNoOffset ) {
        if (Uint4 mask = m_BitSetMask) {
            Uint4* bitsPtr =
                CTypeConverter<Uint4>::SafeCast(CRawPointer::Add(object, m_SetFlagOffset));
            Uint4& bits = *bitsPtr;
            if ( bits & mask ) {
                bits &= ~mask;
                return true;
            }
        } else {
            bool& flag = CTypeConverter<bool>::Get(CRawPointer::Add(object, setFlagOffset));
            if ( flag ) {
                flag = false;
                return true;
            }
        }
    }
    return false;
}

#endif /* def MEMBER__HPP  &&  ndef MEMBER__INL */
