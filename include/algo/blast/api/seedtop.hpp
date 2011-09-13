#ifndef ALGO_BLAST_API___SEEDTOP__HPP
#define ALGO_BLAST_API___SEEDTOP__HPP

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
 * Authors:  Ning Ma
 *
 */

/// @file seedtop.hpp
/// Declares the CSeedTop class.


/** @addtogroup AlgoBlast
 *
 * @{
 */

#include <corelib/ncbistd.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/api/blast_seqinfosrc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

class CSeedTop : public CObject {
public:
    // the return type for seedtop search
    // a vector of results (matches) as seq_loc on each subject
    // the results will be sorted first by subject oid (if multiple subject
    // sequences or database is supplied during construction), then by the first
    // posotion of the match
    typedef vector < CConstRef <CSeq_loc> > TSeedTopResults;

    // constructor 
    CSeedTop(const string & pattern);       // seedtop pattern

    // search a database or a set of subject sequences
    TSeedTopResults Run(CRef<CLocalDbAdapter> db);  

    // search a bioseq
    TSeedTopResults Run(CBioseq_Handle & b_hdl);
    
private:
    const static EBlastProgramType m_Program = eBlastTypePhiBlastp;
    string m_Pattern; 
    CLookupTableWrap m_Lookup;
    CBlastScoreBlk m_ScoreBlk;

    void x_MakeLookupTable();
    void x_MakeScoreBlk();
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


#endif  /* ALGO_BLAST_API___SEEDTOP__HPP */
