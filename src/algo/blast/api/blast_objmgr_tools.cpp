
/* ===========================================================================
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
 * Author:  Christiam Camacho / Kevin Bealer
 *
 */

/// @file blast_objmgr_tools.cpp
/// Functions in xblast API code that interact with object manager.

#include <ncbi_pch.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/create_defline.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqloc/seqloc__.hpp>

#include <algo/blast/api/blast_options.hpp>
#include "blast_setup.hpp"
#include "blast_objmgr_priv.hpp"
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/api/blast_seqinfosrc.hpp>
#include <algo/blast/api/blast_seqinfosrc_aux.hpp>

#include <serial/iterator.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include "blast_seqalign.hpp"

#include "dust_filter.hpp"
#include <algo/blast/api/repeats_filter.hpp>
#include "winmask_filter.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

// N.B.: the const is removed, but the v object is never really changed,
// therefore we keep it as a const argument
CBlastQuerySourceOM::CBlastQuerySourceOM(TSeqLocVector& v,
                                         EBlastProgramType     program)
    : m_TSeqLocVector(&v),
      m_OwnTSeqLocVector(false), 
      m_Options(0), 
      m_CalculatedMasks(true),
      m_Program(program)
{
    x_AutoDetectGeneticCodes();
}

CBlastQuerySourceOM::CBlastQuerySourceOM(TSeqLocVector& v,
                                         const CBlastOptions* opts)
    : m_TSeqLocVector(&v), 
      m_OwnTSeqLocVector(false), 
      m_Options(opts), 
      m_CalculatedMasks(false),
      m_Program(opts->GetProgramType())
{
    x_AutoDetectGeneticCodes();
}

CBlastQuerySourceOM::CBlastQuerySourceOM(CBlastQueryVector   & v,
                                         EBlastProgramType     program)
    : m_QueryVector(& v),
      m_OwnTSeqLocVector(false), 
      m_Options(0), 
      m_CalculatedMasks(false),
      m_Program(program)
{
    x_AutoDetectGeneticCodes();
}

CBlastQuerySourceOM::CBlastQuerySourceOM(CBlastQueryVector   & v,
                                         const CBlastOptions * opts)
    : m_QueryVector(& v),
      m_OwnTSeqLocVector(false), 
      m_Options(opts), 
      m_CalculatedMasks(false),
      m_Program(opts->GetProgramType())
{
    x_AutoDetectGeneticCodes();
}

void
CBlastQuerySourceOM::x_AutoDetectGeneticCodes(void)
{
    if ( ! (Blast_QueryIsTranslated(m_Program) || 
            Blast_SubjectIsTranslated(m_Program)) ) {
        return;
    }

    if (m_QueryVector.NotEmpty()) {
        for (CBlastQueryVector::size_type i = 0; 
             i < m_QueryVector->Size(); i++) {
            CRef<CBlastSearchQuery> query = 
                m_QueryVector->GetBlastSearchQuery(i);

            if((m_Options) && (m_Options->GetQueryGeneticCode() != BLAST_GENETIC_CODE)) {
            	query->SetGeneticCodeId(m_Options->GetQueryGeneticCode());
            }

            if (query->GetGeneticCodeId() != BLAST_GENETIC_CODE) {
                // presumably this has already been set, so skip fetching it
                // again
                continue;
            }

            const CSeq_id* id = query->GetQuerySeqLoc()->GetId();
            CSeqdesc_CI desc_it(query->GetScope()->GetBioseqHandle(*id), 
                                CSeqdesc::e_Source);
            if (desc_it) {
                try {
                    query->SetGeneticCodeId(desc_it->GetSource().GetGenCode());
                }
                catch(CUnassignedMember &) {
                    query->SetGeneticCodeId(BLAST_GENETIC_CODE);
                }
            }
        }
    } else {
        _ASSERT(m_TSeqLocVector);
        NON_CONST_ITERATE(TSeqLocVector, sseqloc, *m_TSeqLocVector) {

        	if((m_Options) && (m_Options->GetQueryGeneticCode() != BLAST_GENETIC_CODE)) {
        		sseqloc->genetic_code_id = m_Options->GetQueryGeneticCode();
            }

            if (sseqloc->genetic_code_id != BLAST_GENETIC_CODE) {
                // presumably this has already been set, so skip fetching it
                // again
                continue;
            }

            const CSeq_id* id = sseqloc->seqloc->GetId();
            CSeqdesc_CI desc_it(sseqloc->scope->GetBioseqHandle(*id),
                                CSeqdesc::e_Source);
            if (desc_it) {
                try {
                    sseqloc->genetic_code_id = desc_it->GetSource().GetGenCode();
                }
                catch(CUnassignedMember &) {
                    sseqloc->genetic_code_id = BLAST_GENETIC_CODE;
                }
            }
        }
    }
}

