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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqset/gb_release_file.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/obj_sniff.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/format/ostream_text_ostream.hpp>
#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <objects/genomecoll/genome_collection__.hpp>
#include <algo/id_mapper/id_mapper.hpp>
#include <objmgr/seq_loc_mapper.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


typedef map<string, CStopWatch> TTimerMap;
extern TTimerMap TimerMap;
#define START_TIMER(X) TimerMap[#X].Start()
#define STOP_TIMER(X) TimerMap[#X].Stop()
#define PRINT_TIMERS(X) ITERATE (TTimerMap, iter, TimerMap) {            \
            ERR_POST(X << "Timer: " << iter->first << ": "                       \
                           << iter->second.AsString() << "s");   \
        }

#define MARKLINE cerr << __FILE__ << ":" << __LINE__ << endl;

const string DELIM = "%s";
const string CHROMO_EXT = "<CHROMOSOME_EXTERNAL>";


bool
s_RevStrLenSort(const string& A, const string& B)
{
    return (B.length() < A.length());
}

CGencollIdMapper::CGencollIdMapper(CConstRef<CGC_Assembly> SourceAsm)
{
    if (SourceAsm.IsNull()) {
        return;
    }
    m_Assembly.Reset(new CGC_Assembly());
    m_Assembly->Assign(*SourceAsm);
    m_Assembly->PostRead();
    m_SourceAsm = m_Assembly->GetAccession();
    x_Init();
}

bool
CGencollIdMapper::Guess(const objects::CSeq_loc& Loc, SIdSpec& Spec) const
{
//#warning FIXME: If it returns null, deeply examine the Loc
    if (Loc.GetId() == NULL) {
        return false;
    }
    if (m_Assembly.IsNull()) {
        return CRef<CSeq_loc>();
    }

    CConstRef<CSeq_id> Id(Loc.GetId());
    Id = x_FixImperfectId(Id, Spec); // But not apply Pattern. This derives Pattern
    if (x_NCBI34_Guess(*Id, Spec)) {
        return true;
    }

    CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(*Id);
    TIdToSeqMap::const_iterator Found = m_IdToSeqMap.find(Idh);
    if (Found == m_IdToSeqMap.end()) {
        const string IdStr = Id->GetSeqIdString(true);
        ITERATE (vector<string>, ChromoIter, m_Chromosomes) {
            if (NStr::Find(IdStr, *ChromoIter) != NPOS) {
                CSeq_id Temp;
                Temp.SetLocal().SetStr() = *ChromoIter;
                Idh = CSeq_id_Handle::GetHandle(Temp);
                Found = m_IdToSeqMap.find(Idh);
                break;
            }
        }
        if (Found == m_IdToSeqMap.end()) {
            return false; // Unknown ID
        }
    }

    const CGC_Sequence& Seq = *Found->second;
    return x_MakeSpecForSeq(*Id, Seq, Spec);
}

CRef<CSeq_loc>
CGencollIdMapper::Map(const objects::CSeq_loc& Loc, const SIdSpec& Spec) const
{
    CRef<CSeq_loc_Mapper> Mapper;
    if (m_Assembly.IsNull()) {
        return CRef<CSeq_loc>();
    }

    // Recurse down Mixes
    if (Loc.GetId() == NULL) {
        if (Loc.IsMix()) {
            CRef<CSeq_loc> Result(new CSeq_loc());
            CTypeConstIterator<CSeq_loc> LocIter(Loc);
            for ( ; LocIter; ++LocIter) {
                if (LocIter->Equals(Loc)) {
                    continue;
                }
                CRef<CSeq_loc> MappedLoc = Map(*LocIter, Spec);
                if (MappedLoc.NotNull() && !MappedLoc->IsNull()) {
                    Result->SetMix().Set().push_back(MappedLoc);
                }
            }
            if (Result->IsMix()) {
                return Result;
            }
        }
        else if (Loc.IsPacked_int() || Loc.IsPacked_pnt()) {
            CSeq_loc MixLoc;
            MixLoc.Assign(Loc);
            MixLoc.ChangeToMix();
            return Map(MixLoc, Spec);
        }
        return CRef<CSeq_loc>();
    }

    CConstRef<CSeq_id> Id(Loc.GetId());
    Id = x_FixImperfectId(Id, Spec);
    Id = x_ApplyPatternToId(Id, Spec);
    Id = x_NCBI34_Map_IdFix(Id);
    
    
    SIdSpec GuessSpec;
    Guess(Loc, GuessSpec);
    if (GuessSpec == Spec) {
        CRef<CSeq_loc> Result(new CSeq_loc());
        Result->Assign(Loc);
        return Result;
    }
    
    CConstRef<CGC_Sequence> Seq;
    {{
        const CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(*Id);
        TIdToSeqMap::const_iterator Found = m_IdToSeqMap.find(Idh);
        if (Found != m_IdToSeqMap.end()) {
            Seq = Found->second;
            if (Seq.NotNull()) {
                if (x_CanSeqMeetSpec(*Seq, Spec) == e_Yes) {
                    CRef<CSeq_loc> Result = x_Map_OneToOne(Loc, *Seq, Spec);
                    if (Result.NotNull() && !Result->IsNull()) {
                        return Result;
                    }
                }
            }
        }
    }}

    {{
        const CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(*Id);
        TIdToSeqMap::const_iterator Found = m_IdToSeqMap.find(Idh);
        if (Found != m_IdToSeqMap.end()) {
            Seq = Found->second;
            if (Seq.NotNull()) {
                if (x_CanSeqMeetSpec(*Seq, Spec) == e_Down) {
                    SIdSpec GiSpec = GuessSpec;
                    GiSpec.Alias = CGC_SeqIdAlias::e_Gi;
                    CRef<CSeq_loc> GiLoc;
                    // The up-mapper only works with Locs that have the same ID-type
                    // as the Structure is built from, so this step maps the given-loc 
                    // sideways to that needed ID. The Up-mapped initial result will be
                    // in the same space, but then it will also be side-mapped to the
                    // requested spec.
                    GiSpec.TypedChoice = CGC_TypedSeqId::e_Genbank;
                    GiLoc = Map(Loc, GiSpec);
                    if(GiLoc.IsNull() || GiLoc->IsNull()) {
                        GiSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
                        GiLoc = Map(Loc, GiSpec);
                    }
                    CRef<CSeq_loc> Result;
                    if(GiLoc.NotNull() && !GiLoc->IsNull()) 
                        Result = x_Map_Down(*GiLoc, *Seq, Spec);
                    else
                        Result = x_Map_Down(Loc, *Seq, Spec);
                    if (Result.NotNull() && !Result->IsNull()) {
                        return Result;
                    }
                }
            }
        }
    }}

    {{
        const CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(*Id);
        TIdToSeqMap::const_iterator Found = m_IdToSeqMap.find(Idh);
        if (Found != m_IdToSeqMap.end()) {
            Seq = Found->second;
            if (Seq.NotNull()) {
                if (x_CanSeqMeetSpec(*Seq, Spec) == e_Up) {
                    SIdSpec GiSpec = GuessSpec;
                    GiSpec.Alias = CGC_SeqIdAlias::e_Gi;
                    CRef<CSeq_loc> GiLoc;
                    // The up-mapper only works with Locs that have the same ID-type
                    // as the Structure is built from, so this step maps the given-loc 
                    // sideways to that needed ID. The Up-mapped initial result will be
                    // in the same space, but then it will also be side-mapped to the
                    // requested spec.
                    GiSpec.TypedChoice = CGC_TypedSeqId::e_Genbank;
                    GiLoc = Map(Loc, GiSpec);
                    if(GiLoc.IsNull() || GiLoc->IsNull()) {
                        GiSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
                        GiLoc = Map(Loc, GiSpec);
                    }
                    if(GiLoc.NotNull() && !GiLoc->IsNull()) {
                        CRef<CSeq_loc> Result = x_Map_Up(*GiLoc, *Seq->GetParent(), Spec);
                        if (Result.NotNull() && !Result->IsNull()) {
                            return Result;
                        }
                    }
                }
            }
        } 
    }}

    {{
        Seq = x_FindChromosomeSequence(*Id, Spec);
        if (Seq.NotNull()) {
            CRef<CSeq_loc> Result = x_Map_OneToOne(Loc, *Seq, Spec);
            if (Result.NotNull() && !Result->IsNull()) {
                return Map(*Result, Spec);
            }
        }
    }}
    
    return CRef<CSeq_loc>();
}

