#ifndef SERIAL___OBJISTR__HPP
#define SERIAL___OBJISTR__HPP

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
*   Base class of object input stream classes.
*   It reads data from an input stream, parses it, and creates a data object
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimempool.hpp>
#include <corelib/ncbiutil.hpp>
#include <util/strbuffer.hpp>
#include <serial/impl/objlist.hpp>
#include <serial/objhook.hpp>
#include <serial/impl/hookdatakey.hpp>
#include <serial/impl/pathhook.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


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

class CObjectInfo;
class CObjectInfoMI;

class CClassTypeInfo;
class CChoiceTypeInfo;
class CContainerTypeInfo;
class CObjectStreamCopier;
class CAliasTypeInfo;

class CReadObjectHook;
class CReadClassMemberHook;
class CReadChoiceVariantHook;
class CSkipObjectHook;
class CSkipClassMemberHook;
class CSkipChoiceVariantHook;

class CReadObjectInfo;
class CReadObjectList;

class CPackString;

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectIStream --
///
/// Base class of serial object stream decoders
class NCBI_XSERIAL_EXPORT CObjectIStream : public CObjectStack
{
public:
    /// Destructor.
    ///
    /// Constructors are protected;
    /// use any one of 'Create' methods to construct the stream
    virtual ~CObjectIStream(void);

//---------------------------------------------------------------------------
// Create methods
    // CObjectIStream will be created on heap, and must be deleted later on

    /// Create serial object reader and attach it to an input stream.
    ///
    /// @param format
    ///   Format of the input data
    /// @param inStream
    ///   Input stream
    /// @param deleteInStream
    ///   When TRUE, the input stream will be deleted automatically
    ///   when the reader is deleted
    /// @return
    ///   Reader (created on heap)
    /// @deprecated
    ///   Use one with EOwnership enum instead
    static NCBI_DEPRECATED CObjectIStream* Open(ESerialDataFormat format,
                                CNcbiIstream& inStream,
                                bool deleteInStream);

    /// Create serial object reader and attach it to an input stream.
    ///
    /// @param format
    ///   Format of the input data
    /// @param inStream
    ///   Input stream
    /// @param deleteInStream
    ///   When eTakeOwnership, the input stream will be deleted automatically
    ///   when the reader is deleted
    /// @return
    ///   Reader (created on heap)
    static CObjectIStream* Open(ESerialDataFormat format,
                                CNcbiIstream& inStream,
                                EOwnership deleteInStream = eNoOwnership);

    /// Create serial object reader and attach it to a file stream.
    ///
    /// @param format
    ///   Format of the input data
    /// @param fileName
    ///   Input file name
    /// @param openFlags
    ///   File open flags
    /// @return
    ///   Reader (created on heap)
    /// @sa ESerialOpenFlags
    static CObjectIStream* Open(ESerialDataFormat format,
                                const string& fileName,
                                TSerialOpenFlags openFlags = 0);

    /// Create serial object reader and attach it to a file stream.
    ///
    /// @param fileName
    ///   Input file name
    /// @param format
    ///   Format of the input data
    /// @return
    ///   Reader (created on heap)
    static CObjectIStream* Open(const string& fileName,
                                ESerialDataFormat format);

    /// Create serial object reader.
    /// The reader must be attached to a data source later on.
    ///
    /// @param format
    ///   Format of the input data
    /// @return
    ///   Reader (created on heap)
    static CObjectIStream* Create(ESerialDataFormat format);

    /// Create serial object reader and attach it to a data source
    ///
    /// @param format
    ///   Format of the input data
    /// @param source
    ///   Data source
    /// @return
    ///   Reader (created on heap)
    /// @sa CByteSource
    static CObjectIStream* Create(ESerialDataFormat format,
                                  CByteSource& source);

    /// Create serial object reader and attach it to a data source
    ///
    /// @param format
    ///   Format of the input data
    /// @param reader
    ///   Data source
    /// @return
    ///   Reader (created on heap)
    /// @sa CByteSourceReader
    static CObjectIStream* Create(ESerialDataFormat format,
                                  CByteSourceReader& reader);

