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
* Revision 1.44  2000/08/15 19:44:50  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.43  2000/07/03 18:42:45  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.42  2000/06/16 16:31:21  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.41  2000/06/07 19:46:00  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.40  2000/06/01 19:07:04  vasilche
* Added parsing of XML data.
*
* Revision 1.39  2000/05/24 20:08:48  vasilche
* Implemented XML dump.
*
* Revision 1.38  2000/05/09 16:38:39  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.37  2000/05/04 16:22:20  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.36  2000/04/28 16:58:13  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.35  2000/04/13 14:50:27  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.34  2000/04/06 16:11:00  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.33  2000/03/29 15:55:29  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.32  2000/02/17 20:02:44  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.31  2000/02/11 17:10:25  vasilche
* Optimized text parsing.
*
* Revision 1.30  2000/01/10 19:46:41  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.29  2000/01/05 19:43:56  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.28  1999/12/28 18:55:51  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.27  1999/12/20 15:29:35  vasilche
* Fixed bug with old ASN structures.
*
* Revision 1.26  1999/12/17 19:05:03  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.25  1999/10/04 16:22:18  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.24  1999/09/27 14:18:02  vasilche
* Fixed bug with overloaded construtors of Block.
*
* Revision 1.23  1999/09/24 18:19:19  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.22  1999/09/23 21:16:08  vasilche
* Removed dependance on asn.h
*
* Revision 1.21  1999/09/23 20:25:04  vasilche
* Added support HAVE_NCBI_C
*
* Revision 1.20  1999/09/23 18:57:00  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.19  1999/09/14 18:54:19  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.18  1999/08/16 16:08:31  vasilche
* Added ENUMERATED type.
*
* Revision 1.17  1999/08/13 20:22:58  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.16  1999/08/13 15:53:51  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.15  1999/07/22 17:33:55  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.14  1999/07/19 15:50:35  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.13  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.12  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.11  1999/07/02 21:31:58  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.10  1999/07/01 17:55:33  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:59  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:57  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/17 20:42:06  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.6  1999/06/16 20:35:33  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.5  1999/06/15 16:19:50  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.4  1999/06/10 21:06:48  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:27  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:47  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:54  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>
#include <serial/typeref.hpp>
#include <serial/objlist.hpp>
#include <serial/memberid.hpp>
#include <serial/typeinfo.hpp>
#include <serial/enumvalues.hpp>
#include <serial/memberlist.hpp>
#include <serial/bytesrc.hpp>
#include <serial/delaybuf.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/continfo.hpp>
#include <serial/objhook.hpp>
#if HAVE_NCBI_C
# include <asn.h>
#endif

BEGIN_NCBI_SCOPE

CObjectOStream* OpenObjectOStreamAsn(CNcbiOstream& out, bool deleteOut);
CObjectOStream* OpenObjectOStreamAsnBinary(CNcbiOstream& out, bool deleteOut);
CObjectOStream* OpenObjectOStreamXml(CNcbiOstream& out, bool deleteOut);

CObjectOStream* CObjectOStream::Open(ESerialDataFormat format,
                                     const string& fileName,
                                     unsigned openFlags)
{
    CNcbiOstream* outStream = 0;
    bool deleteStream;
    if ( (openFlags & eSerial_StdWhenEmpty) && fileName.empty() ||
         (openFlags & eSerial_StdWhenDash) && fileName == "-" ||
         (openFlags & eSerial_StdWhenStd) && fileName == "stdout" ) {
        outStream = &NcbiCout;
        deleteStream = false;
    }
    else {
        switch ( format ) {
        case eSerial_AsnText:
        case eSerial_Xml:
            outStream = new CNcbiOfstream(fileName.c_str());
            break;
        case eSerial_AsnBinary:
            outStream = new CNcbiOfstream(fileName.c_str(),
                                          IOS_BASE::out | IOS_BASE::binary);
            break;
        default:
            THROW1_TRACE(runtime_error,
                         "CObjectOStream::Open: unsupported format");
        }
        if ( !*outStream ) {
            delete outStream;
            THROW1_TRACE(runtime_error, "cannot open file");
        }
        deleteStream = true;
    }

    return Open(format, *outStream, deleteStream);
}