bool
CGencollIdMapper::CanMeetSpec(const objects::CSeq_loc& Loc, const SIdSpec& Spec) const
{
//#warning FIXME: If it returns null, deeply examine the Loc
    if (Loc.GetId() == NULL) {
        return false;
    }

    CConstRef<CSeq_id> Id(Loc.GetId());
    Id = x_FixImperfectId(Id, Spec);
    Id = x_ApplyPatternToId(Id, Spec);
    Id = x_NCBI34_Map_IdFix(Id);

    {{
        const CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(*Id);
        TIdToSeqMap::const_iterator Found = m_IdToSeqMap.find(Idh);
        if (Found != m_IdToSeqMap.end()) {
            CConstRef<CGC_Sequence> Seq = Found->second;
            const bool Result = x_CanSeqMeetSpec(*Seq, Spec);
            if (Result != e_No) {
                return true;
            }
        }
    }}

    {{
        // Look for Parent seq
        CConstRef<CGC_Sequence> Seq = x_FindParentSequence(*Id, *m_Assembly);
        if (Seq.NotNull()) {
            const bool Result = x_CanSeqMeetSpec(*Seq, Spec);
            if (Result != e_No) {
                return true;
            }
        }
    }}

    {{
        CConstRef<CGC_Sequence> Seq = x_FindChromosomeSequence(*Id, Spec);
        if (Seq.NotNull()) {
            const bool Result = x_CanSeqMeetSpec(*Seq, Spec);
            if (Result != e_No) {
                return true;
            }
        }
    }}

    return false;
}

void
CGencollIdMapper::x_Init(void)
{
    CTypeIterator<CGC_Sequence> SeqIter(*m_Assembly);
    for ( ; SeqIter; ++SeqIter) {
        x_RecursiveSeqFix(*SeqIter);
        x_FillGpipeTopRole(*SeqIter);

        NON_CONST_ITERATE (CGC_Sequence::TSequences,
                        ChildIter, SeqIter->SetSequences()) {
            CGC_TaggedSequences& Tagged = **ChildIter;
            CTypeIterator<CGC_Sequence> InnerIter(Tagged);
            for ( ; InnerIter; ++InnerIter) {
                x_RecursiveSeqFix(*InnerIter);
                x_FillGpipeTopRole(*InnerIter);
            }
        }
    }

    x_FillChromosomeIds();
    x_PrioritizeIds();

//cout << MSerial_AsnText << *m_Assembly;

    x_BuildSeqMap(*m_Assembly);

    CTypeConstIterator<CSeq_id> IdIter(*m_Assembly);
    for ( ; IdIter; ++IdIter) {
        const CTextseq_id* textseqid = IdIter->GetTextseq_Id();
        if (textseqid != 0) {
            const string& Acc = textseqid->GetAccession();
            const int Ver(
                textseqid->CanGetVersion() ? textseqid->GetVersion() : 1
            );
            m_AccToVerMap[Acc] = Ver;
        }
    }

    CTypeConstIterator<CGC_Replicon> ReplIter(*m_Assembly);
    for ( ; ReplIter; ++ReplIter) {
        if (ReplIter->CanGetName() &&
            !NStr::EndsWith(ReplIter->GetName(), "_random")
           ) {
            m_Chromosomes.push_back(ReplIter->GetName());
        }
    }
    sort(m_Chromosomes.begin(), m_Chromosomes.end(), s_RevStrLenSort);
    
    //SSeqMapSelector  Sel;
    //UpMapper.Reset(new CSeq_loc_Mapper(*m_Assembly, CSeq_loc_Mapper::eSeqMap_Up, Sel));
    //DownMapper.Reset(new CSeq_loc_Mapper(*m_Assembly, CSeq_loc_Mapper::eSeqMap_Down, Sel));

    m_Assembly->PostRead();
}

bool
CGencollIdMapper::x_NCBI34_Guess(const CSeq_id& Id, SIdSpec& Spec) const
{
    if (!(m_Assembly->GetTaxId() == 9606 &&
          NStr::Equal(m_Assembly->GetName(), "NCBI34")
         )
       ) {
        return false;
    }
    const string seqidstr = Id.GetSeqIdString(true);
    if (NStr::Equal(seqidstr, "NC_000002") || NStr::Equal(seqidstr, "NC_000002.8")) {
        Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
        Spec.Alias = CGC_SeqIdAlias::e_Public;
        Spec.External = kEmptyStr;
        Spec.Pattern = kEmptyStr;
        return true;
    }
    if (NStr::Equal(seqidstr, "NC_000009") || NStr::Equal(seqidstr, "NC_000009.8")) {
        Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
        Spec.Alias = CGC_SeqIdAlias::e_Public;
        Spec.External = kEmptyStr;
        Spec.Pattern = kEmptyStr;
        return true;
    }
    return false;
}


CConstRef<CSeq_id>
CGencollIdMapper::x_NCBI34_Map_IdFix(CConstRef<CSeq_id> SourceId) const
{
    if (!(m_Assembly->GetTaxId() == 9606 &&
          NStr::Equal(m_Assembly->GetName(), "NCBI34")
         )
       ) {
        return SourceId;
    }
    const string seqidstr = SourceId->GetSeqIdString(true);
    if (NStr::Equal(seqidstr, "NC_000002") || NStr::Equal(seqidstr, "NC_000002.8")) {
        CRef<CSeq_id> NewId(new CSeq_id());
        NewId->SetLocal().SetStr("2");
        return NewId;
    }
    if (NStr::Equal(seqidstr, "NC_000009") || NStr::Equal(seqidstr, "NC_000009.8")) {
        CRef<CSeq_id> NewId(new CSeq_id());
        NewId->SetLocal().SetStr("9");
        return NewId;
    }
    return SourceId;
}

