#ifndef OBJISTR__HPP
#define OBJISTR__HPP

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
* Revision 1.55  2000/10/17 18:45:24  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.54  2000/10/13 20:59:12  vasilche
* Avoid using of ssize_t absent on some compilers.
*
* Revision 1.53  2000/10/13 20:22:46  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.52  2000/10/13 16:28:31  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.51  2000/10/03 17:22:34  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.50  2000/09/29 16:18:13  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.49  2000/09/18 20:00:04  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.48  2000/09/01 13:16:00  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.47  2000/08/15 19:44:40  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.46  2000/07/03 18:42:35  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.45  2000/06/16 20:01:20  vasilche
* Avoid use of unexpected_exception() which is unimplemented on Mac.
*
* Revision 1.44  2000/06/16 16:31:06  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.43  2000/06/07 19:45:42  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.42  2000/06/01 19:06:56  vasilche
* Added parsing of XML data.
*
* Revision 1.41  2000/05/24 20:08:13  vasilche
* Implemented XML dump.
*
* Revision 1.40  2000/05/09 16:38:33  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.39  2000/05/04 16:22:23  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.38  2000/04/28 16:58:02  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.37  2000/04/13 14:50:17  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.36  2000/04/06 16:10:50  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.35  2000/03/14 14:43:29  vasilche
* Fixed error reporting.
*
* Revision 1.34  2000/03/07 14:05:30  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.33  2000/01/10 20:04:07  vasilche
* Fixed duplicate name.
*
* Revision 1.32  2000/01/10 19:46:31  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.31  2000/01/05 19:43:44  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.30  1999/12/20 15:29:31  vasilche
* Fixed bug with old ASN structures.
*
* Revision 1.29  1999/12/17 19:04:53  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.28  1999/10/18 20:11:15  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.27  1999/10/04 16:22:08  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.26  1999/09/27 14:17:59  vasilche
* Fixed bug with overloaded construtors of Block.
*
* Revision 1.25  1999/09/24 18:19:13  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.24  1999/09/23 18:56:51  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.23  1999/09/22 20:11:48  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.22  1999/09/14 18:54:03  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.21  1999/08/16 16:07:43  vasilche
* Added ENUMERATED type.
*
* Revision 1.20  1999/08/13 15:53:43  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.19  1999/07/26 18:31:28  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.18  1999/07/22 20:36:37  vasilche
* Fixed 'using namespace' declaration for MSVC.
*
* Revision 1.17  1999/07/22 17:33:40  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.16  1999/07/21 14:19:55  vasilche
* Added serialization of bool.
*
* Revision 1.15  1999/07/19 15:50:15  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.14  1999/07/15 16:54:42  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.13  1999/07/09 16:32:53  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.12  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.11  1999/07/02 21:31:43  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.10  1999/07/01 17:55:19  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:27  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:39  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/17 20:42:00  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.6  1999/06/16 20:35:21  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.5  1999/06/15 16:20:01  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.4  1999/06/10 21:06:38  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:15  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:32  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:24  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeinfo.hpp>
#include <serial/strbuffer.hpp>
#include <serial/objstack.hpp>
#include <serial/objhook.hpp>
#include <serial/hookdatakey.hpp>

struct asnio;

BEGIN_NCBI_SCOPE

class CMemberId;
class CItemsInfo;
class CItemInfo;
class CMemberInfo;
class CVariantInfo;
class CDelayBuffer;
class CByteSource;
class CByteSourceReader;

class CClassTypeInfo;
class CChoiceTypeInfo;
class CContainerTypeInfo;
class CObjectStreamCopier;

class CReadObjectHook;
class CReadClassMemberHook;
class CReadChoiceVariantHook;

class CReadObjectInfo;
class CReadObjectList;

class CObjectIStream : public CObjectStack
{
public:
    // typedefs
    typedef size_t TObjectIndex;

    // open methods
    virtual void Open(const CRef<CByteSourceReader>& reader);
    void Open(const CRef<CByteSource>& source);
    void Open(CNcbiIstream& inStream, bool deleteInStream = false);
    void Close(void);

    static CObjectIStream* Open(ESerialDataFormat format,
                                CNcbiIstream& inStream,
                                bool deleteInStream = false);
    static CObjectIStream* Open(ESerialDataFormat format,
                                const string& fileName,
                                unsigned openFlags = 0);
    static CObjectIStream* Open(const string& fileName,
                                ESerialDataFormat format)
        {
            return Open(format, fileName);
        }