CObjectOStream* CObjectOStream::Open(ESerialDataFormat format,
                                     CNcbiOstream& outStream,
                                     bool deleteStream)
{
    switch ( format ) {
    case eSerial_AsnText:
        return OpenObjectOStreamAsn(outStream, deleteStream);
    case eSerial_AsnBinary:
        return OpenObjectOStreamAsnBinary(outStream, deleteStream);
    case eSerial_Xml:
        return OpenObjectOStreamXml(outStream, deleteStream);
    default:
        break;
    }
    THROW1_TRACE(runtime_error,
                 "CObjectOStream::Open: unsupported format");
}

CObjectOStream::CObjectOStream(CNcbiOstream& out, bool deleteOut)
    : m_Output(out, deleteOut)
{
}

CObjectOStream::~CObjectOStream(void)
{
    _TRACE("~CObjectOStream:"<<m_Objects.GetWrittenObjectCount()<<" objects written");
}

void CObjectOStream::Close(void)
{
    m_Output.Close();
    m_Objects.Clear();
    CObjectStack::Clear();
}

void CObjectOStream::Write(TConstObjectPtr object, TTypeInfo typeInfo)
{
    // root writer
    try {
        CObjectStackNamedFrame m(*this, typeInfo);
        _TRACE("CObjectOStream::Write(" << NStr::PtrToString(object) << ", "
               << typeInfo->GetName() << ')');
        WriteTypeName(typeInfo->GetName());
        WriteObject(object, typeInfo);
#if NCBISER_ALLOW_CYCLES
        m_Objects.CheckAllWritten();
#endif
        FlushBuffer();
        m_Objects.Clear();
        m.End();
    }
    catch (...) {
        m_Objects.Clear();
        throw;
    }
}

void CObjectOStream::Write(TConstObjectPtr object, const CTypeRef& type)
{
    Write(object, type.Get());
}

CWriteObjectHook*
CObjectOStream::GetHook(const TWriteObjectHooks& hooks,
                        TTypeInfo objectType)
{
    if ( !objectType )
        THROW1_TRACE(runtime_error, "invalid argument");
    TWriteObjectHooks::const_iterator hook = hooks.find(objectType);
    if ( hook != hooks.end() )
        return hook->second.GetPointer();
    return 0;
}

void CObjectOStream::SetWriteObjectHook(TTypeInfo objectType,
                                        CWriteObjectHook* hook)
{
    if ( !objectType || !hook )
        THROW1_TRACE(runtime_error, "invalid argument");
    TWriteObjectHooks* hooks = m_WriteObjectHooks.get();
    if ( !hooks )
        m_WriteObjectHooks.reset(hooks = new TWriteObjectHooks);
    (*hooks)[objectType].Reset(hook);
}

void CObjectOStream::ResetWriteObjectHook(TTypeInfo objectType)
{
    if ( !objectType )
        THROW1_TRACE(runtime_error, "invalid argument");
    TWriteObjectHooks* hooks = m_WriteObjectHooks.get();
    if ( hooks ) {
        hooks->erase(objectType);
        if ( hooks->empty() )
            m_WriteObjectHooks.reset(0);
    }
}

CWriteClassMemberHook*
CObjectOStream::GetHook(const TWriteClassMemberHooks& hooks,
                        const CClassTypeInfo* classType,
                        TMemberIndex memberIndex)
{
    _ASSERT(classType);
    const CMemberInfo* memberInfo = classType->GetMemberInfo(memberIndex);
    _ASSERT(memberInfo);
    TWriteClassMemberHooks::const_iterator hook = hooks.find(memberInfo);
    if ( hook != hooks.end() )
        return hook->second.GetPointer();
    return 0;
}

void CObjectOStream::SetWriteClassMemberHook(const CClassTypeInfo* classType,
                                             TMemberIndex memberIndex,
                                             CWriteClassMemberHook* hook)
{
    if ( !hook )
        THROW1_TRACE(runtime_error, "invalid argument");
    _ASSERT(classType);
    const CMemberInfo* memberInfo = classType->GetMemberInfo(memberIndex);
    _ASSERT(memberInfo);
    TWriteClassMemberHooks* hooks = m_WriteClassMemberHooks.get();
    if ( !hooks )
        m_WriteClassMemberHooks.reset(hooks = new TWriteClassMemberHooks);
    (*hooks)[memberInfo].Reset(hook);
}

