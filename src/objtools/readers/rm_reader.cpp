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
 * Author:  Frank Ludwig, Wratko Hlavina
 *
 * File Description:
 *   Repeat Masker file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>

#include <util/line_reader.hpp>
#include <util/value_convert.hpp>
#include <util/static_map.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/rm_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

CSeq_id_Handle CFastaIdsResolver::ResolveSeqId(const string& id) const
{
    CSeq_id_Handle result;
    CBioseq::TId ids;
    CSeq_id::ParseFastaIds(ids, id);
    CBioseq::TId::value_type best(FindBestChoice(ids, CSeq_id::Score));
    if (best) result = CSeq_id_Handle::GetHandle(*best);
    return result;
}

string IRawRepeatRegion::GetRptClassFamily() const
{
    string family(GetRptFamily());
    if (family.empty()) {
        return GetRptClass();
    } else {
        return GetRptClass() + '/' + family;
    }
}

/*
IRepeatRegion::TTaxId IRepeatRegion::GetRptSpecificity() const
{
    return 0;
}

string IRepeatRegion::GetRptSpecificityName() const
{
    return kEmptyStr;
}
*/

void IRepeatRegion::GetLocation(CSeq_loc& result) const
{
    CConstRef<CSeq_loc> location(GetLocation());
    if (location) {
        result.Assign(*location);
    } else {
        result.Reset();
    }
}

string IRepeatRegion::GetSeqIdString() const
{
    return GetLocation()->GetId()->AsFastaString();
}

TSeqPos IRepeatRegion::GetSeqPosBegin() const
{
    return GetLocation()->GetStart(eExtreme_Positional) + 1;
}

TSeqPos IRepeatRegion::GetSeqPosEnd() const
{
    return GetLocation()->GetStop(eExtreme_Positional) + 1;
}

bool IRepeatRegion::IsReverseStrand() const
{
    return GetLocation()->IsReverseStrand();
}

CConstRef<CSeq_loc> SRepeatRegion::GetLocation() const
{
    return query_location;
}

CConstRef<CFeat_id> SRepeatRegion::GetId(void) const
{
    CRef<CFeat_id> result(new CFeat_id);
    result->SetLocal().SetId(GetRptId());
    return result;
}

/// Overridden version returns the orginal unparsed
/// sequence identifier, if it was set (non-empty).
///
string SRepeatRegion::GetSeqIdString() const
{
    if (! query_sequence.empty()) return query_sequence;
    return TParent::GetSeqIdString();
}

// Implement IRepeatRegion interface. If it weren't for virtual
// methods, all of the following could be inlined.

string SRepeatRegion::GetRptName() const
{
    return matching_repeat;
}

string SRepeatRegion::GetRptFamily() const
{
    return rpt_family;
}

string SRepeatRegion::GetRptClass() const
{
    return rpt_class;
}

SRepeatRegion::TTaxId SRepeatRegion::GetRptSpecificity() const {
    return 0;
}

TSeqPos SRepeatRegion::GetRptLength() const {
    if (GetRptPosEnd() == kInvalidSeqPos  ||
        GetRptLeft() == kInvalidSeqPos) return kInvalidSeqPos;
    return GetRptPosEnd() + GetRptLeft();
}

string SRepeatRegion::GetRptSpecificityName() const {
    return kEmptyStr;
}

string SRepeatRegion::GetRptRepbaseId() const {
    return kEmptyStr;
}

SRepeatRegion::TRptId SRepeatRegion::GetRptId() const
{
    return rpt_id;
}

SRepeatRegion::TScore SRepeatRegion::GetSwScore() const
{
    return sw_score;
}

SRepeatRegion::TPercent SRepeatRegion::GetPercDiv() const
{
    return perc_div;
}

SRepeatRegion::TPercent SRepeatRegion::GetPercDel() const
{
    return perc_del;
}

SRepeatRegion::TPercent SRepeatRegion::GetPercIns() const
{
    return perc_ins;
}

TSeqPos SRepeatRegion::GetRptPosBegin() const
{
    return rpt_pos_begin;
}