    bool DetectLoops(void) const;
    void DetectLoops(bool detectLoops);

    // constructors
protected:
    CObjectIStream(void);
    CObjectIStream(CNcbiIstream& in, bool deleteIn = false);

public:
    virtual ~CObjectIStream(void);

    // get data format
    virtual ESerialDataFormat GetDataFormat(void) const = 0;

    // USER INTERFACE
    // root reader
    void Read(const CObjectInfo& object);
    void Read(TObjectPtr object, TTypeInfo type);
    void Skip(TTypeInfo type);

    // file header readers
    virtual string ReadFileHeader(void);
    void SkipFileHeader(TTypeInfo typeInfo);

    enum ENoFileHeader {
        eNoFileHeader
    };
    void Read(const CObjectInfo& object, ENoFileHeader noFileHeader);
    void Read(TObjectPtr object, TTypeInfo type, ENoFileHeader noFileHeader);
    void Skip(TTypeInfo type, ENoFileHeader noFileHeader);

    // subtree reader
    void ReadObject(const CObjectInfo& object);
    void ReadObject(TObjectPtr object, TTypeInfo typeInfo);
    void SkipObject(const CObjectTypeInfo& objectType);
    void SkipObject(TTypeInfo typeInfo);

    // temporary reader
    void ReadSeparateObject(const CObjectInfo& object);

    // END OF USER INTERFACE

    // internal reader
    void ReadExternalObject(TObjectPtr object, TTypeInfo typeInfo);
    void SkipExternalObject(TTypeInfo typeInfo);

    CObjectInfo ReadObject(void);

    virtual void EndOfRead(void);
    
    // try to read enum value name, "" if none
    virtual long ReadEnum(const CEnumeratedTypeValues& values) = 0;

    // std C types readers
    void ReadStd(bool& data)
        {
            data = ReadBool();
        }
    void ReadStd(char& data)
        {
            data = ReadChar();
        }
    void ReadStd(signed char& data)
        {
            data = ReadSChar();
        }
    void ReadStd(unsigned char& data)
        {
            data = ReadUChar();
        }
    void ReadStd(short& data)
        {
            data = ReadShort();
        }
    void ReadStd(unsigned short& data)
        {
            data = ReadUShort();
        }
    void ReadStd(int& data)
        {
            data = ReadInt();
        }
    void ReadStd(unsigned& data)
        {
            data = ReadUInt();
        }
    void ReadStd(long& data)
        {
            data = ReadLong();
        }
    void ReadStd(unsigned long& data)
        {
            data = ReadULong();
        }
    void ReadStd(float& data)
        {
            data = ReadFloat();
        }
    void ReadStd(double& data)
        {
            data = ReadDouble();
        }
    void ReadStd(char* & data)
        {
            data = ReadCString();
        }
    void ReadStd(const char* & data)
        {
            data = ReadCString();
        }
    void ReadStd(string& data)
        {
            ReadString(data);
        }
    virtual void ReadStringStore(string& s);
    
    // std C types readers
    void SkipStd(const bool &)
        {
            SkipBool();
        }
    void SkipStd(const char& )
        {
            SkipChar();
        }
    void SkipStd(const signed char& )
        {
            SkipSChar();
        }
    void SkipStd(const unsigned char& )
        {
            SkipUChar();
        }
    void SkipStd(const short& )
        {
            SkipShort();
        }
    void SkipStd(const unsigned short& )
        {
            SkipUShort();
        }
    void SkipStd(const int& )
        {
            SkipInt();
        }
    void SkipStd(const unsigned& )
        {
            SkipUInt();
        }
    void SkipStd(const long& )
        {
            SkipLong();
        }
    void SkipStd(const unsigned long& )
        {
            SkipULong();
        }
    void SkipStd(const float& )
        {
            SkipFloat();
        }
    void SkipStd(const double& )
        {
            SkipDouble();
        }
    void SkipStd(char* const& )
        {
            SkipCString();
        }
    void SkipStd(const char* const& )
        {
            SkipCString();
        }
    virtual void SkipString(void) = 0;
    virtual void SkipNull(void) = 0;
    virtual void SkipStringStore(void);
    virtual void SkipByteBlock(void) = 0;

    // reads type info
    virtual pair<TObjectPtr, TTypeInfo> ReadPointer(TTypeInfo declaredType);
    enum EPointerType {
        eNullPointer,
        eObjectPointer,
        eThisPointer,
        eOtherPointer
    };

    void SkipPointer(TTypeInfo declaredType);
    void SkipObjectInfo(void);