void CObjectOStream::ResetWriteClassMemberHook(const CClassTypeInfo* classType,
                                               TMemberIndex memberIndex)
{
    _ASSERT(classType);
    const CMemberInfo* memberInfo = classType->GetMemberInfo(memberIndex);
    _ASSERT(memberInfo);
    TWriteClassMemberHooks* hooks = m_WriteClassMemberHooks.get();
    if ( hooks ) {
        hooks->erase(memberInfo);
        if ( hooks->empty() )
            m_WriteClassMemberHooks.reset(0);
    }
}

CWriteChoiceVariantHook*
CObjectOStream::GetHook(const TWriteChoiceVariantHooks& hooks,
                        const CChoiceTypeInfo* choiceType,
                        TMemberIndex variantIndex)
{
    _ASSERT(choiceType);
    const CMemberInfo* memberInfo = choiceType->GetMemberInfo(variantIndex);
    _ASSERT(memberInfo);
    TWriteChoiceVariantHooks::const_iterator hook = hooks.find(memberInfo);
    if ( hook != hooks.end() )
        return hook->second.GetPointer();
    return 0;
}

void
CObjectOStream::SetWriteChoiceVariantHook(const CChoiceTypeInfo* choiceType,
                                          TMemberIndex variantIndex,
                                          CWriteChoiceVariantHook* hook)
{
    if ( !hook )
        THROW1_TRACE(runtime_error, "invalid argument");
    _ASSERT(choiceType);
    const CMemberInfo* memberInfo = choiceType->GetMemberInfo(variantIndex);
    _ASSERT(memberInfo);
    TWriteChoiceVariantHooks* hooks = m_WriteChoiceVariantHooks.get();
    if ( !hooks )
        m_WriteChoiceVariantHooks.reset(hooks = new TWriteChoiceVariantHooks);
    (*hooks)[memberInfo].Reset(hook);
}

void
CObjectOStream::ResetWriteChoiceVariantHook(const CChoiceTypeInfo* choiceType,
                                            TMemberIndex variantIndex)
{
    _ASSERT(choiceType);
    const CMemberInfo* memberInfo = choiceType->GetMemberInfo(variantIndex);
    _ASSERT(memberInfo);
    TWriteChoiceVariantHooks* hooks = m_WriteChoiceVariantHooks.get();
    if ( hooks ) {
        hooks->erase(memberInfo);
        if ( hooks->empty() )
            m_WriteChoiceVariantHooks.reset(0);
    }
}

#if NCBISER_ALLOW_CYCLES
void CObjectOStream::WriteObject(CWriteObjectInfo& info)
{
    m_Objects.MarkObjectWritten(info);
    WriteObject(info.GetObject());
}
#endif

void CObjectOStream::RegisterAndWrite(TConstObjectPtr object,
                                      TTypeInfo typeInfo)
{
#if NCBISER_ALLOW_CYCLES
    WriteObject(m_Objects.RegisterObject(object, typeInfo));
#else
    WriteObject(object, typeInfo);
#endif
}

void CObjectOStream::RegisterAndWrite(const CConstObjectInfo& object)
{
#if NCBISER_ALLOW_CYCLES
    WriteObject(m_Objects.RegisterObject(object));
#else
    WriteObject(object);
#endif
}

void CObjectOStream::WriteSeparateObject(const CConstObjectInfo& object)
{
    size_t firstObject = m_Objects.GetWrittenObjectCount();
    WriteObject(object);
    size_t lastObject = m_Objects.GetWrittenObjectCount();
    m_Objects.ForgetObjects(firstObject, lastObject);
}

void CObjectOStream::WriteExternalObject(TConstObjectPtr object,
                                         TTypeInfo typeInfo)
{
    _TRACE("CObjectOStream::RegisterAndWrite(" <<
           NStr::PtrToString(object) << ", "
           << typeInfo->GetName() << ')');
    RegisterAndWrite(object, typeInfo);
}

bool CObjectOStream::Write(const CRef<CByteSource>& source)
{
    m_Output.Write(source.GetObject().Open());
    return true;
}

void CObjectOStream::WriteTypeName(const string& )
{
    // do nothing by default
}

bool CObjectOStream::WriteEnum(const CEnumeratedTypeValues& values, long value)
{
    if ( values.IsInteger() ) {
        return false;
    }
    values.FindName(value, false);
    WriteLong(value);
    return true;
}

