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
#include <corelib/ncbistr.hpp>

#include <objmgr/util/sequence.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqset/gb_release_file.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seq_loc_mapper.hpp>

#include <serial/objistr.hpp>
#include <objmgr/util/obj_sniff.hpp>
#include <objtools/format/ostream_text_ostream.hpp>
#include <serial/iterator.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <algo/id_mapper/id_mapper.hpp>

#include <objects/genomecoll/genome_collection__.hpp>


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

CGencollIdMapper::CGencollIdMapper(CRef<CGC_Assembly> SourceAsm)
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
    // FIXME: If it returns null, deeply examine the Loc
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
        if (Found == m_IdToSeqMap.end())
            return false; // Unknown ID
    }

    const CGC_Sequence& Seq = *Found->second;
    return x_MakeSpecForSeq(*Id, Seq, Spec);
}


CRef<CSeq_loc> CGencollIdMapper::Map(const objects::CSeq_loc& Loc, const SIdSpec& Spec) const
{
    CRef<CSeq_loc_Mapper> Mapper;
    CRef<CSeq_loc> Result;

    if (m_Assembly.IsNull()) {
        return CRef<CSeq_loc>();
    }

    // Recurse down Mixes
    if (Loc.GetId() == NULL) {
        if (Loc.IsMix()) {
            Result.Reset(new CSeq_loc());
            CTypeConstIterator<CSeq_loc> LocIter(Loc);
            for ( ; LocIter; ++LocIter) {
                if (LocIter->Equals(Loc)) {
                    continue;
                }
                CRef<CSeq_loc> MappedLoc;
                MappedLoc = Map(*LocIter, Spec);
                if (!MappedLoc.IsNull() && !MappedLoc->IsNull()) {
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
        Result.Reset(new CSeq_loc());
        Result->Assign(Loc);
        return Result;
    }

    CConstRef<CGC_Sequence> Seq;

    {{
        CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(*Id);
        TIdToSeqMap::const_iterator Found = m_IdToSeqMap.find(Idh);
        if (Found != m_IdToSeqMap.end()) {
            Seq = Found->second;
            if (!Seq.IsNull()) {
                const int CanMap = x_CanSeqMeetSpec(*Seq, Spec);
                if (CanMap == e_Yes) {
                    Result = x_Map_OneToOne(Loc, *Seq, Spec);
                    if (!Result.IsNull() && !Result->IsNull()) {
                        return Result;
                    }
                }
            }
        }
    }}

    {{
        CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(*Id);
        TIdToSeqMap::const_iterator Found = m_IdToSeqMap.find(Idh);
        if (Found != m_IdToSeqMap.end()) {
            Seq = Found->second;
            if (!Seq.IsNull()) {
                const int CanMap = x_CanSeqMeetSpec(*Seq, Spec);
                if (CanMap == e_Down) {
                    Result = x_Map_Down(Loc, *Seq, Spec);
                    if (!Result.IsNull() && !Result->IsNull()) {
                        return Result;
                    }
                }
            }
        }
    }}

    {{
        CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(*Id);
        TIdToSeqMap::const_iterator Found = m_IdToSeqMap.find(Idh);
        if (Found != m_IdToSeqMap.end()) {
            /*Seq = Found->second;
            ITERATE (CGC_Sequence::TRoles, RoleIter, Seq->GetRoles()) {
                if (*RoleIter == eGC_SequenceRole_top_level) {
                    Result.Reset(new CSeq_loc());
                    Result->Assign(Loc);
                    return Result;
                }
            }*/
        }
        // Look for Parent seq
        Seq = x_FindParentSequence(*Id, *m_Assembly);
        if (!Seq.IsNull()) {
            const int CanMap = x_CanSeqMeetSpec(*Seq, Spec);
            if (CanMap == e_Yes) { // Not Up, but Yes cause its the parent already
                if (m_Assembly->IsRefSeq()) {
                    GuessSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
                }
                else if (m_Assembly->IsGenBank()) {
                    GuessSpec.TypedChoice = CGC_TypedSeqId::e_Genbank;
                }
                GuessSpec.Alias = SIdSpec::e_Gi;
                CRef<CSeq_loc> GiLoc = Map(Loc, GuessSpec);
                Result = x_Map_Up(GiLoc ? *GiLoc : Loc, *Seq, Spec);
                if (!Result.IsNull() && !Result->IsNull()) {
                    return Result;
                }
            }
        }
    }}

    {{
        Seq = x_FindChromosomeSequence(*Id, Spec);
        if (!Seq.IsNull())
            Result = x_Map_OneToOne(Loc, *Seq, Spec);
        if (!Result.IsNull() && !Result->IsNull()) {
            return Result;
        }
    }}

    return CRef<CSeq_loc>();
    // Nothing found, total fallback
    /*
    {{
        CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(*Id);
        TIdToSeqMap::const_iterator Found = m_IdToSeqMap.find(Idh);
        if (Found != m_IdToSeqMap.end()) {
            Seq = Found->second;
        MARKLINE;
            if (Spec.TypedChoice == CGC_TypedSeqId::e_External) {
                SIdSpec Submitter_Spec;
                Submitter_Spec.TypedChoice = CGC_TypedSeqId::e_External;
                Submitter_Spec.Role = eGC_SequenceRole_chromosome;
                Submitter_Spec.External = "SUBMITTER";
                Submitter_Spec.Pattern = DELIM;
                if (x_CanSeqMeetSpec(*Seq, Submitter_Spec)) {
                    return Map(Loc, Submitter_Spec);
                } else {
                    if (Seq->CanGetSeq_id_synonyms()) {
                        ITERATE (CGC_Sequence::TSeq_id_synonyms, it, Seq->GetSeq_id_synonyms()) {
                            if ((*it)->Which() != Spec.TypedChoice)
                                continue;
                            switch ((*it)->Which()) {
                            case CGC_TypedSeqId::e_External: {
                                const CGC_External_Seqid& ExternalId = (*it)->GetExternal();
                                Submitter_Spec.External = ExternalId.GetExternal();
                                if (x_CanSeqMeetSpec(*Seq, Submitter_Spec)) {
                                    return Map(Loc, Submitter_Spec);
                                }
                                break;
                            }}
                        }
                    }
                }
            }
        MARKLINE;
            SIdSpec RefSeq_Spec, GenBank_Spec, Gpipe_Spec, Chrom_Spec, Public_Spec;
            RefSeq_Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
            RefSeq_Spec.Alias = SIdSpec::e_Public;
            RefSeq_Spec.Role = eGC_SequenceRole_chromosome;
            GenBank_Spec.TypedChoice = CGC_TypedSeqId::e_Genbank;
            GenBank_Spec.Alias = SIdSpec::e_Public;
            GenBank_Spec.Role = eGC_SequenceRole_chromosome;
            Gpipe_Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
            Gpipe_Spec.Alias = SIdSpec::e_Gpipe;
            Gpipe_Spec.Role = eGC_SequenceRole_chromosome;
            Chrom_Spec.TypedChoice = CGC_TypedSeqId::e_Private;
            Chrom_Spec.Alias = SIdSpec::e_NotSet;
            Chrom_Spec.Pattern = DELIM;
            Chrom_Spec.Role = eGC_SequenceRole_chromosome;
            Public_Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
            Public_Spec.Alias = SIdSpec::e_Public;
            Public_Spec.Top = SIdSpec::e_Top_Public;
       MARKLINE;
            if (x_CanSeqMeetSpec(*Seq, RefSeq_Spec))
                return Map(Loc, RefSeq_Spec);
        MARKLINE;
            if (x_CanSeqMeetSpec(*Seq, GenBank_Spec))
                return Map(Loc, GenBank_Spec);
        MARKLINE;
            if (x_CanSeqMeetSpec(*Seq, Gpipe_Spec))
                return Map(Loc, Gpipe_Spec);
        MARKLINE;
            if (x_CanSeqMeetSpec(*Seq, Chrom_Spec))
                return Map(Loc, Chrom_Spec);
        MARKLINE;
            if (x_CanSeqMeetSpec(*Seq, Public_Spec))
                return Map(Loc, Public_Spec);
        MARKLINE;
        }
    }}
    */

    if (Seq.IsNull()) {
        return CRef<CSeq_loc>();
    }

    return Result;
}


bool CGencollIdMapper::CanMeetSpec(const objects::CSeq_loc& Loc, const SIdSpec& Spec) const
{
    bool Result = false;

    // FIXME: If it returns null, deeply examine the Loc
    if (Loc.GetId() == NULL)
        return CRef<CSeq_loc>();

    CConstRef<CSeq_id> Id(Loc.GetId());

    Id = x_FixImperfectId(Id, Spec);
    Id = x_ApplyPatternToId(Id, Spec);

    Id = x_NCBI34_Map_IdFix(Id);

    CConstRef<CGC_Sequence> Seq;

    {{
        CSeq_id_Handle Idh = CSeq_id_Handle::GetHandle(*Id);
        TIdToSeqMap::const_iterator Found = m_IdToSeqMap.find(Idh);
        if (Found != m_IdToSeqMap.end()) {
            Seq = Found->second;
            Result = x_CanSeqMeetSpec(*Seq, Spec);
            if (Result != e_No) {
                return true;
            }
        }
    }}

    {{
        // Look for Parent seq
        Seq = x_FindParentSequence(*Id, *m_Assembly);
        if (!Seq.IsNull()) {
            Result = x_CanSeqMeetSpec(*Seq, Spec);
            if (Result != e_No) {
                return true;
            }
        }
    }}

    {{
        Seq = x_FindChromosomeSequence(*Id, Spec);
        if (!Seq.IsNull()) {
            Result = x_CanSeqMeetSpec(*Seq, Spec);
            if (Result != e_No) {
                return true;
            }
        }
    }}

    if (Seq.IsNull()) {
        return false;
    }

    return false;
}

void
CGencollIdMapper::x_Init()
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

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    m_Scope.Reset(new CScope(*object_manager));

    x_BuildSeqMap(*m_Assembly);

    CTypeConstIterator<CSeq_id> IdIter(*m_Assembly);
    for ( ; IdIter; ++IdIter) {
        if (IdIter->GetTextseq_Id() != 0) {
            const string& Acc = IdIter->GetTextseq_Id()->GetAccession();
            int Ver = 1;
            if (IdIter->GetTextseq_Id()->CanGetVersion())
                Ver = IdIter->GetTextseq_Id()->GetVersion();
            m_AccToVerMap[Acc] = Ver;
        }
    }

    CTypeConstIterator<CGC_Replicon> ReplIter(*m_Assembly);
    for ( ; ReplIter; ++ReplIter) {
        if (ReplIter->CanGetName() &&
            NStr::Find(ReplIter->GetName(), "_random") == NPOS
           ) {
            m_Chromosomes.push_back(ReplIter->GetName());
        }
    }
    sort(m_Chromosomes.begin(), m_Chromosomes.end(), s_RevStrLenSort);

    m_Assembly->PostRead();
}

bool
CGencollIdMapper::x_NCBI34_Guess(const CSeq_id& Id, SIdSpec& Spec) const
{
    if (m_Assembly->GetTaxId() != 9606 || m_Assembly->GetName() != "NCBI34") {
        return false;
    }

    if (Id.GetSeqIdString(true) == "NC_000002" ||
        Id.GetSeqIdString(true) == "NC_000002.8"
       ) {
        Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
        Spec.Alias = SIdSpec::e_Public;
        Spec.External = "";
        Spec.Pattern = "";
        return true;
    }

    if (Id.GetSeqIdString(true) == "NC_000009" ||
        Id.GetSeqIdString(true) == "NC_000009.8"
       ) {
        Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
        Spec.Alias = SIdSpec::e_Public;
        Spec.External = "";
        Spec.Pattern = "";
        return true;
    }

    return false;
}


CConstRef<CSeq_id>
CGencollIdMapper::x_NCBI34_Map_IdFix(CConstRef<CSeq_id> SourceId) const
{
    if (m_Assembly->GetTaxId() != 9606 || m_Assembly->GetName() != "NCBI34") {
        return SourceId;
    }

    if (SourceId->GetSeqIdString(true) == "NC_000002" ||
        SourceId->GetSeqIdString(true) == "NC_000002.8"
       ) {
        CRef<CSeq_id> NewId(new CSeq_id());
        NewId->SetLocal().SetStr("2");
        return NewId;
    }


    if (SourceId->GetSeqIdString(true) == "NC_000009" ||
        SourceId->GetSeqIdString(true) == "NC_000009.8"
       ) {
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
                if (IdIter->IsGi())
                    continue;
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
            if ( StructIdIter->Equals(TopId) ) {
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
                if ( SubSeqIdIter->Equals(TopId) ) {
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
        CTypeConstIterator<CSeq_id> IdIter(Seq);
        bool IsRandom = false;
        for ( ; IdIter; ++IdIter) {
            string IdStr = IdIter->GetSeqIdString();
            if (IdStr.find("_random") == IdStr.length()-7) {
                IsRandom = true;
                break;
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
    bool SeqHasGi;
    CConstRef<CSeq_id> GenGi, RefGi;
    GenGi = Seq.GetSynonymSeq_id(CGC_TypedSeqId::e_Genbank, CGC_SeqIdAlias::e_Gi);
    RefGi = Seq.GetSynonymSeq_id(CGC_TypedSeqId::e_Refseq, CGC_SeqIdAlias::e_Gi);
    SeqHasGi = bool(GenGi) || bool(RefGi);

    bool SeqQualifies = false;
    bool ParentQualifies = false;
    if (x_HasTop(Seq, SIdSpec::e_Top_Public) &&
        SeqHasGi) {
        SeqQualifies = true;
    }

    CConstRef<CGC_Sequence> Parent;
    Parent = Seq.GetParent();

    CGC_TaggedSequences::TState Relation;

    if (!Parent.IsNull()) {
        GenGi = Parent->GetSynonymSeq_id(CGC_TypedSeqId::e_Genbank, CGC_SeqIdAlias::e_Gi);
        RefGi = Parent->GetSynonymSeq_id(CGC_TypedSeqId::e_Refseq, CGC_SeqIdAlias::e_Gi);
        bool ParentHasGi = bool(GenGi) || bool(RefGi);

        Relation = Seq.GetParentRelation();

        if (x_HasTop(*Parent, SIdSpec::e_Top_Public) &&
               Relation == CGC_TaggedSequences::eState_placed &&
            ParentHasGi)
            ParentQualifies = true;
    }

    if (SeqQualifies && !ParentQualifies) {
        ITERATE (CGC_Sequence::TRoles, RoleIter, Seq.GetRoles()) {
            if (*RoleIter == SIdSpec::e_Role_Gpipe_Top)
                return;
        }

        Seq.SetRoles().push_back(SIdSpec::e_Role_Gpipe_Top);
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
                switch ((*it)->Which()) {
                case CGC_TypedSeqId::e_Private: {
                    if ( (*it)->GetPrivate().GetSeqIdString() == ReplIter->GetName())
                        NameFound = true;
                } break;
                default:
                    break;
                }
            }
            if (true || !NameFound) {
                CRef<CGC_TypedSeqId> ChromoId(new CGC_TypedSeqId);
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

    if (Id->IsGi() && Id->GetGi() < 50) {
        CRef<CSeq_id> NewId(new CSeq_id());
        NewId->SetLocal().SetStr() = NStr::IntToString(Id->GetGi());
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
    if (Id->GetTextseq_Id() &&
        Id->GetTextseq_Id()->IsSetAccession() &&
        !Id->GetTextseq_Id()->IsSetVersion()) {
        const int Ver = m_AccToVerMap.find(Id->GetTextseq_Id()->GetAccession())->second;
        CRef<CSeq_id> NewId(new CSeq_id());
        NewId->Set(Id->Which(), Id->GetTextseq_Id()->GetAccession(), "", Ver);
        Id = NewId;
    }

    return Id;
}

CConstRef<CSeq_id>
CGencollIdMapper::x_ApplyPatternToId(CConstRef<CSeq_id> Id,
                                     const SIdSpec& Spec
                                    ) const
{
    if (Id->GetTextseq_Id() == NULL && !Id->IsGi() && !Spec.Pattern.empty() /* &&
        Id->GetLocal().GetStr().find(Spec.Pattern) != NPOS */) {
        CRef<CSeq_id> NewId(new CSeq_id());
        NewId->SetLocal().SetStr() = Id->GetSeqIdString();
        string Pre, Post;
        size_t DelimPos = Spec.Pattern.find(DELIM);
        Pre.assign(Spec.Pattern.data(), 0, DelimPos);
        //Post.assign(Spec.Pattern.data(), DelimPos+DELIM.length(),
        //            Spec.Pattern.length()-DelimPos-DELIM.length());
        if (!Pre.empty() || !Post.empty()) {
            NStr::ReplaceInPlace(NewId->SetLocal().SetStr(), Pre, "");
            //NStr::ReplaceInPlace(NewId->SetLocal().SetStr(), Post, "");
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
            if ((*RoleIter) >= eGC_SequenceRole_top_level) {
                continue;
            }
            SeqRole = min(SeqRole, *RoleIter);
        }
    }
    return SeqRole;
}

bool
CGencollIdMapper::x_HasTop(const objects::CGC_Sequence& Seq, int Top) const
{
    if (Top == SIdSpec::e_Top_NotSet) {
        return true;
    }
    bool NotTop = true;
    if (Seq.CanGetRoles()) {
        ITERATE (CGC_Sequence::TRoles, RoleIter, Seq.GetRoles()) {
            const bool role_is_gpipe_top = (*RoleIter == SIdSpec::e_Role_Gpipe_Top);
            const bool role_is_GC_top = (*RoleIter == eGC_SequenceRole_top_level);
            if (role_is_gpipe_top || role_is_GC_top) {
                NotTop = false;
                if ((Top == SIdSpec::e_Top_Gpipe && role_is_gpipe_top) ||
                    (Top == SIdSpec::e_Top_Public && role_is_GC_top)
                   ) {
                    return true;
                }
            }
        }
    }
    if (Top == SIdSpec::e_Top_NotTop && NotTop) {
        return true;
    }
    return false;
}

void
CGencollIdMapper::x_AddSeqToMap(const CSeq_id& Id,
                                CConstRef<CGC_Sequence> Seq
                               )
{
    int Role = x_GetRole(*Seq);
    if (Role == SIdSpec::e_Role_NotSet) {
        return;
    }

    CSeq_id_Handle Handle;
    Handle = CSeq_id_Handle::GetHandle(Id);

    TIdToSeqMap::iterator Found;
    Found = m_IdToSeqMap.find(Handle);
    if (Found != m_IdToSeqMap.end()) {
        int OldRole, NewRole;
        OldRole = x_GetRole(*Found->second);
        NewRole = x_GetRole(*Seq);
        if (NewRole == SIdSpec::e_Role_NotSet ||
          (OldRole != SIdSpec::e_Role_NotSet && OldRole >= NewRole) )
            return;
        //if(Seq->GetSeq_id_synonyms().size() <=
        //    Found->second->GetSeq_id_synonyms().size())
        //    return;
        m_IdToSeqMap.erase(Found);
    }
    TIdToSeqMap::value_type Pair(Handle, Seq);
    m_IdToSeqMap.insert(Pair);

    {{
        CConstRef<CGC_Sequence> ParentSeq;
        CGC_TaggedSequences_Base::TState ParentState;
        ParentSeq = Seq->GetParent();
        ParentState = Seq->GetParentRelation();

        if (ParentSeq &&
            ParentState == CGC_TaggedSequences_Base::eState_placed) {
            CSeq_id_Handle ParentIdH;
            ParentIdH = CSeq_id_Handle::GetHandle(ParentSeq->GetSeq_id());
            TChildToParentMap::value_type ParentPair(Handle, ParentSeq);
            m_ChildToParentMap.insert(ParentPair);
        }
    }}
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
            CConstRef<CGC_Sequence> SeqRef(&Seq);
            x_AddSeqToMap(Seq.GetSeq_id(), SeqRef);
        }
    }

#warning unsure why condition includes Seq.CanGetStructure()
    if (Seq.CanGetSeq_id_synonyms() &&
        (!Seq.GetSeq_id_synonyms().empty() || Seq.CanGetStructure())
       ) {
        CConstRef<CGC_Sequence> SeqRef(&Seq);
        ITERATE (CGC_Sequence::TSeq_id_synonyms, it, Seq.GetSeq_id_synonyms()) {
            const CGC_TypedSeqId::E_Choice syn_type = (*it)->Which();
            const bool is_gb = (syn_type == CGC_TypedSeqId::e_Genbank);
            if (is_gb || syn_type == CGC_TypedSeqId::e_Refseq) {
                CConstRef<CGC_SeqIdAlias> seq_id_alias(
                    is_gb ? &(*it)->GetGenbank()
                          : &(*it)->GetRefseq()
                );
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
            const CGC_Replicon& mol = **iter;
            if (mol.GetSequence().IsSingle()) {
                x_BuildSeqMap(mol.GetSequence().GetSingle());
            }
            else {
                ITERATE (CGC_Replicon::TSequence::TSet,
                         it,
                         mol.GetSequence().GetSet()
                        ) {
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
    if (Seq.CanGetSeq_id_synonyms()) {
        ITERATE (CGC_Sequence::TSeq_id_synonyms, it, Seq.GetSeq_id_synonyms()) {
            if ((*it)->Which() != Spec.TypedChoice)
                continue;
            switch ((*it)->Which()) {
            case CGC_TypedSeqId::e_Genbank: {
                const CGC_SeqIdAlias& Genbank = (*it)->GetGenbank();
                if (Genbank.CanGetPublic() && Spec.Alias == SIdSpec::e_Public) {
                    return CConstRef<CSeq_id>(&Genbank.GetPublic());
                }
                else if (Genbank.CanGetGpipe() && Spec.Alias == SIdSpec::e_Gpipe) {
                    return CConstRef<CSeq_id>(&Genbank.GetGpipe());
                }
                else if (Genbank.CanGetGi() && Spec.Alias == SIdSpec::e_Gi) {
                    return CConstRef<CSeq_id>(&Genbank.GetGi());
                }}
                break;
            case CGC_TypedSeqId::e_Refseq: {
                const CGC_SeqIdAlias& Refseq = (*it)->GetRefseq();
                if (Refseq.CanGetPublic() && Spec.Alias == SIdSpec::e_Public) {
                    return CConstRef<CSeq_id>(&Refseq.GetPublic());
                }
                else if (Refseq.CanGetGpipe() && Spec.Alias == SIdSpec::e_Gpipe) {
                    return CConstRef<CSeq_id>(&Refseq.GetGpipe());
                }
                else if (Refseq.CanGetGi() && Spec.Alias == SIdSpec::e_Gi) {
                    return CConstRef<CSeq_id>(&Refseq.GetGi());
                }}
                break;
            case CGC_TypedSeqId::e_External: {
                const CGC_External_Seqid& ExternalId = (*it)->GetExternal();
                if (ExternalId.GetExternal() == Spec.External) {
                    if (Spec.Pattern.empty()) {
                        return CConstRef<CSeq_id>(&ExternalId.GetId());
                    } else {
                        CRef<CSeq_id> NewId(new CSeq_id());
                        NewId->SetLocal().SetStr() = NStr::Replace(Spec.Pattern, DELIM, ExternalId.GetId().GetSeqIdString());
                        //NewId->SetLocal().SetStr() = Spec.Pattern + ExternalId.GetId().GetSeqIdString();
                        return NewId;
                    }
                }}
                break;
            case CGC_TypedSeqId::e_Private: {
                const CSeq_id& Private = (*it)->GetPrivate();
                if (Spec.Pattern.empty()) {
                    return CConstRef<CSeq_id>(&Private);
                } else {
                    CRef<CSeq_id> NewId(new CSeq_id());
                    NewId->SetLocal().SetStr() = NStr::Replace(Spec.Pattern, DELIM, Private.GetSeqIdString());
                    //NewId->SetLocal().SetStr() = Spec.Pattern + Private.GetSeqIdString();
                    return NewId;
                }}
                break;

            default:
                break;
            }
        }
    }

   /* // Exact Spec not found, Fall back!

    SIdSpec RefSeq_Spec, GenBank_Spec, Gpipe_Spec, Chrom_Spec;
    RefSeq_Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
    RefSeq_Spec.Alias = SIdSpec::e_Public;
    GenBank_Spec.TypedChoice = CGC_TypedSeqId::e_Genbank;
    GenBank_Spec.Alias = SIdSpec::e_Public;
    Gpipe_Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
    Gpipe_Spec.Alias = SIdSpec::e_Gpipe;
    Chrom_Spec.TypedChoice = CGC_TypedSeqId::e_Private;
    Chrom_Spec.Alias = SIdSpec::e_NotSet;
    Chrom_Spec.Pattern = DELIM;

    if (x_CanSeqMeetSpec(Seq, RefSeq_Spec))
        return x_GetIdFromSeqAndSpec(Seq, RefSeq_Spec);

    if (x_CanSeqMeetSpec(Seq, GenBank_Spec))
        return x_GetIdFromSeqAndSpec(Seq, GenBank_Spec);

    if (x_CanSeqMeetSpec(Seq, Gpipe_Spec))
        return x_GetIdFromSeqAndSpec(Seq, Gpipe_Spec);

    if (x_CanSeqMeetSpec(Seq, Chrom_Spec))
        return x_GetIdFromSeqAndSpec(Seq, Chrom_Spec);
*/

    return CConstRef<CSeq_id>();
}

int
CGencollIdMapper::x_CanSeqMeetSpec(const CGC_Sequence& Seq,
                                   const SIdSpec& Spec,
                                   int Level
                                  ) const
{
    if (Level == 3) {
        return e_No;
    }

    //int SeqRole = x_GetRole(Seq);
    bool HasId = false;
    if (Seq.CanGetSeq_id_synonyms()) {
        ITERATE (CGC_Sequence::TSeq_id_synonyms, it, Seq.GetSeq_id_synonyms()) {

            if ((*it)->Which() != Spec.TypedChoice) {
                continue;
            }

            const bool alias_is_public = (Spec.Alias == SIdSpec::e_Public);
            const bool alias_is_gpipe = (Spec.Alias == SIdSpec::e_Gpipe);
            const bool alias_is_gi = (Spec.Alias == SIdSpec::e_Gi);

            switch ((*it)->Which()) {
            case CGC_TypedSeqId::e_Genbank: {
                const CGC_SeqIdAlias& Genbank = (*it)->GetGenbank();
                if (Genbank.CanGetPublic() && alias_is_public) {
                    HasId = true;
                }
                else if (Genbank.CanGetGpipe() && alias_is_gpipe) {
                    HasId = true;
                }
                else if (Genbank.CanGetGi() && alias_is_gi) {
                    HasId = true;
                }}
                break;
            case CGC_TypedSeqId::e_Refseq:
            {
                const CGC_SeqIdAlias& Refseq = (*it)->GetRefseq();
                if (Refseq.CanGetPublic() && alias_is_public) {
                    HasId = true;
                }
                else if (Refseq.CanGetGpipe() && alias_is_gpipe) {
                    HasId = true;
                }
                else if (Refseq.CanGetGi() && alias_is_gi) {
                    HasId = true;
                }
                break;
            }
            case CGC_TypedSeqId::e_External:
            {
                const CGC_External_Seqid& ExternalId = (*it)->GetExternal();
                if (ExternalId.GetExternal() == Spec.External) {
                    HasId = true;
                }
                break;
            }
            case CGC_TypedSeqId::e_Private:
                HasId = true;
                break;
            default:
                break;
            }
        }
    }

    if (HasId) {
        if (Spec.Top != SIdSpec::e_Top_NotSet) {
            if (x_HasTop(Seq, Spec.Top)) {
                return e_Yes;
            }
        }
        else if (Spec.Role != SIdSpec::e_Role_NotSet) {
            if (Seq.HasRole(Spec.Role)) {
                return e_Yes;
            }
        }
    }

    CConstRef<CGC_Sequence> ParentSeq;
    ParentSeq = Seq.GetParent();
    if (ParentSeq) {
        if (Spec.Top != SIdSpec::e_Top_NotTop ||
          (Spec.Role != SIdSpec::e_Role_NotSet && Spec.Role <= x_GetRole(*ParentSeq)) ) {
            const int Parent = x_CanSeqMeetSpec(*ParentSeq, Spec, Level + 1);
            if (Parent == e_Yes) {
                return e_Up;
            }
        }
    }

    if (Seq.CanGetSequences() && Spec.Top != true) {
        ITERATE (CGC_Sequence::TSequences, TagIter, Seq.GetSequences()) {
            if ((*TagIter)->GetState() != CGC_TaggedSequences::eState_placed) {
                continue;
            }
            ITERATE (CGC_TaggedSequences::TSeqs, SeqIter, (*TagIter)->GetSeqs()) {
                const int Child = x_CanSeqMeetSpec(**SeqIter, Spec, Level + 1);
                if (Child == e_Yes) {
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
     Spec.TypedChoice = CGC_TypedSeqId::e_not_set;
    Spec.Alias = SIdSpec::e_NotSet;
    Spec.Role = SIdSpec::e_Role_NotSet;
    Spec.Top = SIdSpec::e_Top_NotSet;
    Spec.External.clear();
    Spec.Pattern.clear();

    if (Seq.CanGetRoles()) {
        Spec.Role = x_GetRole(Seq);
        if (x_HasTop(Seq, SIdSpec::e_Top_Public))
            Spec.Top = SIdSpec::e_Top_Public;
        else if (x_HasTop(Seq, SIdSpec::e_Top_Gpipe))
            Spec.Top = SIdSpec::e_Top_Gpipe;
    }


    // Loop over the IDs, find which matches the given ID
    if (Seq.CanGetSeq_id_synonyms()) {
        ITERATE (CGC_Sequence::TSeq_id_synonyms, it, Seq.GetSeq_id_synonyms()) {
            switch ((*it)->Which()) {
            case CGC_TypedSeqId::e_Genbank:
            {
                const CGC_SeqIdAlias& Genbank = (*it)->GetGenbank();
                if (Genbank.CanGetPublic() && Genbank.GetPublic().Equals(Id)) {
                    Spec.TypedChoice = CGC_TypedSeqId::e_Genbank;
                    Spec.Alias = SIdSpec::e_Public;
                    return true;
                }
                else if (Genbank.CanGetGpipe() && Genbank.GetGpipe().Equals(Id)) {
                    Spec.TypedChoice = CGC_TypedSeqId::e_Genbank;
                    Spec.Alias = SIdSpec::e_Gpipe;
                    return true;
                }
                else if (Genbank.CanGetGi() && Genbank.GetGi().Equals(Id)) {
                    Spec.TypedChoice = CGC_TypedSeqId::e_Genbank;
                    Spec.Alias = SIdSpec::e_Gi;
                    return true;
                }
                break;
            }
            case CGC_TypedSeqId::e_Refseq:
            {
                const CGC_SeqIdAlias& Refseq = (*it)->GetRefseq();
                if (Refseq.CanGetPublic() && Refseq.GetPublic().Equals(Id)) {
                    Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
                    Spec.Alias = SIdSpec::e_Public;
                    return true;
                }
                else if (Refseq.CanGetGpipe() && Refseq.GetGpipe().Equals(Id)) {
                    Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
                    Spec.Alias = SIdSpec::e_Gpipe;
                    return true;
                }
                else if (Refseq.CanGetGi() && Refseq.GetGi().Equals(Id)) {
                    Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
                    Spec.Alias = SIdSpec::e_Gi;
                    return true;
                }
                break;
            }
            case CGC_TypedSeqId::e_External:
            {
                const CGC_External_Seqid& ExternalId = (*it)->GetExternal();
                if (ExternalId.GetId().Equals(Id)) {
                    Spec.TypedChoice = CGC_TypedSeqId::e_External;
                    Spec.External = ExternalId.GetExternal();
                    return true;
                }
                break;
            }
            case CGC_TypedSeqId::e_Private:
            {
                const CSeq_id& Private = (*it)->GetPrivate();
                if (Private.Equals(Id)) {
                    Spec.TypedChoice = CGC_TypedSeqId::e_Private;
                    return true;
                }
                break;
            }
            default:
                break;
            }
        }
    }


    // If we didn't find it normally, try again, looking for Pattern matches
    if (Seq.CanGetSeq_id_synonyms()) {
        ITERATE (CGC_Sequence::TSeq_id_synonyms, it, Seq.GetSeq_id_synonyms()) {
            switch ((*it)->Which()) {
            case CGC_TypedSeqId::e_External:
            {
                const CGC_External_Seqid& ExternalId = (*it)->GetExternal();
                if (ExternalId.GetExternal() != CHROMO_EXT) {
                    continue;
                }
                const size_t Start(
                    NStr::Find(Id.GetSeqIdString(),
                               ExternalId.GetId().GetSeqIdString()
                              )
                );
                if (Start != NPOS) {
                    Spec.TypedChoice = CGC_TypedSeqId::e_External;
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
                    Spec.TypedChoice = CGC_TypedSeqId::e_Private;
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
                                       int Depth
                                      ) const
{
    CConstRef<CGC_Sequence> Result;

    {{
        TChildToParentMap::const_iterator Found;
        CSeq_id_Handle IdH, ParentIdH;
        IdH = CSeq_id_Handle::GetHandle(Id);
        Found = m_ChildToParentMap.find(IdH);
        if (Found != m_ChildToParentMap.end()) {
            Result = Found->second;
        }
        return Result;
    }}

    if (Depth > 5) {
    cerr << "x_FindParentSequence: Depth Bounce " << Id.AsFastaString() << endl;
        return Result;
    }

    if (Assembly.IsAssembly_set()) {
        const CGC_AssemblySet& AssemblySet = Assembly.GetAssembly_set();
        if (AssemblySet.CanGetPrimary_assembly()) {
            Result = x_FindParentSequence(Id, AssemblySet.GetPrimary_assembly(), Depth+1);
        }
        if (!Result.IsNull())
            return Result;
        if (AssemblySet.CanGetMore_assemblies()) {
            ITERATE (CGC_AssemblySet::TMore_assemblies, AssemIter, AssemblySet.GetMore_assemblies()) {
                Result = x_FindParentSequence(Id, **AssemIter, Depth+1);
                if (!Result.IsNull())
                    return Result;
            }
        }
    } else if (Assembly.IsUnit()) {
        const CGC_AssemblyUnit& AsmUnit = Assembly.GetUnit();
        if (AsmUnit.CanGetMols()) {
            ITERATE (CGC_AssemblyUnit::TMols, MolIter, AsmUnit.GetMols()) {
                if ((*MolIter)->GetSequence().IsSingle()) {
                    const CGC_Sequence& Parent = (*MolIter)->GetSequence().GetSingle();

                    bool IsParent = x_IsParentSequence(Id, Parent);
                    if (IsParent) {
                        Result.Reset(&Parent);
                        return Result;
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
    if (!Parent.CanGetSequences())
        return false;


    ITERATE (CGC_Sequence::TSequences, ChildIter, Parent.GetSequences()) {
        if ( (*ChildIter)->GetState() != CGC_TaggedSequences::eState_placed ||
            !(*ChildIter)->CanGetSeqs())
            continue;

        ITERATE (CGC_TaggedSequences::TSeqs, SeqIter, (*ChildIter)->GetSeqs()) {
            if ((*SeqIter)->GetSeq_id().Equals(Id)) {
                return true;
            }
            else {
                ITERATE (CGC_Sequence::TSeq_id_synonyms, SynIter, (*SeqIter)->GetSeq_id_synonyms()) {
                    const CGC_TypedSeqId& TypedId = **SynIter;
                    CTypeConstIterator<CSeq_id> IdIter(TypedId);
                    while(IdIter) {
                        if (IdIter->Equals(Id))
                            return true;
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
    }

    return false;
}

CConstRef<CGC_Sequence>
CGencollIdMapper::x_FindChromosomeSequence(const CSeq_id& Id, const SIdSpec& Spec) const
{
    if (Id.IsGi() && Id.GetGi() > 50) {
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
            while(SynIdIter) {
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

    // Direction
    CSeq_loc_Mapper::ESeqMapDirection Direction = CSeq_loc_Mapper::eSeqMap_Down;
    Direction = CSeq_loc_Mapper::eSeqMap_Up;

    MapId->Assign(*Id);

    CConstRef<CSeq_id> SpecId = x_GetIdFromSeqAndSpec(Seq, Spec);
    if (SpecId) {
        MapId->Assign(*SpecId);
    }

    CRef<CSeq_loc_Mapper> Mapper;
    Mapper.Reset(new CSeq_loc_Mapper(1, *SeqMap, Direction, MapId));

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

    // Direction
    CSeq_loc_Mapper::ESeqMapDirection Direction = CSeq_loc_Mapper::eSeqMap_Down;

    // Parent seq id for the Mapper
    MapId->Assign(*SourceLoc.GetId());

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
CGencollIdMapper::x_Merge_E_Gaps(E_Gap First, E_Gap Second) const
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

END_NCBI_SCOPE