void
CGencollIdMapper::x_RecursiveSeqFix(CGC_Sequence& Seq)
{
    // Hopefully not the ID that recurses
    CSeq_id TopSyn;
    if (Seq.CanGetSeq_id_synonyms()) {
        ITERATE (CGC_Sequence::TSeq_id_synonyms, SynIter, Seq.GetSeq_id_synonyms()) {
            CTypeConstIterator<CSeq_id> IdIter(**SynIter);
            for ( ; IdIter; ++IdIter) {
                if (IdIter->IsGi()) {
                    continue;
                }
                TopSyn.Assign(*IdIter);
                break;
            }
        }
    }

    // Check if the Seq's Structure recurse
    if (Seq.CanGetStructure()) {
        const CSeq_id& TopId = Seq.GetSeq_id();
        bool DoesRecurse = false;
        CTypeConstIterator<CSeq_id> StructIdIter(Seq.GetStructure());
        for ( ; StructIdIter; ++StructIdIter) {
            if (StructIdIter->Equals(TopId)) {
                DoesRecurse = true;
                break;
            }
        }
        if (DoesRecurse) {
            Seq.ResetSeq_id();
            Seq.SetSeq_id().Assign(TopSyn);
        }
    }

    // Check if the Seq's sub-sequences recurse
    if (Seq.CanGetSequences()) {
        const CSeq_id& TopId = Seq.GetSeq_id();
        bool DoesRecurse = false;
        ITERATE (CGC_Sequence::TSequences, TagIter, Seq.GetSequences()) {
            CTypeConstIterator<CSeq_id> SubSeqIdIter(**TagIter);
            for ( ; SubSeqIdIter; ++SubSeqIdIter) {
                if (SubSeqIdIter->Equals(TopId)) {
                    DoesRecurse = true;
                    break;
                }
            }
            if (DoesRecurse) {
                Seq.ResetSeq_id();
                Seq.SetSeq_id().Assign(TopSyn);
            }
        }
    }

    // Bad Random GIs
    if (Seq.GetSeq_id().IsGi()) {
        //CTypeConstIterator<CSeq_id> IdIter(Seq);
        bool IsRandom = false;
        if (Seq.CanGetSeq_id_synonyms()) {
            ITERATE (CGC_Sequence::TSeq_id_synonyms, SynIter, Seq.GetSeq_id_synonyms()) {
                CTypeConstIterator<CSeq_id> IdIter(**SynIter);
                for ( ; IdIter; ++IdIter) {
                    if (NStr::EndsWith(IdIter->GetSeqIdString(), "_random")) {
                        IsRandom = true;
                        break;
                    }
                }
            }
        }
        if (IsRandom) {
            Seq.ResetSeq_id();
            Seq.SetSeq_id().Assign(TopSyn);
        }
    }
}

void
CGencollIdMapper::x_FillGpipeTopRole(CGC_Sequence& Seq)
{
    CConstRef<CSeq_id> GenGi(
        Seq.GetSynonymSeq_id(CGC_TypedSeqId::e_Genbank, CGC_SeqIdAlias::e_Gi)
    );
    CConstRef<CSeq_id> RefGi(
        Seq.GetSynonymSeq_id(CGC_TypedSeqId::e_Refseq, CGC_SeqIdAlias::e_Gi)
    );
    const bool SeqHasGi = bool(GenGi) || bool(RefGi);

    bool SeqQualifies = false;
    bool ParentQualifies = false;
    if (Seq.HasRole(eGC_SequenceRole_top_level) && SeqHasGi) {
        SeqQualifies = true;
    }

    CConstRef<CGC_Sequence> Parent = Seq.GetParent();
    if (Parent.NotNull()) {
        GenGi = Parent->GetSynonymSeq_id(CGC_TypedSeqId::e_Genbank, CGC_SeqIdAlias::e_Gi);
        RefGi = Parent->GetSynonymSeq_id(CGC_TypedSeqId::e_Refseq, CGC_SeqIdAlias::e_Gi);
        const bool ParentHasGi = bool(GenGi) || bool(RefGi);
        if (Parent->HasRole(eGC_SequenceRole_top_level) &&
            Seq.GetParentRelation() == CGC_TaggedSequences::eState_placed &&
            ParentHasGi
           ) {
            ParentQualifies = true;
        }
    }
    if (SeqQualifies &&
        !ParentQualifies &&
        !Seq.HasRole(SIdSpec::e_Role_ExcludePseudo_Top)
       ) {
        Seq.SetRoles().push_back(SIdSpec::e_Role_ExcludePseudo_Top);
    }
}

void
CGencollIdMapper::x_FillChromosomeIds(void)
{
    // For animals like Cow, whom's private ID is 'Chr1', create an extra
    // private ID that is identical to the chromosome name.
    CTypeIterator<CGC_Replicon> ReplIter(*m_Assembly);
    for ( ; ReplIter; ++ReplIter) {
        if (ReplIter->CanGetName() && ReplIter->CanGetSequence() &&
            ReplIter->GetSequence().IsSingle() &&
            ReplIter->GetSequence().GetSingle().CanGetSeq_id_synonyms() &&
            ReplIter->GetSequence().GetSingle().CanGetStructure() ) {
            CGC_Sequence& Seq = ReplIter->SetSequence().SetSingle();
            bool NameFound = false;
            ITERATE (CGC_Sequence::TSeq_id_synonyms, it, Seq.GetSeq_id_synonyms()) {
                if ((*it)->Which() == CGC_TypedSeqId::e_Private) {
                    NameFound = NStr::Equal((*it)->GetPrivate().GetSeqIdString(), ReplIter->GetName());
                }
            }
            if (!NameFound) {
                CRef<CGC_TypedSeqId> ChromoId(new CGC_TypedSeqId());
                ChromoId->SetExternal().SetExternal() = CHROMO_EXT;
                ChromoId->SetExternal().SetId().SetLocal().SetStr() = ReplIter->GetName();
                //ChromoId->SetPrivate().SetLocal().SetStr() = ReplIter->GetName();
                Seq.SetSeq_id_synonyms().push_back(ChromoId);
            }
        }
    }
}

void
CGencollIdMapper::x_PrioritizeIds(void)
{
    CTypeIterator<CGC_Sequence> SeqIter(*m_Assembly);
    while (SeqIter) {
        x_PrioritizeIds(*SeqIter);
        NON_CONST_ITERATE (CGC_Sequence::TSequences,
                           ChildIter,
                           SeqIter->SetSequences()
                          ) {
            CGC_TaggedSequences& Tagged = **ChildIter;
            CTypeIterator<CGC_Sequence> SeqIter(Tagged);
            while (SeqIter) {
                x_PrioritizeIds(*SeqIter);
                ++SeqIter;
            }
        }
        ++SeqIter;
    }
}


void
CGencollIdMapper::x_PrioritizeIds(CGC_Sequence& Sequence)
{
    // The only thing we have right now is making UCSC IDs first,
    // so that they are above 'privite' dupes of UCSC
    //
    CGC_Sequence::TSeq_id_synonyms::iterator IdIter;
    for (IdIter  = Sequence.SetSeq_id_synonyms().begin();
        IdIter != Sequence.SetSeq_id_synonyms().end(); ) {

        if ((*IdIter)->IsExternal() &&
            (*IdIter)->GetExternal().IsSetExternal() &&
            NStr::Equal((*IdIter)->GetExternal().GetExternal(), "UCSC")
           ) {
            CRef<CGC_TypedSeqId> CopyId = *IdIter;
            IdIter = Sequence.SetSeq_id_synonyms().erase(IdIter);
            Sequence.SetSeq_id_synonyms().push_front(CopyId);
        }
        else {
            ++IdIter;
        }
    }
}

CConstRef<CSeq_id>
CGencollIdMapper::x_FixImperfectId(CConstRef<CSeq_id> Id,
                                   const SIdSpec& Spec
                                  ) const
{
    // Fix up the ID if its not as well formed as it could be.
    // Because GenColl only stores perfectly formed IDs.

    if (Id->IsGi() && Id->GetGi() < GI_FROM(TIntId, 50)) {
        CRef<CSeq_id> NewId(new CSeq_id());
        NewId->SetLocal().SetStr() = NStr::NumericToString(Id->GetGi());
        Id = NewId;
    }

    // First make Acc-as-locals into some form of Acc.
    if (Id->IsLocal() && Id->GetLocal().IsStr()) {
        if (CSeq_id::IdentifyAccession(Id->GetLocal().GetStr()) >= CSeq_id::eAcc_type_mask) {
            CRef<CSeq_id> TryAcc;
            TryAcc.Reset(new CSeq_id(Id->GetLocal().GetStr()));
            if (!TryAcc->IsGi() && !TryAcc->IsLocal()) {
                Id = TryAcc.GetPointer();
            }
        }
        /*CRef<CSeq_id> TryAcc;
        try {
            TryAcc.Reset(new CSeq_id(Id->GetLocal().GetStr()));
            if (!TryAcc->IsGi() && !TryAcc->IsLocal()) {
                Id = TryAcc.GetPointer();
            }
        } catch(...) {
            ;
        }*/
    }

    // Second, if the Acc is versionless, see if we can find a version for it
    // in this assembly.
    const CTextseq_id* textseqid = Id->GetTextseq_Id();
    if (textseqid != 0 &&
        textseqid->IsSetAccession() &&
        !textseqid->IsSetVersion()
       ) {
        const int Ver = m_AccToVerMap.find(textseqid->GetAccession())->second;
        CRef<CSeq_id> NewId(new CSeq_id());
        NewId->Set(Id->Which(), textseqid->GetAccession(), kEmptyStr, Ver);
        Id = NewId;
    }

    return Id;
}