void
CBlastQuerySourceOM::x_CalculateMasks(void)
{
    /// Calculate the masks only once
    if (m_CalculatedMasks) {
        return;
    }
    
    // Without the options we cannot obtain the parameters to do the
    // filtering on the queries to obtain the masks
    if ( !m_Options ) {
        m_CalculatedMasks = true;
        return;
    }
    
    if (Blast_QueryIsNucleotide(m_Options->GetProgramType()) &&
        !Blast_QueryIsTranslated(m_Options->GetProgramType())) {
        
        if (m_Options->GetDustFiltering()) {
            if (m_QueryVector.NotEmpty()) {
                Blast_FindDustFilterLoc(*m_QueryVector,
                    static_cast<Uint4>(m_Options->GetDustFilteringLevel()),
                    static_cast<Uint4>(m_Options->GetDustFilteringWindow()),
                    static_cast<Uint4>(m_Options->GetDustFilteringLinker()));
            } else {
                Blast_FindDustFilterLoc(*m_TSeqLocVector, 
                    static_cast<Uint4>(m_Options->GetDustFilteringLevel()),
                    static_cast<Uint4>(m_Options->GetDustFilteringWindow()),
                    static_cast<Uint4>(m_Options->GetDustFilteringLinker()));
            }
        }
        if (m_Options->GetRepeatFiltering()) {
            string rep_db = m_Options->GetRepeatFilteringDB();
            
            if (m_QueryVector.NotEmpty()) {
                Blast_FindRepeatFilterLoc(*m_QueryVector, rep_db.c_str());
            } else {
                Blast_FindRepeatFilterLoc(*m_TSeqLocVector, rep_db.c_str());
            }
        }
        
        if (NULL != m_Options->GetWindowMaskerDatabase() ||
            0    != m_Options->GetWindowMaskerTaxId()) {
            
            if (m_QueryVector.NotEmpty()) {
                Blast_FindWindowMaskerLoc(*m_QueryVector, m_Options);
            } else {
                Blast_FindWindowMaskerLoc(*m_TSeqLocVector, m_Options);
            }
        }
    }
    
    m_CalculatedMasks = true;
}

CBlastQuerySourceOM::~CBlastQuerySourceOM()
{
    if (m_OwnTSeqLocVector && m_TSeqLocVector) {
        delete m_TSeqLocVector;
        m_TSeqLocVector = NULL;
    }
}

ENa_strand 
CBlastQuerySourceOM::GetStrand(int i) const
{
    if (m_QueryVector.NotEmpty()) {
        return sequence::GetStrand(*m_QueryVector->GetQuerySeqLoc(i),
                                   m_QueryVector->GetScope(i));
    } else {
        return sequence::GetStrand(*(*m_TSeqLocVector)[i].seqloc,
                                   (*m_TSeqLocVector)[i].scope);
    }
}

TMaskedQueryRegions
CBlastQuerySourceOM::GetMaskedRegions(int i)
{
    x_CalculateMasks();

    if (m_QueryVector.NotEmpty()) {
        return m_QueryVector->GetMaskedRegions(i);
    } else {
        return PackedSeqLocToMaskedQueryRegions((*m_TSeqLocVector)[i].mask,
                     m_Program, (*m_TSeqLocVector)[i].ignore_strand_in_mask);
    }
}