    /// Create serial object reader and attach it to a data source
    ///
    /// @param format
    ///   Format of the input data
    /// @param buffer
    ///   Data source memory buffer
    /// @param size
    ///   Memory buffer size
    /// @return
    ///   Reader (created on heap)
    static CObjectIStream* CreateFromBuffer(ESerialDataFormat format,
                                            const char* buffer, size_t size);
    /// Get data format
    ///
    /// @return
    ///   Input data format
    ESerialDataFormat GetDataFormat(void) const;

//---------------------------------------------------------------------------
// Open methods

    /// Attach reader to a data source
    ///
    /// @param reader
    ///   Data source
    void Open(CByteSourceReader& reader);

    /// Attach reader to a data source
    ///
    /// @param source
    ///   Data source
    void Open(CByteSource& source);

    /// Attach reader to an input stream
    ///
    /// @param inStream
    ///   Input stream
    /// @param deleteInStream
    ///   When TRUE, the input stream will be deleted automatically
    ///   when the reader is deleted
    /// @deprecated
    ///   Use one with EOwnership enum instead
    void NCBI_DEPRECATED Open(CNcbiIstream& inStream, bool deleteInStream);

    /// Attach reader to an input stream
    ///
    /// @param inStream
    ///   Input stream
    /// @param deleteInStream
    ///   When eTakeOwnership, the input stream will be deleted automatically
    ///   when the reader is deleted
    void Open(CNcbiIstream& inStream, EOwnership deleteInStream = eNoOwnership);

    /// Attach reader to a data source
    ///
    /// @param buffer
    ///   Data source memory buffer
    /// @param size
    ///   Memory buffer size
    void OpenFromBuffer(const char* buffer, size_t size);
    
    /// Detach reader from a data source
    void Close(void);

//---------------------------------------------------------------------------
// Data verification setup

    /// Set up input data verification for this particular stream
    ///
    /// @param verify
    ///   Data verification parameter
    void SetVerifyData(ESerialVerifyData verify);

    /// Get input data verification parameter.
    /// When verification is enabled, stream verifies data on input
    /// and throws CSerialException with eFormatError err.code
    ///
    /// @return
    ///   Data verification parameter
    ESerialVerifyData GetVerifyData(void) const;

    /// Set up default input data verification for streams
    /// created by the current thread
    ///
    /// @param verify
    ///   Data verification parameter
    static  void SetVerifyDataThread(ESerialVerifyData verify);

    /// Set up default input data verification for streams
    /// created by the current process
    ///
    /// @param verify
    ///   Data verification parameter
    static  void SetVerifyDataGlobal(ESerialVerifyData verify);

    /// Set up skipping unknown members for this particular stream
    ///
    /// @param skip
    ///   Skip unknown members parameter
    void SetSkipUnknownMembers(ESerialSkipUnknown skip);

    /// Get skip unknown members parameter
    ///
    /// @return
    ///   Skip unknown members parameter
    ESerialSkipUnknown GetSkipUnknownMembers(void);

    /// Set up default skipping unknown members for streams
    /// created by the current thread
    ///
    /// @param skip
    ///   Skip unknown members parameter
    static  void SetSkipUnknownThread(ESerialSkipUnknown skip);

    /// Set up default skipping unknown members for streams
    /// created by the current process
    ///
    /// @param skip
    ///   Skip unknown members parameter
    static  void SetSkipUnknownGlobal(ESerialSkipUnknown skip);

    /// Set up skipping unknown choice variants for
    /// this particular stream
    ///
    /// @param skip
    ///   Skip unknown choice variants parameter
    /// @note
    ///   Skipping unknown variants can result in invalid object - with unset choice
    void SetSkipUnknownVariants(ESerialSkipUnknown skip);

    /// Get skip unknown choice variants parameter
    ///
    /// @return
    ///   Skip unknown choice variants parameter
    ESerialSkipUnknown GetSkipUnknownVariants(void);

    /// Set up default skipping unknown choice variants for streams
    /// created by the current thread
    ///
    /// @param skip
    ///   Skip unknown choice variants parameter
    static  void SetSkipUnknownVariantsThread(ESerialSkipUnknown skip);

    /// Set up default skipping unknown choice variants for streams
    /// created by the current process
    ///
    /// @param skip
    ///   Skip unknown choice variants parameter
    static  void SetSkipUnknownVariantsGlobal(ESerialSkipUnknown skip);

