#ifndef ALGO_ID_MAPPER___ID_MAPPER__HPP
#define ALGO_ID_MAPPER___ID_MAPPER__HPP

/* $Id$
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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */


#include <objects/seq/seq_id_handle.hpp>
#include <objects/genomecoll/genome_collection__.hpp>
#include <objmgr/seq_loc_mapper.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
class CGC_Assembly;
class CGC_AssemblyUnit;
class CGC_SeqIdAlias;
class CGC_Sequence;
class CGC_TypedSeqId;
class CSeq_id;
class CSeq_interval;
class CSeq_loc;
class CSeq_loc_Mapper;
END_SCOPE(objects)


// Common use case: Going from outside-of-NCBI ids to RefSeq GIs,
//  then back again
//
//   CRef<CGC_Assembly> GenCollAssembly = ... 
//   CSeq_loc GivenLoc = ...
//
//   CGencollIdMapper Mapper(GenCollAssembly)
//   CGencollIdMapper::SIdSpec RsSpec;
//   RsSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
//   RsSpec.Alias = CGC_SeqIdAlias::e_Gi;
//   CRef<CSeq_loc> RefSeqLoc = Mapper.Map(GivenLoc, RsSpec);
//   
//   ... process RefSeqLoc somehow ...
//
//   CGencollIdMapper::SIdSpec OrigSpec;
//   Mapper.Guess(GivenLoc, OrigSpec);
//   CRef<CSeq_loc> BackAgain = Mapper.Map(*RefSeqLoc, OrigSpec);
//

class NCBI_XALGOID_MAPPER_EXPORT CGencollIdMapper : public CObject
{
public:
    CGencollIdMapper(CConstRef<objects::CGC_Assembly> SourceAsm);

    struct NCBI_XALGOID_MAPPER_EXPORT SIdSpec
    {
        typedef objects::CGC_TypedSeqId::E_Choice E_Choice;
        typedef objects::CGC_SeqIdAlias::E_AliasTypes E_Alias;

        bool Primary; // CGC_Sequence::GetSeq_id(), what the Gencoll considers most important
                      // When used by Map, Primary == true overrides the next 4 fields
                      // When used by Guess, it is informing the caller about if it is primary or not.
        E_Choice TypedChoice;
        E_Alias Alias;
        string External; // outside organization name, like UCSC
        string Pattern;  // pattern matching, like chr%s to find 'chrMT' matches replicon named 'MT'
        int Role;  // EGC_SequenceRole
        bool Top;  // if has Sequence Role Top, but do not block out the Role value 

        enum
        {
            // e_Role_ExcludePseudo_Top -- Fake role for Non-Pseudo top
            e_Role_ExcludePseudo_Top = objects::eGC_SequenceRole_top_level + 1,
            e_Role_NotSet = 10000
        };

        SIdSpec();

        operator string() const;
        string ToString(void) const;
        bool operator<(const SIdSpec& Other) const;
        bool operator==(const SIdSpec& Other) const;
    
        bool IsSpecMet(const SIdSpec& Guessed) const;
    };

    // Derives the spec from a given loc
    bool Guess(const objects::CSeq_loc& Loc, SIdSpec& Spec) const;

    // Returning NULL means requested spec could not be met.
    // If the given loc already meets the spec, it returns a copy of itself
    CRef<objects::CSeq_loc> Map(const objects::CSeq_loc& Loc, const SIdSpec& Spec) const;

    bool CanMeetSpec(const objects::CSeq_loc& Loc, const SIdSpec& Spec) const;

    enum E_Gap
    {
        e_None = 0,
        e_Spans,
        e_Overlaps,
        e_Contained,
        e_Complicated
    };
    E_Gap IsLocInAGap(const objects::CSeq_loc& Loc) const;

    CConstRef<objects::CGC_Assembly> GetInternalGencoll(void) const;
    
    void GetSynonyms(const objects::CSeq_id& BaseId, 
                     list< CConstRef<objects::CSeq_id> >& Synonyms, 
                     bool NcbiOnly = true) const;

protected:
    void x_Init(void);

    bool x_NCBI34_Guess(const objects::CSeq_id& Id, SIdSpec& Spec) const;
    CConstRef<objects::CSeq_id>
    x_NCBI34_Map_IdFix(CConstRef<objects::CSeq_id> SourceId) const;