CConstRef<CSeq_id>
CGencollIdMapper::x_ApplyPatternToId(CConstRef<CSeq_id> Id,
                                     const SIdSpec& Spec
                                    ) const
{
    if (Id->GetTextseq_Id() == 0 && !Id->IsGi() && !Spec.Pattern.empty()) {
        // && Id->GetLocal().GetStr().find(Spec.Pattern) != NPOS
        CRef<CSeq_id> NewId(new CSeq_id());
        NewId->SetLocal().SetStr() = Id->GetSeqIdString();
        string Pre, Post;
        const size_t DelimPos = Spec.Pattern.find(DELIM);
        Pre.assign(Spec.Pattern.data(), 0, DelimPos);
        //Post.assign(Spec.Pattern.data(), DelimPos+DELIM.length(),
        //            Spec.Pattern.length()-DelimPos-DELIM.length());
        if (!Pre.empty() || !Post.empty()) {
            NStr::ReplaceInPlace(NewId->SetLocal().SetStr(), Pre, kEmptyStr);
            //NStr::ReplaceInPlace(NewId->SetLocal().SetStr(), Post, kEmptyStr);
            Id = NewId;
        }
    }
    return Id;
}

int
CGencollIdMapper::x_GetRole(const objects::CGC_Sequence& Seq) const
{
    int SeqRole = SIdSpec::e_Role_NotSet;
    if (Seq.CanGetRoles()) {
        ITERATE (CGC_Sequence::TRoles, RoleIter, Seq.GetRoles()) {
            //if ((*RoleIter) >= eGC_SequenceRole_top_level) {
            if ((*RoleIter) >= eGC_SequenceRole_submitter_pseudo_scaffold) {
                continue;
            }
            SeqRole = min(SeqRole, *RoleIter);
        }
    }
    return SeqRole;
}

void
CGencollIdMapper::x_AddSeqToMap(const CSeq_id& Id,
                                CConstRef<CGC_Sequence> Seq
                               )
{
    if (x_GetRole(*Seq) == SIdSpec::e_Role_NotSet) {
        return;
    }
    const CSeq_id_Handle Handle = CSeq_id_Handle::GetHandle(Id);

    TIdToSeqMap::iterator Found;
    Found = m_IdToSeqMap.find(Handle);
    if (Found != m_IdToSeqMap.end()) {
        const int OldRole = x_GetRole(*Found->second);
        const int NewRole = x_GetRole(*Seq);
        if (NewRole == SIdSpec::e_Role_NotSet ||
            (OldRole != SIdSpec::e_Role_NotSet && OldRole >= NewRole)
           ) {
            return;
        }
        //if(Seq->GetSeq_id_synonyms().size() <=
        //    Found->second->GetSeq_id_synonyms().size())
        //    return;
        m_IdToSeqMap.erase(Found);
    }
    m_IdToSeqMap.insert(make_pair(Handle, Seq));

    {{
        CConstRef<CGC_Sequence> ParentSeq = Seq->GetParent();
        CGC_TaggedSequences_Base::TState ParentState = Seq->GetParentRelation();
        if (ParentSeq &&
            ParentState == CGC_TaggedSequences_Base::eState_placed
           ) {
            const CSeq_id_Handle ParentIdH(
                CSeq_id_Handle::GetHandle(ParentSeq->GetSeq_id())
            );
            m_ChildToParentMap.insert(make_pair(Handle, ParentSeq));
        }
    }}
}

CConstRef<CGC_SeqIdAlias>
s_GetSeqIdAlias_GenBankRefSeq(CConstRef<CGC_TypedSeqId> tsid)
{
    const CGC_TypedSeqId::E_Choice syn_type = tsid->Which();
    const bool is_gb = (syn_type == CGC_TypedSeqId::e_Genbank);
    if (is_gb || syn_type == CGC_TypedSeqId::e_Refseq) {
        return ConstRef(is_gb ? &tsid->GetGenbank() : &tsid->GetRefseq());
    }
    return CConstRef<CGC_SeqIdAlias>();
}

void
CGencollIdMapper::x_BuildSeqMap(const CGC_Sequence& Seq)
{
    if (Seq.CanGetSeq_id()) {
        int IdCount = 0;
        CTypeConstIterator<CSeq_id> IdIter(Seq);
        for ( ; IdIter; ++IdIter) {
            if (IdIter->Equals(Seq.GetSeq_id())) {
                IdCount++;
            }
        }
        if (IdCount <= 2) {
            x_AddSeqToMap(Seq.GetSeq_id(), ConstRef(&Seq));
        }
    }

//#warning FIXME: unsure why condition includes Seq.CanGetStructure()
    if (Seq.CanGetSeq_id_synonyms() &&
        (!Seq.GetSeq_id_synonyms().empty() || Seq.CanGetStructure())
       ) {
        CConstRef<CGC_Sequence> SeqRef(&Seq);
        ITERATE (CGC_Sequence::TSeq_id_synonyms, it, Seq.GetSeq_id_synonyms()) {
            const CGC_TypedSeqId::E_Choice syn_type = (*it)->Which();
            CConstRef<CGC_SeqIdAlias> seq_id_alias = s_GetSeqIdAlias_GenBankRefSeq(*it);
            if (seq_id_alias.NotNull()) {
                if (seq_id_alias->IsSetPublic()) {
                    x_AddSeqToMap(seq_id_alias->GetPublic(), SeqRef);
                }
                if (seq_id_alias->IsSetGpipe()) {
                    x_AddSeqToMap(seq_id_alias->GetGpipe(), SeqRef);
                }
                if (seq_id_alias->IsSetGi()) {
                    x_AddSeqToMap(seq_id_alias->GetGi(), SeqRef);
                }
            }
            else if (syn_type == CGC_TypedSeqId::e_External) {
                x_AddSeqToMap((*it)->GetExternal().GetId(), SeqRef);
            }
            else if (syn_type == CGC_TypedSeqId::e_Private) {
                x_AddSeqToMap((*it)->GetPrivate(), SeqRef);
            }
        }
    }

    // child sequences
    if (Seq.CanGetSequences()) {
        ITERATE (CGC_Sequence::TSequences, TagIter, Seq.GetSequences()) {
            if (!(*TagIter)->CanGetSeqs()) {
                continue;
            }
            ITERATE (CGC_TaggedSequences::TSeqs, SeqIter, (*TagIter)->GetSeqs()) {
                x_BuildSeqMap(**SeqIter);
            }
        }
    }
}

