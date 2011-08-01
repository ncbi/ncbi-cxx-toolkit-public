#ifndef NGALIGN_ALIGNMENT_FILTERER__HPP
#define NGALIGN_ALIGNMENT_FILTERER__HPP

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
 * Authors:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Na_strand.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>

#include <algo/align/util/align_filter.hpp>

#include <algo/align/ngalign/ngalign_interface.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_align;
    class CSeq_align_set;
    class CSeq_id;
END_SCOPE(objects)

BEGIN_SCOPE(blast)
    struct SSeqLoc;
END_SCOPE(blast)



class CQueryFilter : public IAlignmentFilter
{
public:
    CQueryFilter(int Rank, const string& Query);

    void FilterAlignments(TAlignResultsRef In, TAlignResultsRef Out);
    unsigned int GetFilterRank() const { return m_Rank; }

private:

    int m_Rank;
    const string& m_Query;
    CRef<CAlignFilter> m_Filter;

};





END_NCBI_SCOPE

#endif
