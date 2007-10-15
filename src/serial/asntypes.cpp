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
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#if HAVE_NCBI_C

#include <corelib/ncbiutil.hpp>
#include <serial/impl/asntypes.hpp>
#include <serial/impl/autoptrinfo.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objcopy.hpp>
#include <serial/impl/classinfob.hpp>
#include <serial/impl/typemap.hpp>
#include <serial/error_codes.hpp>
#include <asn.h>


#define NCBI_USE_ERRCODE_X   Serial_ASNTypes

BEGIN_NCBI_SCOPE

static inline
void* Alloc(size_t size)
{
    return NotNull(calloc(size, 1));
}

template<class T>
static inline
T* Alloc(T*& ptr)
{
    return ptr = static_cast<T*>(Alloc(sizeof(T)));
}

static CTypeInfoMap s_SequenceOfTypeInfo_map;

TTypeInfo CSequenceOfTypeInfo::GetTypeInfo(TTypeInfo base)
{
    return s_SequenceOfTypeInfo_map.GetTypeInfo(base, &CreateTypeInfo);
}

CTypeInfo* CSequenceOfTypeInfo::CreateTypeInfo(TTypeInfo base)
{
    return new CSequenceOfTypeInfo(base);
}

CSequenceOfTypeInfo::CSequenceOfTypeInfo(TTypeInfo type, bool randomOrder)
    : CParent(sizeof(TObjectType), type, randomOrder)
{
    InitSequenceOfTypeInfo();
}

CSequenceOfTypeInfo::CSequenceOfTypeInfo(const char* name,
                                         TTypeInfo type, bool randomOrder)
    : CParent(sizeof(TObjectType), name, type, randomOrder)
{
    InitSequenceOfTypeInfo();
}

CSequenceOfTypeInfo::CSequenceOfTypeInfo(const string& name,
                                         TTypeInfo type, bool randomOrder)
    : CParent(sizeof(TObjectType), name, type, randomOrder)
{
    InitSequenceOfTypeInfo();
}

static
size_t GetFirstItemOffset(const CItemsInfo& items)
{
    TPointerOffsetType offset = INT_MAX;
    for ( CItemsInfo::CIterator i(items); i.Valid(); ++i ) {
        const CItemInfo* itemInfo = items.GetItemInfo(i);
        offset = min(offset, itemInfo->GetOffset());
    }
    return offset;
}

class CSequenceOfTypeInfoFunctionsCI
{
public:
    typedef CContainerTypeInfo::CConstIterator TIterator; 

    static const CSequenceOfTypeInfo* GetType(const TIterator& iter)
        {
            return CTypeConverter<CSequenceOfTypeInfo>::SafeCast(iter.GetContainerType());
        }
    static bool InitIterator(TIterator& iter)
        {
            TObjectPtr nodePtr =
                GetType(iter)->FirstNode(iter.GetContainerPtr());
            iter.m_IteratorData = nodePtr;
            return nodePtr != 0;
        }
    static void ReleaseIterator(TIterator& )
        {
        }
    static void CopyIterator(TIterator& dst,
                             const TIterator& src)
        {
            dst.m_IteratorData = src.m_IteratorData;
        }
    static bool NextElement(TIterator& iter)
        {
            TObjectPtr nodePtr = GetType(iter)->NextNode(iter.m_IteratorData);
            iter.m_IteratorData = nodePtr;
            return nodePtr != 0;
        }
    static TConstObjectPtr GetElementPtr(const TIterator& iter)
        {
            return GetType(iter)->Data(iter.m_IteratorData);
        }
};

class CSequenceOfTypeInfoFunctionsI
{
public:
    typedef CContainerTypeInfo::CIterator TIterator; 