void
CGencollIdMapper::x_BuildSeqMap(const CGC_AssemblyUnit& assm)
{
    if (assm.IsSetMols()) {
        ITERATE (CGC_AssemblyUnit::TMols, iter, assm.GetMols()) {
            const CGC_Replicon::TSequence& s = (*iter)->GetSequence();
            if (s.IsSingle()) {
                x_BuildSeqMap(s.GetSingle());
            }
            else {
                ITERATE (CGC_Replicon::TSequence::TSet, it, s.GetSet()) {
                    x_BuildSeqMap(**it);
                }
            }
        }
    }
    ITERATE (CGC_Sequence::TSequences, TagIter, assm.GetOther_sequences()) {
        //if( (*TagIter)->GetState() != CGC_TaggedSequences::eState_placed ||
        //    !(*TagIter)->CanGetSeqs())
        //    continue;
        if (!(*TagIter)->CanGetSeqs()) {
            continue;
        }
        ITERATE (CGC_TaggedSequences::TSeqs, SeqIter, (*TagIter)->GetSeqs()) {
            x_BuildSeqMap(**SeqIter);
        }
    }
    /*
    if (assm.IsSetUnplaced()) {
        ITERATE (CGC_AssemblyUnit::TUnplaced, it, assm.GetUnplaced()) {
            x_BuildSeqMap(**it);
        }
    }*/
}

void
CGencollIdMapper::x_BuildSeqMap(const CGC_Assembly& assm)
{
    if (assm.IsUnit()) {
        x_BuildSeqMap(assm.GetUnit());
    }
    else if (assm.IsAssembly_set()) {
        x_BuildSeqMap(assm.GetAssembly_set().GetPrimary_assembly());
        if (assm.GetAssembly_set().IsSetMore_assemblies()) {
            ITERATE (CGC_Assembly::TAssembly_set::TMore_assemblies,
                     iter,
                     assm.GetAssembly_set().GetMore_assemblies()
                    ) {
                x_BuildSeqMap(**iter);
            }
        }
    }
}

CConstRef<CSeq_id>
CGencollIdMapper::x_GetIdFromSeqAndSpec(const CGC_Sequence& Seq,
                                        const SIdSpec& Spec
                                       ) const
{
    if (Spec.Primary && Seq.CanGetSeq_id()) {
        return ConstRef(&Seq.GetSeq_id());
    }
    if (Seq.CanGetSeq_id_synonyms()) {
        ITERATE (CGC_Sequence::TSeq_id_synonyms, it, Seq.GetSeq_id_synonyms()) {
            const CGC_TypedSeqId::E_Choice syn_type = (*it)->Which();
            if (syn_type != Spec.TypedChoice) {
                continue;
            }
            CConstRef<CGC_SeqIdAlias> seq_id_alias = s_GetSeqIdAlias_GenBankRefSeq(*it);
            if (seq_id_alias.NotNull()) {
                if (seq_id_alias->IsSetPublic() && Spec.Alias == CGC_SeqIdAlias::e_Public) {
                    return ConstRef(&seq_id_alias->GetPublic());
                }
                if (seq_id_alias->IsSetGpipe() && Spec.Alias == CGC_SeqIdAlias::e_Gpipe) {
                    return ConstRef(&seq_id_alias->GetGpipe());
                }
                if (seq_id_alias->IsSetGi() && Spec.Alias == CGC_SeqIdAlias::e_Gi) {
                    return ConstRef(&seq_id_alias->GetGi());
                }
            }
            else if (syn_type == CGC_TypedSeqId::e_External) {
                const CGC_External_Seqid& ExternalId = (*it)->GetExternal();
                if (ExternalId.GetExternal() == Spec.External) {
                    if (Spec.Pattern.empty()) {
                        return ConstRef(&ExternalId.GetId());
                    }
                    CRef<CSeq_id> NewId(new CSeq_id());
                    NewId->SetLocal().SetStr(
                        NStr::Replace(Spec.Pattern,
                                      DELIM,
                                      ExternalId.GetId().GetSeqIdString()
                                     )
                    );
                    //NewId->SetLocal().SetStr() = Spec.Pattern + ExternalId.GetId().GetSeqIdString();
                    return NewId;
                }
            }
            else if (syn_type == CGC_TypedSeqId::e_Private) {
                const CSeq_id& Private = (*it)->GetPrivate();
                if (Spec.Pattern.empty()) {
                    return ConstRef(&Private);
                }
                CRef<CSeq_id> NewId(new CSeq_id());
                NewId->SetLocal().SetStr(NStr::Replace(Spec.Pattern, DELIM, Private.GetSeqIdString()));
                //NewId->SetLocal().SetStr() = Spec.Pattern + Private.GetSeqIdString();
                return NewId;
            }
        }
    }

   
    return CConstRef<CSeq_id>();
}

int
CGencollIdMapper::x_CanSeqMeetSpec(const CGC_Sequence& Seq,
                                   const SIdSpec& Spec,
                                   const int Level
                                  ) const
{
    if (Level == 3) {
        return e_No;
    }
    //int SeqRole = x_GetRole(Seq);
    bool HasId = false;
    
    if (Spec.Primary && Seq.CanGetSeq_id()) {
        HasId = true;
    }
    
    if (!Spec.Primary && Seq.CanGetSeq_id_synonyms()) {
        ITERATE (CGC_Sequence::TSeq_id_synonyms, it, Seq.GetSeq_id_synonyms()) {
            const CGC_TypedSeqId::E_Choice syn_type = (*it)->Which();
            if (syn_type != Spec.TypedChoice) {
                continue;
            }

            const bool alias_is_public = (Spec.Alias == CGC_SeqIdAlias::e_Public);
            const bool alias_is_gpipe = (Spec.Alias == CGC_SeqIdAlias::e_Gpipe);
            const bool alias_is_gi = (Spec.Alias == CGC_SeqIdAlias::e_Gi);

            CConstRef<CGC_SeqIdAlias> seq_id_alias = s_GetSeqIdAlias_GenBankRefSeq(*it);
            if (seq_id_alias.NotNull()) {
                if ((seq_id_alias->CanGetPublic() && alias_is_public) ||
                    (seq_id_alias->CanGetGpipe() && alias_is_gpipe) ||
                    (seq_id_alias->CanGetGi() && alias_is_gi)
                   ) {
                    HasId = true;
                }
            }
            else if (syn_type == CGC_TypedSeqId::e_External) {
                if ((*it)->GetExternal().GetExternal() == Spec.External) {
                    HasId = true;
                }
            }
            else if (syn_type == CGC_TypedSeqId::e_Private) {
                HasId = true;
            }
        }
    }
    if (HasId) {
        if (Spec.Role == SIdSpec::e_Role_NotSet || Seq.HasRole(Spec.Role)) {
            // Has the ID match, and the Role either doesn't matter, or is matched
            return e_Yes;
        }
    }
    CConstRef<CGC_Sequence> ParentSeq = Seq.GetParent();
    if (ParentSeq.NotNull()) {
        if ((Spec.Role != SIdSpec::e_Role_NotSet && 
            (Spec.Role >= eGC_SequenceRole_top_level || Spec.Role <= x_GetRole(*ParentSeq)))
           ) {
            const int Parent = x_CanSeqMeetSpec(*ParentSeq, Spec, Level + 1);
            if (Parent == e_Yes || Parent == e_Up) {
                return e_Up;
            }
        }
    }
    if (Seq.CanGetSequences() ) { 
        ITERATE (CGC_Sequence::TSequences, TagIter, Seq.GetSequences()) {
            if ((*TagIter)->GetState() != CGC_TaggedSequences::eState_placed) {
                continue;
            }
            ITERATE (CGC_TaggedSequences::TSeqs, SeqIter, (*TagIter)->GetSeqs()) {
                const int Child = x_CanSeqMeetSpec(**SeqIter, Spec, Level + 1);
                if (Child == e_Yes || Child == e_Down) {
                    return e_Down;
                }
            }
        }
    }
    return e_No;
}

