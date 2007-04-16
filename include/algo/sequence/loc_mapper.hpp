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
 * Authors:  Alex Astashyn
 *
 * File Description:
 *
 */

#ifndef LOC_MAPPER_HPP_
#define LOC_MAPPER_HPP_

#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <objtools/alnmgr/alnmix.hpp>
#include <objmgr/seq_loc_mapper.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

///////////////////////////////////////////////////////////////////////////////

/// Feature comparator needs to translate the query seq into target's coordinates.
/// This can be an instance of a wrapper of CSeq_loc_Mapper, a simple 1-to-1 mapper
/// Or a default BLAST-based mapper that will attempt to produce a meaningful alignment.
class ILocMapper : public CObject
{
public:
    virtual CConstRef<CSeq_loc> Map(const CSeq_loc& loc) = 0;
    virtual ~ILocMapper() {}
};


/// Identity mapping between same seq-locs or seq-locs with direct 1:1 mapping
class CLocMapper_Identity : public ILocMapper
{
public:
    
    /// The Map() method will return the same seq-loc.
    /// Used when we have same seq-locs in different scopes with possibly
    /// different annotations.
    CLocMapper_Identity()
    {}
    
    
    /// The Map() method will return same loc but on target id.
    /// Used when we have direct mapping between seq-locs with different ids.
    CLocMapper_Identity(const CSeq_id& target) : m_target_id(new CSeq_id)
    {
        m_target_id->Assign(target);
    }
    
    CConstRef<CSeq_loc> Map(const CSeq_loc& loc) 
    {
        CRef<CSeq_loc> mapped_loc(new CSeq_loc);
        mapped_loc->Assign(loc);
        
        if(!m_target_id.IsNull()) {
            mapped_loc->SetId(*m_target_id);
        }
        return mapped_loc;    
    }
    
private:
    CRef<CSeq_id> m_target_id;
};


/// Map using provided CSeq_loc_Mapper
/// It should provide non-redundant translations with no noise or short gaps.
class CLocMapper_UserMapper : public ILocMapper
{
public:
    CLocMapper_UserMapper(CSeq_loc_Mapper& mapper) : m_mapper(mapper)
    {}
    
    CConstRef<CSeq_loc> Map(const CSeq_loc& loc) 
    {
        return m_mapper.Map(loc);   
    }
    
private:
    CSeq_loc_Mapper& m_mapper;
};

/// Default implementation of BLAST-based mapper.
/// It will naively map individual segments of the query loc
/// and collapse the resulting ranges on the target to close gaps.
class CLocMapper_NucleotideMegablast : public ILocMapper
{
public:
    CLocMapper_NucleotideMegablast(const CSeq_loc& query
                                 , CScope& q_scope
                                 , const CSeq_loc& target
                                 , CScope& t_scope) : m_scope(t_scope)
    {
        //
        // Blast the sequence against the approximate chrom loc
        //
        blast::SSeqLoc blast_query(query, q_scope);
        blast::SSeqLoc blast_target(target, t_scope);
        blast::CBlastNucleotideOptionsHandle blast_options;
        blast_options.SetTraditionalMegablastDefaults();
        //blast_options.SetEvalueThreshold(0.05);
        //blast_options.SetWordSize(28);    
        //blast_options.SetGapOpeningCost(1000);
        //blast_options.SetGapExtensionCost(2);

        blast::CBl2Seq blaster(blast_query, blast_target, blast_options);
        CRef<blast::CSearchResultSet> resultset = blaster.RunEx();
        
        //since we have 1 query vs. one target we expect the resultset to have 1 item
        //and 1 seq_align in seq_align_set.
        const CSeq_align& seq_align = **(*resultset)[0].GetSeqAlign()->Get().begin();
        
        m_aln_mix.Reset(new CAlnMix(t_scope));
        m_aln_mix->Add(seq_align);
        m_aln_mix->Merge(
                      CAlnMix::fTruncateOverlaps 
                    | CAlnMix::fGapJoin 
               //     | CAlnMix::fMinGap
                    | CAlnMix::fSortSeqsByScore 
               //     | CAlnMix::fFillUnalignedRegions 
               //     | CAlnMix::fAllowTranslocation
                    );
                    
        m_mapper.Reset(new CSeq_loc_Mapper(m_aln_mix->GetSeqAlign(), 1));
        m_mapper->SetMergeAll();
    }
    
    CConstRef<CSeq_loc> Map(const CSeq_loc& loc) 
    {
        CRef<CSeq_loc> temp_loc(new CSeq_loc);
        CRef<CSeq_loc> mapped_loc(new CSeq_loc);
        temp_loc->Assign(loc);
        temp_loc->ChangeToMix();
        for(CSeq_loc_CI it(*temp_loc); it; ++it) {
            CRef<CSeq_loc> raw_mapped_interval = m_mapper->Map(it.GetSeq_loc());
            CRef<CSeq_loc> merged_mapped_interval = sequence::Seq_loc_Merge(
                *raw_mapped_interval, 
                CSeq_loc::fMerge_SingleRange, 
                &m_scope);
            mapped_loc->Add(*merged_mapped_interval); 
        }

        return mapped_loc;
    }
    
private:
    CScope& m_scope;
    CRef<CAlnMix> m_aln_mix;
    CRef<CSeq_loc_Mapper> m_mapper;   
};


END_NCBI_SCOPE

#endif
