#ifndef OBJTOOLS_FORMAT___TEXT_OSTREAM_HPP
#define OBJTOOLS_FORMAT___TEXT_OSTREAM_HPP

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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*   Output streams interface for formatted items.
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/serialbase.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class IFlatItem;


/////////////////////////////////////////////////////////////////////////////
//
// TextOStream

class IFlatTextOStream : public CObject
{
public:

    enum EAddNewline {
        eAddNewline_Yes = 1,
        eAddNewline_No
    };

    // TODO: for SC-9, make this pure virtual
    /** This adds a list of strings to the stream one at a time, unconditionally adding a newline to each one. */
    virtual void AddParagraph(const list<string>&  text,
                              const CSerialObject* obj = 0) 
    {
        // must be overridden
        _TROUBLE;
    }

    // TODO: for SC-9, make this pure virtual
    /** This adds its given argument, appending a newline only if the add_newline argument is eAddNewline_Yes. */
    virtual void AddLine( const CTempString& line,
                          const CSerialObject* obj = 0,
                          EAddNewline add_newline = eAddNewline_Yes )
    {
        // must be overridden
        _TROUBLE;
    }

    // TODO: for SC-9, remove this function
    /**
     * This function is deprecated because I wanted to remove it. but could
     * not since some other code still depends on it.
     *
     * This adds its argument "line" without any changes whatsoever,
     * except for adding a newline after the "line".
     */
    NCBI_DEPRECATED
    virtual void AddCLine( const char *line,
                           const CSerialObject* obj = 0) 
    {
        AddLine(line, obj, eAddNewline_Yes);
    }

    // TODO: for SC-9, remove this function
    /**
     * This function is deprecated because I wanted to remove it. but could
     * not since some other code still depends on it.
     *
     * This adds its argument "line" without any changes whatsoever.
     */
    NCBI_DEPRECATED
    virtual void AddRawText( const char *line,
                             const CSerialObject* obj = 0) 
    { 
        AddLine(line, obj, eAddNewline_No);
    }

    virtual ~IFlatTextOStream(void) {}
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___TEXT_OSTREAM_HPP */