bool
CGencollIdMapper::x_MakeSpecForSeq(const CSeq_id& Id,
                                   const CGC_Sequence& Seq,
                                   SIdSpec& Spec
                                  ) const
{
    Spec.Primary = false;
    Spec.TypedChoice = CGC_TypedSeqId::e_not_set;
    Spec.Alias = CGC_SeqIdAlias::e_None;
    Spec.Role = SIdSpec::e_Role_NotSet;
    Spec.External.clear();
    Spec.Pattern.clear();

    if(Id.Equals(Seq.GetSeq_id()))
        Spec.Primary = true;

    if (Seq.CanGetRoles()) {
        Spec.Role = x_GetRole(Seq);
    }
    // Loop over the IDs, find which matches the given ID
    if (Seq.CanGetSeq_id_synonyms()) {
        ITERATE (CGC_Sequence::TSeq_id_synonyms, it, Seq.GetSeq_id_synonyms()) {
            const CGC_TypedSeqId::E_Choice syn_type = (*it)->Which();
            CConstRef<CGC_SeqIdAlias> seq_id_alias = s_GetSeqIdAlias_GenBankRefSeq(*it);
            if (seq_id_alias.NotNull()) {
                Spec.TypedChoice = syn_type;
                if (seq_id_alias->IsSetPublic() && seq_id_alias->GetPublic().Equals(Id)) {
                    Spec.Alias = CGC_SeqIdAlias::e_Public;
                    return true;
                }
                if (seq_id_alias->IsSetGpipe() && seq_id_alias->GetGpipe().Equals(Id)) {
                    Spec.Alias = CGC_SeqIdAlias::e_Gpipe;
                    return true;
                }
                if (seq_id_alias->IsSetGi() && seq_id_alias->GetGi().Equals(Id)) {
                    Spec.Alias = CGC_SeqIdAlias::e_Gi;
                    return true;
                }
            }
            else if (syn_type == CGC_TypedSeqId::e_External) {
                const CGC_External_Seqid& ExternalId = (*it)->GetExternal();
                if (ExternalId.GetId().Equals(Id)) {
                    Spec.TypedChoice = CGC_TypedSeqId::e_External;
                    Spec.External = ExternalId.GetExternal();
                    return true;
                }
            }
            else if (syn_type == CGC_TypedSeqId::e_Private &&
                     (*it)->GetPrivate().Equals(Id)
                    ) {
                Spec.TypedChoice = CGC_TypedSeqId::e_Private;
                return true;
            }
        }
    }
    // If we didn't find it normally, try again, looking for Pattern matches
    if (Seq.CanGetSeq_id_synonyms()) {
        ITERATE (CGC_Sequence::TSeq_id_synonyms, it, Seq.GetSeq_id_synonyms()) {
            const CGC_TypedSeqId::E_Choice syn_type = (*it)->Which();
            switch (syn_type) {
            case CGC_TypedSeqId::e_External:
            {
                const CGC_External_Seqid& ExternalId = (*it)->GetExternal();
                if (!NStr::Equal(ExternalId.GetExternal(), CHROMO_EXT)) {
                    continue;
                }
                const size_t Start(
                    NStr::Find(Id.GetSeqIdString(),
                               ExternalId.GetId().GetSeqIdString()
                              )
                );
                if (Start != NPOS) {
                    Spec.TypedChoice = syn_type;
                    Spec.External = ExternalId.GetExternal();
                    //Spec.Pattern = NStr::Replace(Id.GetSeqIdString(), Private.GetSeqIdString(), DELIM);
                    Spec.Pattern = Id.GetSeqIdString().substr(0, Start) + DELIM;
                    return true;
                }
                break;
            }
            case CGC_TypedSeqId::e_Private:
            {
                const CSeq_id& Private = (*it)->GetPrivate();
                const size_t Start(
                    NStr::Find(Id.GetSeqIdString(),
                               Private.GetSeqIdString()
                              )
                );
                if (Start != NPOS) {
                    Spec.TypedChoice = syn_type;
                    //Spec.Pattern = NStr::Replace(Id.GetSeqIdString(), Private.GetSeqIdString(), DELIM);
                    Spec.Pattern = Id.GetSeqIdString().substr(0, Start) + DELIM;
                    return true;
                }
                break;
            }
            default:
                break;
            }
        }
    }
    return false;
}

CConstRef<objects::CGC_Sequence>
CGencollIdMapper::x_FindParentSequence(const objects::CSeq_id& Id,
                                       const objects::CGC_Assembly& Assembly,
                                       const int Depth
                                      ) const
{
    CConstRef<CGC_Sequence> Result;
    const CSeq_id_Handle IdH = CSeq_id_Handle::GetHandle(Id);
    TChildToParentMap::const_iterator Found = m_ChildToParentMap.find(IdH);
    if (Found != m_ChildToParentMap.end()) {
        return Found->second;
    }
    if (Depth > 5) {
        LOG_POST(Warning << "x_FindParentSequence: Depth Bounce " << Id.AsFastaString());
        return Result;
    }

    if (Assembly.IsAssembly_set()) {
        const CGC_AssemblySet& AssemblySet = Assembly.GetAssembly_set();
        if (AssemblySet.CanGetPrimary_assembly()) {
            Result = x_FindParentSequence(Id, AssemblySet.GetPrimary_assembly(), Depth + 1);
            if (Result.NotNull()) {
                return Result;
            }
        }
        if (AssemblySet.CanGetMore_assemblies()) {
            ITERATE (CGC_AssemblySet::TMore_assemblies, AssemIter, AssemblySet.GetMore_assemblies()) {
                Result = x_FindParentSequence(Id, **AssemIter, Depth + 1);
                if (Result.NotNull()) {
                    return Result;
                }
            }
        }
    }
    else if (Assembly.IsUnit()) {
        const CGC_AssemblyUnit& AsmUnit = Assembly.GetUnit();
        if (AsmUnit.CanGetMols()) {
            ITERATE (CGC_AssemblyUnit::TMols, MolIter, AsmUnit.GetMols()) {
                if ((*MolIter)->GetSequence().IsSingle()) {
                    const CGC_Sequence& Parent = (*MolIter)->GetSequence().GetSingle();
                    if (x_IsParentSequence(Id, Parent)) {
                        return ConstRef(&Parent);
                    } // end IsParent if
                } // end Seq Single
            } // end Mols Loop
        } // end Mols
    } // end AsmUnit
    return Result;
}

bool
CGencollIdMapper::x_IsParentSequence(const CSeq_id& Id,
                                     const CGC_Sequence& Parent
                                    ) const
{
    if (!Parent.CanGetSequences()) {
        return false;
    }
    ITERATE (CGC_Sequence::TSequences, ChildIter, Parent.GetSequences()) {
        if ((*ChildIter)->GetState() != CGC_TaggedSequences::eState_placed ||
            !(*ChildIter)->CanGetSeqs()
           ) {
            continue;
        }
        ITERATE (CGC_TaggedSequences::TSeqs, SeqIter, (*ChildIter)->GetSeqs()) {
            if ((*SeqIter)->GetSeq_id().Equals(Id)) {
                return true;
            }
            ITERATE (CGC_Sequence::TSeq_id_synonyms, SynIter, (*SeqIter)->GetSeq_id_synonyms()) {
                const CGC_TypedSeqId& TypedId = **SynIter;
                CTypeConstIterator<CSeq_id> IdIter(TypedId);
                while (IdIter) {
                    if (IdIter->Equals(Id)) {
                        return true;
                    }
                    ++IdIter;
                }
                //if(TypedId.IsRefseq() && TypedId.GetRefseq().CanGetPublic()) {
                //    if (TypedId.GetRefseq().GetPublic().Equals(Id)) {
                //        return true;
                //    }
                //}
            }
        }
    }
    return false;
}

