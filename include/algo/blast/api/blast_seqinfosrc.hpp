#ifndef ALGO_BLAST_API__BLAST_SEQINFOSRC__HPP
#define ALGO_BLAST_API__BLAST_SEQINFOSRC__HPP

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

/** @file blast_seqinfosrc.hpp
 * Defines interface for retrieving sequence identifiers.
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

// Forward declarations in objects scope
BEGIN_SCOPE(objects)
    class CSeq_id;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/// Abstract base class to encapsulate retrieval of sequence identifiers. 
/// Used for processing of results coming directly from the BLAST 
/// core engine, e.g. on-the-fly tabular output.
class IBlastSeqInfoSrc
{
public:
    /// Method to retrieve a sequence identifier given its ordinal number.
    virtual list< CRef<objects::CSeq_id> > GetId(Uint4 index) const = 0;
    /// Method to retrieve a sequence length given its ordinal number.
    virtual Uint4 GetLength(Uint4 index) const = 0;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.2  2004/10/06 17:45:35  dondosha
 * Removed USING_SCOPE; qualify objects classes directly
 *
 * Revision 1.1  2004/10/06 14:51:04  dondosha
 * Abstract base class IBlastSeqInfoSrc for sequence id and length retrieval in BLAST API
 *
 *
 * ===========================================================================
 */
#endif  /* ALGO_BLAST_API__BLAST_SEQINFOSRC_HPP */