    /// Simple check if it's allowed to skip unknown members
    bool CanSkipUnknownMembers(void);
    /// Simple check if it's allowed to skip unknown variants
    bool CanSkipUnknownVariants(void);
    /// Update skip unknown members option to non-default value
    ESerialSkipUnknown UpdateSkipUnknownMembers(void);
    /// Update skip unknown variants option to non-default value
    ESerialSkipUnknown UpdateSkipUnknownVariants(void);

    EFixNonPrint FixNonPrint(EFixNonPrint how)
    {
        EFixNonPrint tmp = m_FixMethod;
        m_FixMethod = how == eFNP_Default ? x_GetFixCharsMethodDefault() : how;
        return tmp;
    }

//---------------------------------------------------------------------------
// Stream state

    /// Fail flags
    enum EFailFlags {
        /// No error
        fNoError       = 0,             eNoError     = fNoError,
        /// End of file in the middle of reading an object
        fEOF           = 1 << 0,        eEOF         = fEOF,
        /// An unknown error when reading the input file
        fReadError     = 1 << 1,        eReadError   = fReadError,
        /// Input file formatting does not conform with specification
        fFormatError   = 1 << 2,        eFormatError = fFormatError,
        /// Data read is beyond the allowed limits
        fOverflow      = 1 << 3,        eOverflow    = fOverflow,
        /// Input data is incorrect (e.g. invalid enum)
        fInvalidData   = 1 << 4,        eInvalidData = fInvalidData,
        /// Illegal in a given context function call
        fIllegalCall   = 1 << 5,        eIllegalCall = fIllegalCall,
        /// Internal error, the real reason is unclear
        fFail          = 1 << 6,        eFail        = fFail,
        /// No input file
        fNotOpen       = 1 << 7,        eNotOpen     = fNotOpen,
        /// Method is not implemented
        fNotImplemented= 1 << 8,        eNotImplemented = fNotImplemented,
        /// Mandatory value was missing in the input.
        /// This is the variant of fFormatError.
        /// Normally stream throws an exception, but client can request
        /// not to throw one; in this case this flag is set instead.
        fMissingValue  = 1 << 9,        eMissingValue= fMissingValue,
        /// Unknown value was present in the input.
        /// This is the variant of fFormatError.
        /// Normally stream throws an exception, but client can request
        /// not to throw one; in this case this flag is set instead.
        fUnknownValue  = 1 << 10,       eUnknownValue= fUnknownValue,
        /// Input value is 'null'
        fNullValue     = 1 << 11,       eNullValue= fNullValue
    };
    typedef int TFailFlags;

    /// Check if any of fail flags is set
    ///
    /// @return
    ///   TRUE or FALSE
    bool fail(void) const;
    
    /// Get fail flags
    ///
    /// @return
    ///   Fail flags
    TFailFlags GetFailFlags(void) const;

    /// Set fail flags
    ///
    /// @param flags
    ///   Fail flags
    /// @param message
    ///   Optional text message
    TFailFlags SetFailFlags(TFailFlags flags, const char* message=0);

    /// Reset fail flags
    ///
    /// @param flags
    ///   Flags to reset
    TFailFlags ClearFailFlags(TFailFlags flags);

    /// Check fail flags and also the state of input data source
    ///
    /// @return
    ///   TRUE is there is no errors
    bool InGoodState(void);

    /// Check if there is still some meaningful data that can be read;
    /// in text streams this function will skip white spaces and comments
    ///
    /// @return
    ///   TRUE if there is no more data
    virtual bool EndOfData(void);


    /// Set cancellation check callback.
    /// The stream will periodically check for a cancellation request and
    /// throw an exception when requested.
    void SetCanceledCallback(const ICanceled* callback);

    /// @deprecated
    ///   Use GetStreamPos() instead
    /// @sa GetStreamPos()
    NCBI_DEPRECATED CNcbiStreampos GetStreamOffset(void) const;
    
    /// Get the current stream position
    ///
    /// NOTE: 
    ///   This is not the same as istream::tellg();
    ///   rather, this is an offset in the current input
    ///
    /// @return
    ///   stream position
    CNcbiStreampos GetStreamPos(void) const;

    /// @deprecated
    ///  Use SetStreamPos() instead
    /// @sa SetStreamPos() 
    NCBI_DEPRECATED void   SetStreamOffset(CNcbiStreampos pos);