    void x_RecursiveSeqFix(objects::CGC_Sequence& Seq);
    void x_FillGpipeTopRole(objects::CGC_Sequence& Seq);
    void x_RemoveHiddenAccessions(objects::CGC_Sequence& Seq);
    void x_FillChromosomeIds(void);
    void x_PrioritizeIds(void);
    void x_PrioritizeIds(objects::CGC_Sequence& Sequence);

    // Fixes locals that should be accessions, and versionless accessions
    CConstRef<objects::CSeq_id> x_FixImperfectId(CConstRef<objects::CSeq_id> Id,
                                                 const SIdSpec& Spec
                                                ) const;
    CConstRef<objects::CSeq_id> x_ApplyPatternToId(CConstRef<objects::CSeq_id> Id,
                                                   const SIdSpec& Spec
                                                  ) const;
    
    int x_GetRole(const objects::CGC_Sequence& Seq) const;

    void x_AddSeqToMap(const objects::CSeq_id& Id,
                       CConstRef<objects::CGC_Sequence> Seq
                      );

    void x_BuildSeqMap(const objects::CGC_Assembly& assm);
    void x_BuildSeqMap(const objects::CGC_AssemblyUnit& assm);
    void x_BuildSeqMap(const objects::CGC_Sequence& Seq, int Depth=1);

    CConstRef<objects::CSeq_id>
    x_GetIdFromSeqAndSpec(const objects::CGC_Sequence& Seq,
                          const SIdSpec& Spec
                         ) const;

    enum { e_No, e_Yes, e_Up, e_Down };

    int x_CanSeqMeetSpec(const objects::CGC_Sequence& Seq,
                         const SIdSpec& Spec,
                         int Level = 0
                        ) const;

    bool x_MakeSpecForSeq(const objects::CSeq_id& Id,
                          const objects::CGC_Sequence& Seq,
                          SIdSpec& Spec
                         ) const;

    CConstRef<objects::CGC_Sequence>
    x_FindChromosomeSequence(const objects::CSeq_id& Id,
                             const SIdSpec& Spec
                            ) const;

    CConstRef<objects::CGC_Sequence>
    x_FindParentSequence(const objects::CSeq_id& Id,
                         const objects::CGC_Assembly& Assembly,
                         int Depth = 0
                        ) const;

    bool x_IsParentSequence(const objects::CSeq_id& Id,
                            const objects::CGC_Sequence& Parent
                           ) const;


    CRef<objects::CSeq_loc> x_Map_OneToOne(const objects::CSeq_loc& SourceLoc,
                                           const objects::CGC_Sequence& Seq,
                                           const SIdSpec& Spec
                                          ) const;

    CRef<objects::CSeq_loc> x_Map_Up(const objects::CSeq_loc& SourceLoc,
                                     const objects::CGC_Sequence& Seq,
                                     const SIdSpec& Spec
                                    ) const;
    
    CRef<objects::CSeq_loc> x_Map_Down(const objects::CSeq_loc& SourceLoc,
                                       const objects::CGC_Sequence& Seq,
                                       const SIdSpec& Spec
                                      ) const;


    //bool x_IsLocInGap(const objects::CSeq_loc& Loc) const;
    E_Gap x_Merge_E_Gaps(E_Gap First, E_Gap Second) const;
    E_Gap x_IsLoc_Int_InAGap(const objects::CSeq_interval& Int) const;

private:
    CRef<objects::CGC_Assembly> m_Assembly;
    string m_SourceAsm;

    typedef CConstRef<objects::CGC_Sequence> TGC_SequenceCRef;
    typedef map<objects::CSeq_id_Handle, TGC_SequenceCRef> TIdToSeqMap;
    TIdToSeqMap m_IdToSeqMap;

    typedef map<string, int> TAccToVerMap;
    TAccToVerMap m_AccToVerMap;

    vector<string> m_Chromosomes;
    int m_MaxSequenceDepth;

    // All component IDs to the Parent CGC_Sequence
    typedef map<objects::CSeq_id_Handle, TGC_SequenceCRef> TChildToParentMap;
    TChildToParentMap m_ChildToParentMap;
    
    typedef CRef<objects::CSeq_loc_Mapper> TSeqLocMapperRef;
    mutable TSeqLocMapperRef m_UpMapper, m_DownMapper_Shallow, m_DownMapper_Deep;
};


END_NCBI_SCOPE

#endif // ALGO_ID_MAPPER___ID_MAPPER__HPP

