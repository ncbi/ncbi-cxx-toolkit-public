#ifndef BDB_CURSOR_HPP__
#define BDB_CURSOR_HPP__

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
 * File Description: Berkeley DB support library. 
 *                   Berkeley DB Cursor.
 *
 */

#include "bdb_file.hpp"
#include <algorithm>


BEGIN_NCBI_SCOPE


class CBDB_FileCursor;
class CBDB_FC_Condition;



//////////////////////////////////////////////////////////////////
//
// File cursor condition handle.
//
// Used for assigning search conditions to a cursor. 
//
// <pre>
//
// Example:
// 
//    CBDB_FileCursor  cursor(bdb_file);
//    cursor.From << 10;
//    cursor.To << 20;
//
// </pre>
//

class CBDB_ConditionHandle
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



//////////////////////////////////////////////////////////////////
//
// Berkeley DB file cursor class. 
//
// BDB btree cursors can retrieve values using FROM-TO range criteria.
//

class CBDB_FileCursor
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
        eDefault
    };

public:
    CBDB_FileCursor(CBDB_File& dbf);
    ~CBDB_FileCursor();

    void SetCondition(ECondition cond_from, ECondition cond_to = eNotSet);

    EBDB_ErrCode FetchFirst();    
    EBDB_ErrCode Fetch(EFetchDirection fdir = eDefault);

protected:
    // Test "TO" search criteria. Return "true" if current value satisfies it
    bool TestTo() const;

    // Set m_FirstFetched field to FALSE.
    void ResetFirstFetched();

    // Return next field's IBDB_FieldConvert interface 
    // (hidden cast to non-public parent class)
    IBDB_FieldConvert& GetFieldConvert(CBDB_BufferManager& buf, unsigned int n);

protected:
    // Reference on the "mother" file
    CBDB_File&                   m_Dbf;
       
public:
    CBDB_ConditionHandle  From;
    CBDB_ConditionHandle  To;

private:
    CBDB_FileCursor(const CBDB_FileCursor&);
    CBDB_FileCursor& operator= (const CBDB_FileCursor&);

private:
    // Berkeley DB DBC thing
    DBC*                   m_DBC;
    // From condition proxy-object
    ECondition             m_CondFrom;
    // To condition proxy-object
    ECondition             m_CondTo;
    // Fetch direction (forward/backward)
    EFetchDirection        m_FetchDirection;
    // Flag if FetchFirst is already been done
    bool                   m_FirstFetched;

    friend class CBDB_FC_Condition;
};




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



END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/04/24 16:31:16  kuznets
 * Initial revision
 *
 * ===========================================================================
 */


#endif