CConstRef<CGC_Sequence>
CGencollIdMapper::x_FindChromosomeSequence(const CSeq_id& Id, const SIdSpec& Spec) const
{
    if (Id.IsGi() && Id.GetGi() > GI_FROM(TIntId, 50)) {
        return CConstRef<CGC_Sequence>();
    }
    if (CSeq_id::IdentifyAccession(Id.GetSeqIdString(true)) >= CSeq_id::eAcc_type_mask) {
        return CConstRef<CGC_Sequence>();
    }

    const string IdStr = Id.GetSeqIdString(true);
    TIdToSeqMap::const_iterator Found = m_IdToSeqMap.end();
    ITERATE (vector<string>, ChromoIter, m_Chromosomes) {
        if (NStr::Find(IdStr, *ChromoIter) != NPOS) {
            CRef<CSeq_id> Temp(new CSeq_id());
            Temp->SetLocal().SetStr() = *ChromoIter;
            // If we have a pattern, double check it.
            /*if (!Spec.Pattern.empty() &&
                Id.Equals(*x_ApplyPatternToId(Temp, Spec))) {
                CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(*Temp);
                Found = m_IdToSeqMap.find(Idh);
                break;
            }
            // If we have no pattern, just trust the string.find()
            else if (Spec.Pattern.empty())*/ {
                CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(*Temp);
                Found = m_IdToSeqMap.find(Idh);
                break;
            }
        }
    }
    if (Found != m_IdToSeqMap.end()) {
        return Found->second;
    }
    return CConstRef<CGC_Sequence>();
}

CRef<CSeq_loc>
CGencollIdMapper::x_Map_OneToOne(const CSeq_loc& SourceLoc,
                                 const CGC_Sequence& Seq,
                                 const SIdSpec& Spec
                                ) const
{
    //if(!x_CanSeqMeetSpec(Seq, Spec))
    //    return CRef<CSeq_loc>();
    CConstRef<CSeq_id> DestId = x_GetIdFromSeqAndSpec(Seq, Spec);
    if (DestId.IsNull()) {
        return CRef<CSeq_loc>();
    }

    CRef<CSeq_loc> Result(new CSeq_loc());
    Result->Assign(SourceLoc);

    CTypeIterator<CSeq_id> IdIter(*Result);
    for ( ; IdIter; ++IdIter) {
        IdIter->Assign(*DestId);
    }
    return Result;
}

bool
s_DoesBioseqRecurse(const CBioseq& Bioseq)
{
    ITERATE (CBioseq::TId, IdIter, Bioseq.GetId()) {
        CTypeConstIterator<CSeq_id> PartIter(Bioseq.GetInst());
        for ( ; PartIter; ++PartIter) {
            if ((*IdIter)->Equals(*PartIter)) {
                return true;
            }
        }
    }
    return false;
}

CRef<CSeq_loc>
CGencollIdMapper::x_Map_Up(const CSeq_loc& SourceLoc,
                           const CGC_Sequence& Seq,
                           const SIdSpec& Spec
                          ) const
{
    CRef<CSeq_loc> Result2;
    SSeqMapSelector  Sel;
    Sel.SetFlags(CSeqMap::fFindAny).SetResolveCount(10);
    CRef<CSeq_loc_Mapper> LocalUp(new CSeq_loc_Mapper(*m_Assembly, CSeq_loc_Mapper::eSeqMap_Up, Sel));
    Result2 = LocalUp->Map(SourceLoc);
    if(Result2.IsNull() || Result2->IsNull()) {
        return CRef<CSeq_loc>();
    }
    Result2 = Map(*Result2, Spec);
    return Result2;


    if (x_CanSeqMeetSpec(Seq, Spec) == e_No) {
        return CRef<CSeq_loc>();
    }
    if (!Seq.CanGetStructure()) {
        return CRef<CSeq_loc>();
    }
    const CDelta_ext& Ext = Seq.GetStructure();
    CRef<CBioseq> Bioseq(new CBioseq);
    Bioseq->SetInst().SetExt().SetDelta().Assign(Ext);
    Bioseq->SetInst().SetRepr(CSeq_inst::eRepr_delta);
    Bioseq->SetInst().SetMol(CSeq_inst::eMol_na);

    CRef<CSeq_id> Id(new CSeq_id());
    Id->Assign(Seq.GetSeq_id());
    if (Seq.CanGetSeq_id_synonyms()) {
        ITERATE (CGC_Sequence::TSeq_id_synonyms, SynIter, Seq.GetSeq_id_synonyms()) {
            CTypeConstIterator<CSeq_id> SynIdIter(**SynIter);
            while (SynIdIter) {
                CRef<CSeq_id> SynId(new CSeq_id());
                SynId->Assign(*SynIdIter);
                Bioseq->SetId().push_back(SynId);
                ++SynIdIter;
            }
        }
    }
    else {
        Bioseq->SetId().push_back(Id);
    }

    if (s_DoesBioseqRecurse(*Bioseq) ) {
        return CRef<CSeq_loc>();
    }

    CRef<CSeqMap> SeqMap = CSeqMap::CreateSeqMapForBioseq(*Bioseq);
    CRef<CSeq_id> MapId(new CSeq_id());
    MapId->Assign(*Id);
    // Direction
    const CSeq_loc_Mapper::ESeqMapDirection Direction = CSeq_loc_Mapper::eSeqMap_Up;

    CConstRef<CSeq_id> SpecId = x_GetIdFromSeqAndSpec(Seq, Spec);
    if (SpecId) {
        MapId->Assign(*SpecId);
    }

    CRef<CSeq_loc_Mapper> Mapper(
        new CSeq_loc_Mapper(1, *SeqMap, Direction, MapId)
    );
    CRef<CSeq_loc> Result = Mapper->Map(SourceLoc);
    /*if(Result && Result->IsPacked_int()) {
        cerr << "UP BECAME PACKED INT" << endl;
        cerr << MSerial_AsnText << SourceLoc;
        cerr << MSerial_AsnText << *Result;
        cerr << MSerial_AsnText << *Bioseq;
        exit(1);
    }*/
    if (Result.NotNull()) {
        if (Result->IsNull()) {
            Result.Reset();
        }
        else {
            Result = Map(*Result, Spec);
        }
    }
    return Result;
}

