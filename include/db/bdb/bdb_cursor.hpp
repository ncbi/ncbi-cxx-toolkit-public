#ifndef BDB___CURSOR_HPP__
#define BDB___CURSOR_HPP__

/* $Id$
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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description: Berkeley DB Cursor.
 *
 */
/// @file bdb_cursor.hpp
/// Berkeley BDB file cursor

#include <db/bdb/bdb_file.hpp>
#include <algorithm>


BEGIN_NCBI_SCOPE

/** @addtogroup BDB_Files
 *
 * @{
 */

class CBDB_FileCursor;
class CBDB_FC_Condition;



/// File cursor condition handle.
///
/// Used for assigning search conditions to a cursor.
///
/// <pre>
///
/// Example:
///
///    CBDB_FileCursor  cursor(bdb_file);
///    cursor.From << 10;
///    cursor.To << 20;
///
/// </pre>
///

class NCBI_BDB_EXPORT CBDB_ConditionHandle
{
public:
    CBDB_ConditionHandle& operator<< (int           val);
    CBDB_ConditionHandle& operator<< (unsigned int  val);
    CBDB_ConditionHandle& operator<< (float         val);
    CBDB_ConditionHandle& operator<< (double        val);
    CBDB_ConditionHandle& operator<< (const char*   val);
    CBDB_ConditionHandle& operator<< (const string& val);

protected:
    CBDB_ConditionHandle(CBDB_FC_Condition& cond);
    ~CBDB_ConditionHandle();

    CBDB_FC_Condition& m_Condition;

    friend class CBDB_FileCursor;
};



/// Berkeley DB file cursor class.
///
/// BDB btree cursors can retrieve values using FROM-TO range criteria.
///

class NCBI_BDB_EXPORT CBDB_FileCursor
{
public:
    // Cursor search conditions
    enum ECondition {
        eNotSet,
        eFirst,
        eLast,
        eEQ,
        eGT,
        eGE,
        eLT,
        eLE
    };

    enum EFetchDirection {
        eForward,
        eBackward,
        eCurrent,
        eDefault
    };

    /// Type of locking when fetching records
    enum ECursorUpdateType {
        eReadUpdate,        ///< Default mode: optional update after read
        eReadModifyUpdate   ///< Use DB_RMW (write locking) on fetch
    };

    typedef unsigned long TRecordCount;

public:
    CBDB_FileCursor(CBDB_File& dbf, ECursorUpdateType utype = eReadUpdate);
    CBDB_FileCursor(CBDB_File& dbf,
                    CBDB_Transaction& trans,
                    ECursorUpdateType utype = eReadUpdate);

    ~CBDB_FileCursor();

    /// Fetch mode regulates multi-row fetches
    /// eFetchAll (default mode) when buffer ends, cursor automatically
    /// reads the next buffer
    /// eFetchGetBufferEnds - returns eBDB_MultiRowEnd every time
    /// cursor needs to read from disk
    enum EMultiFetchMode
    {
        eFetchAll,
        eFetchGetBufferEnds
    };

    /// Init multi-row fetch
    ///
    /// @param buffer_size
    ///    Size of fetch buffer
    ///    (at least one page size, multiple of 1024 bytes in size)
    ///
    void InitMultiFetch(size_t buffer_size, EMultiFetchMode mfm = eFetchAll);

    /// Set search condition(type of interval)
    ///
    /// @note
    /// SetCondition resets cursor value assignments (From, To) so
    /// should be always called before "cur.From << value ..."
    ///
    void SetCondition(ECondition cond_from, ECondition cond_to = eNotSet);

    void SetFetchDirection(EFetchDirection fdir);
    EFetchDirection GetFetchDirection() const { return m_FetchDirection; }
    EFetchDirection GetReverseFetchDirection() const;
    void ReverseFetchDirection();

    EBDB_ErrCode FetchFirst();

    /// Fetch BLOB
    EBDB_ErrCode FetchFirst(void**       buf,
                            size_t       buf_size,
                            CBDB_RawFile::EReallocMode allow_realloc);

    /// Fetch BLOB
    EBDB_ErrCode FetchFirst(CBDB_RawFile::TBuffer*  buf);


    /// Fetch record
    ///
    /// @note When fetching current record (eCurrent) and record has been
    /// deleted by other thread or program return code is eBDB_KeyEmpty
    ///
    EBDB_ErrCode Fetch(EFetchDirection fdir = eDefault);

    /// Fetch BLOB
    ///
    /// @note When fetching current record (eCurrent) and record has been
    /// deleted by other thread or program return code is eBDB_KeyEmpty
    ///
    EBDB_ErrCode Fetch(EFetchDirection fdir,
                       void**       buf,
                       size_t       buf_size,
                       CBDB_RawFile::EReallocMode allow_realloc);
    /// Fetch BLOB
    EBDB_ErrCode Fetch(CBDB_RawFile::TBuffer*  buf,
                       EFetchDirection fdir = eDefault);