    static const CSequenceOfTypeInfo* GetType(const TIterator& iter)
        {
            return CTypeConverter<CSequenceOfTypeInfo>::SafeCast(iter.GetContainerType());
        }
    static bool InitIterator(TIterator& iter)
        {
            TObjectPtr* nodePtrPtr =
                &GetType(iter)->FirstNode(iter.GetContainerPtr());
            iter.m_IteratorData = nodePtrPtr;
            return *nodePtrPtr != 0;
        }
    static void ReleaseIterator(TIterator& )
        {
        }
    static void CopyIterator(TIterator& dst, const TIterator& src)
        {
            dst.m_IteratorData = src.m_IteratorData;
        }
    static bool NextElement(TIterator& iter)
        {
            TObjectPtr* nodePtrPtr =
                &GetType(iter)->NextNode(*(TObjectPtr*)iter.m_IteratorData);
            iter.m_IteratorData = nodePtrPtr;
            return *nodePtrPtr != 0;
        }
    static TObjectPtr GetElementPtr(const TIterator& iter)
        {
            return GetType(iter)->Data(*(TObjectPtr*)iter.m_IteratorData);
        }
    static bool EraseElement(TIterator& iter)
        {
            const CSequenceOfTypeInfo* type = GetType(iter);

            TObjectPtr* nodePtrPtr = (TObjectPtr*)iter.m_IteratorData;
            TObjectPtr nodePtr = *nodePtrPtr;
            TObjectPtr nextNodePtr = type->NextNode(nodePtr);
            *nodePtrPtr = nextNodePtr;
            type->DeleteNode(nodePtr);
            return nextNodePtr != 0;
        }
    static void EraseAllElements(TIterator& iter)
        {
            const CSequenceOfTypeInfo* type = GetType(iter);

            TObjectPtr* nodePtrPtr = (TObjectPtr*)iter.m_IteratorData;
            TObjectPtr nodePtr = *nodePtrPtr;
            *nodePtrPtr = 0;
            while ( nodePtr ) {
                TObjectPtr nextNodePtr = type->NextNode(nodePtr);
                type->DeleteNode(nodePtr);
                nodePtr = nextNodePtr;
            }
        }
};

class CSequenceOfTypeInfoFunctions
{
public:
    static void ReadSequence(CObjectIStream& in,
                             TTypeInfo containerType,
                             TObjectPtr containerPtr)
        {
            const CSequenceOfTypeInfo* seqType =
                CTypeConverter<CSequenceOfTypeInfo>::SafeCast(containerType);

            BEGIN_OBJECT_FRAME_OF2(in, eFrameArray, seqType);
            in.BeginContainer(seqType);

            TTypeInfo elementType = seqType->GetElementType();
            BEGIN_OBJECT_FRAME_OF2(in, eFrameArrayElement, elementType);

            TObjectPtr* nextNodePtr = &seqType->FirstNode(containerPtr);

            while ( in.BeginContainerElement(elementType) ) {
                // get current node pointer
                TObjectPtr node = *nextNodePtr;
        
                // create node
                _ASSERT(!node);
                node = *nextNodePtr = seqType->CreateNode();

                // read node data
                in.ReadObject(seqType->Data(node), elementType);

                // save next node for next read
                nextNodePtr = &seqType->NextNode(node);
        
                in.EndContainerElement();
            }

            END_OBJECT_FRAME_OF(in);

            in.EndContainer();
            END_OBJECT_FRAME_OF(in);
        }
};

