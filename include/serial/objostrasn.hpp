#ifndef OBJOSTRASN__HPP
#define OBJOSTRASN__HPP

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
* Revision 1.28  2000/08/15 19:44:41  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.27  2000/07/03 18:42:36  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.26  2000/06/16 16:31:07  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.25  2000/06/07 19:45:43  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.24  2000/06/01 19:06:57  vasilche
* Added parsing of XML data.
*
* Revision 1.23  2000/05/24 20:08:14  vasilche
* Implemented XML dump.
*
* Revision 1.22  2000/04/28 16:58:03  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.21  2000/04/13 14:50:18  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.20  2000/04/06 16:10:51  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.19  2000/01/10 20:12:37  vasilche
* Fixed duplicate argument names.
* Fixed conflict between template and variable name.
*
* Revision 1.18  2000/01/10 19:46:32  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.17  1999/09/24 18:19:14  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.16  1999/09/23 18:56:53  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.15  1999/09/22 20:11:50  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.14  1999/08/13 15:53:44  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.13  1999/07/22 17:33:44  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.12  1999/07/21 14:19:59  vasilche
* Added serialization of bool.
*
* Revision 1.11  1999/07/19 15:50:18  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.10  1999/07/14 18:58:04  vasilche
* Fixed ASN.1 types/field naming.
*
* Revision 1.9  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.8  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.7  1999/07/02 21:31:47  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.6  1999/07/01 17:55:20  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.5  1999/06/30 16:04:32  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.4  1999/06/24 14:44:41  vasilche
* Added binary ASN.1 output.
*
* Revision 1.3  1999/06/17 20:42:02  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.2  1999/06/16 20:35:24  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.1  1999/06/15 16:20:04  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.2  1999/06/10 21:06:40  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.1  1999/06/07 19:30:18  vasilche
* More bug fixes
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

class CObjectOStreamAsn : public CObjectOStream
{
public:
    typedef unsigned char TByte;

    CObjectOStreamAsn(CNcbiOstream& out);
    CObjectOStreamAsn(CNcbiOstream& out, bool deleteOut);
    virtual ~CObjectOStreamAsn(void);

    ESerialDataFormat GetDataFormat(void) const;

    virtual void WriteTypeName(const string& name);
    virtual bool WriteEnum(const CEnumeratedTypeValues& values, long value);

protected:
    virtual void WriteBool(bool data);
    virtual void WriteChar(char data);
    virtual void WriteInt(int data);
    virtual void WriteUInt(unsigned data);
    virtual void WriteLong(long data);
    virtual void WriteULong(unsigned long data);
    virtual void WriteDouble(double data);
    virtual void WriteString(const string& str);
    virtual void WriteCString(const char* str);

#if HAVE_NCBI_C
    virtual unsigned GetAsnFlags(void);
    virtual void AsnOpen(AsnIo& asn);
    virtual void AsnWrite(AsnIo& asn, const char* data, size_t length);
#endif

    virtual void WriteNullPointer(void);
    virtual void WriteObjectReference(TObjectIndex index);
    virtual void WriteOther(TConstObjectPtr object,
                            TTypeInfo typeInfo);
    void WriteId(const string& str);

    void WriteNull(void);

    virtual void WriteContainer(const CConstObjectInfo& container,
                                CWriteContainerElementsHook& hook);
    virtual void WriteContainerElement(const CConstObjectInfo& element);

    virtual void BeginClass(CObjectStackClass& cls,
                            const CClassTypeInfo* classInfo);
    virtual void EndClass(CObjectStackClass& cls);
    virtual void BeginClassMember(CObjectStackClassMember& m,
                                  const CMemberId& id);
    virtual void DoWriteClass(const CConstObjectInfo& object,
                              CWriteClassMembersHook& hook);
    virtual void DoWriteClass(TConstObjectPtr objectPtr,
                              const CClassTypeInfo* objectType);
    virtual void DoWriteClassMember(const CMemberId& id,
                                    const CConstObjectInfo& object,
                                    TMemberIndex index,
                                    CWriteClassMemberHook& hook);
    virtual void DoWriteClassMember(const CMemberId& id,
                                    TConstObjectPtr memberPtr,
                                    TTypeInfo memberType);

    virtual void WriteChoice(const CConstObjectInfo& choice,
                             CWriteChoiceVariantHook& hook);
    virtual void WriteChoice(const CConstObjectInfo& choice);

	virtual void BeginBytes(const ByteBlock& block);
	virtual void WriteBytes(const ByteBlock& block,
                            const char* bytes, size_t length);
	virtual void EndBytes(const ByteBlock& block);

private:
    void WriteString(const char* str, size_t length);
};

//#include <serial/objostrasn.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTRASN__HPP */
