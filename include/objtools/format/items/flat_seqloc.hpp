#ifndef OBJTOOLS_FORMAT_ITEMS___FLAT_SEQLOC__HPP
#define OBJTOOLS_FORMAT_ITEMS___FLAT_SEQLOC__HPP

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
*
* File Description:
*   new (early 2003) flat-file generator -- location representation
*
*/

#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// forward declarations
class CInt_fuzz;
class CSeq_id;
class CSeq_interval;
class CSeq_loc;
class CBioseqContext;


class CFlatGapLoc : public CSeq_loc
{
public:
    typedef TSeqPos TLength;

    CFlatGapLoc(TLength value) : m_Length(value) { SetNull(); }

    TLength GetLength(void) const { return m_Length; }
    void SetLength(const TLength& value) { m_Length = value; }

private:
    TLength m_Length;
};


class CFlatSeqLoc : public CObject // derived from CObject to allow for caching
{
public:
    enum EType
    {
        eType_location,     // Seq-loc
        eType_assembly      // Genome assembly
    };
    typedef EType     TType;
    
    CFlatSeqLoc(const CSeq_loc& loc, CBioseqContext& ctx, 
        TType type = eType_location);

    const string&     GetString(void)    const { return m_String;    }
    
private:
    bool x_Add(const CSeq_loc& loc, CNcbiOstrstream& oss,
        CBioseqContext& ctx, TType type, bool show_comp);
    bool x_Add(const CSeq_interval& si, CNcbiOstrstream& oss,
        CBioseqContext& ctx, TType type, bool show_comp);
    bool x_Add(const CSeq_point& pnt, CNcbiOstrstream& oss,
        CBioseqContext& ctx, TType type, bool show_comp);
    bool x_Add(TSeqPos pnt, const CInt_fuzz* fuzz, CNcbiOstrstream& oss,
        bool html = false);
    void x_AddID(const CSeq_id& id, CNcbiOstrstream& oss,
        CBioseqContext& ctx, TType type);

    // data
    string     m_String;    // whole location, as a GB-style string
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2004/08/19 16:43:15  shomrat
* changed return type from void to bool for x_Add methods
*
* Revision 1.4  2004/04/22 15:36:25  shomrat
* Changes in context
*
* Revision 1.3  2004/03/05 18:50:25  shomrat
* clean code
*
* Revision 1.2  2004/02/19 17:53:47  shomrat
* add flag to differentiate between loaction and genome assembly formatting
*
* Revision 1.1  2003/12/17 19:47:33  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_FORMAT_ITEMS___FLAT_SEQLOC__HPP */