void CSequenceOfTypeInfo::InitSequenceOfTypeInfo(void)
{
    TTypeInfo type = GetElementType();
    const CAutoPointerTypeInfo* ptrInfo =
        dynamic_cast<const CAutoPointerTypeInfo*>(type);
    if ( ptrInfo != 0 ) {
        // data type is auto_ptr
        TTypeInfo asnType = ptrInfo->GetPointedType();
        if ( asnType->GetTypeFamily() == eTypeFamilyChoice ) {
            // CHOICE
            SetChoiceNext();
            m_ElementType = asnType;
        }
        else if ( asnType->GetTypeFamily() == eTypeFamilyClass ) {
            // user types
            const CClassTypeInfo* classType =
                CTypeConverter<CClassTypeInfo>::SafeCast(asnType);
            if ( GetFirstItemOffset(classType->GetItems()) < sizeof(void*) ) {
                CNcbiOstrstream msg;
                msg << "CSequenceOfTypeInfo: incompatible type: " <<
                    type->GetName() << ": " << typeid(*type).name() <<
                    " size: " << type->GetSize();
                NCBI_THROW(CSerialException,eInvalidData, CNcbiOstrstreamToString(msg));
            }
            m_NextOffset = 0;
            m_DataOffset = 0;
            m_ElementType = asnType;
        }
        else if ( asnType->GetSize() <= sizeof(dataval) ) {
            // standard types and SET/SEQUENCE OF
            SetValNodeNext();
            m_ElementType = asnType;
        }
        else {
/*
            _ASSERT(type->GetSize() <= sizeof(dataval));
            SetValNodeNext();
*/
            CNcbiOstrstream msg;
            msg << "CSequenceOfTypeInfo: incompatible type: " <<
                type->GetName() << ": " << typeid(*type).name() <<
                " size: " << type->GetSize();
            NCBI_THROW(CSerialException,eInvalidData, CNcbiOstrstreamToString(msg));
        }
    }
    else if ( type->GetSize() <= sizeof(dataval) ) {
        // SEQUENCE OF, SET OF or primitive types
        SetValNodeNext();
    }
    else {
        CNcbiOstrstream msg;
        msg << "CSequenceOfTypeInfo: incompatible type: " <<
            type->GetName() << ": " << typeid(*type).name() <<
            " size: " << type->GetSize();
        NCBI_THROW(CSerialException,eInvalidData, CNcbiOstrstreamToString(msg));
    }

    {
        typedef CSequenceOfTypeInfoFunctions TFunc;
        SetReadFunction(&TFunc::ReadSequence);
    }
    {
        typedef CSequenceOfTypeInfoFunctionsCI TFunc;
        SetConstIteratorFunctions(&TFunc::InitIterator, &TFunc::ReleaseIterator,
                                  &TFunc::CopyIterator, &TFunc::NextElement,
                                  &TFunc::GetElementPtr);
    }
    {
        typedef CSequenceOfTypeInfoFunctionsI TFunc;
        SetIteratorFunctions(&TFunc::InitIterator, &TFunc::ReleaseIterator,
                             &TFunc::CopyIterator, &TFunc::NextElement,
                             &TFunc::GetElementPtr,
                             &TFunc::EraseElement, &TFunc::EraseAllElements);
    }
}

void CSequenceOfTypeInfo::SetChoiceNext(void)
{
    m_NextOffset = offsetof(valnode, next);
    m_DataOffset = 0;
}

void CSequenceOfTypeInfo::SetValNodeNext(void)
{
    m_NextOffset = offsetof(valnode, next);
    m_DataOffset = offsetof(valnode, data);
}

TObjectPtr CSequenceOfTypeInfo::CreateNode(void) const
{
    if ( m_DataOffset == 0 ) {
        _ASSERT(m_NextOffset == 0 || m_NextOffset == offsetof(valnode, next));
        return GetElementType()->Create();
    }
    else {
        _ASSERT(m_NextOffset == offsetof(valnode, next));
        _ASSERT(m_DataOffset == offsetof(valnode, data));
        return Alloc(sizeof(valnode));
    }
}

void CSequenceOfTypeInfo::DeleteNode(TObjectPtr node) const
{
    if ( m_DataOffset == 0 ) {
        _ASSERT(m_NextOffset == 0 || m_NextOffset == offsetof(valnode, next));
        GetElementType()->Delete(node);
    }
    else {
        _ASSERT(m_NextOffset == offsetof(valnode, next));
        _ASSERT(m_DataOffset == offsetof(valnode, data));
        Free(node);
    }
}

bool CSequenceOfTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return FirstNode(object) == 0;
}

void CSequenceOfTypeInfo::SetDefault(TObjectPtr dst) const
{
    FirstNode(dst) = 0;
}

void CSequenceOfTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src,
                                 ESerialRecursionMode how) const
{
    src = FirstNode(src);
    if ( src == 0 ) {
        FirstNode(dst) = 0;
        return;
    }

    TTypeInfo dataType = GetElementType();
    dst = FirstNode(dst) = CreateNode();
    dataType->Assign(Data(dst), Data(src), how);
    while ( (src = NextNode(src)) != 0 ) {
        dst = NextNode(dst) = CreateNode();
        dataType->Assign(Data(dst), Data(src), how);
    }
}