TSeqPos SRepeatRegion::GetRptPosEnd() const
{
    return rpt_pos_end;
}

TSeqPos SRepeatRegion::GetRptLeft() const
{
    return rpt_left;
}

TSeqPos SRepeatRegion::GetSeqLeft() const
{
    return query_left;
}

bool SRepeatRegion::IsOverlapped() const
{
    return overlapped;
}

bool CRepeatLibrary::Get(const string& name, TRepeat& dest) const
{
    TMap::const_iterator it(m_Map.find(name));
    if (it == m_Map.end()) return false;
    dest = it->second;
    return true;
}

void CRepeatLibrary::Read(CNcbiIstream& stream)
{
    TRepeat repeat;
    string line;
    vector<string> tokens;

    while (! stream.eof()) {
        NcbiGetlineEOL(stream, line);
        if (NStr::StartsWith(line, "//")) {
            // Perl equivalent from rpt_lib2repeat_q.pl:
            //     # Repeats with undefined specificity can be skipped because RepeatMasker
            //     # only uses repeats that have a "Species" set in searches. The
            //     # specificity will be undefined when the "Species:" field is empty.
            //     if ( ($class eq "Simple_repeat" || $class eq "Low_complexity")
            //         && $specificity eq "universal" && $family eq "") {
            //         $length = "";   # length of database sequence is arbitrary
            //     }
            if ((repeat.m_RptClass == "Simple_repeat"  ||
                 repeat.m_RptClass == "Low_complexity")  &&
                 repeat.m_RptSpecificityName == "universal"  &&
                 repeat.m_RptFamily == "") repeat.m_RptLength = kInvalidSeqPos;
            m_Map[repeat.m_RptName] = repeat;
            continue;
        }

        // As per EMBL Release 3.4:
        //
        // Each line begins with a two-character line type code.
        // This code is always followed by three blanks, so that the
        // actual information in each line begins in character position 6.
        if (line.length() < 6  ||  line.substr(2, 3) != "   ") continue;
        string code(line.substr(0, 2));
        string value(line.substr(5));
        NStr::TruncateSpacesInPlace(value);

        if (code == "ID") {
            // NOTE: Violates specs as per EMBL Release 3.4.1.
            //       There should be 7 fields.
            //
            // Perl equivalent from rpt_lib2repeat_q.pl:
            //     if (m/^ID\s/) {
            //         die "Multiple ID lines found in one record.\nLine $.: $_\n" if defined $name or defined $length;
            //         ($name, $length) = m/^ID\s+(\S+).*\s([1-9][0-9]*) BP\.$/;
            //         die "Failed to extract a repeat name and length from line:\nLine $.: $_\n"
            //         unless defined $name and $name and defined $length and $length;
            //     }
            repeat.m_RptName = value.substr(0, value.find(' '));
            string bp(value.substr(value.rfind(';') + 1));
            NStr::TruncateSpacesInPlace(bp);
            repeat.m_RptLength = Convert(bp.substr(0, bp.find(' ')));
        } else if (code == "DE") {
            // DE   RepbaseID: ACROBAT1
            if (NStr::StartsWith(value, "RepbaseID:")) {
                repeat.m_RptRepbaseId = NStr::TruncateSpaces(value.substr(10));
            }
        } else if (code == "CC") {
            if (NStr::MatchesMask(value, "RELEASE *;*")) {
                m_Release = value.substr(8, value.find(';') - 8);
            } else if (NStr::StartsWith(value, "Type:")) {
                // Perl equivalent from rpt_lib2repeat_q.pl:
                //     if (m/^CC   +Type:\s*((.*\S)*)\s*$/) {
                //         die "Multiple Type lines found in one record.\nLine $.: $_\n" if defined $class;
                //         $class = $1;
                //         die "Failed to extract a repeat class from line:\nLine $.: $_\n" unless defined $class and $class;
                //     }
                repeat.m_RptClass = NStr::TruncateSpaces(value.substr(5));
            } else if (NStr::StartsWith(value, "SubType:")) {
                // Perl equivalent from rpt_lib2repeat_q.pl:
                //     if (m/^CC   +SubType:\s*((.*\S)*)\s*$/) {
                //         die "Multiple SubType lines found in one record.\nLine $.: $_\n" if defined $family;
                //         $family = $1 || "";    # NULL indicates unknown
                //     }
                repeat.m_RptFamily = NStr::TruncateSpaces(value.substr(8));
            } else if (NStr::StartsWith(value, "Species:")) {
                // Perl equivalent from rpt_lib2repeat_q.pl:
                //     if (m/^CC   +Species:\s*((.*\S)*)\s*$/) {
                //         die "Multiple Species lines found in one record.\nLine $.: $_\n" if defined $specificity;
                //         $specificity = $1;
                //         $specificity =~ s/_/ /g;
                //         $specificity = "universal" if $specificity eq "root";
                //     }
                repeat.m_RptSpecificityName = NStr::TruncateSpaces(value.substr(8));
                if (m_Taxonomy  &&  repeat.m_RptSpecificityName.size()) {
                    pair<TSpecificity2Taxid::iterator, bool> i_specificity =
                            m_Specificity2TaxId.insert( TSpecificity2Taxid::value_type(repeat.m_RptSpecificityName, 0) );
                    if (i_specificity.second) {
                        i_specificity.first->second = m_Taxonomy->GetTaxId(repeat.m_RptSpecificityName);
                        if (! i_specificity.first->second) {
                            LOG_POST(Warning
                                     << "RepeatMasker library species failed lookup to taxonomy: "
                                     << repeat.m_RptSpecificityName);
                        }
                    }
                    repeat.m_RptSpecificity = i_specificity.first->second;
                }
            }
        }
    }

    // Don't need specificity to taxonomy lookups anymore.
    m_Specificity2TaxId.clear();
 }