CRef<CSeq_loc>
CGencollIdMapper::x_Map_Down(const CSeq_loc& SourceLoc,
                             const CGC_Sequence& Seq,
                             const SIdSpec& Spec
                            ) const
{
    if (!Seq.CanGetStructure()) {
        return CRef<CSeq_loc>();
    }
    const CDelta_ext& Ext = Seq.GetStructure();
    CRef<CBioseq> Bioseq(new CBioseq());
    Bioseq->SetInst().SetExt().SetDelta().Assign(Ext);
    Bioseq->SetInst().SetRepr(CSeq_inst::eRepr_delta);
    Bioseq->SetInst().SetMol(CSeq_inst::eMol_na);

    CRef<CSeq_id> Id(new CSeq_id());
    Id->Assign(Seq.GetSeq_id());
    if (Seq.CanGetSeq_id_synonyms()) {
        ITERATE (CGC_Sequence::TSeq_id_synonyms, SynIter, Seq.GetSeq_id_synonyms()) {
            CTypeConstIterator<CSeq_id> SynIdIter(**SynIter);
            while (SynIdIter) {
                CRef<CSeq_id> SynId(new CSeq_id());
                SynId->Assign(*SynIdIter);
                Bioseq->SetId().push_back(SynId);
                ++SynIdIter;
            }
        }
    }
    else {
        Bioseq->SetId().push_back(Id);
    }

    if (s_DoesBioseqRecurse(*Bioseq) ) {
        return CRef<CSeq_loc>();
    }

    CRef<CSeqMap> SeqMap = CSeqMap::CreateSeqMapForBioseq(*Bioseq);
    CRef<CSeq_id> MapId(new CSeq_id());
    MapId->Assign(*SourceLoc.GetId()); // Parent seq id for the Mapper
    // Direction
    const CSeq_loc_Mapper::ESeqMapDirection Direction = CSeq_loc_Mapper::eSeqMap_Down;

    //CConstRef<CSeq_id> SpecId = x_GetIdFromSeqAndSpec(Seq, Spec);
    //if(SpecId)
    //    MapId->Assign(*SpecId);


    CRef<CSeq_loc_Mapper> Mapper(
        new CSeq_loc_Mapper(1, *SeqMap, Direction, MapId)
    );

    CRef<CSeq_loc> Result = Mapper->Map(SourceLoc);

    /*if(Result && Result->IsPacked_int()) {
        cerr << "DOWN BECAME PACKED INT" << endl;
        cerr << MSerial_AsnText << SourceLoc;
        cerr << MSerial_AsnText << *Result;
        cerr << MSerial_AsnText << *Bioseq;
        exit(1);
    }*/

    if (Result.NotNull()) {
        if (Result->IsNull()) {
            Result.Reset();
        }
        else {
            //cerr << MSerial_AsnText << *Bioseq;
            //cerr << MSerial_AsnText << SourceLoc;
            //cerr << MSerial_AsnText << *Result;
            Result = Map(*Result, Spec);
        }
    }

    return Result;
}

CGencollIdMapper::E_Gap
CGencollIdMapper::IsLocInAGap(const CSeq_loc& Loc) const
{
    if (Loc.IsMix()) {
        E_Gap Result = e_None;
        ITERATE (CSeq_loc_mix::Tdata, LocIter, Loc.GetMix().Get()) {
            E_Gap Curr = IsLocInAGap(**LocIter);
            Result = x_Merge_E_Gaps(Result, Curr);
        }
        return Result;
    }
    if (Loc.IsPacked_int()) {
        E_Gap Result = e_None;
        ITERATE (CPacked_seqint::Tdata, IntIter, Loc.GetPacked_int().Get()) {
            E_Gap Curr = x_IsLoc_Int_InAGap(**IntIter);
            Result = x_Merge_E_Gaps(Result, Curr);
        }
        return Result;
    }
    if (Loc.IsInt()) {
        return x_IsLoc_Int_InAGap(Loc.GetInt());
    }
    if (Loc.IsPnt()) {
        CSeq_interval Int;
        Int.SetId().Assign(Loc.GetPnt().GetId());
        Int.SetFrom(Loc.GetPnt().GetPoint());
        Int.SetTo(Loc.GetPnt().GetPoint());
        return x_IsLoc_Int_InAGap(Int);
    }
    return e_None;
}

CConstRef<CGC_Assembly>
CGencollIdMapper::GetInternalGencoll(void) const
{
    return m_Assembly;
}

CGencollIdMapper::E_Gap
CGencollIdMapper::x_IsLoc_Int_InAGap(const CSeq_interval& Int) const
{
    CRange<TSeqPos> LocRange;
    LocRange.SetFrom(Int.GetFrom());
    LocRange.SetTo(Int.GetTo());

    CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(Int.GetId());
    TIdToSeqMap::const_iterator Found = m_IdToSeqMap.find(Idh);

    if (Found != m_IdToSeqMap.end()) {
        CConstRef<CGC_Sequence> Seq = Found->second;
        if (!Seq->CanGetStructure()) {
            return e_None;
        }

        const CDelta_ext& DeltaExt = Seq->GetStructure();
        TSeqPos Start = 0;
        ITERATE (CDelta_ext::Tdata, SeqIter, DeltaExt.Get()) {
            const CDelta_seq& DeltaSeq = **SeqIter;
            if (DeltaSeq.IsLoc()) {
                Start += DeltaSeq.GetLoc().GetInt().GetLength();
                continue;
            }
            if (DeltaSeq.IsLiteral()) {
                CRange<TSeqPos> GapRange(
                    Start,
                    Start + DeltaSeq.GetLiteral().GetLength() - 1
                );
                Start += DeltaSeq.GetLiteral().GetLength();
                CRange<TSeqPos> Intersect = LocRange.IntersectionWith(GapRange);
                if (Intersect.Empty()) {
                    continue;
                }
                if (Intersect == GapRange) {
                    return e_Spans;
                }
                if (Intersect == LocRange) {
                    return e_Contained;
                }
                return e_Overlaps;
            }
        }
        CRange<TSeqPos> ExtendRange(Start, numeric_limits<TSeqPos>::max());
        CRange<TSeqPos> Intersect = LocRange.IntersectionWith(ExtendRange);
        if (Intersect == LocRange) {
            return e_Contained;
        }
        if (!Intersect.Empty()) {
            return e_Overlaps;
        }
    }

    return e_None;
}

CGencollIdMapper::E_Gap
CGencollIdMapper::x_Merge_E_Gaps(const E_Gap First, const E_Gap Second) const
{
    if (First == e_None) {
        return Second;
    }
    if (First != Second) {
        return e_Complicated;
    }
    // First == e_Complicated || First == Second || any other case
    return First;
}

CGencollIdMapper::SIdSpec::SIdSpec()
    : Primary(false),
      TypedChoice(objects::CGC_TypedSeqId::e_not_set), 
      Alias(objects::CGC_SeqIdAlias::e_None),
      External(kEmptyStr),
      Pattern(kEmptyStr), 
      Role(e_Role_NotSet)
{
}

CGencollIdMapper::SIdSpec::operator string() const
{
    return ToString();
}

bool
CGencollIdMapper::SIdSpec::operator<(const SIdSpec& Other) const
{
    return !(TypedChoice < Other.TypedChoice);
}

bool
CGencollIdMapper::SIdSpec::operator==(const SIdSpec& Other) const
{
    if (!(Primary == Other.Primary &&
          TypedChoice == Other.TypedChoice &&
          Alias == Other.Alias &&
          External == Other.External &&
          Pattern == Other.Pattern &&
          Role == Other.Role 
         )
       ) {
        return false;
    }
    return true;
}

string
CGencollIdMapper::SIdSpec::ToString(void) const
{
    string Result;
    Result.reserve(64);
    
    if(Primary)
        Result += "Prim";
    else
        Result += "NotPrim";
    Result += ":";

    switch (TypedChoice) {
    case 0:
        Result += "NotSet";
        break;
    case 1:
        Result += "GenBank";
        break;
    case 2:
        Result += "RefSeq";
        break;
    case 3:
        Result += "Private";
        break;
    case 4:
        Result += "External";
        break;
    }
    Result += ":";
    
    switch (Alias) {
    case 0:
        Result += "NotSet";
        break;
    case 1:
        Result += "Public";
        break;
    case 2:
        Result += "Gpipe";
        break;
    case 3:
        Result += "Gi";
        break;
    }
    Result += ":";
    
    Result += External + ":" + Pattern;
    Result += ":";
    
    switch (Role) {
    case objects::eGC_SequenceRole_chromosome:
        Result += "CHRO";
        break;
    case objects::eGC_SequenceRole_scaffold:
        Result += "SCAF";
        break;
    case objects::eGC_SequenceRole_component:
        Result += "COMP";
        break;
    case objects::eGC_SequenceRole_top_level:
        Result += "TOP";
        break;
    case e_Role_NotSet:
        break;
    default:
        Result += NStr::IntToString(Role);
    }

    return Result;
}


END_NCBI_SCOPE