CConstRef<CSeq_loc>
CBlastQuerySourceOM::GetMask(int i)
{
    x_CalculateMasks();
    
    if (m_QueryVector.NotEmpty()) {
        return MaskedQueryRegionsToPackedSeqLoc( m_QueryVector->GetMaskedRegions(i) );
    } else {
        return (*m_TSeqLocVector)[i].mask;
    }
}

CConstRef<CSeq_loc>
CBlastQuerySourceOM::GetSeqLoc(int i) const
{
    if (m_QueryVector.NotEmpty()) {
        return m_QueryVector->GetQuerySeqLoc(i);
    } else {
        return (*m_TSeqLocVector)[i].seqloc;
    }
}

const CSeq_id*
CBlastQuerySourceOM::GetSeqId(int i) const
{
    if (m_QueryVector.NotEmpty()) {
        return & sequence::GetId(*m_QueryVector->GetQuerySeqLoc(i),
                                 m_QueryVector->GetScope(i));
    } else {
        return & sequence::GetId(*(*m_TSeqLocVector)[i].seqloc,
                                 (*m_TSeqLocVector)[i].scope);
    }
}

Uint4
CBlastQuerySourceOM::GetGeneticCodeId(int i) const
{
    if (m_QueryVector.NotEmpty()) {
        return m_QueryVector->GetBlastSearchQuery(i)->GetGeneticCodeId();
    } else {
        return (*m_TSeqLocVector)[i].genetic_code_id;
    }
}

SBlastSequence
CBlastQuerySourceOM::GetBlastSequence(int i,
                                      EBlastEncoding encoding,
                                      objects::ENa_strand strand,
                                      ESentinelType sentinel,
                                      string* warnings) const
{
    if (m_QueryVector.NotEmpty()) {
        return GetSequence(*m_QueryVector->GetQuerySeqLoc(i), encoding,
                           m_QueryVector->GetScope(i), strand, sentinel, warnings);
    } else {
        return GetSequence(*(*m_TSeqLocVector)[i].seqloc, encoding,
                           (*m_TSeqLocVector)[i].scope, strand, sentinel, warnings);
    }
}

TSeqPos
CBlastQuerySourceOM::GetLength(int i) const
{
    TSeqPos rv = numeric_limits<TSeqPos>::max();
    
    if (m_QueryVector.NotEmpty()) {
        rv = sequence::GetLength(*m_QueryVector->GetQuerySeqLoc(i),
                                 m_QueryVector->GetScope(i));
    } else if (! m_TSeqLocVector->empty()) {
        rv = sequence::GetLength(*(*m_TSeqLocVector)[i].seqloc,
                                 (*m_TSeqLocVector)[i].scope);
    }
    
    if (rv == numeric_limits<TSeqPos>::max()) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   string("Could not find length of query # ")
                   + NStr::IntToString(i) + " with Seq-id ["
                   + GetSeqId(i)->AsFastaString() + "]");
    }
    
    return rv;
}

string
CBlastQuerySourceOM::GetTitle(int i) const
{
    CConstRef<CSeq_loc> seqloc = GetSeqLoc(i);
    CRef<CScope> scope;
    if (m_QueryVector.NotEmpty()) {
        scope = m_QueryVector->GetScope(i);
    } else if (! m_TSeqLocVector->empty()) {
        scope = (*m_TSeqLocVector)[i].scope;
    }
    _ASSERT(seqloc.NotEmpty());
    _ASSERT(scope.NotEmpty());
    if ( !seqloc->GetId() ) {
        return kEmptyStr;
    }

    CBioseq_Handle bh = scope->GetBioseqHandle(*seqloc->GetId());
    return bh ? sequence::CDeflineGenerator().GenerateDefline(bh) : kEmptyStr;
}

