#ifndef OBJOSTR__HPP
#define OBJOSTR__HPP

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
* Revision 1.42  2000/07/03 18:42:36  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.41  2000/06/16 16:31:07  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.40  2000/06/07 19:45:43  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.39  2000/06/01 19:06:57  vasilche
* Added parsing of XML data.
*
* Revision 1.38  2000/05/24 20:08:13  vasilche
* Implemented XML dump.
*
* Revision 1.37  2000/05/09 16:38:34  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.36  2000/05/04 16:22:23  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.35  2000/04/28 16:58:02  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.34  2000/04/13 14:50:17  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.33  2000/04/06 16:10:51  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.32  2000/03/29 15:55:21  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.31  2000/03/07 14:05:31  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.30  2000/01/10 20:12:37  vasilche
* Fixed duplicate argument names.
* Fixed conflict between template and variable name.
*
* Revision 1.29  2000/01/10 19:46:32  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.28  2000/01/05 19:43:46  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.27  1999/12/20 15:29:31  vasilche
* Fixed bug with old ASN structures.
*
* Revision 1.26  1999/12/17 19:04:53  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.25  1999/10/04 16:22:09  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.24  1999/09/27 14:17:59  vasilche
* Fixed bug with overloaded construtors of Block.
*
* Revision 1.23  1999/09/24 18:19:14  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.22  1999/09/23 18:56:53  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.21  1999/09/22 20:11:49  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.20  1999/09/14 18:54:04  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.19  1999/08/16 16:07:43  vasilche
* Added ENUMERATED type.
*
* Revision 1.18  1999/08/13 15:53:44  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.17  1999/07/22 17:33:43  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.16  1999/07/21 14:19:58  vasilche
* Added serialization of bool.
*
* Revision 1.15  1999/07/19 15:50:17  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.14  1999/07/14 18:58:03  vasilche
* Fixed ASN.1 types/field naming.
*
* Revision 1.13  1999/07/09 16:32:53  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.12  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.11  1999/07/02 21:31:46  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.10  1999/07/01 17:55:20  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:30  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:40  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/17 20:42:02  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.6  1999/06/16 20:35:23  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.5  1999/06/15 16:20:03  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.4  1999/06/10 21:06:40  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:17  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:35  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:26  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeinfo.hpp>
#include <serial/objlist.hpp>
#include <serial/object.hpp>
#include <serial/strbuffer.hpp>
#include <serial/memberlist.hpp>
#include <serial/objstack.hpp>
#include <map>

struct asnio;

BEGIN_NCBI_SCOPE

class CMemberId;
class CDelayBuffer;

class CObjectArrayWriter
{
public:
    CObjectArrayWriter(bool empty = true)
        : m_NoMoreElements(empty)
        {
        }
    virtual ~CObjectArrayWriter(void);

    bool NoMoreElements(void) const
        {
            return m_NoMoreElements;
        }
    virtual void WriteElement(CObjectOStream& out) = 0;

protected:
    bool m_NoMoreElements;
};

class CObjectClassWriter
{
public:
    CObjectClassWriter(void)
        : m_Empty(true)
        {
        }
    virtual ~CObjectClassWriter(void);

    // returns amount of members written
    virtual void WriteMembers(CObjectOStream& out,
                              const CMembersInfo& members) = 0;

    bool m_Empty;
};

class CObjectOStream : public CObjectStack
{
public:
    typedef CWriteObjectInfo::TObjectIndex TObjectIndex;
    typedef CMembers::TMemberIndex TMemberIndex;

    static CObjectOStream* Open(ESerialDataFormat format,
                                const string& fileName,
                                unsigned openFlags = 0);
    static CObjectOStream* Open(const string& fileName,
                                ESerialDataFormat format)
        {
            return Open(format, fileName);
        }
    static CObjectOStream* Open(ESerialDataFormat format,
                                CNcbiOstream& outStream,
                                bool deleteOutStream = false);

protected:
    CObjectOStream(CNcbiOstream& out, bool deleteOut = false);

public:
    virtual ~CObjectOStream(void);

    virtual ESerialDataFormat GetDataFormat(void) const = 0;

    void FlushBuffer(void)
        {
            m_Output.FlushBuffer();
        }
    void Flush(void)
        {
            m_Output.Flush();
        }

    // root writer
    void Write(TConstObjectPtr object, TTypeInfo type);
    void Write(TConstObjectPtr object, const CTypeRef& type);
    void WriteObject(const CObjectInfo& object)
        {
            Write(object.GetObjectPtr(), object.GetTypeInfo());
        }

    virtual void WriteTypeName(const string& name);

    // try to write enum value name, false if none
    virtual bool WriteEnum(const CEnumeratedTypeValues& values, long value);

    // std C types readers
    void WriteStd(const bool& data)
        {
            WriteBool(data);
        }
    void WriteStd(const char& data)
        {
            WriteChar(data);
        }
    void WriteStd(const unsigned char& data)
        {
            WriteUChar(data);
        }
    void WriteStd(const signed char& data)
        {
            WriteSChar(data);
        }
    void WriteStd(const short& data)
        {
            WriteShort(data);
        }
    void WriteStd(const unsigned short& data)
        {
            WriteUShort(data);
        }
    void WriteStd(const int& data)
        {
            WriteInt(data);
        }
    void WriteStd(const unsigned int& data)
        {
            WriteUInt(data);
        }
    void WriteStd(const long& data)
        {
            WriteLong(data);
        }
    void WriteStd(const unsigned long& data)
        {
            WriteULong(data);
        }
    void WriteStd(const float& data)
        {
            WriteFloat(data);
        }
    void WriteStd(const double& data)
        {
            WriteDouble(data);
        }
    void WriteStd(const char* const& data)
        {
            WriteCString(data);
        }
    void WriteStd(char* const& data)
        {
            WriteCString(data);
        }
    void WriteStd(const string& data)
        {
            WriteString(data);
        }
    virtual void WriteStringStore(const string& data);