    /// Set the current read position in underlying input stream
    /// This is the same as istream::seekg()
    ///
    /// @param pos
    ///   stream position
    void   SetStreamPos(CNcbiStreampos pos);

    /// Get current stack trace as string.
    /// Useful for diagnostic and information messages.
    ///
    /// @return
    ///   string
    virtual string GetStackTrace(void) const;

    /// Get current stream position as string.
    /// Useful for diagnostic and information messages.
    ///
    /// @return
    ///   string
    virtual string GetPosition(void) const;

//---------------------------------------------------------------------------
// Local read hooks
    void SetPathReadObjectHook( const string& path, CReadObjectHook*        hook);
    void SetPathSkipObjectHook( const string& path, CSkipObjectHook*        hook);
    void SetPathReadMemberHook( const string& path, CReadClassMemberHook*   hook);
    void SetPathSkipMemberHook( const string& path, CSkipClassMemberHook*   hook);
    void SetPathReadVariantHook(const string& path, CReadChoiceVariantHook* hook);
    void SetPathSkipVariantHook(const string& path, CSkipChoiceVariantHook* hook);

    void SetMonitorType(TTypeInfo type);
    void AddMonitorType(TTypeInfo type);
    void ResetMonitorType(void);
    
    /// DelayBuffer parsing policy
    enum EDelayBufferParsing {
        /// Parse only if local hook are present
        eDelayBufferPolicyNotSet,
        /// Parse always
        eDelayBufferPolicyAlwaysParse,
        /// Never parse
        eDelayBufferPolicyNeverParse
    };
    void SetDelayBufferParsingPolicy(EDelayBufferParsing policy);
    EDelayBufferParsing GetDelayBufferParsingPolicy(void) const;
    bool ShouldParseDelayBuffer(void) const;

//---------------------------------------------------------------------------
// User interface

    /// Identify the type of data in the stream.
    ///
    /// Method matches each of the supplied known types against
    /// the stream data. Ideally, only ony type matches.
    /// Shorter lookup depth can result in mutiple matches;
    /// longer depth does not necessarily improve the result. 
    ///
    /// @param known_types
    ///   Set of known types.
    /// @param max_length
    ///   Maximum depth of the lookup.
    /// @param max_bytes
    ///   Maximum number of input bytes to use for the lookup.
    /// @return
    ///   Set of matching types.
    virtual set<TTypeInfo> GuessDataType(set<TTypeInfo>& known_types,
                                         size_t max_length = 16,
                                         size_t max_bytes  = 1024*1024);
    // root reader

    /// Read object of know type
    void Read(const CObjectInfo& object);
    /// Read object of know type
    void Read(TObjectPtr object, TTypeInfo type);
    /// Read object of know type
    CObjectInfo Read(const CObjectTypeInfo& type);
    /// Read object of know type
    CObjectInfo Read(TTypeInfo type);
    /// Skip object of know type
    void Skip(const CObjectTypeInfo& type);
    /// Skip object of know type
    void Skip(TTypeInfo type);

    /// Read file header
    ///
    /// Text data files have data type name in the very beginning of a file.
    /// By inspecting the header, we know what to expect next.
    /// Binary ASN.1 input files have no such information; so, the data type
    /// should be known in advance.
    ///
    /// @return
    ///   Data type name
    virtual string ReadFileHeader(void);
    
    /// Read file header and compare the type name with the expected one
    ///
    /// @param typeInfo
    ///   Expected data type
    void SkipFileHeader(TTypeInfo typeInfo);

    enum ENoFileHeader {
        eNoFileHeader
    };
    
    /// Read object of know type when the file header is already read
    void Read(const CObjectInfo& object, ENoFileHeader noFileHeader);

    /// Read object of know type when the file header is already read
    void Read(TObjectPtr object, TTypeInfo type, ENoFileHeader noFileHeader);

    /// Skip object of know type when the file header is already read
    void Skip(TTypeInfo type, ENoFileHeader noFileHeader);

    /// Read child object
    ///
    /// Newly created child object will be instantiated as a member
    /// of its parent object.
    void ReadObject(const CObjectInfo& object);

    /// Read child object
    ///
    /// Newly created child object will be instantiated as a member
    /// of its parent object.
    void ReadObject(TObjectPtr object, TTypeInfo typeInfo);