TSeqPos
CBlastQuerySourceOM::Size() const
{
    if (m_QueryVector.NotEmpty()) {
        return static_cast<TSeqPos>(m_QueryVector->Size());
    } else {
        return static_cast<TSeqPos>(m_TSeqLocVector->size());
    }
}

void
SetupQueryInfo(TSeqLocVector& queries, 
               EBlastProgramType prog,
               objects::ENa_strand strand_opt,
               BlastQueryInfo** qinfo)
{
    SetupQueryInfo_OMF(CBlastQuerySourceOM(queries, prog), prog, strand_opt, qinfo);
}

void
SetupQueries(TSeqLocVector& queries, 
             BlastQueryInfo* qinfo, 
             BLAST_SequenceBlk** seqblk,
             EBlastProgramType prog,
             objects::ENa_strand strand_opt,
             TSearchMessages& messages)
{
    CBlastQuerySourceOM query_src(queries, prog);
    SetupQueries_OMF(query_src, qinfo, seqblk, prog, 
                     strand_opt, messages);
}

void
SetupSubjects(TSeqLocVector& subjects, 
              EBlastProgramType prog,
              vector<BLAST_SequenceBlk*>* seqblk_vec, 
              unsigned int* max_subjlen)
{
    CBlastQuerySourceOM subj_src(subjects, prog);
    SetupSubjects_OMF(subj_src, prog, seqblk_vec, max_subjlen);
}


