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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  1999/08/13 20:22:57  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.16  1999/08/13 15:53:49  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.15  1999/07/21 20:02:53  vasilche
* Added embedding of ASN.1 binary output from ToolKit to our binary format.
* Fixed bugs with storing pointers into binary ASN.1
*
* Revision 1.14  1999/07/21 18:05:09  vasilche
* Fixed OPTIONAL attribute for ASN.1 structures.
*
* Revision 1.13  1999/07/20 18:23:08  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.12  1999/07/19 15:50:30  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.11  1999/07/15 16:54:48  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.10  1999/07/13 20:54:05  vasilche
* Fixed minor bugs.
*
* Revision 1.9  1999/07/13 20:18:15  vasilche
* Changed types naming.
*
* Revision 1.8  1999/07/09 20:27:05  vasilche
* Fixed some bugs
*
* Revision 1.7  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.6  1999/07/07 19:59:03  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.5  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.4  1999/07/02 21:31:51  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.3  1999/07/01 17:55:25  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.2  1999/06/30 18:54:58  vasilche
* Fixed some errors under MSVS
*
* Revision 1.1  1999/06/30 16:04:46  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/asntypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <asn.h>

BEGIN_NCBI_SCOPE

class BytestorePtr {
public:
    BytestorePtr(const bytestore* bs)
        : unit(bs->chain), pos(0)
        {
        }

    bool End(void) const
        {
            return unit == 0;
        }

    void* Ptr(void) const
        {
            return static_cast<char*>(unit->str) + pos;
        }

    size_t Available(void) const
        {
            return unit->len - pos;
        }

    void Next(void)
        {
            _ASSERT(Available() == 0);
            unit = unit->next;
            pos = 0;
        }

    void operator+=(size_t skip)
        {
            _ASSERT(skip < Available());
            pos += skip;
        }

private:
    bsunit* unit;
    size_t pos;
};

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

#ifndef WINDOWS
static inline
bsunit* BSUnitNew(size_t size)
{
	bsunit* unit;
	Alloc(unit);
	if ( size ) {
		unit->len_avail = size;
		unit->str = static_cast<char*>(Alloc(size));
	}
	return unit;
}

static
bytestore* BSNew(size_t size)
{
	bytestore* bs;
	Alloc(bs)->chain = BSUnitNew(size);
	return bs;
}

static
void BSFree(bytestore* bs)
{
	if ( bs ) {
		bsunit* unit = bs->chain;
		while ( unit ) {
			free(unit->str);
			bsunit* next = unit->next;
			free(unit);
			unit = next;
		}
		free(bs);
	}
}

static
bytestore* BSDup(const bytestore* bs)
{
	bytestore* copy = BSNew(0);
	size_t totlen = copy->totlen = bs->totlen;
	bsunit** unitPtr = &copy->chain;
	for ( const bsunit* unit = bs->chain; unit;
          unit = unit->next, unitPtr = &(*unitPtr)->next ) {
		size_t len = unit->len;
		*unitPtr = BSUnitNew(len);
		(*unitPtr)->len = len;
		memcpy((*unitPtr)->str, unit->str, len);
		totlen -= len;
	}
	if ( totlen != 0 )
		THROW1_TRACE(runtime_error, "bad totlen in bytestore");
	return copy;
}

static
void BSWrite(bytestore* bs, char* data, size_t length)
{
	bsunit* unit = bs->curchain;
	if ( !unit ) {
		_ASSERT(bs->chain_offset == 0);
		_ASSERT(bs->totlen == 0);
		if ( length == 0 )
			return;
		unit = bs->chain;
		if ( !unit )
			unit = bs->chain = BSUnitNew(length);
		bs->curchain = unit;
	}
	while ( length > 0 ) {
		size_t currLen = unit->len;
		size_t count = min(length, unit->len_avail - currLen);
		if ( !count ) {
			_ASSERT(unit->next == 0);
			bs->curchain = unit = unit->next = BSUnitNew(length);
			bs->chain_offset += currLen;
			unit->len = length;
			memcpy(unit->str, data, length);
			bs->totlen += length;
			bs->seekptr += length;
			return;
		}
		unit->len = currLen + count;
		memcpy(static_cast<char*>(unit->str) + currLen, data, count);
		bs->totlen += count;
		bs->seekptr += count;
		data += count;
		length -= count;
	}
}
#endif