    /// Skip child object
    void SkipObject(const CObjectTypeInfo& objectType);
    /// Skip child object
    void SkipObject(TTypeInfo typeInfo);

    /// Temporary reader
    ///
    /// Method instantiates the child object in the local temporary variable only,
    /// the corresponding data member in the parent object is set to an appropriate null
    /// representation for that data type.
    /// An attempt to reference this child object after exiting the scope where it was
    /// created generates an error.
    void ReadSeparateObject(const CObjectInfo& object);

    // member
    void ReadClassMember(const CObjectInfoMI& member);

    // variant
    void ReadChoiceVariant(const CObjectInfoCV& object);

    /// Discard the object, which has been just read.
    ///
    /// Call this function inside hooks to discard the object,
    /// which has been just read.
    /// Such an object was created before the hook function was called,
    /// and can be deleted only after the hook processing completes.
    /// The option lets save memory when processing large amount of data.
    /// Please keep in mind though, that the 'root' object constructed by
    /// such read operation will be invalid.
    void SetDiscardCurrObject(bool discard=true)
        {m_DiscardCurrObject = discard;}
    bool GetDiscardCurrObject(void) const
        {return m_DiscardCurrObject;}

    /// Peek next data type name in XML stream
    virtual string PeekNextTypeName(void);
//---------------------------------------------------------------------------
// Standard type readers
    // bool
    void ReadStd(bool& data);
    void SkipStd(const bool &);

    // char
    void ReadStd(char& data);
    void SkipStd(const char& );

    // integer numbers
    void ReadStd(signed char& data);
    void ReadStd(unsigned char& data);
    void SkipStd(const signed char& );
    void SkipStd(const unsigned char& );
    void ReadStd(short& data);
    void ReadStd(unsigned short& data);
    void SkipStd(const short& );
    void SkipStd(const unsigned short& );
    void ReadStd(int& data);
    void ReadStd(unsigned& data);
    void SkipStd(const int& );
    void SkipStd(const unsigned& );
#ifndef NCBI_INT8_IS_LONG
    void ReadStd(long& data);
    void ReadStd(unsigned long& data);
    void SkipStd(const long& );
    void SkipStd(const unsigned long& );
#endif
    void ReadStd(Int8& data);
    void ReadStd(Uint8& data);
    void SkipStd(const Int8& );
    void SkipStd(const Uint8& );

    // float numbers
    void ReadStd(float& data);
    void ReadStd(double& data);
    void SkipStd(const float& );
    void SkipStd(const double& );
#if SIZEOF_LONG_DOUBLE != 0
    virtual void ReadStd(long double& data);
    virtual void SkipStd(const long double& );
#endif

    // string
    void ReadStd(string& data);
    void SkipStd(const string& );

    // C string
    void ReadStd(char* & data);
    void ReadStd(const char* & data);
    void SkipStd(char* const& );
    void SkipStd(const char* const& );

    void ReadStd(CBitString& data);
    void SkipStd(CBitString& data);

    // primitive readers
    // bool
    virtual bool ReadBool(void) = 0;
    virtual void SkipBool(void) = 0;

    // char
    virtual char ReadChar(void) = 0;
    virtual void SkipChar(void) = 0;

    // integer numbers
    virtual Int1 ReadInt1(void);
    virtual Uint1 ReadUint1(void);
    virtual Int2 ReadInt2(void);
    virtual Uint2 ReadUint2(void);
    virtual Int4 ReadInt4(void);
    virtual Uint4 ReadUint4(void);
    virtual Int8 ReadInt8(void) = 0;
    virtual Uint8 ReadUint8(void) = 0;

    virtual void SkipInt1(void);
    virtual void SkipUint1(void);
    virtual void SkipInt2(void);
    virtual void SkipUint2(void);
    virtual void SkipInt4(void);
    virtual void SkipUint4(void);
    virtual void SkipInt8(void);
    virtual void SkipUint8(void);

    virtual void SkipSNumber(void) = 0;
    virtual void SkipUNumber(void) = 0;

    // float numbers
    virtual float ReadFloat(void);
    virtual double ReadDouble(void) = 0;
    virtual void SkipFloat(void);
    virtual void SkipDouble(void);
#if SIZEOF_LONG_DOUBLE != 0
    virtual long double ReadLDouble(void);
    virtual void SkipLDouble(void);
#endif
    virtual void SkipFNumber(void) = 0;