void CObjectOStream::WritePointer(TConstObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("WritePointer(" << NStr::PtrToString(object) << ", "
           << typeInfo->GetName() << ")");
    if ( object == 0 ) {
        _TRACE("WritePointer: " << NStr::PtrToString(object) << ": null");
        WriteNullPointer();
        return;
    }
#if NCBISER_ALLOW_CYCLES
    WritePointer(m_Objects.RegisterObject(object, typeInfo), typeInfo);
#else
    TTypeInfo realTypeInfo = typeInfo->GetRealTypeInfo(object);
    if ( typeInfo == realTypeInfo ) {
        _TRACE("WritePointer: " <<
               NStr::PtrToString(object) << ": new");
        WriteThis(object, realTypeInfo);
    }
    else {
        _TRACE("WritePointer: " <<
               NStr::PtrToString(object)
               << ": new " << realTypeInfo->GetName());
        WriteOther(object, realTypeInfo);
    }
#endif
}

void CObjectOStream::WritePointer(CWriteObjectInfo& info,
                                  TTypeInfo declaredTypeInfo)
{
    if ( info.IsWritten() ) {
        // put reference on it
        _TRACE("WritePointer: " << NStr::PtrToString(info.GetObject().GetObjectPtr()) <<
               ": @" << info.GetIndex());
        WriteObjectReference(info.GetIndex());
    }
    else {
        // new object
        m_Objects.MarkObjectWritten(info);
        TTypeInfo realTypeInfo = info.GetTypeInfo();
        if ( declaredTypeInfo == realTypeInfo ) {
            _TRACE("WritePointer: " <<
                   NStr::PtrToString(info.GetObject().GetObjectPtr()) << ": new");
            WriteThis(info.GetObject().GetObjectPtr(), info.GetTypeInfo());
        }
        else {
            _TRACE("WritePointer: " <<
                   NStr::PtrToString(info.GetObject().GetObjectPtr())
                   << ": new " << realTypeInfo->GetName());
            WriteOther(info.GetObject().GetObjectPtr(), info.GetTypeInfo());
        }
    }
}

void CObjectOStream::WriteSChar(signed char data)
{
    WriteInt(data);
}

void CObjectOStream::WriteUChar(unsigned char data)
{
    WriteUInt(data);
}

void CObjectOStream::WriteShort(short data)
{
    WriteInt(data);
}

void CObjectOStream::WriteUShort(unsigned short data)
{
    WriteUInt(data);
}

void CObjectOStream::WriteInt(int data)
{
    WriteLong(data);
}

void CObjectOStream::WriteUInt(unsigned data)
{
    WriteULong(data);
}

void CObjectOStream::WriteFloat(float data)
{
    WriteDouble(data);
}

void CObjectOStream::WriteCString(const char* str)
{
	WriteString(str);
}

void CObjectOStream::WriteStringStore(const string& str)
{
	WriteString(str);
}

void CObjectOStream::WriteThis(TConstObjectPtr object, TTypeInfo typeInfo)
{
    WriteObject(object, typeInfo);
}

void CObjectOStream::WriteNamedType(TTypeInfo /*namedTypeInfo*/,
                                    TTypeInfo typeInfo,
                                    TConstObjectPtr object)
{
    WriteObject(object, typeInfo);
}

void CObjectOStream::EndClass(CObjectStackClass& cls)
{
    cls.End();
}

void CObjectOStream::EndClassMember(CObjectStackClassMember& m)
{
    m.End();
}

class CDelayedBufferWriter : public CVoidTypeInfo
{
protected:
    void WriteData(CObjectOStream& out, TConstObjectPtr delayBufferPtr) const
        {
            out.Write(CType<CDelayBuffer>::Get(delayBufferPtr).GetSource());
        }
};

static CDelayedBufferWriter delayedBufferWriter;