static CTypeInfoMap s_SetOfTypeInfo_map;

TTypeInfo CSetOfTypeInfo::GetTypeInfo(TTypeInfo base)
{
    return s_SetOfTypeInfo_map.GetTypeInfo(base, &CreateTypeInfo);
}

CTypeInfo* CSetOfTypeInfo::CreateTypeInfo(TTypeInfo base)
{
    return new CSetOfTypeInfo(base);
}

CSetOfTypeInfo::CSetOfTypeInfo(TTypeInfo type)
    : CParent(type, true)
{
}

CSetOfTypeInfo::CSetOfTypeInfo(const char* name, TTypeInfo type)
    : CParent(name, type, true)
{
}

CSetOfTypeInfo::CSetOfTypeInfo(const string& name, TTypeInfo type)
    : CParent(name, type, true)
{
}

COctetStringTypeInfo::COctetStringTypeInfo(void)
    : CParent(sizeof(TObjectType), ePrimitiveValueOctetString)
{
    SetReadFunction(&ReadOctetString);
    SetWriteFunction(&WriteOctetString);
    SetCopyFunction(&CopyOctetString);
    SetSkipFunction(&SkipOctetString);
}

bool COctetStringTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return Get(object)->totlen == 0;
}

bool COctetStringTypeInfo::Equals(TConstObjectPtr obj1, TConstObjectPtr obj2,
                                  ESerialRecursionMode) const
{
    bytestore* bs1 = Get(obj1);
    bytestore* bs2 = Get(obj2);
    if ( bs1 == 0 || bs2 == 0 )
        return bs1 == bs2;

    Int4 len = BSLen(bs1);
    if ( len != BSLen(bs2) )
        return false;
    
    BSSeek(bs1, 0, SEEK_SET);
    BSSeek(bs2, 0, SEEK_SET);
    char buff1[1024], buff2[1024];
    while ( len > 0 ) {
        Int4 chunk = Int4(sizeof(buff1));
        if ( chunk > len )
            chunk = len;
        BSRead(bs1, buff1, chunk);
        BSRead(bs2, buff2, chunk);
        if ( memcmp(buff1, buff2, chunk) != 0 )
            return false;
        len -= chunk;
    }
    return true;
}

void COctetStringTypeInfo::SetDefault(TObjectPtr dst) const
{
    BSFree(Get(dst));
    Get(dst) = BSNew(0);
}

void COctetStringTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src,
                                  ESerialRecursionMode) const
{
    if ( Get(src) == 0 ) {
        NCBI_THROW(CSerialException,eInvalidData, "null bytestore pointer");
    }
    BSFree(Get(dst));
    Get(dst) = BSDup(Get(src));
}

void COctetStringTypeInfo::ReadOctetString(CObjectIStream& in,
                                           TTypeInfo /*objectType*/,
                                           TObjectPtr objectPtr)
{
    CObjectIStream::ByteBlock block(in);
    BSFree(Get(objectPtr));
    char buffer[1024];
    Int4 count = Int4(block.Read(buffer, sizeof(buffer)));
    bytestore* bs = Get(objectPtr) = BSNew(count);
    BSWrite(bs, buffer, count);
    while ( (count = Int4(block.Read(buffer, sizeof(buffer)))) != 0 ) {
        BSWrite(bs, buffer, count);
    }
    block.End();
}

void COctetStringTypeInfo::WriteOctetString(CObjectOStream& out,
                                            TTypeInfo /*objectType*/,
                                            TConstObjectPtr objectPtr)
{
    bytestore* bs = const_cast<bytestore*>(Get(objectPtr));
    if ( bs == 0 )
        out.ThrowError(out.fIllegalCall, "null bytestore pointer");
    Int4 len = BSLen(bs);
    CObjectOStream::ByteBlock block(out, len);
    BSSeek(bs, 0, SEEK_SET);
    char buff[1024];
    while ( len > 0 ) {
        Int4 chunk = Int4(sizeof(buff));
        if ( chunk > len )
            chunk = len;
        BSRead(bs, buff, chunk);
        block.Write(buff, chunk);
        len -= chunk;
    }
    block.End();
}