bool CRepeatLibrary::TestSpecificityMatchesName(TRepeat::TTaxId taxid,
                                                const string& name) const
{
    return m_Taxonomy  &&  m_Taxonomy->GetName(taxid) == name;
}

template <typename T>
static void s_SetQual(CSeq_feat::TQual& qual_list,
                      const string& qual, const T val)
{
    CRef<CGb_qual> result(new CGb_qual);
    result->SetQual(qual);
    string s = Convert(val).operator string();
    result->SetVal(s);
    qual_list.push_back(result);
}

/// Translate RepeatMasker output to INSDC standard
/// nomenclature for repeats. This includes remapping repeat
/// family to satellite and mobile element qualifiers, as
/// appropriate.
///
/// Available INSDC qualifiers are:
///     rpt_family, rpt_type, rpt_unit_seq, satellite, standard_name
///
static bool s_StandardizeNomenclature(const IRepeatRegion& repeat,
                                      CSeq_feat::TQual& qual_list)
{
    string val;

    string klass = repeat.GetRptClass();
    string family = repeat.GetRptFamily();

    if (NStr::EqualNocase(klass, "Satellite")) {
        val = "satellite:";
        if (! family.empty()) val += family;
        val += ' ';
        val += repeat.GetRptName();
        s_SetQual(qual_list, "satellite", val);
        if (! family.empty()) s_SetQual(qual_list, "rpt_family", family);
        return true;
    }

    if (NStr::EqualNocase(klass, "Simple_repeat")) {
        // Simple_repeat is the family in ASN.1, not the class, based on
        // evidence of prior submissions to GenBank. For example:
        // GI:45269107, although this is weak evidence (stuffing
        // RepeatMasker into Genbank qualifiers without much
        // effort at standardization).
        //
        // Do not expect Simple_repeat/xxx.
        s_SetQual(qual_list, "rpt_family", klass);
        s_SetQual(qual_list, "rpt_unit", repeat.GetRptName());
        return true;
    }

    if (NStr::EqualNocase(klass, "SINE")  ||
        NStr::EqualNocase(klass, "LINE")  ||
        NStr::EqualNocase(klass, "LTR")) {
        // Other valid INSDC mobile elements:
        // "transposon", "retrotransposon", "integron",
        // "insertion sequence", "non-LTR retrotransposon",
        // "MITE", "other"
        val = klass;
        val += ':';
        val += repeat.GetRptName();
        s_SetQual(qual_list, "mobile_element", val);
        if (! family.empty()) s_SetQual(qual_list, "rpt_family", family);
        return true;
    }

    return false;
}