void CObjectOStream::WriteClassMembers(TConstObjectPtr objectPtr,
                                       const CClassTypeInfo* objectType)
{
    const CMembersInfo& members = objectType->GetMembers();

    TWriteClassMemberHooks* hooks = m_WriteClassMemberHooks.get();

    TMemberIndex lastIndex = members.LastMemberIndex();
    for ( TMemberIndex i = members.FirstMemberIndex(); i <= lastIndex; ++i ) {
        const CMemberInfo* memberInfo = members.GetMemberInfo(i);

        bool haveSetFlag = memberInfo->HaveSetFlag();
        if ( haveSetFlag && !memberInfo->GetSetFlag(objectPtr) ) {
            // not set -> skip this member
            continue;
        }
        
        if ( memberInfo->CanBeDelayed() ) {
            const CDelayBuffer& buffer = memberInfo->GetDelayBuffer(objectPtr);
            if ( buffer ) {
                if ( buffer.HaveFormat(GetDataFormat()) ) {
                    // try to write delayed buffer
                    if ( hooks ) {
                        TWriteClassMemberHooks::iterator hooki =
                            hooks->find(memberInfo);
                        if ( hooki != hooks->end() ) {
                            // have hook
                            DoWriteClassMember(memberInfo->GetId(),
                                               CConstObjectInfo(objectPtr,
                                                                objectType,
                                                                CConstObjectInfo::eNonCObject),
                                               i, *hooki->second);
                            continue;
                        }
                    }
                    // plain write of delayed member
                    DoWriteClassMember(memberInfo->GetId(),
                                       &buffer, &delayedBufferWriter);
                    continue;
                }
                // cannot write delayed buffer -> proceed after update
                const_cast<CDelayBuffer&>(buffer).Update();
            }
        }

        TConstObjectPtr memberPtr =
            memberInfo->GetMember(objectPtr);
        TTypeInfo memberType = memberInfo->GetTypeInfo();

        if ( !haveSetFlag && memberInfo->Optional() ) {
            TConstObjectPtr defaultPtr = memberInfo->GetDefault();
            if ( !defaultPtr ) {
                if ( memberType->IsDefault(memberPtr) )
                    continue; // DEFAULT
            }
            else {
                if ( memberType->Equals(memberPtr, defaultPtr) )
                    continue; // OPTIONAL
            }
        }

        if ( hooks ) {
            TWriteClassMemberHooks::iterator hooki =
                hooks->find(memberInfo);
            if ( hooki != hooks->end() ) {
                // have hook
                DoWriteClassMember(memberInfo->GetId(),
                                   CConstObjectInfo(objectPtr,
                                                    objectType,
                                                    CConstObjectInfo::eNonCObject),
                                   i, *hooki->second.GetPointer());
                continue;
            }
        }

        // plain member write
        DoWriteClassMember(memberInfo->GetId(),
                           memberPtr, memberType);
    }
}

void CObjectOStream::DoWriteClass(TConstObjectPtr objectPtr,
                                  const CClassTypeInfo* objectType)
{
    CObjectStackClass cls(*this, objectType);
    BeginClass(cls, objectType);
    
    WriteClassMembers(objectPtr, objectType);
    
    EndClass(cls);
}

void CObjectOStream::DoWriteClass(const CConstObjectInfo& object,
                                  CWriteClassMembersHook& hook)
{
    const CClassTypeInfo* classType = object.GetClassTypeInfo();
    CObjectStackClass cls(*this, classType);
    BeginClass(cls, classType);
    
    hook.WriteClassMembers(*this, object);
    
    EndClass(cls);
}

void CObjectOStream::WriteClassMember(const CConstObjectInfo& object,
                                      TMemberIndex index,
                                      CWriteClassMemberHook& hook)
{
    const CClassTypeInfo* classType = object.GetClassTypeInfo();
    const CMemberInfo* memberInfo = classType->GetMemberInfo(index);
    DoWriteClassMember(memberInfo->GetId(), object, index, hook);
}

void CObjectOStream::WriteClassMemberNoHook(const CConstObjectInfo& object,
                                            TMemberIndex index)
{
    const CClassTypeInfo* classType = object.GetClassTypeInfo();
    const CMemberInfo* memberInfo = classType->GetMemberInfo(index);
    if ( memberInfo->CanBeDelayed() ) {
        const CDelayBuffer& buffer =
            memberInfo->GetDelayBuffer(object.GetObjectPtr());
        if ( buffer ) {
            if ( buffer.HaveFormat(GetDataFormat()) ) {
                // plain write delayed buffer
                DoWriteClassMember(memberInfo->GetId(),
                                   &buffer, &delayedBufferWriter);
                return;
            }
            const_cast<CDelayBuffer&>(buffer).Update();
        }
    }
    // plain write member
    DoWriteClassMember(memberInfo->GetId(),
                       memberInfo->GetMember(object.GetObjectPtr()),
                       memberInfo->GetTypeInfo());
}