void COctetStringTypeInfo::CopyOctetString(CObjectStreamCopier& copier,
                                           TTypeInfo /*objectType*/)
{
    copier.CopyByteBlock();
}

void COctetStringTypeInfo::SkipOctetString(CObjectIStream& in,
                                           TTypeInfo /*objectType*/)
{
    in.SkipByteBlock();
}

void COctetStringTypeInfo::GetValueOctetString(TConstObjectPtr objectPtr,
                                               vector<char>& value) const
{
    bytestore* bs = const_cast<bytestore*>(Get(objectPtr));
    if ( bs == 0 ) {
        NCBI_THROW(CSerialException,eInvalidData, "null bytestore pointer");
    }
    Int4 len = BSLen(bs);
    value.resize(len);
    BSSeek(bs, 0, SEEK_SET);
    BSRead(bs, &value.front(), len);
}

void COctetStringTypeInfo::SetValueOctetString(TObjectPtr objectPtr,
                                               const vector<char>& value) const
{
    Int4 count = Int4(value.size());
    bytestore* bs = Get(objectPtr) = BSNew(count);
    BSWrite(bs, const_cast<char*>(&value.front()), count);
}

TTypeInfo COctetStringTypeInfo::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = 0;
    if ( !typeInfo )
        typeInfo = new COctetStringTypeInfo();
    return typeInfo;
}

COldAsnTypeInfo::COldAsnTypeInfo(const char* name,
                                 TAsnNewProc newProc,
                                 TAsnFreeProc freeProc,
                                 TAsnReadProc readProc,
                                 TAsnWriteProc writeProc)
    : CParent(sizeof(TObjectType), name, ePrimitiveValueSpecial),
      m_NewProc(newProc), m_FreeProc(freeProc),
      m_ReadProc(readProc), m_WriteProc(writeProc)
{
    SetReadFunction(&ReadOldAsnStruct);
    SetWriteFunction(&WriteOldAsnStruct);
}

COldAsnTypeInfo::COldAsnTypeInfo(const string& name,
                                 TAsnNewProc newProc,
                                 TAsnFreeProc freeProc,
                                 TAsnReadProc readProc,
                                 TAsnWriteProc writeProc)
    : CParent(sizeof(TObjectType), name, ePrimitiveValueSpecial),
      m_NewProc(newProc), m_FreeProc(freeProc),
      m_ReadProc(readProc), m_WriteProc(writeProc)
{
    SetReadFunction(&ReadOldAsnStruct);
    SetWriteFunction(&WriteOldAsnStruct);
}

bool COldAsnTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return Get(object) == 0;
}

bool COldAsnTypeInfo::Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                             ESerialRecursionMode) const
{
    return Get(object1) == 0 && Get(object2) == 0;
}

void COldAsnTypeInfo::SetDefault(TObjectPtr dst) const
{
    Get(dst) = 0;
}

void COldAsnTypeInfo::Assign(TObjectPtr , TConstObjectPtr,
                             ESerialRecursionMode ) const
{
    NCBI_THROW(CSerialException,eInvalidData, "cannot assign non default value");
}

void COldAsnTypeInfo::ReadOldAsnStruct(CObjectIStream& in,
                                       TTypeInfo objectType,
                                       TObjectPtr objectPtr)
{
    const COldAsnTypeInfo* oldAsnType =
        CTypeConverter<COldAsnTypeInfo>::SafeCast(objectType);

    CObjectIStream::AsnIo io(in, oldAsnType->GetName());
    if ( (Get(objectPtr) = oldAsnType->m_ReadProc(io, 0)) == 0 )
        in.ThrowError(in.fFail, "read fault");
    io.End();
}

void COldAsnTypeInfo::WriteOldAsnStruct(CObjectOStream& out,
                                        TTypeInfo objectType,
                                        TConstObjectPtr objectPtr)
{
    const COldAsnTypeInfo* oldAsnType =
        CTypeConverter<COldAsnTypeInfo>::SafeCast(objectType);

    CObjectOStream::AsnIo io(out, oldAsnType->GetName());
    if ( !oldAsnType->m_WriteProc(Get(objectPtr), io, 0) )
        out.ThrowError(out.fFail, "write fault");
    io.End();
}