    virtual void ReadNull(void) = 0;

    // low level readers:
    enum EFailFlags {
        eNoError = 0,
        eEOF = 1,
        eReadError = 2,
        eFormatError = 4,
        eOverflow = 8,
        eIllegalCall = 16,
        eFail = 32,
        eNotOpen = 64
    };
    bool fail(void) const
        {
            return m_Fail != 0;
        }
    unsigned GetFailFlags(void) const
        {
            return m_Fail;
        }
    unsigned SetFailFlags(unsigned flags);
    unsigned ClearFailFlags(unsigned flags)
        {
            unsigned old = m_Fail;
            m_Fail &= ~flags;
            return old;
        }
    bool InGoodState(void);
    virtual string GetStackTrace(void) const;
    virtual string GetPosition(void) const = 0;

    void ThrowError1(EFailFlags fail, const char* message);
    void ThrowError1(EFailFlags fail, const string& message);
    void ThrowIOError1(CNcbiIstream& in);
    void CheckIOError1(CNcbiIstream& in)
        {
            if ( !in )
                ThrowIOError1(in);
        }

    void ThrowError1(const char* file, int line,
                     EFailFlags fail, const char* message);
    void ThrowError1(const char* file, int line,
                     EFailFlags fail, const string& message);
    void ThrowIOError1(const char* file, int line, CNcbiIstream& in);
    void CheckIOError1(const char* file, int line, CNcbiIstream& in)
        {
            if ( !in )
                ThrowIOError1(file, line, in);
        }

#ifdef _DEBUG
# define FILE_LINE __FILE__, __LINE__, 
#else
# define FILE_LINE
#endif
#define ThrowError(flag, mess) ThrowError1(FILE_LINE flag, mess)
#define ThrowIOError(in) ThrowIOError1(FILE_LINE in)
#define CheckIOError(in) CheckIOError1(FILE_LINE in)

	class ByteBlock
    {
	public:
		ByteBlock(CObjectIStream& in);
		~ByteBlock(void);

        void End(void);

        CObjectIStream& GetStream(void) const;

		size_t Read(void* dst, size_t length, bool forceLength = false);

        bool KnownLength(void) const;
		size_t GetExpectedLength(void) const;

        void SetLength(size_t length);
        void EndOfBlock(void);
        
	private:
        CObjectIStream& m_Stream;
		bool m_KnownLength;
        bool m_Ended;
		size_t m_Length;

		friend class CObjectIStream;
	};

#if HAVE_NCBI_C
    // ASN.1 interface
    class AsnIo
    {
    public:
        AsnIo(CObjectIStream& in, const string& rootTypeName);
        ~AsnIo(void);

        void End(void);

        CObjectIStream& GetStream(void) const;

        size_t Read(char* data, size_t length);
        
        operator asnio*(void);
        asnio* operator->(void);
        const string& GetRootTypeName(void) const;

    private:
        CObjectIStream& m_Stream;
        bool m_Ended;
        string m_RootTypeName;
        asnio* m_AsnIo;
        
    public:
        size_t m_Count;
    };
    friend class AsnIo;
protected:
    // ASN.1 interface
    virtual unsigned GetAsnFlags(void);
    virtual void AsnOpen(AsnIo& asn);
    virtual size_t AsnRead(AsnIo& asn, char* data, size_t length);
    virtual void AsnClose(AsnIo& asn);
public:
#endif
    
    // mid level I/O
    // named type
    MLIOVIR void ReadNamedType(TTypeInfo namedTypeInfo,
                               TTypeInfo typeInfo, TObjectPtr object);
    MLIOVIR void SkipNamedType(TTypeInfo namedTypeInfo,
                               TTypeInfo typeInfo);

    // container
    MLIOVIR void ReadContainer(const CContainerTypeInfo* containerType,
                               TObjectPtr containerPtr);
    MLIOVIR void SkipContainer(const CContainerTypeInfo* containerType);
    
    // class
    MLIOVIR void ReadClassSequential(const CClassTypeInfo* classType,
                                     TObjectPtr classPtr);
    MLIOVIR void ReadClassRandom(const CClassTypeInfo* classType,
                                 TObjectPtr classPtr);
    MLIOVIR void SkipClassSequential(const CClassTypeInfo* classType);
    MLIOVIR void SkipClassRandom(const CClassTypeInfo* classType);

    // choice
    MLIOVIR void ReadChoice(const CChoiceTypeInfo* choiceType,
                            TObjectPtr choicePtr);
    MLIOVIR void SkipChoice(const CChoiceTypeInfo* choiceType);

