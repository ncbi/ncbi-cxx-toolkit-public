#ifndef ALGO_BLAST_API__SEQINFOSRC_SEQVEC__HPP
#define ALGO_BLAST_API__SEQINFOSRC_SEQVEC__HPP

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
 * Author:  Ilya Dondoshansky
 *
 */

/** @file seqinfosrc_seqvec.hpp
 * Defines a concrete strategy for the IBlastSeqInfoSrc interface for
 * sequence identifiers retrieval from a CSeqDB class.
 */

#include <algo/blast/api/blast_seqinfosrc.hpp>
#include <algo/blast/api/blast_types.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Implementation of the IBlastSeqInfoSrc interface to encapsulate retrieval
/// of sequence identifiers and lengths from a vector of Seq-locs. 
class CSeqVecSeqInfoSrc : public IBlastSeqInfoSrc
{
public:
    /// Constructor from a vector of sequence locations.
    CSeqVecSeqInfoSrc(const TSeqLocVector& seqv);
    /// Destructor
    virtual ~CSeqVecSeqInfoSrc();
    /// Retrieve a sequence identifier given its index in the vector.
    list< CRef<objects::CSeq_id> > GetId(Uint4 index) const;
    /// Retrieve sequence length given its index in the vector.
    Uint4 GetLength(Uint4 index) const;
private:
    TSeqLocVector m_SeqVec; ///< Vector of subject sequence locations to get 
                            /// information from

};

END_SCOPE(blast)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.5  2005/03/31 16:17:26  dondosha
 * Some doxygen fixes
 *
 * Revision 1.4  2004/10/18 20:33:24  dondosha
 * Pass TSeqLocVector to constructor by reference
 *
 * Revision 1.3  2004/10/06 17:47:34  dondosha
 * Removed ncbi_pch.hpp
 *
 * Revision 1.2  2004/10/06 17:45:35  dondosha
 * Removed USING_SCOPE; qualify objects classes directly
 *
 * Revision 1.1  2004/10/06 14:51:37  dondosha
 * Implementation of IBlastSeqInfoSrc with TSeqLocVector
 *
 *
 * ===========================================================================
 */
#endif  /* ALGO_BLAST_API__SEQINFOSRC_SEQVEC_HPP */