    // string
    virtual void ReadString(string& s,
                            EStringType type = eStringTypeVisible) = 0;
    virtual void ReadPackedString(string& s,
                                  CPackString& pack_string,
                                  EStringType type = eStringTypeVisible);
    virtual void SkipString(EStringType type = eStringTypeVisible) = 0;

    // StringStore
    virtual void ReadStringStore(string& s);
    virtual void SkipStringStore(void);
    
    // C string
    virtual char* ReadCString(void);
    virtual void SkipCString(void);

    // null
    virtual void ReadNull(void) = 0;
    virtual void SkipNull(void) = 0;

    // any content object
    virtual void ReadAnyContentObject(CAnyContentObject& obj) = 0;
    virtual void SkipAnyContentObject(void) = 0;
    virtual void SkipAnyContentVariant(void);

    virtual void ReadBitString(CBitString& obj) = 0;
    virtual void SkipBitString(void) = 0;
    void ReadCompressedBitString(CBitString& data);

    // octet string
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

#ifdef NCBI_STRICT_GI
    void ReadStd(TGi& data);
    void SkipStd(TGi& data);

    virtual void ReadGi(TGi& obj);
    virtual void SkipGi(void);
#endif

//---------------------------------------------------------------------------
// Internals

    // memory pool to use to create new objects when reading data
    void SetMemoryPool(CObjectMemoryPool* memory_pool)
        {
            m_MemoryPool = memory_pool;
        }
    CObjectMemoryPool* GetMemoryPool(void)
        {
            return m_MemoryPool;
        }
    // create and set new memory pool
    void UseMemoryPool(void);

    // internal reader
    void ReadExternalObject(TObjectPtr object, TTypeInfo typeInfo);
    void SkipExternalObject(TTypeInfo typeInfo);

    CObjectInfo ReadObject(void);
    virtual void EndOfRead(void);
    
    // try to read enum value name, "" if none
    virtual TEnumValueType ReadEnum(const CEnumeratedTypeValues& values) = 0;

    void ResetLocalHooks(void);
    bool DetectLoops(void) const;
    void HandleEOF(CEofException&);

    void ThrowError1(const CDiagCompileInfo& diag_info,
                     TFailFlags fail, const char* message);
    void ThrowError1(const CDiagCompileInfo& diag_info,
                     TFailFlags fail, const string& message);
    // report unended block
    void Unended(const string& msg);
    // report unended object stack frame
    virtual void UnendedFrame(void);
    // report class member errors
    void DuplicatedMember(const CMemberInfo* memberInfo);
    bool ExpectedMember(const CMemberInfo* memberInfo);

    // check if m_Input has any more data to read
    // (ANY data, including white spaces and comments)
    bool HaveMoreData(void);

    enum EFlags {
        fFlagNone                = 0,
        eFlagNone                = fFlagNone,
        fFlagAllowNonAsciiChars  = 1 << 0,
        eFlagAllowNonAsciiChars  = fFlagAllowNonAsciiChars,
        fFlagEnforcedStdXml      = 1 << 1,
        eFlagEnforcedStdXml      = fFlagEnforcedStdXml
    };
    typedef int TFlags;
    TFlags GetFlags(void) const;
    TFlags SetFlags(TFlags flags);
    TFlags ClearFlags(TFlags flags);

    class NCBI_XSERIAL_EXPORT ByteBlock
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
    class NCBI_XSERIAL_EXPORT CharBlock
    {
    public:
        CharBlock(CObjectIStream& in);
        ~CharBlock(void);

        void End(void);

        CObjectIStream& GetStream(void) const;

        size_t Read(char* dst, size_t length, bool forceLength = false);

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
    class NCBI_XSERIAL_EXPORT AsnIo
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
public:
#endif
    
//---------------------------------------------------------------------------
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
    MLIOVIR void ReadChoiceSimple(const CChoiceTypeInfo* choiceType,
                                  TObjectPtr choicePtr);
    MLIOVIR void SkipChoiceSimple(const CChoiceTypeInfo* choiceType);

    // alias
    MLIOVIR void ReadAlias(const CAliasTypeInfo* aliasType,
                           TObjectPtr aliasPtr);
    MLIOVIR void SkipAlias(const CAliasTypeInfo* aliasType);

//---------------------------------------------------------------------------
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
    virtual void UndoClassMember(void) {}

