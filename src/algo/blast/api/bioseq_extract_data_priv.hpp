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
 * Author:  Christiam Camacho
 *
 */

/** @file bioseq_extract_data_priv.hpp
 * Internal auxiliary setup classes/functions for extracting sequence data from
 * Bioseqs. These facilities are free of any dependencies on the NCBI C++ object
 * manager.
 */

#ifndef ALGO_BLAST_API___BIOSEQ_EXTRACT_DATA_PRIV__HPP
#define ALGO_BLAST_API___BIOSEQ_EXTRACT_DATA_PRIV__HPP

#include <objects/seq/Seq_data.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <util/sequtil/sequtil.hpp>
#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
    class CBioseq_set;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/////////////////////////////////////////////////////////////////////////////
//
// CBlastSeqVectorFromCSeq_data
//
/////////////////////////////////////////////////////////////////////////////

class CBlastSeqVectorFromCSeq_data : public IBlastSeqVector
{
public:
    CBlastSeqVectorFromCSeq_data(const objects::CSeq_data& seq_data, 
                                 TSeqPos length);
    virtual void SetCoding(objects::CSeq_data::E_Choice c);
    virtual Uint1 operator[] (TSeqPos pos) const;
    virtual SBlastSequence GetCompressedPlusStrand();

protected:
    TSeqPos x_Size() const;
    void x_SetPlusStrand();
    void x_SetMinusStrand();

private:
    vector<char> m_SequenceData;
    CSeqUtil::ECoding m_Encoding;

    void x_ComplementData();
    CSeqUtil::ECoding 
    x_Encoding_CSeq_data2CSeqUtil(objects::CSeq_data::E_Choice c);
};

/////////////////////////////////////////////////////////////////////////////
//
// CBlastQuerySourceBioseqSet
//
/////////////////////////////////////////////////////////////////////////////

/// Implements the IBlastQuerySource interface using a CBioseq_set as data
/// source
class CBlastQuerySourceBioseqSet : public IBlastQuerySource
{
public:
    /// Parametrized constructor
    /// @param bss set of bioseqs from which to extract the data [in]
    /// @param is_prot whether the bs argument contains protein or nucleotide
    /// sequences [in]
    CBlastQuerySourceBioseqSet(const objects::CBioseq_set& bss, bool is_prot);
    /// Return strand for a sequence
    /// @param index of the sequence in the sequence container [in]
    virtual objects::ENa_strand GetStrand(int index) const;
    /// Return the number of elements in the sequence container
    virtual TSeqPos Size() const;
    /// Return the filtered (masked) regions for a sequence
    /// @param index index of the sequence in the sequence container [in]
    virtual CConstRef<objects::CSeq_loc> GetMask(int index) const;
    /// Return the CSeq_loc associated with a sequence
    /// @param index index of the sequence in the sequence container [in]
    virtual CConstRef<objects::CSeq_loc> GetSeqLoc(int index) const;
    /// Return the sequence data for a sequence
    /// @param index index of the sequence in the sequence container [in]
    /// @param encoding desired encoding [in]
    /// @param strand strand to fetch [in]
    /// @param sentinel specifies to use or not to use sentinel bytes around
    ///        sequence data [in]
    /// @param warnings if not NULL, warnings will be returned in this string
    ///        [in|out]
    /// @return SBlastSequence structure containing sequence data requested
    virtual SBlastSequence
    GetBlastSequence(int index, 
                     EBlastEncoding encoding, 
                     objects::ENa_strand strand,
                     ESentinelType sentinel, 
                     string* warnings = 0) const;
    /// Return the length of a sequence
    /// @param index index of the sequence in the sequence container [in]
    virtual TSeqPos GetLength(int index) const;

private:
    bool m_IsProt;
    vector< CConstRef<objects::CBioseq> > m_Bioseqs;

    void x_BioseqSanityCheck(const objects::CBioseq& bs);
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BIOSEQ_EXTRACT_DATA_PRIV__HPP */