CRepeatToFeat::CRepeatToFeat(TFlags flags,
                             CConstRef<TRepeatLibrary> lib ,
                             TIdGenerator& ids)
        : m_Flags(flags)
        , m_Library(lib)
        , m_Ids(&ids)
{
}

void CRepeatToFeat::ResetRepeatLibrary()
{
    m_Library.Reset();
}

void CRepeatToFeat::SetRepeatLibrary(const TRepeatLibrary& lib)
{
    m_Library.Reset(&lib);
}

void CRepeatToFeat::ResetIdGenerator()
{
    m_Ids.Reset(new COrdinalFeatIdGenerator);
}

void CRepeatToFeat::SetIdGenerator(TIdGenerator& generator)
{
    m_Ids.Reset(&generator);
}

void CRepeatToFeat::AssertReferencesResolved()
{
    // We can forget old IDs once references have been resolved.
    m_IdMap.clear();
}

CRef<CSeq_feat> CRepeatToFeat::operator()(const IRepeatRegion& repeat)
{
    CRef<CSeq_feat> feat(new CSeq_feat);

    //  data:
    CSeqFeatData& sfdata = feat->SetData();
    CImp_feat_Base& imp = sfdata.SetImp();
    imp.SetKey("repeat_region");

    CRef<CFeat_id> id(m_Ids->GenerateId());
    feat->SetId(*id);
    TIdMap::iterator id_it(m_IdMap.find(repeat.GetRptId()));
    if (id_it == m_IdMap.end()) {
        m_IdMap[repeat.GetRptId()] = id;
    } else {
        CRef<CSeqFeatXref> ref(new CSeqFeatXref);
        ref->SetId().Assign(*id_it->second);
        feat->SetXref().push_back(ref);
    }

    //  location:
    repeat.GetLocation(feat->SetLocation());

    //  qualifiers & ext's.
    if (m_Flags) {
        // Record if attributes were modified to conform with INSDC standards.
        bool standardized(false);

        CRepeatLibrary::TRepeat extra;
        if (m_Library) m_Library->Get(repeat.GetRptName(), extra);

        CSeq_feat::TQual& qual_list = feat->SetQual();

        if (m_Flags & fIncludeRepeatFamily) {
            if (m_Flags & fStandardizeNomenclature) {
                standardized = s_StandardizeNomenclature(repeat, qual_list);
            }

            if (! standardized) {
                // Did not succeed in standardizing nomenclature
                // from RepeatMasker to INSDC standards. Fall back to
                // storing the class/family verbatim.
                s_SetQual(qual_list, "rpt_family", repeat.GetRptClassFamily());
            }
        }

        if (m_Flags & fIncludeRepeatName  &&  ! standardized) {
            s_SetQual(qual_list, "standard_name", repeat.GetRptName());
        }

        if (m_Flags & fIncludeRepeatPos) {
            s_SetQual(qual_list, "rpt_unit_range",
                      NStr::IntToString(repeat.GetRptPosBegin()) +
                      ".." + NStr::IntToString(repeat.GetRptPosEnd()));
        }

        // Get specificity and check it for redundancy (taxid vs name).
        bool include_specificity_name(false);
        if (m_Flags & fIncludeRepeatSpecificity) {
            const IRepeat::TTaxId specificity(extra.GetRptSpecificity());
            const string specificity_name(extra.GetRptSpecificityName());
            include_specificity_name = ! specificity_name.empty();
            if (specificity) {
                CRef<CDbtag> tag(new CDbtag);
                // eDbtagType_taxon except the enum is almost useless,
                // being available to only one function in the Dbtag API.
                tag->SetDb("taxon");
                tag->SetTag().SetId(specificity);
                feat->SetDbxref().push_back(tag);
                if (fRemoveRedundancy  &&  m_Library  &&
                    m_Library->TestSpecificityMatchesName(
                            specificity,
                            specificity_name)) {
                    // Name matches taxonomy exactly, so don't store both.
                    include_specificity_name=false;
                }
            }
        }

        // Get repeat length and check it for redundancy with rpt_left.
        TSeqPos rpt_length(extra.GetRptLength());
        if (rpt_length == kInvalidSeqPos) {
            rpt_length = repeat.GetRptPosEnd() +
                         repeat.GetRptLeft();
        }
        bool include_rpt_left(m_Flags & fIncludeCoreStatistics);
        if ((m_Flags & fRemoveRedundancy)  &&
                (m_Flags & fIncludeRepeatLength)  &&
                (rpt_length == repeat.GetRptPosEnd() +
                               repeat.GetRptLeft())) {
            // Do not store rpt_left if we know the repeat length,
            // rpt_left matches it (so it's redundant), and we
            // want to remove redundancy.
            include_rpt_left = false;
        }

        // Store anything beyond what is possible in INDSC-approved
        // qualifiers using either non-standard qualifiers or user objects.
        // There are two options.

        if (m_Flags & fAllowNonstandardQualifiers) {
            // Option 1: Use Genbank qualifiers beyond the INDSC-approved set.

            if (m_Flags & fIncludeCoreStatistics) {
                s_SetQual(qual_list, "sw_score", repeat.GetSwScore());
                s_SetQual(qual_list, "perc_div", repeat.GetPercDiv());
                s_SetQual(qual_list, "perc_del", repeat.GetPercDel());
                s_SetQual(qual_list, "perc_ins", repeat.GetPercIns());
                if (include_rpt_left) {
                    s_SetQual(qual_list, "rpt_left", repeat.GetRptLeft());
                }
            }

            if (m_Flags & fIncludeExtraStatistics ) {
                if (! (m_Flags & fRemoveRedundancy)) {
                    // Query length is always redundant, since sequences
                    // have a bioseq length, and we know the location.
                    s_SetQual(qual_list, "query_length",
                            repeat.GetSeqPosEnd() + repeat.GetSeqLeft());
                }
                if (repeat.IsOverlapped()) {
                    s_SetQual(qual_list, "overlapped", true);
                }
            }

            if (m_Flags & fIncludeRepeatId) {
                s_SetQual(qual_list, "rpt_id", repeat.GetRptId());
            }

            if (m_Flags & fIncludeRepeatLength) {
                s_SetQual(qual_list, "rpt_length", rpt_length);
            }

            if (include_specificity_name) {
                s_SetQual(qual_list, "specificity",
                          extra.GetRptSpecificityName());
            }

        } else {
            // Option 2: Use user objects.

            CRef<CUser_object> uo(new CUser_object);
            feat->SetExts().push_back(uo);
            uo->SetType().SetStr("RepeatMasker");

            if (m_Flags & fIncludeCoreStatistics) {
                uo->AddField("sw_score", static_cast<double>(repeat.GetSwScore()));
                uo->AddField("perc_div", repeat.GetPercDiv());
                uo->AddField("perc_del", repeat.GetPercDel());
                uo->AddField("perc_ins", repeat.GetPercIns());
                if (include_rpt_left) {
                    uo->AddField("rpt_left", static_cast<int>(repeat.GetRptLeft()));
                }
            }

            if (m_Flags & fIncludeExtraStatistics) {
                if (! (m_Flags & fRemoveRedundancy)) {
                    // Query length is always redundant, since sequences
                    // have a bioseq length, and we know the location.
                    uo->AddField("query_length", static_cast<int>(
                            repeat.GetSeqPosEnd() + repeat.GetSeqLeft()));
                }
                if (repeat.IsOverlapped()) {
                    uo->AddField("overlapped", true);
                }
            }

            if (m_Flags & fIncludeRepeatId) {
                uo->AddField("rpt_id", static_cast<int>(repeat.GetRptId()));
            }

            if (m_Flags & fIncludeRepeatLength) {
                uo->AddField("rpt_length", static_cast<int>(rpt_length));
            }

            if (include_specificity_name) {
                uo->AddField("specificity", extra.GetRptSpecificityName());
            }

            // Clear out storage of empty user objects.
            if (! uo->IsSetData()) feat->ResetExts();
        }

        // Clear out storage if empty Genbank qualifier lists.
        if (qual_list.empty()) feat->ResetQual();

        if (m_Flags & fIncludeRepeatRepbaseId  &&
                ! extra.GetRptRepbaseId().empty()) {
            CRef<CDbtag> tag(new CDbtag);
            tag->SetDb("REPBASE");
            tag->SetTag().SetStr(extra.GetRptRepbaseId());
            feat->SetDbxref().push_back(tag);
        }

        if (m_Flags & fSetComment) {
            // Redundantly, store comments with original information.
            // The comment tries to stay close to RepeatMasker native
            // nomenclature. For example, query_left is reported,
            // rather than the normalized query_length as stored
            // in user objects or Genbank qualifiers. To accommodate
            // the possibility the annotation is remapped, the original
            // query identifier is preserved.

            CNcbiOstrstream comment;
            const char eq('='), sep(' ');

            comment << "source=RepeatMasker";
            if (m_Flags & fIncludeRepeatName) {
                comment << sep
                        << "rpt_name" << eq << repeat.GetRptName();
            }
            if (m_Flags & fIncludeCoreStatistics) {
                comment << sep
                        << "sw_score" << eq << repeat.GetSwScore() << sep
                        << "perc_div" << eq << repeat.GetPercDiv() << sep
                        << "perc_del" << eq << repeat.GetPercDel() << sep
                        << "perc_ins" << eq << repeat.GetPercIns() << sep
                        << "rpt_left" << eq << repeat.GetRptLeft();
            }
            if (m_Flags & fIncludeExtraStatistics) {
                comment << sep
                        << "query" << eq << repeat.GetSeqIdString() << sep
                        << "query_range" << eq;
                bool reverse(repeat.IsReverseStrand());
                if (reverse) comment << "complement(";
                comment << repeat.GetSeqPosBegin()
                        << ".." << repeat.GetSeqPosEnd();
                if (reverse) comment << ")";
                comment << sep
                        << "query_left" << eq << repeat.GetSeqLeft();
            }
            if (m_Flags & fIncludeRepeatId) {
                 comment << sep
                         << "ID" << eq << repeat.GetRptId();
            }
            if (m_Flags & fIncludeExtraStatistics  &&  repeat.IsOverlapped()) {
                comment << " *";
            }
            if (! extra.GetRptSpecificityName().empty()) {
                comment << sep
                        << "specificity" << eq << extra.GetRptSpecificityName();
            }
            if (extra.GetRptLength() != kInvalidSeqPos) {
                comment << sep
                        << "rpt_length" << eq << extra.GetRptLength();
            }
            feat->SetComment(CNcbiOstrstreamToString(comment));
        }
    }

    return feat;
}