static const TConstObjectPtr zeroPointer = 0;

CTypeInfoMap<CSequenceOfTypeInfo> CSequenceOfTypeInfo::sm_Map;

CSequenceOfTypeInfo::CSequenceOfTypeInfo(TTypeInfo type)
    : CTypeInfo("SEQUENCE OF " + type->GetName()), m_DataType(type)
{
	Init();
}

CSequenceOfTypeInfo::CSequenceOfTypeInfo(const string& name, TTypeInfo type)
    : CTypeInfo(name), m_DataType(type)
{
	Init();
}

void CSequenceOfTypeInfo::Init(void)
{
	TTypeInfo type = m_DataType;
    _TRACE("SequenceOf(" << type->GetName() << ") " << typeid(*type).name());
    const CAutoPointerTypeInfo* ptrInfo =
        dynamic_cast<const CAutoPointerTypeInfo*>(type);
    if ( ptrInfo != 0 ) {
        TTypeInfo asnType = ptrInfo->GetDataTypeInfo();
        if ( dynamic_cast<const CChoiceTypeInfo*>(asnType) != 0 ) {
            // CHOICE
            _TRACE("SequenceOf(" << type->GetName() << ") AUTO CHOICE");
            SetChoiceNext();
            m_DataType = asnType;
        }
        else if ( asnType->GetSize() <= sizeof(dataval) ) {
            // statndard types and SET/SEQUENCE OF
            _TRACE("SequenceOf(" << type->GetName() << ") AUTO VALNODE");
            SetValNodeNext();
			m_DataType = asnType;
        }
		else {
            // user types
			_ASSERT(type->GetSize() <= sizeof(dataval));
            _TRACE("SequenceOf(" << type->GetName() << ") VALNODE");
            SetValNodeNext();
		}
    }
    else if ( type->GetSize() <= sizeof(dataval) ) {
        // SEQUENCE OF, SET OF or primitive types
        _TRACE("SequenceOf(" << type->GetName() << ") VALNODE");
        SetValNodeNext();
    }
	else {
		THROW1_TRACE(runtime_error,
			"CSequenceOfTypeInfo: incompatible type: " +
			type->GetName() + ": " + typeid(*type).name());
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

bool CSequenceOfTypeInfo::RandomOrder(void) const
{
    return false;
}

size_t CSequenceOfTypeInfo::GetSize(void) const
{
    return sizeof(void*);
}

TConstObjectPtr CSequenceOfTypeInfo::GetDefault(void) const
{
    return &zeroPointer;
}

TObjectPtr CSequenceOfTypeInfo::CreateData(void) const
{
	_ASSERT(m_NextOffset == offsetof(valnode, next));
    if ( m_DataOffset == 0 ) {
        return GetDataTypeInfo()->Create();
	}
    else {
		_ASSERT(m_DataOffset == offsetof(valnode, data));
        return Alloc(sizeof(valnode));
	}
}

bool CSequenceOfTypeInfo::Equals(TConstObjectPtr object1,
                                 TConstObjectPtr object2) const
{
    for ( object1 = First(object1), object2 = First(object2);
          object1 != 0; object1 = Next(object1), object2 = Next(object2) ) {
        if ( object2 == 0 )
            return false;
        if ( !GetDataTypeInfo()->Equals(Data(object1), Data(object2)) )
            return false;
    }
    return object2 == 0;
}

void CSequenceOfTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    src = First(src);
    if ( src == 0 ) {
        First(dst) = 0;
        return;
    }

    TTypeInfo dataType = GetDataTypeInfo();
    dst = First(dst) = CreateData();
    dataType->Assign(Data(dst), Data(src));
    while ( (src = Next(src)) != 0 ) {
        dst = Next(dst) = CreateData();
        dataType->Assign(Data(dst), Data(src));
    }
}

void CSequenceOfTypeInfo::CollectExternalObjects(COObjectList& l,
                                                 TConstObjectPtr object) const
{
    _TRACE("SequenceOf<" << GetDataTypeInfo()->GetName() << ">::Collect: " << unsigned(object));
    for ( object = First(object); object != 0; object = Next(object) ) {
        GetDataTypeInfo()->CollectObjects(l, Data(object));
    }
}

void CSequenceOfTypeInfo::WriteData(CObjectOStream& out,
                                    TConstObjectPtr object) const
{
    CObjectOStream::Block block(out, RandomOrder());
    for ( object = First(object); object != 0; object = Next(object) ) {
        block.Next();
        out.WriteExternalObject(Data(object), GetDataTypeInfo());
    }
}

void CSequenceOfTypeInfo::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
    _TRACE("SequenceOf<" << GetDataTypeInfo()->GetName() << ">::ReadData(" << unsigned(object) << ")");
    CObjectIStream::Block block(in, RandomOrder());
    if ( !block.Next() ) {
        First(object) = 0;
        return;
    }
    TTypeInfo dataType = GetDataTypeInfo();

    TObjectPtr next = First(object);
    if ( next == 0 ) {
        ERR_POST(Warning << "null sequence pointer"); 
        next = First(object) = CreateData();
        _TRACE("new " << dataType->GetName() << ": " << unsigned(next));
    }
    object = next;

    in.ReadExternalObject(Data(object), dataType);

    while ( block.Next() ) {

        next = Next(object);
        if ( next == 0 ) {
            ERR_POST(Warning << "null sequence pointer"); 
            next = Next(object) = CreateData();
            _TRACE("new " << dataType->GetName() << ": " << unsigned(next));
        }
        object = next;

        in.ReadExternalObject(Data(object), dataType);
    }
}

