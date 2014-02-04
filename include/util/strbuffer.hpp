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
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/util_exception.hpp>
#include <util/icanceled.hpp>
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
    CIStreamBuffer(void);
    CIStreamBuffer(const char* buffer, size_t size);
    ~CIStreamBuffer(void);
    
    bool fail(void) const;
    void ResetFail(void);
    const char* GetError(void) const;

    void Open(CByteSourceReader& reader);
    void Open(const char* buffer, size_t size);
    void Close(void);

    // Set cancellation check callback.
    // The CIStreamBuffer will throw an exception when the callback return
    // cancel request. The callback is called from FillBuffer().
    void SetCanceledCallback(const ICanceled* callback);

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
    // returns true if succeeded
    bool TrySetCurrentPos(const char* pos);

    // return: current line counter
    size_t GetLine(void) const THROWS1_NONE;

    /// @deprecated
    ///   Use GetStreamPos() instead
    /// @sa GetStreamPos()
    NCBI_DEPRECATED CNcbiStreampos GetStreamOffset(void) const THROWS1_NONE;

    /// @deprecated
    ///  Use SetStreamPos() instead
    /// @sa SetStreamPos() 
    NCBI_DEPRECATED void SetStreamOffset(CNcbiStreampos pos);

    CNcbiStreampos GetStreamPos(void) const THROWS1_NONE;
    Int8 GetStreamPosAsInt8(void) const;
    void   SetStreamPos(CNcbiStreampos pos);
    
    // action: read in buffer up to end of line
    size_t ReadLine(char* buff, size_t size)
        THROWS1((CIOException));

    char SkipWs(void);
    Int4 GetInt4(void)
        THROWS1((CIOException,CUtilException));
    Uint4 GetUint4(void)
        THROWS1((CIOException,CUtilException));
    Int8 GetInt8(void)
        THROWS1((CIOException,CUtilException));
    Uint8 GetUint8(void)
        THROWS1((CIOException,CUtilException));

    void StartSubSource(void);
    CRef<CByteSource> EndSubSource(void);
    CRef<CSubSourceCollector>& GetSubSourceCollector(void)
    {
        return m_Collector;
    }
    bool EndOfData(void) const
    {
        return m_CurrentPos >= m_DataEndPos &&
            (!m_Input || m_Input->EndOfData());
    }

    // Setting buffer lock to a non-zero value locks read data until unlocked.
    // The argument is the maximum size for the input buffer in bytes.
    // If the reading would pass beyond the maximum size while locked,
    // the reading will fail with the CIOException::eOverflow.
    // The method returns current position in buffer to be used
    // as an argument to ResetBufferLock().
    size_t SetBufferLock(size_t size);
    // Unlock buffer and restore buffer position.
    void ResetBufferLock(size_t pos);

protected:
    // action: fill buffer so *pos char is valid
    // return: new value of pos pointer if buffer content was shifted
    const char* FillBuffer(const char* pos, bool noEOF = false)
        THROWS1((CIOException, bad_alloc));
    char FillBufferNoEOF(const char* pos);
    bool TryToFillBuffer(void);

    // report number parsing exceptions
    void BadNumber(void)
        THROWS1((CUtilException));
    void NumberOverflow(void)
        THROWS1((CUtilException));

private:
    CRef<CByteSourceReader> m_Input;

    const char* m_Error;

    Int8 m_BufferPos; // offset of current buffer in source stream
    size_t m_BufferSize;      // buffer size, 0 if the buffer is external
    char* m_Buffer;           // buffer pointer
    const char* m_CurrentPos; // current char position in buffer
    const char* m_DataEndPos; // end of valid content in buffer
    size_t m_Line;            // current line counter

    const char* m_CollectPos;
    CRef<CSubSourceCollector> m_Collector;

    CConstIRef<ICanceled> m_CanceledCallback;
    size_t m_BufferLockSize;
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

    // Set cancellation check callback.
    // The COStreamBuffer will throw an exception when the callback return
    // cancel request. The callback is called from FlushBuffer().
    void SetCanceledCallback(const ICanceled* callback);

    // return: current line counter
    size_t GetLine(void) const THROWS1_NONE;
    // deprecated; use GetStreamPos instead
    NCBI_DEPRECATED CNcbiStreampos GetStreamOffset(void) const THROWS1_NONE;
    CNcbiStreampos GetStreamPos(void) const THROWS1_NONE;
    Int8 GetStreamPosAsInt8(void) const;

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
    void SetUseEol(bool set);
    bool GetUseEol(void) const;

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

    Int8 m_BufferPos; // offset of current buffer in source stream
    char* m_Buffer;           // buffer pointer
    char* m_CurrentPos;       // current char position in buffer
    char* m_BufferEnd;       // end of valid content in buffer
    size_t m_Line;            // current line counter
    size_t m_LineLength;
    size_t m_BackLimit;
    bool m_UseIndentation;
    bool m_UseEol;

    CConstIRef<ICanceled> m_CanceledCallback;
};


/* @} */


#include <util/strbuffer.inl>

END_NCBI_SCOPE

#endif
