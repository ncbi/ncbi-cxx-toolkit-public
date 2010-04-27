/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

Author: Tom Madden

******************************************************************************/

/** @file vecscreen_run.hpp
 * 
*/

#ifndef VECSCREEN_RUN__HPP
#define VECSCREEN_RUN__HPP

#include <corelib/ncbi_limits.hpp>
#include <util/range.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/align_format/align_format_util.hpp>
#include <objtools/align_format/vectorscreen.hpp>

BEGIN_NCBI_SCOPE
using namespace ncbi::align_format;
using namespace ncbi::objects;

/// This class runs vecscreen
class NCBI_XBLASTFORMAT_EXPORT CVecscreenRun
{
public:

     /// Summary of hits.
     struct SVecscreenSummary {
        /// Seq-id of query.
        const CSeq_id* seqid; 
        /// range of match.
        CRange<TSeqPos> range;
        /// Categorizes strength of match.
        string match_type;
     };

     /// Constructor
     ///@param seq_loc sequence locations to screen.
     ///@param scope CScope used to fetch sequence on seq_loc
     ///@param db Database to screen with (UniVec is default).
     CVecscreenRun(CRef<CSeq_loc> seq_loc, CRef<CScope> scope, const string & db = string("UniVec"));

     /// Destructor 
     ~CVecscreenRun() {delete m_Vecscreen;}

     /// Fetches summary list
     list<SVecscreenSummary> GetList() const;

     /// Fetches seqalign-set already processed by vecscreen.
     CRef<objects::CSeq_align_set> GetSeqalignSet() const;
     

private:

     /// Seq-loc to screen
     CRef<CSeq_loc> m_SeqLoc;
     /// Scope used to fetch query.
     CRef<CScope> m_Scope;
     /// vecscreen instance for search.
     CVecscreen* m_Vecscreen;
     /// Database to use (UniVec is default).
     string m_DB;
     /// Processed Seq-align
     CRef<objects::CSeq_align_set> m_Seqalign_set;

     /// Prohibit copy constructor
     CVecscreenRun(const CVecscreenRun&);
     /// Prohibit assignment operator
     CVecscreenRun & operator=(const CVecscreenRun&);
};

END_NCBI_SCOPE

#endif /* VECSCREEN_RUN__HPP */