static unsigned char ctable[16] = {0xFF, 0x00, 0x01, 0xFF, 0x02, 0xFF, 0xFF, 0xFF,
		                           0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void s_Ncbi4naToNcbi2na(const string & ncbi4na, int base_length,
                        unsigned char * ncbi2na)
{
    int inp_bytes   = base_length;
    CRandom random(base_length);

    for(int i = 0; i < inp_bytes; i++) {
        // one input byte
        unsigned char inp = ncbi4na[i];

        // represents 2 bases
        unsigned char b = inp & 0xF;

        // compress each to 2 bits
        unsigned char c = ctable[b];

        if (c  != 0xFF) {
            // No ambiguities, so we can do this the easy way.
        	ncbi2na[i] = c;

        } else {
            if (b == 0 || b == 0x0F) {
            	//gap or N
                ncbi2na[i] = random.GetRand() & 0x3;
            }
            else {

            	int bitcount = ((b & 1) + ((b >> 1) & 1) +
            	               ((b >> 2) & 1) + ((b >> 3) & 1));

            	// 1-bit ambiguities here, indicate an error in this class.
            	_ASSERT(bitcount >= 2);
            	_ASSERT(bitcount <= 3);

            	int pick = random.GetRand() % bitcount;

            	for(int j = 0; j < 4; j++) {
            		// skip 0 bits in input.
            	    if ((b & (1 << j)) == 0)
            	    	continue;

            	    // If the bitcount is zero, this is the bit we want.
            	    if (! pick) {
            	    	ncbi2na[i] = j;
            	    	break;
            	    }
            	    // Else, decrement.
            	    pick--;
            	}
            }
        }
    }

}

/// Implementation of the IBlastSeqVector interface which obtains data from a
/// CSeq_loc and a CScope relying on the CSeqVector class
class CBlastSeqVectorOM : public IBlastSeqVector
{
public:
    CBlastSeqVectorOM(const CSeq_loc& seqloc, CScope& scope)
        : m_SeqLoc(seqloc), m_Scope(scope), m_SeqVector(seqloc, scope)
    {
        m_Strand = m_SeqLoc.GetStrand();
    }
    
    /** @inheritDoc */
    virtual void SetCoding(CSeq_data::E_Choice coding) {
        m_SeqVector.SetCoding(coding);
    }

    /** @inheritDoc */
    virtual Uint1 operator[] (TSeqPos pos) const { return m_SeqVector[pos]; }

    /** @inheritDoc */
    virtual void GetStrandData(objects::ENa_strand strand, unsigned char* buf) {
        x_FixStrand(strand);
        for (CSeqVector_CI itr(m_SeqVector, strand); itr; ++itr) {
            // treat gap '-' as 'N'
            if (itr.IsInGap()) {
                *buf++ = 0xf;
            } else {
                *buf++ = *itr;
            }
        }
    }

    /** @inheritDoc */
    virtual SBlastSequence GetCompressedPlusStrand() {
        SBlastSequence retval(size());
        string ncbi4na = kEmptyStr;
        m_SeqVector.GetSeqData(m_SeqVector.begin(), m_SeqVector.end(), ncbi4na);
        s_Ncbi4naToNcbi2na(ncbi4na, size(), retval.data.get());
        return retval;
    }

protected:
    /** @inheritDoc */
    virtual TSeqPos x_Size() const { 
        return m_SeqVector.size(); 
    }
    /** @inheritDoc */
    virtual void x_SetPlusStrand() {
        x_SetStrand(eNa_strand_plus);
    }
    /** @inheritDoc */
    virtual void x_SetMinusStrand() {
        x_SetStrand(eNa_strand_minus);
    }
    /** @inheritDoc 
     * @note for this class, this might be inefficient, please use
     * GetStrandData with the appropriate strand
     */
    void x_SetStrand(ENa_strand s) {
        x_FixStrand(s);
        _ASSERT(m_Strand == m_SeqVector.GetStrand());
        if (s != m_Strand) {
            m_SeqVector = CSeqVector(m_SeqLoc, m_Scope,
                                     CBioseq_Handle::eCoding_Ncbi, s);
        }
    }
    
    /// If the Seq-loc is on the minus strand and the user is 
    /// asking for the minus strand, we change the user's request
    /// to the plus strand. If we did not do this, we would get
    /// the plus strand (ie it would be reversed twice)
    /// @param strand strand to handle [in|out]
    void x_FixStrand(objects::ENa_strand& strand) const {
        if (eNa_strand_minus == strand && 
            eNa_strand_minus == m_SeqLoc.GetStrand()) {
            strand = eNa_strand_plus;
        }
    }

private:
    const CSeq_loc & m_SeqLoc;
    CScope         & m_Scope;
    CSeqVector       m_SeqVector;
};

SBlastSequence
GetSequence(const objects::CSeq_loc& sl, EBlastEncoding encoding, 
            objects::CScope* scope,
            objects::ENa_strand strand, 
            ESentinelType sentinel,
            std::string* warnings) 
{
    // Retrieves the correct strand (plus or minus), but not both
    CBlastSeqVectorOM sv = CBlastSeqVectorOM(sl, *scope);
    return GetSequence_OMF(sv, encoding, strand, sentinel, warnings);
}


CRef<CPacked_seqint>
TSeqLocVector2Packed_seqint(const TSeqLocVector& sequences)
{
    CRef<CPacked_seqint> retval;
    if (sequences.empty()) {
        return retval;
    }

    retval.Reset(new CPacked_seqint);
    ITERATE(TSeqLocVector, seq, sequences) {
        const CSeq_id& id(sequence::GetId(*seq->seqloc, &*seq->scope));
        TSeqRange range(TSeqRange::GetWhole());
        if (seq->seqloc->IsWhole()) {
            try {
                range.Set(0, sequence::GetLength(*seq->seqloc, &*seq->scope));
            } catch (const CException&) {
                range = TSeqRange::GetWhole();
            }
        } else if (seq->seqloc->IsInt()) {
            try {
                range.SetFrom(sequence::GetStart(*seq->seqloc, &*seq->scope));
                range.SetTo(sequence::GetStop(*seq->seqloc, &*seq->scope));
            } catch (const CException&) {
                range = TSeqRange::GetWhole();
            }
        } else {
            NCBI_THROW(CBlastException, eNotSupported, 
                       "Unsupported Seq-loc type used for query");
        }
        retval->AddInterval(id, range.GetFrom(), range.GetTo());
    }
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