CTypeInfoMap<CSetOfTypeInfo> CSetOfTypeInfo::sm_Map;

CSetOfTypeInfo::CSetOfTypeInfo(TTypeInfo type)
    : CSequenceOfTypeInfo("SET OF " + type->GetName(), type)
{
}

CSetOfTypeInfo::CSetOfTypeInfo(const string& name, TTypeInfo type)
    : CSequenceOfTypeInfo(name, type)
{
}

bool CSetOfTypeInfo::RandomOrder(void) const
{
    return true;
}

#if 0 
CTypeInfoMap<CAsnPointerTypeInfo> CAsnPointerTypeInfo::sm_Map;

size_t CAsnPointerTypeInfo::GetSize(void) const
{
    return sizeof(void*);
}

TConstObjectPtr CAsnPointerTypeInfo::GetDefault(void) const
{
    return &zeroPointer;
}

bool CAsnPointerTypeInfo::Equals(TConstObjectPtr object1,
                             TConstObjectPtr object2) const
{
    object1 = Get(object1);
    object2 = Get(object2);
    if ( object1 == 0 || object2 == 0 )
        THROW1_TRACE(runtime_error, "null ASN struct pointer");
    return GetAsnTypeInfo()->Equals(object1, object2);
}

void CAsnPointerTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    src = Get(src);
    if ( src == 0 )
        THROW1_TRACE(runtime_error, "null ASN struct pointer");

    TTypeInfo asnType = GetAsnTypeInfo();
    TObjectPtr dstObj = Get(dst);
    if ( dstObj == 0 ) {
        _TRACE("null ASN struct pointer");
        dstObj = Get(dst) = asnType->Create();
        _TRACE("new " << asnType->GetName() << ": " << unsigned(dstObj));
    }
    dst = dstObj;

    asnType->Assign(dst, src);
}

void CAsnPointerTypeInfo::CollectExternalObjects(COObjectList& l,
                                                 TConstObjectPtr object) const
{
    _TRACE("AsnPointer<" << GetAsnTypeInfo()->GetName() << ">::Collect: " << unsigned(object));
    object = Get(object);
    if ( object == 0 )
        THROW1_TRACE(runtime_error, "null ASN struct pointer");
    GetAsnTypeInfo()->CollectObjects(l, object);
}

void CAsnPointerTypeInfo::WriteData(CObjectOStream& out,
                                    TConstObjectPtr object) const
{
    object = Get(object);
    if ( object == 0 )
        THROW1_TRACE(runtime_error, "null ASN struct pointer");
    out.WriteExternalObject(object, GetAsnTypeInfo());
}

void CAsnPointerTypeInfo::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
    TTypeInfo asnType = GetAsnTypeInfo();
    _TRACE("AsnPointer<" << asnType->GetName() << ">::ReadData(" << unsigned(object) << ")");

    TObjectPtr objData = Get(object);
    if ( objData == 0 ) {
        _TRACE("null ASN struct pointer");
        objData = Get(object) = asnType->Create();
        _TRACE("new " << asnType->GetName() << ": " << unsigned(objData));
    }
    object = objData;

    in.ReadExternalObject(object, asnType);
}
#endif