CRepeatMaskerReader::CRepeatMaskerReader(TFlags flags,
                                         CConstRef<TRepeatLibrary> lib,
                                         const ISeqIdResolver& seqid_resolver,
                                         TIdGenerator& ids)
            : m_SeqIdResolver(&seqid_resolver)
            , m_ToFeat(flags, lib, ids)
{
}

CRepeatMaskerReader::~CRepeatMaskerReader(void)
{
}

void CRepeatMaskerReader::ResetSeqIdResolver()
{
    m_SeqIdResolver.Reset(new CFastaIdsResolver);
}

void CRepeatMaskerReader::SetSeqIdResolver(ISeqIdResolver& seqid_resolver)
{
    m_SeqIdResolver.Reset(&seqid_resolver);
}

CRepeatMaskerReader::TConverter& CRepeatMaskerReader::SetConverter()
{
    return m_ToFeat;
}

CRef<CSerialObject>
CRepeatMaskerReader::ReadObject(ILineReader& lr, ILineErrorListener* pMessageListener)
{
    CRef<CSerialObject> object(
            ReadSeqAnnot(lr, pMessageListener).ReleaseOrNull());
    return object;
}

CRef<CSeq_annot>
CRepeatMaskerReader::ReadSeqAnnot(ILineReader& lr, ILineErrorListener* pMessageListener)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    // CRef<CAnnot_descr> desc(new CAnnot_descr);
    // annot->SetDesc(*desc);
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();

    string line;
    size_t record_counter = 0;

    while ( ! lr.AtEOF() ) {
        line = *++lr;

        if ( IsHeaderLine( line ) || IsIgnoredLine( line ) ) {
            continue;
        }
        ++record_counter;

        SRepeatRegion mask_data;
        if ( ! ParseRecord( line, mask_data ) ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                lr.GetLineNumber(),
                "RepeatMasker Reader: Parse error in record = " + line) );
            ProcessError(*pErr, pMessageListener);
            continue;
        }

        if ( ! VerifyData( mask_data ) ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                lr.GetLineNumber(),
                "RepeatMasker Reader: Verification error in record = " + line) );
            ProcessError(*pErr, pMessageListener);
            continue;
        }

        CRef<CSeq_feat> feat(m_ToFeat(mask_data));
        if ( ! feat ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                lr.GetLineNumber(),
                "RepeatMasker Reader: Aborting file import, "
                "unable to create feature table for record = " + line) );
            ProcessError(*pErr, pMessageListener);
            // we don't tolerate even a few errors here!
            break;
        }

        ftable.push_back(feat);
    }
    // if (! record_counter) annot.Reset();
    xAddConversionInfo(annot, pMessageListener);
    return annot;
}


