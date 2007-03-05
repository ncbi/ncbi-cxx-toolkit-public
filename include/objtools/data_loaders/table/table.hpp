#ifndef OBJTOOLS_DATA_LOADERS_TABLE___TABLE__HPP
#define OBJTOOLS_DATA_LOADERS_TABLE___TABLE__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE


//
// class ITableIterator defines a forward iterator over a set of rows
// the set of rows is defined by some action on the table data itself
//

class ITableIterator : public CObject
{
public:

    // virtual dtor
    virtual ~ITableIterator() { }

    // retrieve the current row's data
    virtual void GetRow(list<string>& data) const = 0;

    // advance to the next row
    virtual void Next(void) = 0;

    // check to see if the current iterator is valid
    virtual bool IsValid(void) const = 0;

    // pre-increment for advance
    // this is implemented here as operators can't be virtual
    ITableIterator& operator++(void)
    {
        Next();
        return *this;
    }

    // operator bool for validity checking
    // this is implemented here as operators can't be virtual
    DECLARE_OPERATOR_BOOL(IsValid());
};


//
// class ITableData defines an abstraction for dealing with tabular structures
//
class ITableData : public CObject
{
public:
    typedef CRef<ITableIterator> TIterator;

    virtual ~ITableData() { }

    // retrieve the number of rows in the table
    virtual int GetNumRows(void) const = 0;

    // retrieve the number of columns in the table
    virtual int GetNumCols(void) const = 0;

    // retrieve a single, numbered row
    virtual void GetRow        (int row, list<string>& data) const = 0;

    // retrieve the title for a given column
    virtual void GetColumnTitle(int col, string& title) const = 0;

    // retrieve a list of the columns in the table
    virtual void GetColumnTitles(list<string>& titles) const = 0;

    // access an iterator for the entire table
    virtual TIterator Begin(void) = 0;

    // obtain an iterator valid for a given ID
    virtual TIterator Begin(const objects::CSeq_id& id) = 0;

    // obtain an iterator valid for a given location
    virtual TIterator Begin(const objects::CSeq_loc& id) = 0;

    // obtain an iterator valid for a given SQL query
    virtual TIterator Begin(const string& query) = 0;
};


END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_TABLE___TABLE__HPP