// CObjectOStream, CObjectIStream, and corresponding AsnIo

extern "C" {
    Int2 LIBCALLBACK WriteAsn(Pointer object, CharPtr data, Uint2 length)
    {
        if ( !object || !data )
            return -1;
    
        static_cast<CObjectOStream::AsnIo*>(object)->Write(data, length);
        return length;
    }

    Int2 LIBCALLBACK ReadAsn(Pointer object, CharPtr data, Uint2 length)
    {
        if ( !object || !data )
            return -1;
        CObjectIStream::AsnIo* asnio =
            static_cast<CObjectIStream::AsnIo*>(object);
        return Uint2(asnio->Read(data, length));
    }
}

CObjectOStream::AsnIo::AsnIo(CObjectOStream& out, const string& rootTypeName)
    : m_Stream(out), m_RootTypeName(rootTypeName), m_Ended(false), m_Count(0)
{
    Int1 flags = ASNIO_OUT;
    ESerialDataFormat format = out.GetDataFormat();
    if ( format == eSerial_AsnText )
        flags |= ASNIO_TEXT;
    else if ( format == eSerial_AsnBinary )
        flags |= ASNIO_BIN;
    else
        out.ThrowError(out.fIllegalCall,
                       "incompatible stream format - must be ASN.1 (text or binary)");
    m_AsnIo = AsnIoNew(flags, 0, this, 0, WriteAsn);
    if ( format == eSerial_AsnText ) {
        // adjust indent level and buffer
        size_t indent = out.m_Output.GetIndentLevel();
        m_AsnIo->indent_level = Int1(indent);
        size_t max_indent = m_AsnIo->max_indent;
        if ( indent >= max_indent ) {
            Boolean* tmp = m_AsnIo->first;
            m_AsnIo->first = (BoolPtr) MemNew((sizeof(Boolean) * (indent + 10)));
            MemCopy(m_AsnIo->first, tmp, (size_t)(sizeof(Boolean) * max_indent));
            MemFree(tmp);
            m_AsnIo->max_indent = Int1(indent);
        }
    }
}

void CObjectOStream::AsnIo::End(void)
{
    _ASSERT(!m_Ended);
    if ( GetStream().InGoodState() ) {
        AsnIoClose(*this);
        m_Ended = true;
    }
}

CObjectOStream::AsnIo::~AsnIo(void)
{
    if ( !m_Ended ) {
        try {
            GetStream().Unended("AsnIo write error");
        }
        catch (...) {
            ERR_POST_X(1, "AsnIo write error");
        }
    }
}

CObjectOStream& CObjectOStream::AsnIo::GetStream(void) const
{
    return m_Stream;
}

CObjectOStream::AsnIo::operator asnio*(void)
{
    return m_AsnIo;
}

asnio* CObjectOStream::AsnIo::operator->(void)
{
    return m_AsnIo;
}

const string& CObjectOStream::AsnIo::GetRootTypeName(void) const
{
    return m_RootTypeName;
}