    EBDB_ErrCode Update(CBDB_File::EAfterWrite write_flag
                                    = CBDB_File::eDiscardData);
    EBDB_ErrCode Delete(CBDB_File::EIgnoreError on_error =
                        CBDB_File::eThrowOnError);

    EBDB_ErrCode UpdateBlob(const void* data,
                            size_t size,
                            CBDB_File::EAfterWrite write_flag
                                = CBDB_File::eDiscardData);


    /// Get data pointer from last fetch (only when in multifetch mode)
    ///
    const void* GetLastMultiFetchData() const;

    /// Get data length from last fetch (only when in multifetch mode)
    ///
    size_t GetLastMultiFetchDataLen() const;

    // Returns number of records with same key as this cursor refers
    TRecordCount KeyDupCount() const;

    /// Return database file on which cursor is based
    CBDB_File& GetDBFile() { return m_Dbf; }

    /// Close underlying cursor.
    /// All associated buffers remain, so cursor can be quickly
    /// reopened.
    void Close();

    /// Reopen cursor after Close.
    ///
    /// @param trans
    ///    Transaction pointer (optional)
    ///      (ownership is NOT be taken.)
    void ReOpen(CBDB_Transaction* trans);

    /// TRUE when cursor open
    bool IsOpen() const;

protected:
    /// Test "TO" search criteria. Return "true" if current value satisfies it
    bool TestTo() const;

    /// Set m_FirstFetched field to FALSE.
    void ResetFirstFetched();

    /// Return next field's IBDB_FieldConvert interface
    /// (hidden cast to non-public parent class)
    IBDB_FieldConvert& GetFieldConvert(CBDB_BufferManager& buf,
                                       unsigned int n);

protected:
    /// Reference on the "mother" file
    CBDB_File&                   m_Dbf;

public:
    CBDB_ConditionHandle  From;
    CBDB_ConditionHandle  To;

private:
    /// forbidden
    CBDB_FileCursor(const CBDB_FileCursor&);
    CBDB_FileCursor& operator= (const CBDB_FileCursor&);
private:
    void x_FetchFirst_Prolog(unsigned int& flag);

private:
    /// Berkeley DB DBC thing
    DBC*                   m_DBC;
    /// From condition proxy-object
    ECondition             m_CondFrom;
    /// To condition proxy-object
    ECondition             m_CondTo;
    /// Fetch direction (forward/backward)
    EFetchDirection        m_FetchDirection;
    /// Flag if FetchFirst is already been done
    bool                   m_FirstFetched;
    /// Type of locking (conventional or RMW)
    unsigned int           m_FetchFlags;
    /// Buffer class for multiple fetch
    CBDB_MultiRowBuffer*   m_MultiRowBuf;
    /// Multifetch control mode
    EMultiFetchMode        m_MultiFetchMode;
    /// when true, last multifetch was successfull
    bool                   m_LastMultiFetchSuccess;

    friend class CBDB_FC_Condition;
};

/// BDB Cursor guard. Automatically closes cursor, when goes out of scope.
///
class CBDB_CursorGuard
{
public:
    CBDB_CursorGuard(CBDB_FileCursor& cur) : m_Cur(cur) {}
    ~CBDB_CursorGuard() { m_Cur.Close(); }
private:
    CBDB_CursorGuard(CBDB_CursorGuard&);
    CBDB_CursorGuard& operator=(const CBDB_CursorGuard&);
private:
    CBDB_FileCursor& m_Cur;
};


/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  CBDB_FileCursor::
//


inline void CBDB_FileCursor::ResetFirstFetched()
{
    m_FirstFetched = false;
}


inline
IBDB_FieldConvert& CBDB_FileCursor::GetFieldConvert(CBDB_BufferManager& buf,
                                                    unsigned int        n)
{
    return static_cast<IBDB_FieldConvert&> (buf.GetField(n));
}

inline
void CBDB_FileCursor::SetFetchDirection(EFetchDirection fdir)
{
    m_FetchDirection = fdir;
}

inline
CBDB_FileCursor::EFetchDirection
CBDB_FileCursor::GetReverseFetchDirection() const
{
    if (m_FetchDirection == eForward) return eBackward;
    return eForward;
}

inline
void CBDB_FileCursor::ReverseFetchDirection()
{
    if (m_FetchDirection == eForward) m_FetchDirection = eBackward;
    if (m_FetchDirection == eBackward) m_FetchDirection = eForward;
}

END_NCBI_SCOPE

#endif
