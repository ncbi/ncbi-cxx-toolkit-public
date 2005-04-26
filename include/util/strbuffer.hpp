#ifndef STRBUFFER__HPP
#define STRBUFFER__HPP

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
*   Reading buffer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.34  2005/04/26 14:11:04  vasilche
* Implemented optimized reading methods CSkipExpected*() and GetChars(string&).
*
* Revision 1.33  2005/02/23 21:06:13  vasilche
* Added HasMore().
*
* Revision 1.32  2004/08/30 18:14:23  gouriano
* use CNcbiStreamoff instead of size_t for stream offset operations
*
* Revision 1.31  2004/05/24 18:12:44  gouriano
* In text output files make indentation optional
*
* Revision 1.30  2003/12/31 20:52:17  gouriano
* added possibility to seek (when possible) in CByteSourceReader
*
* Revision 1.29  2003/04/17 17:50:32  siyan
* Added doxygen support
*
* Revision 1.28  2003/02/26 21:34:06  gouriano
* modify C++ exceptions thrown by this library
*
* Revision 1.27  2002/12/19 14:51:00  dicuccio
* Added export specifier for Win32 DLL builds.
*
* Revision 1.26  2002/11/04 21:29:00  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.25  2002/01/29 16:21:20  grichenk
* COStreamBuffer destructor fixed - no exceptions thrown
*
* Revision 1.24  2001/05/17 15:01:19  lavr
* Typos corrected
*
* Revision 1.23  2001/03/14 17:59:24  vakatov
* COStreamBuffer::  renamed GetFreeSpace() -> GetAvailableSpace()
* to avoid clash with MS-Win system headers' #define
*
* Revision 1.22  2001/01/05 20:08:53  vasilche
* Added util directory for various algorithms and utility classes.
*
* Revision 1.21  2000/12/26 22:23:45  vasilche
* Fixed errors of compilation on Mac.
*
* Revision 1.20  2000/12/15 15:38:02  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.19  2000/10/20 15:51:28  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.18  2000/10/13 20:59:12  vasilche
* Avoid using of ssize_t absent on some compilers.
*
* Revision 1.17  2000/10/13 20:22:47  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.16  2000/08/15 19:44:42  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.15  2000/07/03 20:47:18  vasilche
* Removed unused variables/functions.
*
* Revision 1.14  2000/06/16 20:01:21  vasilche
* Avoid use of unexpected_exception() which is unimplemented on Mac.
*
* Revision 1.13  2000/06/16 16:31:08  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.12  2000/06/01 19:06:59  vasilche
* Added parsing of XML data.
*
* Revision 1.11  2000/05/24 20:08:16  vasilche
* Implemented XML dump.
*
* Revision 1.10  2000/05/03 14:38:06  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.9  2000/04/28 16:58:03  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.8  2000/04/13 14:50:18  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.7  2000/04/10 21:01:40  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.6  2000/04/06 16:10:52  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.5  2000/03/07 14:05:33  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.4  2000/02/17 20:02:29  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.3  2000/02/11 17:10:20  vasilche
* Optimized text parsing.
*
* Revision 1.2  2000/02/02 19:07:38  vasilche
* Added THROWS_NONE to constructor/destructor of exception.
*
* Revision 1.1  2000/02/01 21:44:36  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/util_exception.hpp>
#include <string.h>

#include <util/bytesrc.hpp>