    // low level I/O
    // named type (alias)
    virtual void BeginNamedType(TTypeInfo namedTypeInfo);
    virtual void EndNamedType(void);

    // container
    virtual void BeginContainer(const CContainerTypeInfo* containerType) = 0;
    virtual void EndContainer(void) = 0;
    virtual bool BeginContainerElement(TTypeInfo elementType) = 0;
    virtual void EndContainerElement(void);

    // class
    virtual void BeginClass(const CClassTypeInfo* classInfo) = 0;
    virtual void EndClass(void);

    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType) = 0;
    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType,
                                          TMemberIndex pos) = 0;
    virtual void EndClassMember(void);

    // choice
    virtual TMemberIndex BeginChoiceVariant(const CChoiceTypeInfo* choiceType) = 0;
    virtual void EndChoiceVariant(void);

    // byte block
	virtual void BeginBytes(ByteBlock& block) = 0;
	virtual size_t ReadBytes(ByteBlock& block, char* buffer, size_t count) = 0;
	virtual void EndBytes(const ByteBlock& block);

    // report error about unended block
    void Unended(const string& msg);
    // report error about unended object stack frame
    virtual void UnendedFrame(void);

    // report error about class members
    void DuplicatedMember(const CMemberInfo* memberInfo);
    void ExpectedMember(const CMemberInfo* memberInfo);

    void StartDelayBuffer(void);
    void EndDelayBuffer(CDelayBuffer& buffer,
                        const CItemInfo* itemInfo, TObjectPtr objectPtr);

protected:
    friend class CObjectStreamCopier;

    // low level readers
    pair<TObjectPtr, TTypeInfo> ReadObjectInfo(void);
    virtual EPointerType ReadPointerType(void) = 0;
    virtual TObjectIndex ReadObjectPointer(void) = 0;
    virtual void ReadThisPointerEnd(void);
    virtual string ReadOtherPointer(void) = 0;
    virtual void ReadOtherPointerEnd(void);

    virtual bool ReadBool(void) = 0;
    virtual char ReadChar(void) = 0;
    virtual signed char ReadSChar(void);
    virtual unsigned char ReadUChar(void);
    virtual short ReadShort(void);
    virtual unsigned short ReadUShort(void);
    virtual int ReadInt(void);
    virtual unsigned ReadUInt(void);
    virtual long ReadLong(void) = 0;
    virtual unsigned long ReadULong(void) = 0;
    virtual float ReadFloat(void);
    virtual double ReadDouble(void) = 0;
    virtual void ReadString(string& s) = 0;
    virtual char* ReadCString(void);

    virtual void SkipBool(void) = 0;
    virtual void SkipChar(void) = 0;
    virtual void SkipSNumber(void) = 0;
    virtual void SkipUNumber(void);
    virtual void SkipSChar(void);
    virtual void SkipUChar(void);
    virtual void SkipShort(void);
    virtual void SkipUShort(void);
    virtual void SkipInt(void);
    virtual void SkipUInt(void);
    virtual void SkipLong(void);
    virtual void SkipULong(void);
    virtual void SkipFNumber(void) = 0;
    virtual void SkipFloat(void);
    virtual void SkipDouble(void);
    virtual void SkipCString(void);

    void RegisterObject(TTypeInfo typeInfo);
    void RegisterObject(TObjectPtr object, TTypeInfo typeInfo);
    const CReadObjectInfo& GetRegisteredObject(TObjectIndex index) const;

public:
    // open helpers
    static CObjectIStream* Create(ESerialDataFormat format);
    static CObjectIStream* Create(ESerialDataFormat format,
                                  const CRef<CByteSource>& source);
private:
    static CRef<CByteSource> GetSource(ESerialDataFormat format,
                                       const string& fileName,
                                       unsigned openFlags = 0);
    static CRef<CByteSource> GetSource(CNcbiIstream& inStream,
                                       bool deleteInStream = false);

protected:
    CIStreamBuffer m_Input;
    
private:
    AutoPtr<CReadObjectList> m_Objects;

    unsigned m_Fail;

public:
    // hook support
    CHookDataKey<CReadObjectHook> m_ObjectHookKey;
    CHookDataKey<CReadClassMemberHook> m_ClassMemberHookKey;
    CHookDataKey<CReadChoiceVariantHook> m_ChoiceVariantHookKey;
};

#include <serial/objistr.inl>

END_NCBI_SCOPE

#endif  /* OBJISTR__HPP */