    // choice
    virtual void BeginChoice(const CChoiceTypeInfo* choiceType);
    virtual void EndChoice(void);
    virtual TMemberIndex BeginChoiceVariant(const CChoiceTypeInfo* choiceType) = 0;
    virtual void EndChoiceVariant(void);

    // byte block
    virtual void BeginBytes(ByteBlock& block) = 0;
    virtual size_t ReadBytes(ByteBlock& block, char* buffer, size_t count) = 0;
    virtual void EndBytes(const ByteBlock& block);

    // char block
    virtual void BeginChars(CharBlock& block) = 0;
    virtual size_t ReadChars(CharBlock& block, char* buffer, size_t count) = 0;
    virtual void EndChars(const CharBlock& block);

    virtual void StartDelayBuffer(void);
    virtual CRef<CByteSource> EndDelayBuffer(void);
    void EndDelayBuffer(CDelayBuffer& buffer,
                        const CItemInfo* itemInfo, TObjectPtr objectPtr);

    TObjectPtr GetParentObjectPtr(TTypeInfo type,
                                  size_t max_depth = 1,
                                  size_t min_depth = 1) const;

protected:
    CObjectIStream(ESerialDataFormat format);
    CObjectIStream(CNcbiIstream& in, bool deleteIn = false);
    // reset state machine
    virtual void ResetState(void);

    typedef size_t TObjectIndex;
    // low level readers
    pair<TObjectPtr, TTypeInfo> ReadObjectInfo(void);
    virtual EPointerType ReadPointerType(void) = 0;
    virtual TObjectIndex ReadObjectPointer(void) = 0;
    virtual string ReadOtherPointer(void) = 0;
    virtual void ReadOtherPointerEnd(void);

    void RegisterObject(TTypeInfo typeInfo);
    void RegisterObject(TObjectPtr object, TTypeInfo typeInfo);
    const CReadObjectInfo& GetRegisteredObject(TObjectIndex index);
    virtual void x_SetPathHooks(bool set);
    bool x_HavePathHooks() const;
    EFixNonPrint x_GetFixCharsMethodDefault(void) const;
    EFixNonPrint x_FixCharsMethod(void) const {
        return m_FixMethod;
    }

    CIStreamBuffer m_Input;
    bool m_DiscardCurrObject;
    ESerialDataFormat   m_DataFormat;
    EDelayBufferParsing  m_ParseDelayBuffers;
    
private:
    static CObjectIStream* CreateObjectIStreamAsn(void);
    static CObjectIStream* CreateObjectIStreamAsnBinary(void);
    static CObjectIStream* CreateObjectIStreamXml(void);
    static CObjectIStream* CreateObjectIStreamJson(void);

    static CRef<CByteSource> GetSource(ESerialDataFormat format,
                                       const string& fileName,
                                       TSerialOpenFlags openFlags = 0);
    static CRef<CByteSource> GetSource(CNcbiIstream& inStream,
                                       bool deleteInStream = false);

    static ESerialVerifyData  x_GetVerifyDataDefault(void);
    static ESerialSkipUnknown x_GetSkipUnknownDefault(void);
    static ESerialSkipUnknown x_GetSkipUnknownVariantsDefault(void);

    EFixNonPrint m_FixMethod; // method of fixing wrong (eg, non-printable) chars
    ESerialVerifyData   m_VerifyData;
    ESerialSkipUnknown m_SkipUnknown;
    ESerialSkipUnknown m_SkipUnknownVariants;
    AutoPtr<CReadObjectList> m_Objects;

    TFailFlags m_Fail;
    TFlags m_Flags;
    CStreamObjectPathHook<CReadObjectHook*>                m_PathReadObjectHooks;
    CStreamObjectPathHook<CSkipObjectHook*>                m_PathSkipObjectHooks;
    CStreamPathHook<CMemberInfo*, CReadClassMemberHook*>   m_PathReadMemberHooks;
    CStreamPathHook<CMemberInfo*, CSkipClassMemberHook*>   m_PathSkipMemberHooks;
    CStreamPathHook<CVariantInfo*,CReadChoiceVariantHook*> m_PathReadVariantHooks;
    CStreamPathHook<CVariantInfo*,CSkipChoiceVariantHook*> m_PathSkipVariantHooks;