bool CRepeatMaskerReader::IsHeaderLine(const string& line)
{
    string labels_1st_line[] = { "SW", "perc", "query", "position", "matching", "" };
    string labels_2nd_line[] = { "score", "div.", "del.", "ins.", "sequence", "" };

    // try to identify 1st line of column labels:
    size_t current_offset = 0;
    size_t i = 0;
    for ( ; labels_1st_line[i] != ""; ++i ) {
        current_offset = NStr::FindCase( line, labels_1st_line[i], current_offset );
        if ( NPOS == current_offset ) {
            break;
        }
    }
    if ( labels_1st_line[i] == "" ) {
        return true;
    }

    // try to identify 2nd line of column labels:
    current_offset = 0;
    i = 0;
    for ( ; labels_2nd_line[i] != ""; ++i ) {
        current_offset = NStr::FindCase( line, labels_2nd_line[i], current_offset );
        if ( NPOS == current_offset ) {
            return false;
        }
    }
    return true;
}


bool CRepeatMaskerReader::IsIgnoredLine(const string& line)
{
    if ( NStr::StartsWith(line, "There were no repetitive sequences detected in "))
        return true;
    if ( NStr::FindCase(line, "only contains ambiguous bases") != NPOS)
        return true;
    return ( NStr::TruncateSpaces( line ).length() == 0  );
}