    // object level writers
    void WriteExternalObject(TConstObjectPtr object, TTypeInfo typeInfo);
    // type info writers
    virtual void WritePointer(TConstObjectPtr object, TTypeInfo typeInfo);

    virtual void WriteNull(void) = 0;

    virtual bool Write(const CRef<CByteSource>& source);

	class ByteBlock;
	friend class ByteBlock;
	class ByteBlock
    {
	public:
		ByteBlock(CObjectOStream& out, size_t length);
		~ByteBlock(void);

        CObjectOStream& GetStream(void) const;

		size_t GetLength(void) const;

		void Write(const void* bytes, size_t length);

	private:
        CObjectOStream& m_Stream;
		size_t m_Length;
	};

#if HAVE_NCBI_C
    class AsnIo
    {
    public:
        AsnIo(CObjectOStream& out, const string& rootTypeName);
        ~AsnIo(void);

        CObjectOStream& GetStream(void) const;

        void Write(const char* data, size_t length);

        operator asnio*(void);
        asnio* operator->(void);
        const string& GetRootTypeName(void) const;

    private:
        CObjectOStream& m_Stream;
        string m_RootTypeName;
        asnio* m_AsnIo;

    public:
        size_t m_Count;
    };
    friend class AsnIo;
protected:
    virtual unsigned GetAsnFlags(void);
    virtual void AsnOpen(AsnIo& asn);
    virtual void AsnWrite(AsnIo& asn, const char* data, size_t length);
    virtual void AsnClose(AsnIo& asn);

public:
#endif

    // array interface
    virtual void WriteArray(CObjectArrayWriter& writer,
                            TTypeInfo arrayType, bool randomOrder,
                            TTypeInfo elementType) = 0;

    // named type interface
    virtual void WriteNamedType(TTypeInfo namedTypeInfo,
                                TTypeInfo typeInfo, TConstObjectPtr object);

    // class interface
    virtual void BeginClass(CObjectStackClass& cls) = 0;
    virtual void EndClass(CObjectStackClass& cls);

    virtual void BeginClassMember(CObjectStackClassMember& member,
                                  const CMemberId& id) = 0;
    virtual void EndClassMember(CObjectStackClassMember& member);
    virtual void WriteClass(CObjectClassWriter& writer,
                            const CClassTypeInfo* classInfo, 
                            const CMembersInfo& members,
                            bool randomOrder);
    virtual void WriteClassMember(CObjectClassWriter& writer,
                                  const CMemberId& id,
                                  TTypeInfo memberInfo,
                                  TConstObjectPtr memberPtr);
    virtual void WriteDelayedClassMember(CObjectClassWriter& writer,
                                         const CMemberId& id,
                                         const CDelayBuffer& buffer);

    // choice interface
    virtual void WriteChoice(TTypeInfo choiceType,
                             const CMemberId& id,
                             TTypeInfo memberInfo,
                             TConstObjectPtr memberPtr) = 0;
    virtual void WriteDelayedChoice(TTypeInfo choiceType,
                                    const CMemberId& id,
                                    const CDelayBuffer& buffer) = 0;

	// write byte blocks
	virtual void BeginBytes(const ByteBlock& block);
	virtual void WriteBytes(const ByteBlock& block,
                            const char* bytes, size_t length) = 0;
	virtual void EndBytes(const ByteBlock& block);

protected:
    // low level writers
    virtual void WritePointer(TConstObjectPtr object,
                              CWriteObjectInfo& info,
                              TTypeInfo declaredTypeInfo);
    virtual void WriteNullPointer(void) = 0;
    virtual void WriteObjectReference(TObjectIndex index) = 0;
    virtual void WriteThis(TConstObjectPtr object,
                           CWriteObjectInfo& info);
    virtual void WriteOther(TConstObjectPtr object,
                            CWriteObjectInfo& info) = 0;

    virtual void WriteBool(bool data) = 0;
    virtual void WriteChar(char data) = 0;
    virtual void WriteSChar(signed char data);
    virtual void WriteUChar(unsigned char data);
    virtual void WriteShort(short data);
    virtual void WriteUShort(unsigned short data);
    virtual void WriteInt(int data);
    virtual void WriteUInt(unsigned int data);
    virtual void WriteLong(long data) = 0;
    virtual void WriteULong(unsigned long data) = 0;
    virtual void WriteFloat(float data);
    virtual void WriteDouble(double data) = 0;
	virtual void WriteCString(const char* str);
    virtual void WriteString(const string& str) = 0;

    COObjectList m_Objects;
#ifdef NCBISER_ALLOW_CYCLES
    void WriteObject(TConstObjectPtr object, CWriteObjectInfo& info)
        {
            m_Objects.ObjectWritten(info);
            _TRACE("CObjectOStream::RegisterObject(x, "<<info.GetTypeInfo()->GetName()<<") = "<<info.GetIndex());
            info.GetTypeInfo()->WriteData(*this, object);
        }
#else
    void WriteData(TConstObjectPtr object, TTypeInfo typeInfo)
        {
            typeInfo->WriteData(*this, object);
        }
#endif

protected:
    COStreamBuffer m_Output;
};

#include <serial/objostr.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTR__HPP */