void CObjectOStream::AsnIo::Write(const char* data, size_t length)
{
    if ( GetStream().GetDataFormat() == eSerial_AsnText ) {
        if ( m_Count == 0 ) {
            // dirty hack to skip structure name with '::='
            const char* p = (const char*)memchr(data, ':', length);
            if ( p && p[1] == ':' && p[2] == '=' ) {
                // check type name
                const char* beg = data;
                const char* end = p;
                while ( beg < end && isspace((unsigned char) beg[0]) )
                    beg++;
                while ( end > beg && isspace((unsigned char) end[-1]) )
                    end--;
                if ( string(beg, end) != GetRootTypeName() ) {
                    ERR_POST_X(2, "AsnWrite: wrong ASN.1 type name: is \""
                               << string(beg, end) << "\", must be \""
                               << GetRootTypeName() << "\"");
                }
                // skip header
                size_t skip = p + 3 - data;
                _TRACE(Warning <<
                       "AsnWrite: skipping \"" << string(data, skip) << "\"");
                data += skip;
                length -= skip;
            }
            else {
                ERR_POST_X(3, "AsnWrite: no \"Asn-Type ::=\" header  (data=\""
                               << data << "\")");
            }
            m_Count = 1;
        }
        GetStream().m_Output.PutString(data, length);
    }
    else {
        if ( length == 0 )
            return;
        CObjectOStreamAsnBinary& out =
            static_cast<CObjectOStreamAsnBinary&>(GetStream());
#if CHECK_STREAM_INTEGRITY
        _TRACE("WriteBytes: " << length);
        if ( out.m_CurrentTagState != out.eTagStart )
            out.ThrowError(out.fIllegalCall,
                string("AsnWrite only allowed at tag start: data= ")+data);
        if ( out.m_CurrentTagLimit != 0 && out.m_CurrentPosition + length > out.m_CurrentTagLimit )
            out.ThrowError(out.fIllegalCall,
                string("tag DATA overflow: data= ")+data);
        out.m_CurrentPosition += CNcbiStreamoff(length);
#endif
        out.m_Output.PutString(data, length);
    }
}

CObjectIStream::AsnIo::AsnIo(CObjectIStream& in, const string& rootTypeName)
    : m_Stream(in), m_Ended(false),
      m_RootTypeName(rootTypeName), m_Count(0)
{
    Int1 flags = ASNIO_IN;
    ESerialDataFormat format = in.GetDataFormat();
    if ( format == eSerial_AsnText )
        flags |= ASNIO_TEXT;
    else if ( format == eSerial_AsnBinary )
        flags |= ASNIO_BIN;
    else
        in.ThrowError(in.fIllegalCall,
            "incompatible stream format - must be ASN.1 (text or binary)");
    m_AsnIo = AsnIoNew(flags, 0, this, ReadAsn, 0);
    if ( format == eSerial_AsnBinary ) {
#if CHECK_STREAM_INTEGRITY
        CObjectIStreamAsnBinary& sin =
            static_cast<CObjectIStreamAsnBinary&>(in);
        if ( sin.m_CurrentTagState != sin.eTagStart ) {
            in.ThrowError(in.fIllegalCall,
                string("double tag read: rootTypeName= ")+ rootTypeName);
        }
#endif
    }
}

void CObjectIStream::AsnIo::End(void)
{
    _ASSERT(!m_Ended);
    if ( GetStream().InGoodState() ) {
        AsnIoClose(*this);
        m_Ended = true;
    }
}

CObjectIStream::AsnIo::~AsnIo(void)
{
    if ( !m_Ended )
        GetStream().Unended("AsnIo read error");
}

CObjectIStream& CObjectIStream::AsnIo::GetStream(void) const
{
    return m_Stream;
}

CObjectIStream::AsnIo::operator asnio*(void)
{
    return m_AsnIo;
}

asnio* CObjectIStream::AsnIo::operator->(void)
{
    return m_AsnIo;
}

const string& CObjectIStream::AsnIo::GetRootTypeName(void) const
{
    return m_RootTypeName;
}

size_t CObjectIStream::AsnIo::Read(char* data, size_t length)
{
    if ( GetStream().GetDataFormat() == eSerial_AsnText ) {
        size_t count = 0;
        if ( m_Count == 0 ) {
            // dirty hack to add structure name with '::='
            const string& name = GetRootTypeName();
            SIZE_TYPE nameLength = name.size();
            count = nameLength + 3;
            if ( length < count ) {
                GetStream().ThrowError(GetStream().fFail,
                    string("buffer too small to put structure name in: name= ")
                    + name);
            }
            memcpy(data, name.data(), nameLength);
            data[nameLength] = ':';
            data[nameLength + 1] = ':';
            data[nameLength + 2] = '=';
            data += count;
            length -= count;
            m_Count = 1;
        }
        return count + GetStream().m_Input.ReadLine(data, length);
    }
    else {
        *data = GetStream().m_Input.GetChar();
        return 1;
    }
}

END_NCBI_SCOPE

#endif