    CRef<CObjectMemoryPool> m_MemoryPool;

    TTypeInfo m_MonitorType;
    vector<TTypeInfo> m_ReqMonitorType;
    
public:
    enum ESpecialCaseRead {
        eReadAsNormal  = 0,
        eReadAsDefault = 1,
        eReadAsNil     = 2
    };
private:
    TConstObjectPtr m_MemberDefault;
    int m_SpecialCaseToExpect;
    ESpecialCaseRead m_SpecialCaseUsed;

    void SetMemberDefault( TConstObjectPtr def)
    {
        m_MemberDefault = def;
        m_SpecialCaseUsed = eReadAsNormal;
        m_SpecialCaseToExpect = def ? eReadAsDefault : eReadAsNormal;
    }
    void SetMemberNillable()
    {
        m_SpecialCaseToExpect = (m_SpecialCaseToExpect | (int)eReadAsNil);
    }
public:
    int ExpectSpecialCase(void) const
    {
        return m_SpecialCaseToExpect;
    }
    TConstObjectPtr GetMemberDefault( void) const
    {
        return m_MemberDefault;
    }
    void SetSpecialCaseUsed(ESpecialCaseRead used)
    {
        m_SpecialCaseUsed = used;
    }
    ESpecialCaseRead GetSpecialCaseUsed(void) const
    {
        return m_SpecialCaseUsed;
    }

    // read hooks
    CLocalHookSet<CReadObjectHook> m_ObjectHookKey;
    CLocalHookSet<CReadClassMemberHook> m_ClassMemberHookKey;
    CLocalHookSet<CReadChoiceVariantHook> m_ChoiceVariantHookKey;
    CLocalHookSet<CSkipObjectHook> m_ObjectSkipHookKey;
    CLocalHookSet<CSkipClassMemberHook> m_ClassMemberSkipHookKey;
    CLocalHookSet<CSkipChoiceVariantHook> m_ChoiceVariantSkipHookKey;

    friend class CObjectStreamCopier;
    friend class CMemberInfoFunctions;
    friend class CClassTypeInfo;
    friend class CAutoPointerTypeInfo;
};

inline
bool GoodVisibleChar(char c);

NCBI_XSERIAL_EXPORT
char ReplaceVisibleChar(char c, EFixNonPrint fix_method,
    const CObjectStack* io, const string& str);

inline
void FixVisibleChar(char& c, EFixNonPrint fix_method,
    const CObjectStack* io, const string& str);


/// Guard class for CObjectIStream::StartDelayBuffer/EndDelayBuffer
///
/// CObjectIStream::StartDelayBuffer() should be followed by 
/// CObjectIStream::EndDelayBuffer() call. If it's not called we have a delay 
/// buffer leak. This class works as an guard (or auto pointer) to avoid call
/// leaks.
class NCBI_XSERIAL_EXPORT CStreamDelayBufferGuard 
{
public:
    /// Construct empty guard instance
    ///
    CStreamDelayBufferGuard(void);
    /// Construct instance on a given CObjectIStream object.
    /// Call istr.StartDelayBuffer()
    ///
    /// @param istr
    ///   Guard protected instance
    CStreamDelayBufferGuard(CObjectIStream& istr);

    ~CStreamDelayBufferGuard(void);


    /// Start deley buffer collection on a given CObjectIStream object.
    /// Call istr.StartDelayBuffer()
    ///
    /// @param istr
    ///   Guard protected instance
    void StartDelayBuffer(CObjectIStream& istr);

    /// Redirect call to protected CObjectIStream
    /// After this call guarding is finished.
    CRef<CByteSource> EndDelayBuffer(void);

    /// Redirect call to protected CObjectIStream
    /// After this call guarding is finished.
    void EndDelayBuffer(CDelayBuffer&    buffer,
                        const CItemInfo* itemInfo, 
                        TObjectPtr       objectPtr);
    
private:
    CStreamDelayBufferGuard(const CStreamDelayBufferGuard&);
    CStreamDelayBufferGuard& operator=(const CStreamDelayBufferGuard& );
private:
    CObjectIStream*  m_ObjectIStream;
};


/* @} */


#include <serial/impl/objistr.inl>

END_NCBI_SCOPE

#endif  /* SERIAL___OBJISTR__HPP */