static void StripParens(string& s)
{
    SIZE_TYPE b = 0;
    SIZE_TYPE e = s.size();
    if (e > 0 && s[b] == '(') {
        ++b;
        if (s[e - 1] == ')') --e;
        if (e == b)
            s = kEmptyStr;
        else
            s = s.substr(b, e - b);
    }
}

bool CRepeatMaskerReader::ParseRecord(const string& record, SRepeatRegion& mask_data)
{
    const size_t MIN_VALUE_COUNT = 15;

    string line = NStr::TruncateSpaces( record );
    list< string > values;
    if ( NStr::Split( line, " \t", values ).size() < MIN_VALUE_COUNT ) {
        return false;
    }

    try {
        // 1: "SW score"
        list<string>::iterator it = values.begin();
        mask_data.sw_score = NStr::StringToUInt( *it );

        // 2: "perc div."
        ++it;
        mask_data.perc_div = NStr::StringToDouble( *it );

        // 3: "perc del."
        ++it;
        mask_data.perc_del = NStr::StringToDouble( *it );

        // 4: "perc ins."
        ++it;
        mask_data.perc_ins = NStr::StringToDouble( *it );

        // 5: "query sequence"
        ++it;
        mask_data.query_sequence = *it;
        CSeq_id_Handle idh(m_SeqIdResolver->ResolveSeqId(mask_data.query_sequence));
        CConstRef<CSeq_id> id(idh.GetSeqIdOrNull());
        if (! id) return false;
        mask_data.query_location.Reset(new CSeq_loc);
        CSeq_interval& location(mask_data.query_location->SetInt());
        location.SetId().Assign(*id);

        // 6: "position begin"
        ++it;
        TSeqPos pos_begin = NStr::StringToUInt(*it);
        if (pos_begin == 0) return false;
        location.SetFrom(pos_begin - 1);

        // 7: "in end"
        ++it;
        TSeqPos pos_end = NStr::StringToUInt(*it);
        if (pos_end == 0  ||  pos_end < pos_begin) return false;
        location.SetTo(pos_end - 1);

        // 8: "query (left)"
        ++it;
        StripParens(*it);
        mask_data.query_left = NStr::StringToUInt( *it );

        // 9: "" (meaning "strand")
        ++it;
        // Having the strand, we now have all fields to populate the location.
        location.SetStrand(*it == "C" ? eNa_strand_minus : eNa_strand_plus);

        // 10: "matching repeat"
        ++it;
        mask_data.matching_repeat = *it;

        // 11: "repeat class/family"
        ++it;
        string class_family = *it;
        NStr::SplitInTwo(class_family, "/",
                         mask_data.rpt_class, mask_data.rpt_family);

        // 12: "position in"
        ++it;
        string field12 = *it;

        // 13: "in end"
        ++it;
        mask_data.rpt_pos_end = NStr::StringToUInt( *it );

        // 14: "repeat left"
        ++it;
        string field14 = *it;

        // fields position 12 and 14 flip depending on the strand value.
        string rpt_left;
        if (mask_data.IsReverseStrand()) {
            mask_data.rpt_pos_begin = NStr::StringToUInt( field14 );
            rpt_left = field12;
        } else {
            mask_data.rpt_pos_begin = NStr::StringToUInt( field12 );
            rpt_left = field14;
        }

        StripParens(rpt_left);
        mask_data.rpt_left = NStr::StringToUInt(rpt_left);

        // 15: "ID"
        ++it;
        mask_data.rpt_id = NStr::StringToUInt(*it);

        // 16: overlapped (higher score repeat overlaps)
        ++it;
        mask_data.overlapped = (it != values.end()  &&  (*it) == "*");
    }
    catch( ... ) {
        return false;
    }

    return true;
}