void CObjectOStream::WriteClassMember(const CConstObjectInfo& object,
                                      TMemberIndex index)
{
    const CClassTypeInfo* classType = object.GetClassTypeInfo();
    TWriteClassMemberHooks* hooks = m_WriteClassMemberHooks.get();
    const CMemberInfo* memberInfo = classType->GetMemberInfo(index);
    if ( hooks ) {
        TWriteClassMemberHooks::iterator hooki = hooks->find(memberInfo);
        if ( hooki != hooks->end() ) {
            // hooked write member
            DoWriteClassMember(memberInfo->GetId(),
                               object, index, *hooki->second);
            return;
        }
    }
    if ( memberInfo->CanBeDelayed() ) {
        const CDelayBuffer& buffer =
            memberInfo->GetDelayBuffer(object.GetObjectPtr());
        if ( buffer ) {
            if ( buffer.HaveFormat(GetDataFormat()) ) {
                // plain write delayed buffer
                DoWriteClassMember(memberInfo->GetId(),
                                   &buffer, &delayedBufferWriter);
                return;
            }
            const_cast<CDelayBuffer&>(buffer).Update();
        }
    }
    // plain write member
    DoWriteClassMember(memberInfo->GetId(),
                       memberInfo->GetMember(object.GetObjectPtr()),
                       memberInfo->GetTypeInfo());
}

void CObjectOStream::DoWriteClassMember(const CMemberId& id,
                                        const CConstObjectInfo& object,
                                        TMemberIndex index,
                                        CWriteClassMemberHook& hook)
{
    CObjectStackClassMember m(*this, id);
    BeginClassMember(m, id);

    hook.WriteClassMember(*this, object, index);
    
    EndClassMember(m);
}

void CObjectOStream::DoWriteClassMember(const CMemberId& id,
                                        TConstObjectPtr memberPtr,
                                        TTypeInfo memberType)
{
    CObjectStackClassMember m(*this, id);
    BeginClassMember(m, id);

    WriteObject(memberPtr, memberType);
    
    EndClassMember(m);
}

void CObjectOStream::WriteChoiceVariantNoHook(const CConstObjectInfo& choice,
                                              TMemberIndex index)
{
    const CChoiceTypeInfo* choiceType = choice.GetChoiceTypeInfo();
    const CMembersInfo& variants = choiceType->GetMembers();
    const CMemberInfo* variantInfo = variants.GetMemberInfo(index);
    if ( variantInfo->CanBeDelayed() ) {
        const CDelayBuffer& buffer =
            variantInfo->GetDelayBuffer(choice.GetObjectPtr());
        if ( buffer ) {
            if ( buffer.HaveFormat(GetDataFormat()) ) {
                Write(buffer.GetSource());
                return;
            }
            const_cast<CDelayBuffer&>(buffer).Update();
        }
    }
    TConstObjectPtr variantPtr =
        choiceType->GetData(choice.GetObjectPtr(), index);
    TTypeInfo variantType = variantInfo->GetTypeInfo();
    if ( variantInfo->IsObjectPointer() )
        WriteExternalObject(variantPtr, variantType);
    else
        WriteObject(variantPtr, variantType);
}

void CObjectOStream::BeginBytes(const ByteBlock& )
{
}

void CObjectOStream::EndBytes(const ByteBlock& )
{
}

#if HAVE_NCBI_C
extern "C" {
    Int2 LIBCALLBACK WriteAsn(Pointer object, CharPtr data, Uint2 length)
    {
        if ( !object || !data )
            return -1;
    
        static_cast<CObjectOStream::AsnIo*>(object)->Write(data, length);
        return length;
    }
}

CObjectOStream::AsnIo::AsnIo(CObjectOStream& out, const string& rootTypeName)
    : m_Stream(out), m_RootTypeName(rootTypeName), m_Count(0)
{
    m_AsnIo = AsnIoNew(out.GetAsnFlags() | ASNIO_OUT, 0, this, 0, WriteAsn);
    out.AsnOpen(*this);
}

CObjectOStream::AsnIo::~AsnIo(void)
{
    AsnIoClose(*this);
    GetStream().AsnClose(*this);
}

void CObjectOStream::AsnOpen(AsnIo& )
{
}

void CObjectOStream::AsnClose(AsnIo& )
{
}

unsigned CObjectOStream::GetAsnFlags(void)
{
    return 0;
}

void CObjectOStream::AsnWrite(AsnIo& , const char* , size_t )
{
    THROW1_TRACE(runtime_error, "illegal call");
}
#endif

END_NCBI_SCOPE