void CChoiceTypeInfo::AddVariant(const CMemberId& id,
                                 const CTypeRef& typeRef)
{
    m_Variants.AddMember(id);
    m_VariantTypes.push_back(typeRef);
}

void CChoiceTypeInfo::AddVariant(const string& name,
                                 const CTypeRef& typeRef)
{
    AddVariant(CMemberId(name), typeRef);
}

size_t CChoiceTypeInfo::GetSize(void) const
{
    return sizeof(valnode);
}

TObjectPtr CChoiceTypeInfo::Create(void) const
{
    return Alloc(sizeof(valnode));
}

bool CChoiceTypeInfo::Equals(TConstObjectPtr object1,
                             TConstObjectPtr object2) const
{
    const valnode* val1 = static_cast<const valnode*>(object1);
    const valnode* val2 = static_cast<const valnode*>(object2);
    TMemberIndex choice = val1->choice;
    if ( choice != val2->choice )
        return false;
    TMemberIndex index = choice - 1;
    if ( index >= 0 && index < GetVariantsCount() ) {
        return GetVariantTypeInfo(index)->Equals(&val1->data, &val2->data);
    }
    return choice == 0;
}

void CChoiceTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    valnode* valDst = static_cast<valnode*>(dst);
    const valnode* valSrc = static_cast<const valnode*>(src);
    TMemberIndex choice = valSrc->choice;
    valDst->choice = choice;
    TMemberIndex index = choice;
    if ( index >= 0 && index < GetVariantsCount() ) {
        GetVariantTypeInfo(index)->Assign(&valDst->data, &valSrc->data);
    }
    else {
        valDst->data.ptrvalue = 0;
    }
}

void CChoiceTypeInfo::CollectExternalObjects(COObjectList& l,
                                             TConstObjectPtr object) const
{
    _TRACE("Choice<" << GetName() << ">::Collect: " << unsigned(object));
    const valnode* node = static_cast<const valnode*>(object);
    TMemberIndex index = node->choice - 1;
    if ( index < 0 || index >= GetVariantsCount() ) {
        THROW1_TRACE(runtime_error,
                     "illegal choice value: " +
                     NStr::IntToString(node->choice));
    }
    GetVariantTypeInfo(index)->CollectExternalObjects(l, &node->data);
}

void CChoiceTypeInfo::WriteData(CObjectOStream& out,
                                TConstObjectPtr object) const
{
    const valnode* node = static_cast<const valnode*>(object);
    TMemberIndex index = node->choice - 1;
    if ( index < 0 || index >= GetVariantsCount() ) {
        THROW1_TRACE(runtime_error,
                     "illegal choice value: " +
                     NStr::IntToString(node->choice));
    }
    CObjectOStream::Member m(out, m_Variants.GetCompleteMemberId(index));
    GetVariantTypeInfo(index)->WriteData(out, &node->data);
}

void CChoiceTypeInfo::ReadData(CObjectIStream& in,
                               TObjectPtr object) const
{
    _TRACE("Choice<" << GetName() << ">::ReadData(" << unsigned(object) << ")");
    CObjectIStream::Member id(in);
    TMemberIndex index = m_Variants.FindMember(id);
    if ( index < 0 ) {
        THROW1_TRACE(runtime_error,
                     "illegal choice variant: " + id.ToString());
    }
    valnode* node = static_cast<valnode*>(object);
    node->choice = index + 1;
    GetVariantTypeInfo(index)->ReadData(in, &node->data);
}

COctetStringTypeInfo::COctetStringTypeInfo(void)
    : CTypeInfo("OCTET STRING")
{
}

size_t COctetStringTypeInfo::GetSize(void) const
{
	return sizeof(void*);
}

TConstObjectPtr COctetStringTypeInfo::GetDefault(void) const
{
	return &zeroPointer;
}

bool COctetStringTypeInfo::Equals(TConstObjectPtr obj1,
                                  TConstObjectPtr obj2) const
{
    bytestore* bs1 = Get(obj1);
    bytestore* bs2 = Get(obj2);
    if ( bs1 == 0 || bs2 == 0 )
		return bs1 == bs2;

    if ( bs1->totlen != bs2->totlen )
        return false;
    
    BytestorePtr p1(bs1), p2(bs2);
    while ( !p1.End() ) {
        size_t c1 = p1.Available();
        if ( c1 == 0 ) {
            p1.Next();
            continue;
        }
        size_t c2 = p2.Available();
        if ( c2 == 0 ) {
            p2.Next();
            continue;
        }
        size_t count = min(c1, c2);
        if ( memcmp(p1.Ptr(), p2.Ptr(), count) != 0 )
            return false;
        p1 += count;
        p2 += count;
    }
    _ASSERT(p2.End());
	return true;
}

void COctetStringTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
	if ( Get(src) == 0 )
		THROW1_TRACE(runtime_error, "null bytestore pointer");
	BSFree(Get(dst));
	Get(dst) = BSDup(Get(src));
}

void COctetStringTypeInfo::CollectExternalObjects(COObjectList& ,
                                                  TConstObjectPtr ) const
{
}

void COctetStringTypeInfo::WriteData(CObjectOStream& out,
                                     TConstObjectPtr object) const
{
	const bytestore* bs = Get(object);
	if ( bs == 0 )
		THROW1_TRACE(runtime_error, "null bytestore pointer");
	CObjectOStream::ByteBlock block(out, bs->totlen);
	for ( const bsunit* unit = bs->chain; unit; unit = unit->next ) {
		block.Write(unit->str, unit->len);
	}
}

void COctetStringTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
	CObjectIStream::ByteBlock block(in);
	BSFree(Get(object));
    if ( block.KnownLength() ) {
        size_t length = block.GetExpectedLength();
        bytestore* bs = Get(object) = BSNew(length);
        bsunit* unit = bs->chain;
        _ASSERT(unit != 0 && unit->len_avail >= length);
        if ( block.Read(unit->str, length) != length )
            THROW1_TRACE(runtime_error, "read fault");
        unit->len = length;
        bs->totlen = length;
        bs->curchain = unit;
    }
    else {
        // length is known -> copy via buffer
        char buffer[1024];
        size_t count = block.Read(buffer, sizeof(buffer));
        bytestore* bs = Get(object) = BSNew(count);
        BSWrite(bs, buffer, count);
        while ( (count = block.Read(buffer, sizeof(buffer))) != 0 ) {
            BSWrite(bs, buffer, count);
        }
    }
}

map<COldAsnTypeInfo::TNewProc, COldAsnTypeInfo*> COldAsnTypeInfo::m_Types;

COldAsnTypeInfo::COldAsnTypeInfo(TNewProc newProc, TFreeProc freeProc,
                                 TReadProc readProc, TWriteProc writeProc)
    : CTypeInfo("old ASN.1"),
      m_NewProc(newProc), m_FreeProc(freeProc),
      m_ReadProc(readProc), m_WriteProc(writeProc)
{
}

TTypeInfo COldAsnTypeInfo::GetTypeInfo(TNewProc newProc, TFreeProc freeProc,
                                       TReadProc readProc, TWriteProc writeProc)
{
    COldAsnTypeInfo*& info = m_Types[newProc];
    if ( info ) {
        if ( info->m_NewProc != newProc || info->m_FreeProc != freeProc ||
             info->m_ReadProc != readProc || info->m_WriteProc != writeProc )
            THROW1_TRACE(runtime_error, "changed ASN.1 procedures pointers");
    }
    else {
        info = new COldAsnTypeInfo(newProc, freeProc, readProc, writeProc);
    }
    return info;
}

size_t COldAsnTypeInfo::GetSize(void) const
{
    return sizeof(TObjectPtr);
}

TConstObjectPtr COldAsnTypeInfo::CreateDefault(void) const
{
    return &zeroPointer;
}

bool COldAsnTypeInfo::Equals(TConstObjectPtr object1,
                             TConstObjectPtr object2) const
{
    return Get(object1) == 0 && Get(object2) == 0;
}

void COldAsnTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    if ( src != GetDefault() )
        THROW1_TRACE(runtime_error, "cannot assign non default value");
    Get(dst) = m_NewProc();
}

void COldAsnTypeInfo::WriteData(CObjectOStream& out,
                                TConstObjectPtr object) const
{
    if ( !m_WriteProc(Get(object), CObjectOStream::AsnIo(out), 0) )
        THROW1_TRACE(runtime_error, "write fault");
}

void COldAsnTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    if ( (Get(object) = m_ReadProc(CObjectIStream::AsnIo(in), 0)) == 0 )
        THROW1_TRACE(runtime_error, "read fault");
}

END_NCBI_SCOPE