bool CRepeatMaskerReader::VerifyData(const SRepeatRegion& mask_data)
{
    //
    //  This would be the place for any higher level checks of the mask data
    //  collected from the record ...
    //
    return true;
}


CRmReader::CRmReader(CNcbiIstream& istr) : m_Istr(istr)
{
}

CRmReader* CRmReader::OpenReader(CNcbiIstream& istr)
{
    //
    //  This is the point to make sure we are dealing with the right file type and
    //  to allocate the specialist reader for any subtype (OUT, HTML) we encouter.
    //  When this function returns the file pointer should be past the file header
    //  and at the beginning of the actual mask data.
    //
    //  Note:
    //  If something goes wrong during header processing then the file pointer will
    //  still be modified. It's the caller's job to restore the file pointer if this
    //  is possible for this type of stream.
    //

    //
    //  2006-03-31: Only supported file type at this time: ReadMasker OUT.
    //
    return new CRmReader(istr);
}

void CRmReader::CloseReader(CRmReader* reader)
{
    delete reader;
}

void CRmReader::Read(CRef<CSeq_annot> annot,
                     TFlags flags, size_t errors)
{
    annot->Reset();
    CRepeatMaskerReader impl(flags);
    CMessageListenerWithLog error_container(DIAG_COMPILE_INFO);
    CRef<CSeq_annot> result(impl.ReadSeqAnnot(m_Istr, &error_container));
    annot->Assign(*result, eShallow);
}


END_objects_SCOPE
END_NCBI_SCOPE