/** @addtogroup StreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CByteSource;
class CByteSourceReader;
class CSubSourceCollector;

#define THROWS1(arg)
#define THROWS1_NONE


class NCBI_XUTIL_EXPORT CIStreamBuffer
{
public:
    CIStreamBuffer(void)
        THROWS1((bad_alloc));
    ~CIStreamBuffer(void);
    
    bool fail(void) const;
    void ResetFail(void);
    const char* GetError(void) const;

    void Open(CByteSourceReader& reader);
    void Close(void);

    char PeekChar(size_t offset = 0)
        THROWS1((CIOException, bad_alloc));
    char PeekCharNoEOF(size_t offset = 0);
    bool HasMore(void);
    char GetChar(void)
        THROWS1((CIOException, bad_alloc));
    // precondition: GetChar or SkipChar was last method called
    void UngetChar(char c);
    // precondition: PeekChar(c) was called when c >= count
    void SkipChars(size_t count);
    // equivalent of SkipChars(1)
    void SkipChar(void);
    bool SkipExpectedChar(char c, size_t offset = 0);
    bool SkipExpectedChars(char c1, char c2, size_t offset = 0);

    // read chars in buffer
    void GetChars(char* buffer, size_t count)
        THROWS1((CIOException));
    // read chars in string
    void GetChars(string& str, size_t count)
        THROWS1((CIOException));
    // skip chars which may not be in buffer
    void GetChars(size_t count)
        THROWS1((CIOException));

    // precondition: last char extracted was either '\r' or '\n'
    // action: increment line count and
    //         extract next complimentary '\r' or '\n' char if any
    void SkipEndOfLine(char lastChar)
        THROWS1((CIOException));
    // action: skip all spaces (' ') and return next non space char
    //         without extracting it
    char SkipSpaces(void)
        THROWS1((CIOException));

    // find specified symbol and set position on it
    void FindChar(char c)
        THROWS1((CIOException));
    // find specified symbol without skipping
    // limit - search by 'limit' symbols
    // return relative offset of symbol from current position
    //     (limit if not found)
    size_t PeekFindChar(char c, size_t limit)
        THROWS1((CIOException));

    const char* GetCurrentPos(void) const THROWS1_NONE;

    // return: current line counter
    size_t GetLine(void) const THROWS1_NONE;
    CNcbiStreamoff GetStreamOffset(void) const THROWS1_NONE;
    void   SetStreamOffset(CNcbiStreamoff pos);
    
    // action: read in buffer up to end of line
    size_t ReadLine(char* buff, size_t size)
        THROWS1((CIOException));

    Int4 GetInt4(void)
        THROWS1((CIOException));
    Uint4 GetUint4(void)
        THROWS1((CIOException));
    Int8 GetInt8(void)
        THROWS1((CIOException));
    Uint8 GetUint8(void)
        THROWS1((CIOException));

    void StartSubSource(void);
    CRef<CByteSource> EndSubSource(void);

protected:
    // action: fill buffer so *pos char is valid
    // return: new value of pos pointer if buffer content was shifted
    char* FillBuffer(char* pos, bool noEOF = false)
        THROWS1((CIOException, bad_alloc));
    char FillBufferNoEOF(char* pos);
    bool TryToFillBuffer(void);

    void BadNumber(void);

private:
    CRef<CByteSourceReader> m_Input;

    const char* m_Error;

    CNcbiStreamoff m_BufferOffset; // offset of current buffer in source stream
    size_t m_BufferSize;      // buffer size
    char* m_Buffer;           // buffer pointer
    char* m_CurrentPos;       // current char position in buffer
    char* m_DataEndPos;       // end of valid content in buffer
    size_t m_Line;            // current line counter

    char* m_CollectPos;
    CRef<CSubSourceCollector> m_Collector;
};

class NCBI_XUTIL_EXPORT COStreamBuffer
{
public:
    COStreamBuffer(CNcbiOstream& out, bool deleteOut = false)
        THROWS1((bad_alloc));
    ~COStreamBuffer(void);

    bool fail(void) const;
    void ResetFail(void);
    const char* GetError(void) const;

    void Close(void);

    // return: current line counter
    size_t GetLine(void) const THROWS1_NONE;
    CNcbiStreamoff GetStreamOffset(void) const THROWS1_NONE;

    size_t GetCurrentLineLength(void) const THROWS1_NONE;

    bool ZeroIndentLevel(void) const THROWS1_NONE;
    size_t GetIndentLevel(size_t step = 2) const THROWS1_NONE;
    void IncIndentLevel(size_t step = 2) THROWS1_NONE;
    void DecIndentLevel(size_t step = 2) THROWS1_NONE;

    void SetBackLimit(size_t limit);

    void FlushBuffer(bool fullBuffer = true) THROWS1((CIOException));
    void Flush(void) THROWS1((CIOException));

    void SetUseIndentation(bool set);
    bool GetUseIndentation(void) const;

protected:
    // flush contents of buffer to underlying stream
    // make sure 'reserve' char area is available in buffer
    // return beginning of area
    char* DoReserve(size_t reserve = 0)
        THROWS1((CIOException, bad_alloc));
    // flush contents of buffer to underlying stream
    // make sure 'reserve' char area is available in buffer
    // skip 'reserve' chars
    // return beginning of skipped area
    char* DoSkip(size_t reserve)
        THROWS1((CIOException, bad_alloc));

    // allocates count bytes area in buffer and skip this area
    // returns beginning of this area
    char* Skip(size_t count)
        THROWS1((CIOException, bad_alloc));
    char* Reserve(size_t count)
        THROWS1((CIOException, bad_alloc));

public:
    void PutChar(char c)
        THROWS1((CIOException));
    void BackChar(char c);

    void PutString(const char* str, size_t length)
        THROWS1((CIOException, bad_alloc));
    void PutString(const char* str)
        THROWS1((CIOException, bad_alloc));
    void PutString(const string& str)
        THROWS1((CIOException, bad_alloc));

    void PutIndent(void)
        THROWS1((CIOException, bad_alloc));

    void PutEol(bool indent = true)
        THROWS1((CIOException, bad_alloc));

    void PutEolAtWordEnd(size_t lineLength)
        THROWS1((CIOException, bad_alloc));

    void WrapAt(size_t lineLength, bool keepWord)
        THROWS1((CIOException, bad_alloc));

    void PutInt4(Int4 v)
        THROWS1((CIOException, bad_alloc));
    void PutUint4(Uint4 v)
        THROWS1((CIOException, bad_alloc));
    void PutInt8(Int8 v)
        THROWS1((CIOException, bad_alloc));
    void PutUint8(Uint8 v)
        THROWS1((CIOException, bad_alloc));

    void Write(const char* data, size_t dataLength)
        THROWS1((CIOException, bad_alloc));
    void Write(CByteSourceReader& reader)
        THROWS1((CIOException, bad_alloc));

private:
    CNcbiOstream& m_Output;
    bool m_DeleteOutput;

    const char* m_Error;

    size_t GetUsedSpace(void) const;
    size_t GetAvailableSpace(void) const;
    size_t GetBufferSize(void) const;

    size_t m_IndentLevel;

    CNcbiStreamoff m_BufferOffset; // offset of current buffer in source stream
    char* m_Buffer;           // buffer pointer
    char* m_CurrentPos;       // current char position in buffer
    char* m_BufferEnd;       // end of valid content in buffer
    size_t m_Line;            // current line counter
    size_t m_LineLength;
    size_t m_BackLimit;
    bool m_UseIndentation;
};


/* @} */


#include <util/strbuffer.inl>

END_NCBI_SCOPE

#endif
