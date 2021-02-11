/*
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
* Author: Jonathan Kans, Aaron Ucko
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objmgr/util/create_defline.hpp>

#include <corelib/ncbi_safe_static.hpp>
#include <util/text_joiner.hpp>
#include <serial/iterator.hpp>

#include <objects/misc/sequence_macros.hpp>
#include <objects/seq/Map_ext.hpp>
#include <objects/seqfeat/Rsite_ref.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/error_codes.hpp>

#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/autodef.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(sequence);
USING_SCOPE(feature);

#define NCBI_USE_ERRCODE_X   ObjMgr_SeqUtil

enum EHidePart { eHideNone, eHideType, eHideValue };
class CDefLineJoiner
{
public:
    CDefLineJoiner(bool show_mods = false)
        : m_ShowMods(show_mods)
    {
    }
    void Add(const CTempString &name, const CTempString &value, EHidePart hide = eHideNone)
    {
        if (m_ShowMods)
        {
            if (name.empty() || value.empty()) {
                return;
            }
            // The case of no quotes is much more common, so optimize for that
            if (value.find_first_of("\"=") != string::npos) {
                // rarer case: bad characters in value name, so
                // we need surrounding double-quotes and we need to change
                // double-quotes to single-quotes.
                m_Joiner.Add(" [").Add(name).Add("=\"");
                ReplaceAndAdd(value, "\"", "'");
                m_Joiner.Add("\"]");
            } else {
                m_Joiner.Add(" [").Add(name).Add("=").Add(value).Add("]");
            }
        }
        else
        {
            if (eHideNone == hide && !name.empty()) {
                m_Joiner.Add(" ").Add(name);
            }
            if (!value.empty()) {
                m_Joiner.Add(" ").Add(value);
            }
        }
    }
    void Join(std::string* result) const
    {
        m_Joiner.Join(result);
    }
private:
    void ReplaceAndAdd(const CTempString &value, 
        const CTempString &replace_what, const CTempString &replace_with)
    {
        // commented out: CTempString is immutable
        //string fixed = NStr::Replace(value, "\"", "'");
        CTempString::size_type p1 = 0, p2 = value.length();
        for (; (p2 = value.find(replace_what, p1)) != string::npos; 
            p1 = p2 + 1, p2 = value.length()) {
            m_Joiner.Add(value.substr(p1, p2 - p1)).Add(replace_with);
        }
        m_Joiner.Add(value.substr(p1, p2 - p1));
    }
    bool m_ShowMods;
    CTextJoiner<64, CTempString> m_Joiner;
};

// constructor
CDeflineGenerator::CDeflineGenerator (void)
{
    m_ConstructedFeatTree = false;
    m_InitializedFeatTree = false;
    x_Init();
}

// constructor
CDeflineGenerator::CDeflineGenerator (const CSeq_entry_Handle& tseh)
{
    // initialize common bits (FSA)
    x_Init();

    // then store top SeqEntry Handle for building CFeatTree when first needed
    m_TopSEH = tseh;
    m_ConstructedFeatTree = true;
    m_InitializedFeatTree = false;
}

// destructor
CDeflineGenerator::~CDeflineGenerator (void)

{
}

void CDeflineGenerator::x_Init (void)
{
    // nothing here yet
}

// macros

// SEQENTRY_HANDLE_ON_SEQENTRY_HANDLE_ITERATOR
// FOR_EACH_SEQENTRY_HANDLE_ON_SEQENTRY_HANDLE
// CSeq_entry_Handle as input,
//  dereference with CSeq_entry_Handle var = *Itr;

#define SEQENTRY_HANDLE_ON_SEQENTRY_HANDLE_ITERATOR(Itr, Var) \
CSeq_entry_CI Itr(Var)

#define FOR_EACH_SEQENTRY_HANDLE_ON_SEQENTRY_HANDLE(Itr, Var) \
for (SEQENTRY_HANDLE_ON_SEQENTRY_HANDLE_ITERATOR(Itr, Var); Itr;  ++Itr)

// FOR_EACH_SEQID_ON_BIOSEQ_HANDLE
// CBioseq_Handle& as input,
//  dereference with CSeq_id_Handle sid = *Itr;

#define FOR_EACH_SEQID_ON_BIOSEQ_HANDLE(Itr, Var) \
ITERATE (CBioseq_Handle::TId, Itr, Var.GetId())

// SEQFEAT_ON_BIOSEQ_HANDLE_ITERATOR
// FOR_EACH_SEQFEAT_ON_BIOSEQ_HANDLE
// CBioseq_Handle& as input,
//  dereference with const CSeq_feat& sft = Itr->GetOriginalFeature();

#define SEQFEAT_ON_BIOSEQ_HANDLE_ITERATOR(Itr, Var, Chs) \
CFeat_CI Itr(Var, CSeqFeatData::e_##Chs)

#define FOR_EACH_SEQFEAT_ON_BIOSEQ_HANDLE(Itr, Var, Chs) \
for (SEQFEAT_ON_BIOSEQ_HANDLE_ITERATOR(Itr, Var, Chs); Itr;  ++Itr)

// SEQFEAT_ON_SCOPE_ITERATOR
// FOR_EACH_SEQFEAT_ON_SCOPE
// CScope& as input,
//  dereference with const CSeq_feat& sft = Itr->GetOriginalFeature();

#define SEQFEAT_ON_SCOPE_ITERATOR(Itr, Var, Loc, Chs) \
CFeat_CI Itr(Var, Loc, CSeqFeatData::e_##Chs)

#define FOR_EACH_SEQFEAT_ON_SCOPE(Itr, Var, Loc, Chs) \
for (SEQFEAT_ON_SCOPE_ITERATOR(Itr, Var, Loc, Chs); Itr;  ++Itr)

// SELECTED_SEQFEAT_ON_BIOSEQ_HANDLE_ITERATOR
// FOR_SELECTED_SEQFEAT_ON_BIOSEQ_HANDLE
// CBioseq_Handle& and SAnnotSelector as input,
//  dereference with const CSeq_feat& sft = Itr->GetOriginalFeature();

#define SELECTED_SEQFEAT_ON_BIOSEQ_HANDLE_ITERATOR(Itr, Var, Sel) \
CFeat_CI Itr(Var, Sel)

#define FOR_SELECTED_SEQFEAT_ON_BIOSEQ_HANDLE(Itr, Var, Sel) \
for (SELECTED_SEQFEAT_ON_BIOSEQ_HANDLE_ITERATOR(Itr, Var, Sel); Itr;  ++Itr)

// Copied from CleanAndCompress in objtools/format/utils.cpp

// two-bytes combinations we're looking to clean
#define twochars(a,b) Uint2((a) << 8 | (b))
#define twocommas twochars(',',',')
#define twospaces twochars(' ',' ')
#define space_comma twochars(' ',',')
#define space_bracket twochars(' ',')')
#define bracket_space twochars('(',' ')
#define space_semicolon twochars(' ',';')
#define comma_space twochars(',',' ')
#define semicolon_space twochars(';',' ')

void x_CleanAndCompress(string& dest, const CTempString& instr, bool isProt)
{
    size_t left = instr.size();
    // this is the input stream
    const char* in = instr.data();

    // skip front white spaces
    while (left && *in == ' ')
    {
        in++;
        left--;
    }
    // forget end white spaces
    while (left && in[left - 1] == ' ')
    {
        left--;
    }

    dest.resize(left);

    if (left < 1) return;

    // this is where we write result
    char* out = (char*)dest.c_str();

    char curr = *in++; // initialize with first character
    left--;

    char next = 0;
    Uint2 two_chars = curr; // this is two bytes storage where we see current and previous symbols

    while (left > 0) {
        next = *in++;

        two_chars = Uint2((two_chars << 8) | next);

        switch (two_chars)
        {
        case twocommas: // replace double commas with comma+space
            *out++ = curr;
            next = ' ';
            break;
        case twospaces: // skip multispaces (only print last one)
            break;
        case bracket_space: // skip space after bracket
            next = curr;
            two_chars = curr;
            break;
        case space_bracket: // skip space before bracket
            break;
        case space_comma:
        case space_semicolon: // swap characters
            *out++ = next;
            next = curr;
            two_chars = curr;
            break;
        case comma_space:
            *out++ = curr;
            *out++ = ' ';
            while (next == ' ' || next == ',') {
                next = *in;
                in++;
                left--;
            }
            two_chars = next;
            break;
        case semicolon_space:
            *out++ = curr;
            *out++ = ' ';
            while (next == ' ' || next == ';') {
                next = *in;
                in++;
                left--;
            }
            two_chars = next;
            break;
        default:
            *out++ = curr;
            break;
        }

        curr = next;
        left--;
    }

    if (curr > 0 && curr != ' ') {
        *out++ = curr;
    }

    dest.resize(out - dest.c_str());

    if (isProt) {
        NStr::ReplaceInPlace (dest, ". [", " [");
        NStr::ReplaceInPlace (dest, ", [", " [");
    }
}

static const char* x_OrganelleName (
    TBIOSOURCE_GENOME genome,
    bool has_plasmid,
    bool virus_or_phage,
    bool wgs_suffix
)

{
    const char* result = kEmptyCStr;

    switch (genome) {
        case NCBI_GENOME(chloroplast):
            result = "chloroplast";
            break;
        case NCBI_GENOME(chromoplast):
            result = "chromoplast";
            break;
        case NCBI_GENOME(kinetoplast):
            result = "kinetoplast";
            break;
        case NCBI_GENOME(mitochondrion):
        {
            if (has_plasmid || wgs_suffix) {
                result = "mitochondrial";
            } else {
                result = "mitochondrion";
            }
            break;
        }
        case NCBI_GENOME(plastid):
            result = "plastid";
            break;
        case NCBI_GENOME(macronuclear):
        {
            result = "macronuclear";
            break;
        }
        case NCBI_GENOME(extrachrom):
        {
            if (! wgs_suffix) {
                result = "extrachromosomal";
            }
            break;
        }
        case NCBI_GENOME(plasmid):
        {
            if (! wgs_suffix) {
                result = "plasmid";
            }
            break;
        }
        // transposon and insertion-seq are obsolete
        case NCBI_GENOME(cyanelle):
            result = "cyanelle";
            break;
        case NCBI_GENOME(proviral):
        {
            if (! virus_or_phage) {
                if (has_plasmid || wgs_suffix) {
                    result = "proviral";
                } else {
                    result = "provirus";
                }
            }
            break;
        }
        case NCBI_GENOME(virion):
        {
            if (! virus_or_phage) {
                result = "virus";
            }
            break;
        }
        case NCBI_GENOME(nucleomorph):
        {
            if (! wgs_suffix) {
                result = "nucleomorph";
            }
           break;
        }
        case NCBI_GENOME(apicoplast):
            result = "apicoplast";
            break;
        case NCBI_GENOME(leucoplast):
            result = "leucoplast";
            break;
        case NCBI_GENOME(proplastid):
            result = "proplastid";
            break;
        case NCBI_GENOME(endogenous_virus):
            result = "endogenous virus";
            break;
        case NCBI_GENOME(hydrogenosome):
            result = "hydrogenosome";
            break;
        case NCBI_GENOME(chromosome):
            result = "chromosome";
            break;
        case NCBI_GENOME(chromatophore):
            result = "chromatophore";
            break;
    }

    return result;
}

// set instance variables from Seq-inst, Seq-ids, MolInfo, etc., but not
//  BioSource
void CDeflineGenerator::x_SetFlagsIdx (
    const CBioseq_Handle& bsh,
    TUserFlags flags
)

{
    CRef<CBioseqIndex> bsx = m_Idx->GetBioseqIndex (bsh);
    if (! bsx) {
        return;
    }

    // set flags from record components
    m_Reconstruct = (flags & fIgnoreExisting) != 0;
    m_AllProtNames = (flags & fAllProteinNames) != 0;
    m_LocalAnnotsOnly = (flags & fLocalAnnotsOnly) != 0;
    m_GpipeMode = (flags & fGpipeMode) != 0;
    m_OmitTaxonomicName = (flags & fOmitTaxonomicName) != 0;
    m_DevMode = (flags & fDevMode) != 0;

    // reset member variables to cleared state
    m_IsNA = bsx->IsNA();
    m_IsAA = bsx->IsAA();
    m_Topology = bsx->GetTopology();
    m_Length = bsx->GetLength();

    m_IsSeg = false;
    m_IsDelta = bsx->IsDelta();
    m_IsVirtual = bsx->IsVirtual();
    m_IsMap = bsx->IsMap();

    m_IsNC = bsx->IsNC();
    m_IsNM = bsx->IsNM();
    m_IsNR = bsx->IsNR();
    m_IsNZ = bsx->IsNZ();
    m_IsPatent = bsx->IsPatent();
    m_IsPDB = bsx->IsPDB();
    m_IsWP = bsx->IsWP();
    m_ThirdParty = bsx->IsThirdParty();
    m_WGSMaster = bsx->IsWGSMaster();
    m_TSAMaster = bsx->IsTSAMaster();
    m_TLSMaster = bsx->IsTLSMaster();

    m_GeneralStr = bsx->GetGeneralStr();
    m_GeneralId = bsx->GetGeneralId();

    m_PatentCountry= bsx->GetPatentCountry();
    m_PatentNumber = bsx->GetPatentNumber();
    m_PatentSequence = bsx->GetPatentSequence();

    m_PDBChain = bsx->GetPDBChain();
    m_PDBChainID = bsx->GetPDBChainID();

    m_MIBiomol = bsx->GetBiomol();
    m_MITech = bsx->GetTech();
    m_MICompleteness = bsx->GetCompleteness();

    m_HTGTech = bsx->IsHTGTech();
    m_HTGSUnfinished = bsx->IsHTGSUnfinished();
    m_IsTLS = bsx->IsTLS();
    m_IsTSA = bsx->IsTSA();
    m_IsWGS = bsx->IsWGS();
    m_IsEST_STS_GSS = bsx->IsEST_STS_GSS();

    m_MainTitle.clear();
    if (! m_HTGSUnfinished && ! m_Reconstruct) {
        m_MainTitle = bsx->GetTitle();
    }

    m_UseBiosrc = bsx->IsUseBiosrc();

    m_HTGSCancelled = bsx->IsHTGSCancelled();
    m_HTGSDraft = bsx->IsHTGSDraft();
    m_HTGSPooled = bsx->IsHTGSPooled();
    m_TPAExp = bsx->IsTPAExp();
    m_TPAInf = bsx->IsTPAInf();
    m_TPAReasm = bsx->IsTPAReasm();
    m_Unordered = bsx->IsUnordered();

    m_PDBCompound = bsx->GetPDBCompound();

    m_Source = bsx->GetBioSource();
    m_Taxname = bsx->GetTaxname();
    m_Genus = bsx->GetGenus();
    m_Species = bsx->GetSpecies();
    m_Multispecies = bsx->IsMultispecies();
    m_Genome = bsx->GetGenome();
    m_IsPlasmid = bsx->IsPlasmid();
    m_IsChromosome = bsx->IsChromosome();

    m_Organelle = bsx->GetOrganelle();

    m_FirstSuperKingdom = bsx->GetFirstSuperKingdom();
    m_SecondSuperKingdom = bsx->GetSecondSuperKingdom();
    m_IsCrossKingdom = bsx->IsCrossKingdom();

    m_Chromosome = bsx->GetChromosome();
    m_LinkageGroup = bsx->GetLinkageGroup();
    m_Clone = bsx->GetClone();
    m_has_clone = bsx->HasClone();
    m_Map = bsx->GetMap();
    m_Plasmid = bsx->GetPlasmid();
    m_Segment = bsx->GetSegment();

    m_Breed = bsx->GetBreed();
    m_Cultivar = bsx->GetCultivar();
    m_Isolate = bsx->GetIsolate();
    m_Strain = bsx->GetStrain();
    m_Substrain = bsx->GetSubstrain();

    m_IsUnverified = bsx->IsUnverified();
    m_UnverifiedPrefix.clear();
    if (m_IsUnverified) {
        int unverified_count = 0;
        m_UnverifiedPrefix = "UNVERIFIED: ";
        if (bsx->IsUnverifiedOrganism()) {
            m_UnverifiedPrefix = "UNVERIFIED_ORG: ";
            unverified_count++;
        }
        if (bsx->IsUnverifiedMisassembled()) {
            m_UnverifiedPrefix = "UNVERIFIED_ASMBLY: ";
            unverified_count++;
        }
        if (bsx->IsUnverifiedContaminant()) {
            m_UnverifiedPrefix = "UNVERIFIED_CONTAM: ";
            unverified_count++;
        }
        if (bsx->IsUnverifiedFeature()) {
            m_UnverifiedPrefix = "UNVERIFIED: ";
            unverified_count++;
        }
        if (unverified_count > 1) {
            m_UnverifiedPrefix = "UNVERIFIED: ";
        }
    }

    m_Comment = bsx->GetComment();
    m_IsPseudogene = bsx->IsPseudogene();
    m_TargetedLocus = bsx->GetTargetedLocus();

    m_rEnzyme = bsx->GetrEnzyme();
    
    m_UsePDBCompoundForDefline = false;

    if (m_IsPDB) {
        if (m_Comment.empty()) {
            m_UsePDBCompoundForDefline = true;
        } else if (m_IsNA) {
            if ( m_Length < 25 ) {
                m_UsePDBCompoundForDefline = true;
            } else if (NStr::Find(m_Comment, "COMPLETE GENOME") != NPOS ||
                NStr::Find(m_Comment, "CHROMOSOME XII") != NPOS) {
                m_UsePDBCompoundForDefline = true;
            } else if (NStr::Find(m_Comment, "Dna (5'") != NPOS ||
                NStr::Find(m_Comment, "SEQRES") != NPOS) {
                m_UsePDBCompoundForDefline = true;
            }
        } else {
            if (NStr::Find(m_Comment, "hypothetical protein") != NPOS ||
                NStr::Find(m_Comment, "uncharacterized protein") != NPOS ||
                NStr::Find(m_Comment, "putative uncharacterized protein") != NPOS ||
                NStr::Find(m_Comment, "putative protein") != NPOS ||
                NStr::Find(m_Comment, "SEQRES") != NPOS) {
                m_UsePDBCompoundForDefline = true;
            }
        }
    }
}

// set instance variables from Seq-inst, Seq-ids, MolInfo, etc., but not
//  BioSource
void CDeflineGenerator::x_SetFlags (
    const CBioseq_Handle& bsh,
    TUserFlags flags
)

{
    // set flags from record components
    m_Reconstruct = (flags & fIgnoreExisting) != 0;
    m_AllProtNames = (flags & fAllProteinNames) != 0;
    m_LocalAnnotsOnly = (flags & fLocalAnnotsOnly) != 0;
    m_GpipeMode = (flags & fGpipeMode) != 0;
    m_OmitTaxonomicName = (flags & fOmitTaxonomicName) != 0;
    m_DevMode = (flags & fDevMode) != 0;

    // reset member variables to cleared state
    m_IsNA = false;
    m_IsAA = false;
    m_Topology = NCBI_SEQTOPOLOGY(not_set);
    m_Length = 0;

    m_IsSeg = false;
    m_IsDelta = false;
    m_IsVirtual = false;
    m_IsMap = false;

    m_IsNC = false;
    m_IsNM = false;
    m_IsNR = false;
    m_IsNZ = false;
    m_IsPatent = false;
    m_IsPDB = false;
    m_IsWP = false;
    m_ThirdParty = false;
    m_WGSMaster = false;
    m_TSAMaster = false;
    m_TLSMaster = false;

    m_MainTitle.clear();
    m_GeneralStr.clear();
    m_GeneralId = 0;
    m_PatentCountry.clear();
    m_PatentNumber.clear();

    m_PatentSequence = 0;

    m_PDBChain = 0;
    m_PDBChainID.clear();

    m_MIBiomol = NCBI_BIOMOL(unknown);
    m_MITech = NCBI_TECH(unknown);
    m_MICompleteness = NCBI_COMPLETENESS(unknown);

    m_HTGTech = false;
    m_HTGSUnfinished = false;
    m_IsTLS = false;
    m_IsTSA = false;
    m_IsWGS = false;
    m_IsEST_STS_GSS = false;

    m_UseBiosrc = false;

    m_HTGSCancelled = false;
    m_HTGSDraft = false;
    m_HTGSPooled = false;
    m_TPAExp = false;
    m_TPAInf = false;
    m_TPAReasm = false;
    m_Unordered = false;

    m_PDBCompound.clear();

    m_Source.Reset();
    m_Taxname.clear();
    m_Genus.clear();
    m_Species.clear();
    m_Multispecies = false;
    m_Genome = NCBI_GENOME(unknown);
    m_IsPlasmid = false;
    m_IsChromosome = false;

    m_Organelle.clear();

    m_FirstSuperKingdom.clear();
    m_SecondSuperKingdom.clear();
    m_IsCrossKingdom = false;

    m_Chromosome.clear();
    m_LinkageGroup.clear();
    m_Clone.clear();
    m_has_clone = false;
    m_Map.clear();
    m_Plasmid.clear();
    m_Segment.clear();

    m_Breed.clear();
    m_Cultivar.clear();
    m_Isolate.clear();
    m_Strain.clear();
    m_Substrain.clear();

    m_IsUnverified = false;
    m_UnverifiedPrefix.clear();
    m_TargetedLocus.clear();

    m_Comment.clear();
    m_IsPseudogene = false;

    m_rEnzyme.clear();

    m_UsePDBCompoundForDefline = false;

    // now start setting member variables
    m_IsNA = bsh.IsNa();
    m_IsAA = bsh.IsAa();
    m_Topology = bsh.GetInst_Topology();
    m_Length = bsh.GetInst_Length();

    if (bsh.IsSetInst()) {
        if (bsh.IsSetInst_Repr()) {
            TSEQ_REPR repr = bsh.GetInst_Repr();
            m_IsSeg = (repr == CSeq_inst::eRepr_seg);
            m_IsDelta = (repr == CSeq_inst::eRepr_delta);
            m_IsVirtual = (repr == CSeq_inst::eRepr_virtual);
            m_IsMap = (repr == CSeq_inst::eRepr_map);
        }
    }

    // process Seq-ids
    FOR_EACH_SEQID_ON_BIOSEQ_HANDLE (sid_itr, bsh) {
        CSeq_id_Handle sid = *sid_itr;
        switch (sid.Which()) {
            case NCBI_SEQID(Tpg):
            case NCBI_SEQID(Tpe):
            case NCBI_SEQID(Tpd):
                m_ThirdParty = true;
                // fall through
            case NCBI_SEQID(Other):
            case NCBI_SEQID(Genbank):
            case NCBI_SEQID(Embl):
            case NCBI_SEQID(Ddbj):
            {
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CTextseq_id& tsid = *id->GetTextseq_Id ();
                if (tsid.IsSetAccession()) {
                    const string& acc = tsid.GetAccession ();
                    TACCN_CHOICE type = CSeq_id::IdentifyAccession (acc);
                    TACCN_CHOICE div = (TACCN_CHOICE) (type & NCBI_ACCN(division_mask));
                    if ( div == NCBI_ACCN(wgs) )
                    {
                        if( (type & CSeq_id::fAcc_master) != 0 ) {
                            m_WGSMaster = true;
                        }
                    } else if ( div == NCBI_ACCN(tsa) )
                    {
                        if( (type & CSeq_id::fAcc_master) != 0 && m_IsVirtual ) {
                            m_TSAMaster = true;
                        }
                    } else if (type == NCBI_ACCN(refseq_chromosome)) {
                        m_IsNC = true;
                    } else if (type == NCBI_ACCN(refseq_mrna)) {
                        m_IsNM = true;
                    } else if (type == NCBI_ACCN(refseq_mrna_predicted)) {
                        m_IsNM = true;
                    } else if (type == NCBI_ACCN(refseq_ncrna)) {
                        m_IsNR = true;
                    } else if (type == NCBI_ACCN(refseq_contig)) {
                        m_IsNZ = true;
                    } else if (type == NCBI_ACCN(refseq_unique_prot)) {
                        m_IsWP = true;
                    }
                }
                break;
            }
            case NCBI_SEQID(General):
            {
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CDbtag& gen_id = id->GetGeneral ();
                if (! gen_id.IsSkippable ()) {
                    if (gen_id.IsSetTag ()) {
                        const CObject_id& oid = gen_id.GetTag();
                        if (oid.IsStr()) {
                            m_GeneralStr = oid.GetStr();
                        } else if (oid.IsId()) {
                            m_GeneralId = oid.GetId();
                        }
                    }
                }
                break;
            }
            case NCBI_SEQID(Pdb):
            {
                m_IsPDB = true;
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CPDB_seq_id& pdb_id = id->GetPdb ();
                if (pdb_id.IsSetChain_id()) {
                    m_PDBChainID = pdb_id.GetChain_id();
                } else if (pdb_id.IsSetChain()) {
                    m_PDBChain = pdb_id.GetChain();
                }
                break;
            }
            case NCBI_SEQID(Patent):
            {
                m_IsPatent = true;
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CPatent_seq_id& pat_id = id->GetPatent();
                if (pat_id.IsSetSeqid()) {
                    m_PatentSequence = pat_id.GetSeqid();
                }
                if (pat_id.IsSetCit()) {
                    const CId_pat& cit = pat_id.GetCit();
                    m_PatentCountry = cit.GetCountry();
                    m_PatentNumber = cit.GetSomeNumber();
                }
                break;
            }
            case NCBI_SEQID(Gpipe):
                break;
            default:
                break;
        }
    }

    enum ENeededDescChoices {
        fMolinfo = 1 << 0,
        fUser    = 1 << 1,
        fSource  = 1 << 2,
        fGenbank = 1 << 3,
        fEmbl    = 1 << 4,
        fTitle   = 1 << 5,
        fPdb     = 1 << 6,
        fComment = 1 << 7
    };
    int needed_desc_choices = fMolinfo | fUser | fSource | fGenbank | fEmbl | fComment;

    CSeqdesc_CI::TDescChoices desc_choices;
    desc_choices.reserve(7);
    desc_choices.push_back(CSeqdesc::e_Molinfo);
    desc_choices.push_back(CSeqdesc::e_User);
    desc_choices.push_back(CSeqdesc::e_Source);
    // Only truly needed if (m_HTGTech || m_ThirdParty), but
    // determining m_HTGTech requires a descriptor scan.
    desc_choices.push_back(CSeqdesc::e_Genbank);
    desc_choices.push_back(CSeqdesc::e_Embl);
    desc_choices.push_back(CSeqdesc::e_Comment);
    if (! m_Reconstruct) {
        needed_desc_choices |= fTitle;
        desc_choices.push_back(CSeqdesc::e_Title);
    }
    if (m_IsPDB) {
        needed_desc_choices |= fPdb;
        desc_choices.push_back(CSeqdesc::e_Pdb);
    }

    const list <string> *keywords = NULL;

    int num_super_kingdom = 0;
    bool super_kingdoms_different = false;

    for (CSeqdesc_CI desc_it(bsh, desc_choices);
         needed_desc_choices != 0  &&  desc_it;  ++desc_it) {
        switch (desc_it->Which()) {
        case CSeqdesc::e_Molinfo:
        {
            // process MolInfo tech
            if ((needed_desc_choices & fMolinfo) == 0) {
                continue; // already covered
            }

            const CMolInfo& molinf = desc_it->GetMolinfo();
            m_MIBiomol = molinf.GetBiomol();
            m_MITech = molinf.GetTech();
            m_MICompleteness = molinf.GetCompleteness();
            switch (m_MITech) {
                case NCBI_TECH(htgs_0):
                case NCBI_TECH(htgs_1):
                case NCBI_TECH(htgs_2):
                    m_HTGSUnfinished = true;
                    // manufacture all titles for unfinished HTG sequences
                    m_Reconstruct = true;
                    needed_desc_choices &= ~fTitle;
                    m_MainTitle.clear();
                    // fall through
                case NCBI_TECH(htgs_3):
                    m_HTGTech = true;
                    m_UseBiosrc = true;
                    break;
                case NCBI_TECH(est):
                case NCBI_TECH(sts):
                case NCBI_TECH(survey):
                    m_IsEST_STS_GSS = true;
                    m_UseBiosrc = true;
                    break;
                case NCBI_TECH(wgs):
                    m_IsWGS = true;
                    m_UseBiosrc = true;
                    break;
                case NCBI_TECH(tsa):
                    m_IsTSA = true;
                    m_UseBiosrc = true;
                    if (m_IsVirtual) {
                        m_TSAMaster = true;
                    }
                    break;
                case NCBI_TECH(targeted):
                    m_IsTLS = true;
                    m_UseBiosrc = true;
                    if (m_IsVirtual) {
                        m_TLSMaster = true;
                    }
                    break;
                default:
                    break;
            }

            // take first, then skip remainder
            needed_desc_choices &= ~fMolinfo;
            break;
        }

        case CSeqdesc::e_User:
        {
            // process Unverified user object
            if ((needed_desc_choices & fUser) == 0) {
                continue; // already covered
            }

            const CUser_object& user_obj = desc_it->GetUser();
            if (FIELD_IS_SET_AND_IS(user_obj, Type, Str)) {
                if (user_obj.IsUnverified()) {
                    m_IsUnverified = true;
                    int unverified_count = 0;
                    needed_desc_choices &= ~fUser;
                    if (user_obj.IsUnverifiedOrganism()) {
                        m_UnverifiedPrefix = "UNVERIFIED_ORG: ";
                        unverified_count++;
                    }
                    if (user_obj.IsUnverifiedMisassembled()) {
                        m_UnverifiedPrefix = "UNVERIFIED_ASMBLY: ";
                        unverified_count++;
                    }
                    if (user_obj.IsUnverifiedContaminant()) {
                        m_UnverifiedPrefix = "UNVERIFIED_CONTAM: ";
                        unverified_count++;
                    }
                    if (user_obj.IsUnverifiedFeature()) {
                        m_UnverifiedPrefix = "UNVERIFIED: ";
                        unverified_count++;
                    }
                    if (unverified_count > 1) {
                        m_UnverifiedPrefix = "UNVERIFIED: ";
                    }
                } else if (user_obj.GetType().GetStr() == "AutodefOptions" ) {
                    FOR_EACH_USERFIELD_ON_USEROBJECT (uitr, user_obj) {
                        const CUser_field& fld = **uitr;
                        if (! FIELD_IS_SET_AND_IS(fld, Label, Str)) continue;
                        const string &label_str = GET_FIELD(fld.GetLabel(), Str);
                        if (! NStr::EqualNocase(label_str, "Targeted Locus Name")) continue;
                        if (fld.IsSetData() && fld.GetData().IsStr()) {
                            m_TargetedLocus = fld.GetData().GetStr();
                        }
                    }
                }
            }
            break;
        }

        case CSeqdesc::e_Comment:
        {
            // process comment
            if ((needed_desc_choices & fComment) == 0) {
                continue; // already covered
            }

            m_Comment = desc_it->GetComment();
            if (NStr::Find (m_Comment, "[CAUTION] Could be the product of a pseudogene") != string::npos) {
                m_IsPseudogene = true;
            }
            break;
        }

        case CSeqdesc::e_Source:
            if ((needed_desc_choices & fSource) != 0) {
                m_Source.Reset(&desc_it->GetSource());
                // take first, then skip remainder
                needed_desc_choices &= ~fSource;
            }
            if (m_IsWP) {
                const CBioSource &bsrc = desc_it->GetSource();
                if (! bsrc.IsSetOrgname()) break;
                const COrgName& onp = bsrc.GetOrgname();
                if (! onp.IsSetName()) break;
                const COrgName::TName& nam = onp.GetName();
                if (! nam.IsPartial()) break;
                const CPartialOrgName& pon = nam.GetPartial();
                if (! pon.IsSet()) break;
                const CPartialOrgName::Tdata& tx = pon.Get();
                ITERATE (CPartialOrgName::Tdata, itr, tx) {
                    const CTaxElement& te = **itr;
                    if (! te.IsSetFixed_level()) continue;
                    if (te.GetFixed_level() != 0) continue;
                    if (! te.IsSetLevel()) continue;
                    const string& lvl = te.GetLevel();
                    if (! NStr::EqualNocase (lvl, "superkingdom")) continue;
                    num_super_kingdom++;
                    if (m_FirstSuperKingdom.empty() && te.IsSetName()) {
                        m_FirstSuperKingdom = te.GetName();
                    } else if (te.IsSetName() && ! NStr::EqualNocase (m_FirstSuperKingdom, te.GetName())) {
                        if (m_SecondSuperKingdom.empty()) {
                            super_kingdoms_different = true;
                            m_SecondSuperKingdom = te.GetName();
                        }
                    }
                    if (num_super_kingdom > 1 && super_kingdoms_different) {
                        m_IsCrossKingdom = true;
                    }
                }
            }
            break;

        case CSeqdesc::e_Title:
            if ((needed_desc_choices & fTitle) != 0) {
                // for non-PDB proteins, title must be packaged on Bioseq
                if (m_IsNA  ||  m_IsPDB
                    ||  desc_it.GetSeq_entry_Handle().IsSeq()) {
                    m_MainTitle = desc_it->GetTitle();
                }
                // take first, then skip remainder
                needed_desc_choices &= ~fTitle;
            }
            break;

        case CSeqdesc::e_Genbank:
        {
            if ((needed_desc_choices & fGenbank) == 0) {
                continue; // already covered
            }
            const CGB_block& gbk = desc_it->GetGenbank();
            if (gbk.IsSetKeywords()) {
                keywords = &gbk.GetKeywords();
            }

            // take first, then skip remainder along with any EMBL blocks
            needed_desc_choices &= ~(fGenbank | fEmbl);
            break;
        }

        case CSeqdesc::e_Embl:
        {
            if ((needed_desc_choices & fEmbl) == 0) {
                continue; // already covered
            }
            const CEMBL_block& ebk = desc_it->GetEmbl();
            if (ebk.IsSetKeywords()) {
                keywords = &ebk.GetKeywords();
            }

            // take first, then skip remainder
            needed_desc_choices &= ~fEmbl;
            break;
        }

        case CSeqdesc::e_Pdb:
        {
            if ((needed_desc_choices & fPdb) == 0) {
                continue; // already covered
            }
            _ASSERT(m_IsPDB);
            const CPDB_block& pbk = desc_it->GetPdb();
            FOR_EACH_COMPOUND_ON_PDBBLOCK (cp_itr, pbk) {
                if (m_PDBCompound.empty()) {
                    m_PDBCompound = *cp_itr;

                    // take first, then skip remainder
                    needed_desc_choices &= ~fPdb;
                }
            }
            break;
        }

        default:
            _TROUBLE;
        }
    }

    if (keywords != NULL) {
        FOR_EACH_STRING_IN_LIST (kw_itr, *keywords) {
            const string& clause = *kw_itr;
            list<string> kywds;
            NStr::Split( clause, ";", kywds, NStr::fSplit_Tokenize );
            FOR_EACH_STRING_IN_LIST ( k_itr, kywds ) {
                const string& str = *k_itr;
                if (NStr::EqualNocase (str, "UNORDERED")) {
                    m_Unordered = true;
                }
                if ((! m_HTGTech) && (! m_ThirdParty)) continue;
                if (NStr::EqualNocase (str, "HTGS_DRAFT")) {
                    m_HTGSDraft = true;
                } else if (NStr::EqualNocase (str, "HTGS_CANCELLED")) {
                    m_HTGSCancelled = true;
                } else if (NStr::EqualNocase (str, "HTGS_POOLED_MULTICLONE")) {
                    m_HTGSPooled = true;
                } else if (NStr::EqualNocase (str, "TPA:experimental")) {
                    m_TPAExp = true;
                } else if (NStr::EqualNocase (str, "TPA:inferential")) {
                    m_TPAInf = true;
                } else if (NStr::EqualNocase (str, "TPA:reassembly")) {
                    m_TPAReasm = true;
                } else if (NStr::EqualNocase (str, "TPA:assembly")) {
                    m_TPAReasm = true;
                }
            }
        }
    }

    if (m_IsMap) {
        if (bsh.IsSetInst_Ext() && bsh.GetInst_Ext().IsMap()) {
            const CMap_ext& mp = bsh.GetInst_Ext().GetMap();
            if (mp.IsSet()) {
                const CMap_ext::Tdata& ft = mp.Get();
                ITERATE (CMap_ext::Tdata, itr, ft) {
                    const CSeq_feat& feat = **itr;
                    const CSeqFeatData& data = feat.GetData();
                    if (! data.IsRsite()) continue;
                    const CRsite_ref& rsite = data.GetRsite();
                    if (rsite.IsStr()) {
                        m_rEnzyme = rsite.GetStr();
                    }
                }
            }
        }
    }

    if (m_IsPDB) {
        if (m_Comment.empty()) {
            m_UsePDBCompoundForDefline = true;
        } else if (m_IsNA) {
            if ( m_Length < 25 ) {
                m_UsePDBCompoundForDefline = true;
            } else if (NStr::Find(m_Comment, "COMPLETE GENOME") != NPOS ||
                NStr::Find(m_Comment, "CHROMOSOME XII") != NPOS) {
                m_UsePDBCompoundForDefline = true;
            } else if (NStr::Find(m_Comment, "Dna (5'") != NPOS ||
                NStr::Find(m_Comment, "SEQRES") != NPOS) {
                m_UsePDBCompoundForDefline = true;
            }
        } else {
            if (NStr::Find(m_Comment, "hypothetical protein") != NPOS ||
                NStr::Find(m_Comment, "uncharacterized protein") != NPOS ||
                NStr::Find(m_Comment, "putative uncharacterized protein") != NPOS ||
                NStr::Find(m_Comment, "putative protein") != NPOS ||
                NStr::Find(m_Comment, "SEQRES") != NPOS) {
                m_UsePDBCompoundForDefline = true;
            }
        }
    }
}

void CDeflineGenerator::x_SetBioSrcIdx (
    const CBioseq_Handle& bsh
)

{
    CRef<CBioseqIndex> bsx = m_Idx->GetBioseqIndex (bsh);
    if (! bsx) {
        return;
    }

    m_Source = bsx->GetBioSource();
    m_Taxname = bsx->GetTaxname();

    m_Genome = bsx->GetGenome();
    m_IsPlasmid = bsx->IsPlasmid();
    m_IsChromosome = bsx->IsChromosome();

    m_Chromosome = bsx->GetChromosome();
    m_LinkageGroup = bsx->GetLinkageGroup();
    m_Clone = bsx->GetClone();
    m_has_clone = bsx->HasClone();
    m_Map = bsx->GetMap();
    m_Plasmid = bsx->GetPlasmid();
    m_Segment = bsx->GetSegment();

    m_Genus = bsx->GetGenus();
    m_Species = bsx->GetSpecies();
    m_Multispecies = bsx->IsMultispecies();

    m_Strain = bsx->GetStrain();
    m_Substrain = bsx->GetSubstrain();
    m_Cultivar = bsx->GetCultivar();
    m_Isolate = bsx->GetIsolate();
    m_Breed = bsx->GetBreed();

    m_Organelle = bsx->GetOrganelle();

    if (m_has_clone) return;

    try {
        CFeat_CI feat_it(bsh, SAnnotSelector(CSeqFeatData::e_Biosrc));
        while (feat_it) {
            const CSeq_feat& feat = feat_it->GetOriginalFeature();
            if (! feat.IsSetData ()) continue;
            const CSeqFeatData& sfdata = feat.GetData ();
            const CBioSource& source = sfdata.GetBiosrc();

            // process SubSource
            FOR_EACH_SUBSOURCE_ON_BIOSOURCE (sbs_itr, source) {
                const CSubSource& sbs = **sbs_itr;
                if (! sbs.IsSetName()) continue;
                SWITCH_ON_SUBSOURCE_CHOICE (sbs) {
                    case NCBI_SUBSOURCE(clone):
                        m_has_clone = true;
                        return;
                    default:
                        break;
                }
            }
            ++feat_it;
        }
    } catch ( const exception&  ) {
        // ERR_POST(Error << "Unable to iterate source features while constructing default definition line");
    }
}

// set instance variables from BioSource
void CDeflineGenerator::x_SetBioSrc (
    const CBioseq_Handle& bsh
)

{
    if (m_Source.NotEmpty()) {
        // get organism name
        if (m_Source->IsSetTaxname()) {
            m_Taxname = m_Source->GetTaxname();
        }
        if (m_Source->IsSetGenome()) {
            m_Genome = m_Source->GetGenome();
            m_IsPlasmid = (m_Genome == NCBI_GENOME(plasmid));
            m_IsChromosome = (m_Genome == NCBI_GENOME(chromosome));
        }

        // process SubSource
        FOR_EACH_SUBSOURCE_ON_BIOSOURCE (sbs_itr, *m_Source) {
            const CSubSource& sbs = **sbs_itr;
            if (! sbs.IsSetName()) continue;
            const string& str = sbs.GetName();
            SWITCH_ON_SUBSOURCE_CHOICE (sbs) {
                case NCBI_SUBSOURCE(chromosome):
                    m_Chromosome = str;
                    break;
                case NCBI_SUBSOURCE(clone):
                    m_Clone = str;
                    m_has_clone = true;
                    break;
                case NCBI_SUBSOURCE(map):
                    m_Map = str;
                    break;
                case NCBI_SUBSOURCE(plasmid_name):
                    m_Plasmid = str;
                    break;
                case NCBI_SUBSOURCE(segment):
                    m_Segment = str;
                    break;
                case NCBI_SUBSOURCE(linkage_group):
                    m_LinkageGroup = str;
                    break;
                default:
                    break;
            }
        }

        if (m_Source->IsSetOrgname()) {
            const COrgName& onp = m_Source->GetOrgname();
            if (onp.IsSetName()) {
                const COrgName::TName& nam = onp.GetName();
                if (nam.IsBinomial()) {
                    const CBinomialOrgName& bon = nam.GetBinomial();
                    if (bon.IsSetGenus()) {
                        m_Genus = bon.GetGenus();
                    }
                    if (bon.IsSetSpecies()) {
                        m_Species = bon.GetSpecies();
                    }
                } else if (nam.IsPartial()) {
                    const CPartialOrgName& pon = nam.GetPartial();
                    if (pon.IsSet()) {
                        const CPartialOrgName::Tdata& tx = pon.Get();
                        ITERATE (CPartialOrgName::Tdata, itr, tx) {
                            const CTaxElement& te = **itr;
                            if (te.IsSetFixed_level()) {
                                int fl = te.GetFixed_level();
                                if (fl > 0) {
                                    m_Multispecies = true;
                                } else if (te.IsSetLevel()) {
                                    const string& lvl = te.GetLevel();
                                    if (! NStr::EqualNocase (lvl, "species")) {
                                        m_Multispecies = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // process OrgMod
        FOR_EACH_ORGMOD_ON_BIOSOURCE (omd_itr, *m_Source) {
            const COrgMod& omd = **omd_itr;
            if (! omd.IsSetSubname()) continue;
            const string& str = omd.GetSubname();
            SWITCH_ON_ORGMOD_CHOICE (omd) {
                case NCBI_ORGMOD(strain):
                    if (m_Strain.empty()) {
                        m_Strain = str;
                    }
                    break;
                case NCBI_ORGMOD(substrain):
                    if (m_Substrain.empty()) {
                        m_Substrain = str;
                    }
                    break;
                case NCBI_ORGMOD(cultivar):
                    if (m_Cultivar.empty()) {
                        m_Cultivar = str;
                    }
                    break;
                case NCBI_ORGMOD(isolate):
                    if (m_Isolate.empty()) {
                        m_Isolate = str;
                    }
                    break;
                case NCBI_ORGMOD(breed):
                    if (m_Breed.empty()) {
                        m_Breed = str;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    bool virus_or_phage = false;
    bool has_plasmid = false;
    bool wgs_suffix = false;

    if (NStr::FindNoCase(m_Taxname, "virus") != NPOS  ||
        NStr::FindNoCase(m_Taxname, "phage") != NPOS) {
        virus_or_phage = true;
    }

    if (! m_Plasmid.empty()) {
        has_plasmid = true;
        /*
        if (NStr::FindNoCase(m_Plasmid, "plasmid") == NPOS  &&
            NStr::FindNoCase(m_Plasmid, "element") == NPOS) {
            pls_pfx = " plasmid ";
        }
        */
    }

    if (m_IsWGS) {
        wgs_suffix = true;
    }

    m_Organelle = x_OrganelleName (m_Genome, has_plasmid, virus_or_phage, wgs_suffix);

    if (m_has_clone) return;

    try {
        CFeat_CI feat_it(bsh, SAnnotSelector(CSeqFeatData::e_Biosrc));
        while (feat_it) {
            const CSeq_feat& feat = feat_it->GetOriginalFeature();
            if (! feat.IsSetData ()) continue;
            const CSeqFeatData& sfdata = feat.GetData ();
            const CBioSource& source = sfdata.GetBiosrc();

            // process SubSource
            FOR_EACH_SUBSOURCE_ON_BIOSOURCE (sbs_itr, source) {
                const CSubSource& sbs = **sbs_itr;
                if (! sbs.IsSetName()) continue;
                SWITCH_ON_SUBSOURCE_CHOICE (sbs) {
                    case NCBI_SUBSOURCE(clone):
                        m_has_clone = true;
                        return;
                    default:
                        break;
                }
            }
            ++feat_it;
        }
    } catch ( const exception&  ) {
        // ERR_POST(Error << "Unable to iterate source features while constructing default definition line");
    }
}

// generate title from BioSource fields
void CDeflineGenerator::x_DescribeClones (
    vector<CTempString>& desc,
    string& buf
)

{
    if (m_HTGSUnfinished && m_HTGSPooled && m_has_clone) {
        desc.push_back(", pooled multiple clones");
        return;
    }

    if( m_Clone.empty() ) {
        return;
    }

    SIZE_TYPE count = 1;
    for (SIZE_TYPE pos = m_Clone.find(';'); pos != NPOS;
         pos = m_Clone.find(';', pos + 1)) {
        ++count;
    }
    if (count > 3) {
        buf = NStr::NumericToString(count);
        desc.reserve(3);
        desc.push_back(", ");
        desc.push_back(buf);
        desc.push_back(" clones");
    } else {
        desc.reserve(2);
        desc.push_back(" clone ");
        desc.push_back(m_Clone);
    }
}

static bool x_EndsWithStrain (
    const CTempString& taxname,
    const CTempString& strain
)

{
    // return NStr::EndsWith(taxname, strain, NStr::eNocase);
    if (strain.size() >= taxname.size()) {
        return false;
    }
    SIZE_TYPE pos = taxname.find(' ');
    if (pos == NPOS) {
        return false;
    }
    pos = taxname.find(' ', pos + 1);
    if (pos == NPOS) {
        return false;
    }

    pos = NStr::Find (taxname, strain, NStr::eNocase, NStr::eReverseSearch);
    if (pos == taxname.size() - strain.size()) {
        // check for space to avoid fortuitous match to end of taxname
        char ch = taxname[pos - 1];
        if (ispunct (ch) || isspace (ch)) {
            return true;
        }
    } else if (pos == taxname.size() - strain.size() - 1
               &&  taxname[pos - 1] == '\''
               &&  taxname[taxname.size() - 1] == '\'') {
        return true;
    }
    return false;
}

void CDeflineGenerator::x_SetTitleFromBioSrc (void)

{
    CDefLineJoiner joiner;

    joiner.Add("organism", m_Taxname, eHideType);

    if (! m_Strain.empty()) {
        CTempString add(m_Strain, 0, m_Strain.find(';'));
        if (! x_EndsWithStrain (m_Taxname, add)) {
            joiner.Add("strain", add);
        }
    }
    if (! m_Substrain.empty()) {
        CTempString add(m_Substrain, 0, m_Substrain.find(';'));
        if (! x_EndsWithStrain (m_Taxname, add)) {
            joiner.Add("substr.", add);
        }
    }
    if (! m_Breed.empty()) {
        joiner.Add("breed", m_Breed.substr (0, m_Breed.find(';')));
    }
    if (! m_Cultivar.empty()) {
        joiner.Add("cultivar", m_Cultivar.substr (0, m_Cultivar.find(';')));
    }
    if (! m_Isolate.empty()) {
        // x_EndsWithStrain just checks for supplied pattern, using here for isolate
        if (! x_EndsWithStrain (m_Taxname, m_Isolate)) {
            joiner.Add("isolate", m_Isolate);
        }
    }

    if (! m_Chromosome.empty()) {
        joiner.Add("location", "chromosome", eHideType);
        joiner.Add("chromosome", m_Chromosome, eHideType);
    } else if ( !m_LinkageGroup.empty()) {
        joiner.Add("location", "linkage group", eHideType);
        joiner.Add("linkage group", m_LinkageGroup, eHideType);
    } else if ( !m_Plasmid.empty()) {
        joiner.Add("location", m_Organelle, eHideType); //"plasmid"
        joiner.Add("plasmid name", m_Plasmid, eHideType);
    } else if (! m_Organelle.empty()) {
        joiner.Add("location", m_Organelle, eHideType);
    }
    
    string clnbuf;
    vector<CTempString> clnvec;
    if (m_has_clone) {
        x_DescribeClones (clnvec, clnbuf);
        ITERATE (vector<CTempString>, it, clnvec) {
            joiner.Add("clone", *it, eHideType);
        }
    }
    if (! m_Map.empty()) {
        joiner.Add("map", m_Map);
    }

    joiner.Join(&m_MainTitle);
    NStr::TruncateSpacesInPlace(m_MainTitle);
}

// generate title for NC
void CDeflineGenerator::x_SetTitleFromNC (void)

{
    if (m_MIBiomol != NCBI_BIOMOL(genomic) &&
         m_MIBiomol != NCBI_BIOMOL(other_genetic)) return;

    // require taxname to be set
    if (m_Taxname.empty()) return;

    CDefLineJoiner joiner;

    joiner.Add("organism", m_Taxname, eHideType);

    bool add_gen_tag = false;
    if (NStr::FindNoCase (m_Taxname, "plasmid") != NPOS) {
        //
    } else if (m_IsPlasmid || ! m_Plasmid.empty()) {
        if (m_Plasmid.empty()) {
            joiner.Add("", "unnamed plasmid", eHideType);
        } else {
            if ( !m_IsPlasmid) { // do we need this?
                joiner.Add("location", m_Organelle, eHideType);
            }
            if (NStr::FindNoCase(m_Plasmid, "plasmid") == NPOS  &&
                NStr::FindNoCase(m_Plasmid, "element") == NPOS) {
                joiner.Add("plasmid", m_Plasmid);
            } else {
                joiner.Add("", m_Plasmid, eHideType);
            }
        }
    } else if ( ! m_Organelle.empty() ) {
        if ( m_Chromosome.empty() ) {
            switch (m_Genome) {
                case NCBI_GENOME(mitochondrion):
                case NCBI_GENOME(chloroplast):
                case NCBI_GENOME(kinetoplast):
                case NCBI_GENOME(plastid):
                case NCBI_GENOME(apicoplast):
                    joiner.Add("location", m_Organelle, eHideType);
                    break;
            }
            /*
            if ( m_LinkageGroup.empty() ) {
                add_gen_tag = true;
            }
            */
        } else {
            if (! m_IsChromosome) {
                joiner.Add("location", m_Organelle, eHideType);
            }
            joiner.Add("chromosome", m_Chromosome);
        }
    } else if (! m_Segment.empty()) {
        if (m_Segment.find ("DNA") == NPOS &&
            m_Segment.find ("RNA") == NPOS &&
            m_Segment.find ("segment") == NPOS &&
            m_Segment.find ("Segment") == NPOS) {
            joiner.Add("segment", m_Segment);
        } else {
            joiner.Add("", m_Segment, eHideType);
        }
    } else if (! m_Chromosome.empty()) {
        joiner.Add("chromosome", m_Chromosome);
    } else /* if ( m_LinkageGroup.empty() ) */ {
        add_gen_tag = true;
    }

    if (add_gen_tag) {
        joiner.Add("completeness", (x_IsComplete() ? ", complete genome" : ", genome"), eHideType);
    } else {
        joiner.Add("completeness", (x_IsComplete() ? ", complete sequence" : ", partial sequence"), eHideType);
    }
    joiner.Join(&m_MainTitle);

    NStr::ReplaceInPlace (m_MainTitle, "Plasmid", "plasmid");
    NStr::ReplaceInPlace (m_MainTitle, "Element", "element");
}

// generate title for NM
static void x_FlyCG_PtoR (
    string& s
)

{
    // s =~ s/\b(CG\d*-)P([[:alpha:]])\b/$1R$2/g, more or less.
    SIZE_TYPE pos = 0, len = s.size();
    while (pos + 3 < len && (pos = NStr::FindCase (s, "CG", pos)) != NPOS) {
        if (pos > 0  &&  !isspace((unsigned char)s[pos - 1]) ) {
            pos += 2;
            continue;
        }
        pos += 2;
        while (pos + 3 < len && isdigit((unsigned char)s[pos])) {
            ++pos;
        }
        if (s[pos] == '-'  &&  s[pos + 1] == 'P' &&
            isalpha((unsigned char)s[pos + 2]) &&
            (pos + 3 == len  ||  strchr(" ,;", s[pos + 3])) ) {
            s[pos + 1] = 'R';
        }
    }
}

void CDeflineGenerator::x_SetTitleFromNM (
    const CBioseq_Handle& bsh
)

{
    unsigned int         genes = 0, cdregions = 0, prots = 0;
    CConstRef<CSeq_feat> gene(0);
    CConstRef<CSeq_feat> cdregion(0);

    // require taxname to be set
    if (m_Taxname.empty()) return;

    CScope& scope = bsh.GetScope();

    SAnnotSelector sel;
    sel.SetFeatType(CSeqFeatData::e_Gene);
    sel.IncludeFeatType(CSeqFeatData::e_Cdregion);
    sel.IncludeFeatType(CSeqFeatData::e_Prot);
    sel.SetResolveTSE();

    FOR_SELECTED_SEQFEAT_ON_BIOSEQ_HANDLE (feat_it, bsh, sel) {
        const CSeq_feat& sft = feat_it->GetOriginalFeature();
        SWITCH_ON_FEATURE_CHOICE (sft) {
            case CSeqFeatData::e_Gene:
                ++genes;
                gene.Reset(&sft);
                break;
            case CSeqFeatData::e_Cdregion:
                ++cdregions;
                cdregion.Reset(&sft);
                break;
            case CSeqFeatData::e_Prot:
                ++prots;
                break;
            default:
                break;
        }
    }

    if (genes == 1 && cdregions == 1 && (! m_Taxname.empty())) {
        string cds_label, gene_label;
        CTextJoiner<6, CTempString> joiner;

        feature::GetLabel(*cdregion, &cds_label, feature::fFGL_Content, &scope);
        if (NStr::EqualNocase (m_Taxname, "Drosophila melanogaster")) {
            x_FlyCG_PtoR (cds_label);
        }
        NStr::ReplaceInPlace (cds_label, "isoform ", "transcript variant ");
        feature::GetLabel(*gene, &gene_label, feature::fFGL_Content, &scope);
        joiner.Add(m_Taxname).Add(" ").Add(cds_label).Add(" (")
            .Add(gene_label).Add("), mRNA");
        joiner.Join(&m_MainTitle);
    }
}

// generate title for NR
void CDeflineGenerator::x_SetTitleFromNR (
    const CBioseq_Handle& bsh
)

{
    // require taxname to be set
    if (m_Taxname.empty()) return;

    FOR_EACH_SEQFEAT_ON_BIOSEQ_HANDLE (feat_it, bsh, Gene) {
        const CSeq_feat& sft = feat_it->GetOriginalFeature();
        m_MainTitle = string(m_Taxname) + " ";
        feature::GetLabel(sft, &m_MainTitle, feature::fFGL_Content);
        m_MainTitle += ", ";
        switch (m_MIBiomol) {
            case NCBI_BIOMOL(pre_RNA):
                m_MainTitle += "precursorRNA";
                break;
            case NCBI_BIOMOL(mRNA):
                m_MainTitle += "mRNA";
                break;
            case NCBI_BIOMOL(rRNA):
                m_MainTitle += "rRNA";
                break;
            case NCBI_BIOMOL(tRNA):
                m_MainTitle += "tRNA";
                break;
            case NCBI_BIOMOL(snRNA):
                m_MainTitle += "snRNA";
                break;
            case NCBI_BIOMOL(scRNA):
                m_MainTitle += "scRNA";
                break;
            case NCBI_BIOMOL(cRNA):
                m_MainTitle += "cRNA";
                break;
            case NCBI_BIOMOL(snoRNA):
                m_MainTitle += "snoRNA";
                break;
            case NCBI_BIOMOL(transcribed_RNA):
                m_MainTitle += "miscRNA";
                break;
            case NCBI_BIOMOL(ncRNA):
                m_MainTitle += "ncRNA";
                break;
            case NCBI_BIOMOL(tmRNA):
                m_MainTitle += "tmRNA";
                break;
            default:
                break;
        }

        // take first, then break to skip remainder
        break;
    }
}

// generate title for Patent
void CDeflineGenerator::x_SetTitleFromPatent (void)

{
    string seqno = NStr::IntToString(m_PatentSequence);
    CTextJoiner<6, CTempString> joiner;
    joiner.Add("Sequence ").Add(seqno).Add(" from Patent ")
        .Add(m_PatentCountry).Add(" ").Add(m_PatentNumber);
    joiner.Join(&m_MainTitle);
}

void CDeflineGenerator::x_SetTitleFromPDB (void)

{
    if (! m_PDBChainID.empty()) {
      string chain(m_PDBChainID);
      CTextJoiner<4, CTempString> joiner;
      if (m_UsePDBCompoundForDefline) {
          joiner.Add("Chain ").Add(chain).Add(", ").Add(m_PDBCompound);
      } else {
          std::size_t found = m_Comment.find_first_not_of("0123456789");
          if (found != std::string::npos && found < m_Comment.length() && m_Comment[found] == ' ') {
              joiner.Add("Chain ").Add(chain).Add(", ").Add(m_Comment.substr (found));
          } else {
              joiner.Add("Chain ").Add(chain).Add(", ").Add(m_Comment);
          }
      }
      joiner.Join(&m_MainTitle);
    } else if (isprint ((unsigned char) m_PDBChain)) {
        string chain(1, (char) m_PDBChain);
        CTextJoiner<4, CTempString> joiner;
        if (m_UsePDBCompoundForDefline) {
            joiner.Add("Chain ").Add(chain).Add(", ").Add(m_PDBCompound);
        } else {
            std::size_t found = m_Comment.find_first_not_of("0123456789");
            if (found != std::string::npos && found < m_Comment.length() && m_Comment[found] == ' ') {
                joiner.Add("Chain ").Add(chain).Add(", ").Add(m_Comment.substr (found));
            } else {
                joiner.Add("Chain ").Add(chain).Add(", ").Add(m_Comment);
            }
        }
        joiner.Join(&m_MainTitle);
    } else {
        m_MainTitle = m_PDBCompound;
    }
}

void CDeflineGenerator::x_SetTitleFromGPipe (void)

{
    CDefLineJoiner joiner;

    joiner.Add("organism", m_Taxname, eHideType);

    if ( ! m_Organelle.empty() && NStr::FindNoCase (m_Organelle, "plasmid") != NPOS) {
        joiner.Add("location", m_Organelle, eHideType);
    }

    if (! m_Strain.empty()) {
        CTempString add(m_Strain, 0, m_Strain.find(';'));
        if (! x_EndsWithStrain (m_Taxname, add)) {
            joiner.Add("strain", add);
        }
    }
    if (! m_Strain.empty()) {
        CTempString add(m_Substrain, 0, m_Substrain.find(';'));
        if (! x_EndsWithStrain (m_Taxname, add)) {
            joiner.Add("substr.", add);
        }
    }
    if (! m_Chromosome.empty()) {
        joiner.Add("chromosome", m_Chromosome);
    }
    if (m_has_clone) {
        string clnbuf;
        vector<CTempString> clnvec;
        x_DescribeClones (clnvec, clnbuf);
        ITERATE (vector<CTempString>, it, clnvec) {
            joiner.Add("clone", *it, eHideType);
        }
    }
    if (! m_Map.empty()) {
        joiner.Add("map", m_Map);
    }
    if (! m_Plasmid.empty()) {
        if (NStr::FindNoCase(m_Plasmid, "plasmid") == NPOS  &&
            NStr::FindNoCase(m_Plasmid, "element") == NPOS) {
            joiner.Add("plasmid", m_Plasmid);
        } else {
            joiner.Add("", m_Plasmid);
        }
    }
    
    if (x_IsComplete()) {
        joiner.Add("completeness", ", complete sequence", eHideType);
    }

    joiner.Join(&m_MainTitle);
    NStr::TruncateSpacesInPlace(m_MainTitle);
}

// generate title for protein
CConstRef<CSeq_feat> CDeflineGenerator::x_GetLongestProtein (
    const CBioseq_Handle& bsh
)

{
    TSeqPos               longest = 0;
    CProt_ref::EProcessed bestprocessed = CProt_ref::eProcessed_not_set;
    CProt_ref::EProcessed processed;
    CConstRef<CProt_ref>  prot;
    CConstRef<CSeq_feat>  prot_feat;
    TSeqPos               seq_len = UINT_MAX;

    CScope& scope = bsh.GetScope();

    if (bsh.IsSetInst ()) {
        if (bsh.IsSetInst_Length ()) {
            seq_len = bsh.GetInst_Length ();
        }
    }

    FOR_EACH_SEQFEAT_ON_BIOSEQ_HANDLE (feat_it, bsh, Prot) {
        const CSeq_feat& feat = feat_it->GetOriginalFeature();
        if (! feat.IsSetData ()) continue;
        const CSeqFeatData& sfdata = feat.GetData ();
        const CProt_ref& prp = sfdata.GetProt();
        processed = CProt_ref::eProcessed_not_set;
        if (prp.IsSetProcessed()) {
            processed = prp.GetProcessed();
        }
        if (! feat.IsSetLocation ()) continue;
        const CSeq_loc& loc = feat.GetLocation ();
        TSeqPos prot_length = GetLength (loc, &scope);
        if (prot_length > longest) {
            prot_feat = &feat;
            longest = prot_length;
            bestprocessed = processed;
        } else if (prot_length == longest) {
            // unprocessed 0 preferred over preprotein 1 preferred
            // over mat peptide 2
            if (processed < bestprocessed) {
                prot_feat = &feat;
                longest = prot_length;
                bestprocessed = processed;
            }
        }
    }

    if (longest == seq_len && prot_feat) {
        return prot_feat;
    }

    // confirm that this will automatically check features on
    // parts and segset in pathological segmented protein ???

    if (prot_feat) {
        return prot_feat;
    }

    CSeq_loc everywhere;
    everywhere.SetWhole().Assign(*bsh.GetSeqId());

    prot_feat = GetBestOverlappingFeat (everywhere, CSeqFeatData::e_Prot,
                                        eOverlap_Contained, scope);

    if (prot_feat) {
        return prot_feat;
    }

    return CConstRef<CSeq_feat> ();
}

// m_LocalAnnotsOnly test is unnecessary because feature iterator is built on local features only
//  sqd-4081: it appears that test still does matter. reinstated and even more rigorously applied.
CConstRef<CGene_ref> CDeflineGenerator::x_GetGeneRefViaCDS (
    const CMappedFeat& mapped_cds)

{
    CConstRef<CGene_ref> gene_ref;

    if (mapped_cds) {
        const CSeq_feat& cds_feat = mapped_cds.GetOriginalFeature();
        FOR_EACH_SEQFEATXREF_ON_FEATURE (xf_itr, cds_feat) {
            const CSeqFeatXref& sfx = **xf_itr;
            if (sfx.IsSetData()) {
                const CSeqFeatData& sfd = sfx.GetData();
                if (sfd.IsGene()) {
                    gene_ref = &sfd.GetGene();
                }
            }
        }

        if (gene_ref) {
            return gene_ref;
        }

        if (m_ConstructedFeatTree) {
            if (! m_InitializedFeatTree) {
                CFeat_CI iter (m_TopSEH);
                m_Feat_Tree.Reset (new CFeatTree (iter));
                m_InitializedFeatTree = true;
            }
        }
        if (m_Feat_Tree.Empty ()) {
            m_Feat_Tree.Reset (new CFeatTree);
        }
        if (! m_ConstructedFeatTree) {
            m_Feat_Tree->AddGenesForCds (mapped_cds);
        }

        try {
            CMappedFeat mapped_gene = GetBestGeneForCds (mapped_cds, m_Feat_Tree);
            if (mapped_gene) {
                const CSeq_feat& gene_feat = mapped_gene.GetOriginalFeature();
                gene_ref = &gene_feat.GetData().GetGene();
            }
        } catch ( const exception&  ) {
            // ERR_POST(Error << "x_GetGeneRefViaCDS GetBestGeneForCds failure");
        }

        // clearing m_InitializedFeatTree may remove artifact after first protein is indexed and second protein is requested
        if (m_ConstructedFeatTree) {
            m_InitializedFeatTree = false;
        }
    }

    return gene_ref;
}

static CConstRef<CBioSource> x_GetSourceFeatViaCDS  (
    const CBioseq_Handle& bsh
)

{
    CConstRef<CSeq_feat>   cds_feat;
    CConstRef<CSeq_loc>    cds_loc;
    CConstRef<CBioSource>  src_ref;

    CScope& scope = bsh.GetScope();

    cds_feat = GetCDSForProduct (bsh);

    if (cds_feat) {
        /*
        const CSeq_feat& feat = *cds_feat;
        */
        cds_loc = &cds_feat->GetLocation();
        if (cds_loc) {
            CRef<CSeq_loc> cleaned_location( new CSeq_loc );
            cleaned_location->Assign( *cds_loc );
            CConstRef<CSeq_feat> src_feat
                = GetBestOverlappingFeat (*cleaned_location, CSeqFeatData::eSubtype_biosrc, eOverlap_SubsetRev, scope);
            if (! src_feat  &&  cleaned_location->IsSetStrand()  &&  IsReverse(cleaned_location->GetStrand())) {
                CRef<CSeq_loc> rev_loc(SeqLocRevCmpl(*cleaned_location, &scope));
                cleaned_location->Assign(*rev_loc);
                src_feat = GetBestOverlappingFeat (*cleaned_location, CSeqFeatData::eSubtype_biosrc, eOverlap_SubsetRev, scope);
            }
            if (src_feat) {
                const CSeq_feat& feat = *src_feat;
                if (feat.IsSetData()) {
                    const CSeqFeatData& sfd = feat.GetData();
                    if (sfd.IsBiosrc()) {
                        src_ref = &sfd.GetBiosrc();
                    }
                }
            }
        }
    }

    if (src_ref) {
        return src_ref;
    }

    return CConstRef<CBioSource> ();
}

bool CDeflineGenerator::x_CDShasLowQualityException (
    const CSeq_feat& sft
)

{
    if (! FEATURE_CHOICE_IS (sft, NCBI_SEQFEAT(Cdregion))) return false;
    if (! sft.IsSetExcept()) return false;
    if (! sft.GetExcept()) return false;
    if (! sft.IsSetExcept_text()) return false;

    const string& str = sft.GetExcept_text();
    int current_state = 0;
    FOR_EACH_CHAR_IN_STRING (str_itr, str) {
        const char ch = *str_itr;
        int next_state = ms_p_Low_Quality_Fsa->GetNextState (current_state, ch);
        if (ms_p_Low_Quality_Fsa->IsMatchFound (next_state)) {
            return true;
        }
        current_state = next_state;
    }


    return false;
}

static const char* s_proteinOrganellePrefix [] = {
  "",                  // "",
  "",                  // "",
  "chloroplast",       // "chloroplast",
  "chromoplast",       // "chromoplast",
  "kinetoplast",       // "kinetoplast",
  "mitochondrion",     // "mitochondrion",
  "plastid",           // "plastid",
  "macronuclear",      // "macronuclear",
  "",                  // "extrachromosomal",
  "plasmid",           // "plasmid",
  "",                  // "",
  "",                  // "",
  "cyanelle",          // "cyanelle",
  "",                  // "proviral",
  "",                  // "virus",
  "nucleomorph",       // "nucleomorph",
  "apicoplast",        // "apicoplast",
  "leucoplast",        // "leucoplast",
  "protoplast",        // "protoplast",
  "endogenous virus",  // "endogenous virus",
  "hydrogenosome",     // "hydrogenosome",
  "",                  // "chromosome",
  "chromatophore"      // "chromatophore"
};

static string s_RemoveBracketedOrgFromEnd (string str, string taxname)

{
    string final;
    if (str.empty()) return str;
    if (taxname.empty()) return str;
    SIZE_TYPE taxlen = taxname.length();
    int len = str.length();
    if (len < 5) return str;
    if (str [len - 1] != ']') return str;
    SIZE_TYPE cp = NStr::Find(str, "[", NStr::eNocase, NStr::eReverseSearch);
    if (cp == NPOS) return str;
    string suffix = str.substr(cp+1);
    if (NStr::StartsWith(suffix, "NAD")) return str;
    if (suffix.length() != taxlen + 1) return str;
    if (NStr::StartsWith(suffix, taxname)) {
        str.erase (cp);
        x_CleanAndCompress(final, str, true);
        return final;

    }
    return str;
}

void CDeflineGenerator::x_SetTitleFromProteinIdx (
    const CBioseq_Handle& bsh
)

{
    CConstRef<CSeq_feat>  cds_feat;
    CConstRef<CProt_ref>  prot;
    CConstRef<CSeq_feat>  prot_feat;
    CConstRef<CGene_ref>  gene;
    CConstRef<CBioSource> src;
    CTempString           locus_tag;

    CRef<CBioseqIndex> bsx = m_Idx->GetBioseqIndex (bsh);
    if (! bsx) {
        return;
    }

    CRef<CFeatureIndex> prtx = bsx->GetBestProteinFeature();

    CRef<CFeatureIndex> sfxp = bsx->GetFeatureForProduct();

    if (prtx) {
        const CMappedFeat mf = prtx->GetMappedFeat();
        const CProt_ref& prp = mf.GetData().GetProt();

        const char* prefix = "";
        FOR_EACH_NAME_ON_PROT (prp_itr, prp) {
            const string& str = *prp_itr;
            string trimmed = s_RemoveBracketedOrgFromEnd (str, m_Taxname);
            m_MainTitle += prefix;
            m_MainTitle += trimmed;
            if (! m_AllProtNames) {
                break;
            }
            prefix = "; ";
        }

        if (! m_MainTitle.empty()) {
            // strip trailing periods, commas, and spaces
            SIZE_TYPE pos = m_MainTitle.find_last_not_of (".,;~ ");
            if (pos != NPOS) {
                m_MainTitle.erase (pos + 1);
            }

            int offset = 0;
            int delta = 0;
            string comma = "";
            string isoform = "";
            if (NStr::StartsWith (m_MainTitle, "hypothetical protein")) {
                offset = 20;
            } else if (NStr::StartsWith (m_MainTitle, "uncharacterized protein")) {
                offset = 23;
            }
            if (offset > 0 && offset < m_MainTitle.length()) {
                if (m_MainTitle [offset] == ',' && m_MainTitle [offset + 1] == ' ') {
                    comma = ", isoform ";
                    delta = 2;
                }
                if (m_MainTitle [offset] == ' ') {
                    comma = " isoform ";
                    delta = 1;
                }
                if (NStr::StartsWith (m_MainTitle.substr (offset + delta), "isoform ")) {
                    isoform = m_MainTitle.substr (offset + delta + 8);
                    // !!! check for single alphanumeric string
                    m_MainTitle.erase (offset);
                }
            }
            if ((NStr::EqualNocase (m_MainTitle, "hypothetical protein")  ||
                 NStr::EqualNocase (m_MainTitle, "uncharacterized protein"))
                /* &&  !m_LocalAnnotsOnly */ ) {
                if (sfxp) {
                    CRef<CFeatureIndex> fsx = sfxp->GetBestGene();
                    if (fsx) {
                        const CGene_ref& grp = fsx->GetMappedFeat().GetData().GetGene();
                        if (grp.IsSetLocus_tag()) {
                            locus_tag = grp.GetLocus_tag();
                        }
                    }
                }
                if (! locus_tag.empty()) {
                    m_MainTitle += " " + string(locus_tag) + string(comma) + string(isoform);
                }
            }
        }
        if (m_MainTitle.empty()  &&  !m_LocalAnnotsOnly) {
            if (prp.IsSetDesc()) {
                m_MainTitle = prp.GetDesc();
            }
        }
        if (m_MainTitle.empty()  &&  !m_LocalAnnotsOnly) {
            FOR_EACH_ACTIVITY_ON_PROT (act_itr, prp) {
                const string& str = *act_itr;
                m_MainTitle = str;
                break;
            }
        }
    }

    if (m_MainTitle.empty()  &&  !m_LocalAnnotsOnly) {
        if (sfxp) {
            CRef<CFeatureIndex> fsx = sfxp->GetBestGene();
            if (fsx) {
                const CGene_ref& grp = fsx->GetMappedFeat().GetData().GetGene();
                if (grp.IsSetLocus()) {
                    m_MainTitle = grp.GetLocus();
                }
                if (m_MainTitle.empty()) {
                    FOR_EACH_SYNONYM_ON_GENE (syn_itr, grp) {
                        const string& str = *syn_itr;
                        m_MainTitle = str;
                        break;
                    }
                }
                if (m_MainTitle.empty()) {
                    if (grp.IsSetDesc()) {
                        m_MainTitle = grp.GetDesc();
                    }
                }
            }
        }
        if (! m_MainTitle.empty()) {
            m_MainTitle += " gene product";
        }
    }

    if (m_MainTitle.empty()  &&  !m_LocalAnnotsOnly) {
        m_MainTitle = "unnamed protein product";
        if (sfxp) {
            CRef<CFeatureIndex> fsx = sfxp->GetBestGene();
            if (fsx) {
                const CGene_ref& grp = fsx->GetMappedFeat().GetData().GetGene();
                if (grp.IsSetLocus_tag()) {
                    locus_tag = grp.GetLocus_tag();
                }
            }
        }
        if (! locus_tag.empty()) {
            m_MainTitle += " " + string(locus_tag);
        }
    }

    if (sfxp) {
        const CMappedFeat mf = sfxp->GetMappedFeat();
        const CSeq_feat& cds = mf.GetOriginalFeature();
        if (x_CDShasLowQualityException (cds)) {
          const string& low_qual = "LOW QUALITY PROTEIN: ";
          if (NStr::FindNoCase (m_MainTitle, low_qual, 0) == NPOS) {
              string tmp = m_MainTitle;
              m_MainTitle = low_qual + tmp;
          }
        }
    }

    // strip trailing periods, commas, and spaces
    SIZE_TYPE pos = m_MainTitle.find_last_not_of (".,;~ ");
    if (pos != NPOS) {
        m_MainTitle.erase (pos + 1);
    }

    if (! x_IsComplete() /* && m_MainTitle.find(", partial") == NPOS */) {
        m_MainTitle += ", partial";
    }

    if (m_OmitTaxonomicName) return;

    CTempString taxname = m_Taxname;

    if (m_Genome >= NCBI_GENOME(chloroplast) && m_Genome <= NCBI_GENOME(chromatophore)) {
        const char * organelle = s_proteinOrganellePrefix [m_Genome];
        if ( organelle[0] != '\0'  &&  ! taxname.empty()
            /* &&  NStr::Find (taxname, organelle) == NPOS */) {
            m_MainTitle += " (";
            m_MainTitle += organelle;
            m_MainTitle += ")";
        }
    }

    // check for special taxname, go to overlapping source feature
    if ((taxname.empty()  ||
         (!NStr::EqualNocase (taxname, "synthetic construct")  &&
          !NStr::EqualNocase (taxname, "artificial sequence")  &&
          taxname.find ("vector") == NPOS  &&
          taxname.find ("Vector") == NPOS))  &&
        !m_LocalAnnotsOnly) {
        CWeakRef<CBioseqIndex> bsxp = bsx->GetBioseqForProduct();
        auto nucx = bsxp.Lock();
        if (nucx) {
            if (nucx->HasSource()) {
                src = x_GetSourceFeatViaCDS (bsh);
                if (src.NotEmpty()  &&  src->IsSetTaxname()) {
                    taxname = src->GetTaxname();
                }
            }
        }
    }

    if (m_IsCrossKingdom && ! m_FirstSuperKingdom.empty() && ! m_SecondSuperKingdom.empty()) {
        m_MainTitle += " [" + string(m_FirstSuperKingdom) + "][" + string(m_SecondSuperKingdom) + "]";
    } else if (! taxname.empty() /* && m_MainTitle.find(taxname) == NPOS */) {
        m_MainTitle += " [" + string(taxname) + "]";
    }
}

void CDeflineGenerator::x_SetTitleFromProtein (
    const CBioseq_Handle& bsh
)

{
    CConstRef<CSeq_feat>  cds_feat;
    CConstRef<CProt_ref>  prot;
    CConstRef<CSeq_feat>  prot_feat;
    CConstRef<CGene_ref>  gene;
    CConstRef<CBioSource> src;
    CTempString           locus_tag;

    // gets longest protein on Bioseq, parts set, or seg set, even if not
    // full-length

    prot_feat = x_GetLongestProtein (bsh);

    if (prot_feat) {
        prot = &prot_feat->GetData().GetProt();
    }

    const CMappedFeat& mapped_cds = GetMappedCDSForProduct (bsh);

    if (prot) {
        const CProt_ref& prp = *prot;
        const char* prefix = "";
        FOR_EACH_NAME_ON_PROT (prp_itr, prp) {
            const string& str = *prp_itr;
            string trimmed = s_RemoveBracketedOrgFromEnd (str, m_Taxname);
            m_MainTitle += prefix;
            m_MainTitle += trimmed;
            if (! m_AllProtNames) {
                break;
            }
            prefix = "; ";
        }

        if (! m_MainTitle.empty()) {
            // strip trailing periods, commas, and spaces
            SIZE_TYPE pos = m_MainTitle.find_last_not_of (".,;~ ");
            if (pos != NPOS) {
                m_MainTitle.erase (pos + 1);
            }

            int offset = 0;
            int delta = 0;
            string comma = "";
            string isoform = "";
            if (NStr::StartsWith (m_MainTitle, "hypothetical protein")) {
                offset = 20;
            } else if (NStr::StartsWith (m_MainTitle, "uncharacterized protein")) {
                offset = 23;
            }
            if (offset > 0 && offset < m_MainTitle.length()) {
                if (m_MainTitle [offset] == ',' && m_MainTitle [offset + 1] == ' ') {
                    comma = ", isoform ";
                    delta = 2;
                }
                if (m_MainTitle [offset] == ' ') {
                    comma = " isoform ";
                    delta = 1;
                }
                if (NStr::StartsWith (m_MainTitle.substr (offset + delta), "isoform ")) {
                    isoform = m_MainTitle.substr (offset + delta + 8);
                    // !!! check for single alphanumeric string
                    m_MainTitle.erase (offset);
                }
            }
            if ((NStr::EqualNocase (m_MainTitle, "hypothetical protein")  ||
                 NStr::EqualNocase (m_MainTitle, "uncharacterized protein"))
                /* &&  !m_LocalAnnotsOnly */ ) {
                gene = x_GetGeneRefViaCDS (mapped_cds);
                if (gene) {
                    const CGene_ref& grp = *gene;
                    if (grp.IsSetLocus_tag()) {
                        locus_tag = grp.GetLocus_tag();
                    }
                }
                if (! locus_tag.empty()) {
                    m_MainTitle += " " + string(locus_tag) + string(comma) + string(isoform);
                }
            }
        }
        if (m_MainTitle.empty()  &&  !m_LocalAnnotsOnly) {
            if (prp.IsSetDesc()) {
                m_MainTitle = prp.GetDesc();
            }
        }
        if (m_MainTitle.empty()  &&  !m_LocalAnnotsOnly) {
            FOR_EACH_ACTIVITY_ON_PROT (act_itr, prp) {
                const string& str = *act_itr;
                m_MainTitle = str;
                break;
            }
        }
    }

    if (m_MainTitle.empty()  &&  !m_LocalAnnotsOnly) {
        gene = x_GetGeneRefViaCDS (mapped_cds);
        if (gene) {
            const CGene_ref& grp = *gene;
            if (grp.IsSetLocus()) {
                m_MainTitle = grp.GetLocus();
            }
            if (m_MainTitle.empty()) {
                FOR_EACH_SYNONYM_ON_GENE (syn_itr, grp) {
                    const string& str = *syn_itr;
                    m_MainTitle = str;
                    break;
                }
            }
            if (m_MainTitle.empty()) {
                if (grp.IsSetDesc()) {
                    m_MainTitle = grp.GetDesc();
                }
            }
        }
        if (! m_MainTitle.empty()) {
            m_MainTitle += " gene product";
        }
    }

    if (m_MainTitle.empty()  &&  !m_LocalAnnotsOnly) {
        m_MainTitle = "unnamed protein product";
        gene = x_GetGeneRefViaCDS (mapped_cds);
        if (gene) {
            const CGene_ref& grp = *gene;
            if (grp.IsSetLocus_tag()) {
                locus_tag = grp.GetLocus_tag();
            }
        }
        if (! locus_tag.empty()) {
            m_MainTitle += " " + string(locus_tag);
        }
    }

    if (mapped_cds) {
        const CSeq_feat& cds = mapped_cds.GetOriginalFeature();
        if (x_CDShasLowQualityException (cds)) {
          const string& low_qual = "LOW QUALITY PROTEIN: ";
          if (NStr::FindNoCase (m_MainTitle, low_qual, 0) == NPOS) {
              string tmp = m_MainTitle;
              m_MainTitle = low_qual + tmp;
          }
        }
    }

    // strip trailing periods, commas, and spaces
    SIZE_TYPE pos = m_MainTitle.find_last_not_of (".,;~ ");
    if (pos != NPOS) {
        m_MainTitle.erase (pos + 1);
    }

    if (! x_IsComplete() /* && m_MainTitle.find(", partial") == NPOS */) {
        m_MainTitle += ", partial";
    }

    if (m_OmitTaxonomicName) return;

    CTempString taxname = m_Taxname;

    if (m_Genome >= NCBI_GENOME(chloroplast) && m_Genome <= NCBI_GENOME(chromatophore)) {
        const char * organelle = s_proteinOrganellePrefix [m_Genome];
        if ( organelle[0] != '\0'  &&  ! taxname.empty()
            /* &&  NStr::Find (taxname, organelle) == NPOS */) {
            m_MainTitle += " (";
            m_MainTitle += organelle;
            m_MainTitle += ")";
        }
    }

    // check for special taxname, go to overlapping source feature
    if ((taxname.empty()  ||
         (!NStr::EqualNocase (taxname, "synthetic construct")  &&
          !NStr::EqualNocase (taxname, "artificial sequence")  &&
          taxname.find ("vector") == NPOS  &&
          taxname.find ("Vector") == NPOS))  &&
        !m_LocalAnnotsOnly) {
        src = x_GetSourceFeatViaCDS (bsh);
        if (src.NotEmpty()  &&  src->IsSetTaxname()) {
            taxname = src->GetTaxname();
        }
    }

    if (m_IsCrossKingdom && ! m_FirstSuperKingdom.empty() && ! m_SecondSuperKingdom.empty()) {
        m_MainTitle += " [" + string(m_FirstSuperKingdom) + "][" + string(m_SecondSuperKingdom) + "]";
    } else if (! taxname.empty() /* && m_MainTitle.find(taxname) == NPOS */) {
        m_MainTitle += " [" + string(taxname) + "]";
    }
}

// generate title for segmented sequence
static bool x_GetSegSeqInfoViaCDS (
    string& locus,
    string& product,
    const char*& completeness,
    const CBioseq_Handle& bsh
)

{
    CScope& scope = bsh.GetScope();

    // check C toolkit code to understand what is happening here ???

    CSeq_loc everywhere;
    everywhere.SetMix().Set() = bsh.GetInst_Ext().GetSeg();

    FOR_EACH_SEQFEAT_ON_SCOPE (it, scope, everywhere, Cdregion) {
        const CSeq_feat& cds = it->GetOriginalFeature();
        if (! cds.IsSetLocation ()) continue;
        const CSeq_loc& cds_loc = cds.GetLocation();

        GetLabel (cds, &product, feature::fFGL_Content, &scope);

        if (cds.IsSetPartial()) {
            completeness = "partial";
        }

        FOR_EACH_SEQFEATXREF_ON_SEQFEAT (xr_itr, cds) {
            const CSeqFeatXref& sfx = **xr_itr;
            if (! FIELD_IS_SET (sfx, Data)) continue;
            const CSeqFeatData& sfd = GET_FIELD (sfx, Data);
            if (! FIELD_IS (sfd, Gene)) continue;
            const CGene_ref& gr = GET_FIELD (sfd, Gene);
            if (FIELD_IS_SET (gr, Locus)) {
                locus = GET_FIELD (gr, Locus);
            } else {
                FOR_EACH_SYNONYM_ON_GENEREF (syn_itr, gr) {
                    locus = *syn_itr;
                    // take first, then break to skip remainder
                    break;
                }
            }
        }

        if (locus.empty()) {
            CConstRef<CSeq_feat> gene_feat
                = GetBestOverlappingFeat(cds_loc,
                                         CSeqFeatData::eSubtype_gene,
                                         eOverlap_Contained,
                                         scope);
            if (gene_feat.NotEmpty()) {
                const CSeq_feat& gene = *gene_feat;
                GetLabel (gene, &locus, feature::fFGL_Content, &scope);
                /*
                if (gene_feat->GetData().GetGene().IsSetLocus()) {
                    locus = gene_feat->GetData().GetGene().GetLocus();
                } else if (gene_feat->GetData().GetGene().IsSetSyn()) {
                    locus = *gene_feat->GetData().GetGene().GetSyn().begin();
                }
                */
            }
        }

        return true;
    }

    return false;
}

void CDeflineGenerator::x_SetTitleFromSegSeq  (
    const CBioseq_Handle& bsh
)

{
    const char * completeness = "complete";
    bool         cds_found    = false;
    string       locus, product;
    CDefLineJoiner joiner;

    if (m_Taxname.empty()) {
        m_Taxname = "Unknown";
    }
    joiner.Add("organism", m_Taxname, eHideType);

    if ( !m_LocalAnnotsOnly ) {
        cds_found = x_GetSegSeqInfoViaCDS(locus, product, completeness, bsh);
    }
    if ( !cds_found) {
        if (! m_Strain.empty()
            &&  ! x_EndsWithStrain (m_Taxname, m_Strain) ) {
            joiner.Add("strain", m_Strain);
        } else if (! m_Clone.empty()
                   /* && m_Clone.find(" clone ") != NPOS */) {
            string clnbuf;
            vector<CTempString> clnvec;
            x_DescribeClones (clnvec, clnbuf);
            ITERATE (vector<CTempString>, it, clnvec) {
                joiner.Add("clone", *it, eHideType);
            }
        } else if (! m_Isolate.empty() ) {
            joiner.Add("isolate", m_Isolate);
        }
    }
    if (! product.empty()) {
        joiner.Add("product", product, eHideType);
    }
    joiner.Join(&m_MainTitle);
    if (! locus.empty()) {
        m_MainTitle += " (" + locus + ")";
    }
    if ((! product.empty()) || (! locus.empty())) {
        m_MainTitle += " gene, " + string(completeness) + " cds";
    }
    NStr::TruncateSpacesInPlace(m_MainTitle);
}

// generate title for TSA or non-master WGS
void CDeflineGenerator::x_SetTitleFromWGS (void)

{
    CDefLineJoiner joiner;

    joiner.Add("organism", m_Taxname, eHideType);

    if (! m_Strain.empty()) {
        if (! x_EndsWithStrain (m_Taxname, m_Strain)) {
            joiner.Add("strain", m_Strain.substr (0, m_Strain.find(';')));
        }
        if (! m_Substrain.empty() && ! x_EndsWithStrain (m_Taxname, m_Substrain)) {
            joiner.Add("substr.", m_Substrain.substr (0, m_Substrain.find(';')));
        }
    } else if (! m_Breed.empty()) {
        joiner.Add("breed", m_Breed.substr (0, m_Breed.find(';')));
    } else if (! m_Cultivar.empty()) {
        joiner.Add("cultivar", m_Cultivar.substr (0, m_Cultivar.find(';')));
    }
    if (! m_Isolate.empty()) {
        // x_EndsWithStrain just checks for supplied pattern, using here for isolate
        if (! x_EndsWithStrain (m_Taxname, m_Isolate)) {
            joiner.Add("isolate", m_Isolate);
        }
    }
    if (! m_Chromosome.empty()) {
        joiner.Add("chromosome", m_Chromosome);
    } else if ( !m_LinkageGroup.empty()) {
        joiner.Add("linkage group", m_LinkageGroup);
    }
    if (! m_Clone.empty()) {
        string clnbuf;
        vector<CTempString> clnvec;
        x_DescribeClones (clnvec, clnbuf);
        ITERATE (vector<CTempString>, it, clnvec) {
            joiner.Add("clone", *it, eHideType);
        }
    }
    if (! m_Map.empty()) {
        joiner.Add("map", m_Map);
    }
    if (! m_Plasmid.empty()) {
        if (m_IsWGS) {
            joiner.Add("plasmid", m_Plasmid);
        }
    }
    // string tmp needs to be in scope for final joiner.Join statement
    string tmp;
    if (m_Genome == NCBI_GENOME(plasmid) && m_Topology == NCBI_SEQTOPOLOGY(circular)) {
    } else if (m_Genome == NCBI_GENOME(chromosome)) {
    } else if (! m_GeneralStr.empty()) {
        if (m_GeneralStr != m_Chromosome  &&  (! m_IsWGS  ||  m_GeneralStr != m_Plasmid)) {
            joiner.Add("", m_GeneralStr, eHideType);
        }
    } else if (m_GeneralId > 0) {
        tmp = NStr::NumericToString (m_GeneralId);
        if (! tmp.empty()) {
            if (tmp != m_Chromosome  &&  (! m_IsWGS  ||  tmp != m_Plasmid)) {
                joiner.Add("", tmp, eHideType);
            }
        }
    }

    joiner.Join(&m_MainTitle);
    NStr::TruncateSpacesInPlace(m_MainTitle);
}

// generate title for optical map
void CDeflineGenerator::x_SetTitleFromMap (void)

{
    CDefLineJoiner joiner;

    joiner.Add("organism", m_Taxname, eHideType);

    if (! m_Strain.empty()) {
        if (! x_EndsWithStrain (m_Taxname, m_Strain)) {
            joiner.Add("strain", m_Strain.substr (0, m_Strain.find(';')));
        }
    }
    if (! m_Substrain.empty()) {
        if (! x_EndsWithStrain (m_Taxname, m_Substrain)) {
            joiner.Add("substr.", m_Substrain.substr (0, m_Substrain.find(';')));
        }
    }
    if (! m_Chromosome.empty()) {
        joiner.Add("chromosome", m_Chromosome);
    } else if (m_IsChromosome) {
        joiner.Add("location", "chromosome", eHideType);
    }
    if (! m_Plasmid.empty()) {
        joiner.Add("plasmid", m_Plasmid);
    } else if (m_IsPlasmid) {
        joiner.Add("location", "plasmid", eHideType);
    }
    if (! m_Isolate.empty()) {
        joiner.Add("isolate", m_Isolate);
    }
    joiner.Join(&m_MainTitle);

    if (! m_rEnzyme.empty()) {
        m_MainTitle += ", " + m_rEnzyme + " whole genome map";
    }

    NStr::TruncateSpacesInPlace(m_MainTitle);
}

// generate TPA or TSA prefix
void CDeflineGenerator::x_SetPrefix (
    string& prefix
)

{
    prefix = kEmptyCStr;

    if (m_IsUnverified) {
        if (m_MainTitle.find ("UNVERIFIED") == NPOS) {
            prefix = m_UnverifiedPrefix;
        }
    } else if (m_ThirdParty) {
        if (m_TPAExp) {
            prefix = "TPA_exp: ";
        } else if (m_TPAInf) {
            prefix = "TPA_inf: ";
        } else if (m_TPAReasm) {
            prefix = "TPA_asm: ";
        } else {
            prefix = "TPA: ";
        }
    } else if (m_IsTSA) {
        prefix = "TSA: ";
    } else if (m_IsTLS) {
        prefix = "TLS: ";
    } else if (m_Multispecies && m_IsWP) {
        prefix = "MULTISPECIES: ";
    } else if (m_IsPseudogene) {
        if (m_MainTitle.find ("PUTATIVE PSEUDOGENE") == NPOS) {
            prefix = "PUTATIVE PSEUDOGENE: ";
        }
    }
}

// generate suffix if not already present
void CDeflineGenerator::x_SetSuffix (
    string& suffix,
    const CBioseq_Handle& bsh,
    bool appendComplete
)

{
    string type;
    string study;
    string comp;

    switch (m_MITech) {
        case NCBI_TECH(htgs_0):
            if (m_MainTitle.find ("LOW-PASS") == NPOS) {
                type = ", LOW-PASS SEQUENCE SAMPLING";
            }
            break;
        case NCBI_TECH(htgs_1):
        case NCBI_TECH(htgs_2):
        {
            if (m_HTGSDraft) {
                if (m_MainTitle.find ("WORKING DRAFT") == NPOS) {
                    type = ", WORKING DRAFT SEQUENCE";
                }
            } else if (!m_HTGSCancelled) {
                if (m_MainTitle.find ("SEQUENCING IN") == NPOS) {
                    type = ", *** SEQUENCING IN PROGRESS ***";
                }
            }

            string un;
            if (m_MITech == NCBI_TECH(htgs_1)) {
                un = "un";
            }
            if (m_IsDelta) {
                unsigned int pieces = 1;
                for (CSeqMap_CI it (bsh, CSeqMap::fFindGap); it; ++it) {
                    ++pieces;
                }
                if (pieces == 1) {
                    // type += (", 1 " + un + "ordered piece");
                } else {
                    type += (", " + NStr::IntToString (pieces)
                               + " " + un + "ordered pieces");
                }
            } else {
                // type += ", in " + un + "ordered pieces";
            }
            break;
        }
        case NCBI_TECH(htgs_3):
            if (m_MainTitle.find ("complete sequence") == NPOS) {
                type = ", complete sequence";
            }
            break;
        case NCBI_TECH(est):
            if (m_MainTitle.find ("mRNA sequence") == NPOS) {
                type = ", mRNA sequence";
            }
            break;
        case NCBI_TECH(sts):
            if (m_MainTitle.find ("sequence tagged site") == NPOS) {
                type = ", sequence tagged site";
            }
            break;
        case NCBI_TECH(survey):
            if (m_MainTitle.find ("genomic survey sequence") == NPOS) {
                type = ", genomic survey sequence";
            }
            break;
        case NCBI_TECH(wgs):
            if (m_WGSMaster) {
                if (m_MainTitle.find ("whole genome shotgun sequencing project")
                    == NPOS){
                    type = ", whole genome shotgun sequencing project";
                }
            } else if (m_MainTitle.find ("whole genome shotgun sequence")
                       == NPOS) {
                if (! m_Organelle.empty()  &&  m_MainTitle.find(m_Organelle) == NPOS) {
                    if ((NStr::EqualNocase (m_Organelle, "mitochondrial") || NStr::EqualNocase (m_Organelle, "mitochondrion")) &&
                        (m_MainTitle.find("mitochondrial") != NPOS || m_MainTitle.find("mitochondrion") != NPOS)) {
                    } else if (NStr::EqualNocase (m_Organelle, "chromosome") &&
                        (m_MainTitle.find("linkage group") != NPOS || m_MainTitle.find("chromosome") != NPOS)) {
                    } else {
                      type = " ";
                      type += m_Organelle;
                    }
                }
                type += ", whole genome shotgun sequence";
            }
            break;
        case NCBI_TECH(tsa):
            if (m_TSAMaster) {
                if (m_MainTitle.find
                    ("transcriptome shotgun assembly")
                    == NPOS) {
                    type = ", transcriptome shotgun assembly";
                }
            } else {
                if (m_MainTitle.find ("RNA sequence") == NPOS) {
                    switch (m_MIBiomol) {
                        case NCBI_BIOMOL(mRNA):
                            type = ", mRNA sequence";
                            break;
                        case NCBI_BIOMOL(rRNA):
                            type = ", rRNA sequence";
                            break;
                        case NCBI_BIOMOL(ncRNA):
                            type = ", ncRNA sequence";
                            break;
                        case NCBI_BIOMOL(pre_RNA):
                        case NCBI_BIOMOL(snRNA):
                        case NCBI_BIOMOL(scRNA):
                        case NCBI_BIOMOL(cRNA):
                        case NCBI_BIOMOL(snoRNA):
                        case NCBI_BIOMOL(transcribed_RNA):
                            type = ", transcribed RNA sequence";
                            break;
                        default:
                            break;
                    }
                }
            }
            break;
        case NCBI_TECH(targeted):
            if (m_TLSMaster) {
                if (m_MainTitle.find ("targeted locus study") == NPOS) {
                    type = ", targeted locus study";
                }
            } else {
                if (m_MainTitle.find ("sequence") == NPOS) {
                   type += ", sequence";
                }
            }
            if (! m_TargetedLocus.empty() && m_MainTitle.find (m_TargetedLocus) == NPOS) {
                study = m_TargetedLocus;
            }
            break;
        default:
            break;
    }

    if (appendComplete && m_MainTitle.find ("complete") == NPOS && m_MainTitle.find ("partial") == NPOS) {
        if (m_MICompleteness == NCBI_COMPLETENESS(complete)) {
            if (m_IsPlasmid) {
                comp = ", complete sequence";
            } else if (m_Genome == NCBI_GENOME(mitochondrion) ||
                       m_Genome == NCBI_GENOME(chloroplast) ||
                       m_Genome == NCBI_GENOME(kinetoplast) ||
                       m_Genome == NCBI_GENOME(plastid) ||
                       m_Genome == NCBI_GENOME(apicoplast)) {
                comp = ", complete genome";
            } else if (m_IsChromosome) {
                if (! m_Chromosome.empty()) {
                    comp = ", complete sequence";
                } else if (! m_LinkageGroup.empty()) {
                    comp = ", complete sequence";
                } else {
                    comp = ", complete genome";
                }
            }
        }
    }

    if (m_Unordered && m_IsDelta) {
        unsigned int num_gaps = 0;
        for (CSeqMap_CI it (bsh, CSeqMap::fFindGap); it; ++it) {
            ++num_gaps;
        }
        if (num_gaps > 0) {
            type += (", " + NStr::IntToString (num_gaps + 1)
                       + " unordered pieces");
        }
    }

    if (! study.empty()) {
        suffix = " " + study + " " + type + comp;
    } else {
        suffix = type + comp;
    }
}

static inline void s_TrimMainTitle (string& str)
{
    size_t pos = str.find_last_not_of (".,;~ ");
    if (pos != NPOS) {
        str.erase (pos + 1);
    }
}

/*
// Strips all spaces in string in following manner. If the function
// meets several spaces (spaces and tabs) in succession it replaces them
// with one space. Strips all spaces after '(' and before ( ')' or ',' ).
// (Code adapted from BasicCleanup.)
static void x_CompressRunsOfSpaces (string& str)
{
    if (str.empty()) {
        return;
    }

    string::iterator end = str.end();
    string::iterator it = str.begin();
    string::iterator new_str = it;
    while (it != end) {
        *new_str++ = *it;
        if ( (*it == ' ')  ||  (*it == '\t')  ||  (*it == '(') ) {
            for (++it; (it != end) && (*it == ' ' || *it == '\t'); ++it) continue;
            if ((it != end) && (*it == ')' || *it == ',') ) {
                // this "if" protects against the case "(...bunch of spaces and tabs...)".
                // Otherwise, the first '(' is unintentionally erased
                if( *(new_str - 1) != '(' ) { 
                    --new_str;
                }
            }
        } else {
            ++it;
        }
    }
    str.erase(new_str, str.end());
}
*/

static size_t s_TitleEndsInOrganism (
    string& title,
    CTempString taxname
)

{
    size_t  pos;
    int     len1, len2, idx;

    len1 = title.length();
    len2 = taxname.length();

    idx = len1 - len2 - 3;
    if (len1 > len2 + 4 && title [idx] == ' ' && title [idx + 1] == '[' && title [len1 - 1] == ']') {
        pos = NStr::Find(title, taxname, NStr::eNocase, NStr::eReverseSearch);
        if (pos == idx + 2) {
            return pos - 1;
        }
    }

    return NPOS;
}

void CDeflineGenerator::x_AdjustProteinTitleSuffixIdx (
    const CBioseq_Handle& bsh
)

{
    CBioSource::TGenome   genome;
    size_t                pos, tpos = NPOS, opos = NPOS;
    int                   len1, len2;
    CConstRef<CBioSource> src;

    CRef<CBioseqIndex> bsx = m_Idx->GetBioseqIndex (bsh);
    if (! bsx) {
        return;
    }

    m_Source = bsx->GetBioSource();
    m_Taxname = bsx->GetTaxname();

    m_Genome = bsx->GetGenome();

    m_Genus = bsx->GetGenus();
    m_Species = bsx->GetSpecies();

    m_Organelle = bsx->GetOrganelle();

    if (m_Source.Empty()) return;

    s_TrimMainTitle (m_MainTitle);

    len1 = m_MainTitle.length();
    len2 = m_Taxname.length();

    // find [taxname]

    if (len1 > len2 + 4) {
        tpos = s_TitleEndsInOrganism(m_MainTitle, m_Taxname);
        if (tpos == NPOS) {
            string descTaxname = bsx->GetDescTaxname();
            tpos = s_TitleEndsInOrganism(m_MainTitle, descTaxname);
        }
        if (tpos == NPOS) {
            string binomial = m_Genus;
            binomial += " ";
            binomial += m_Species;
            tpos = s_TitleEndsInOrganism(m_MainTitle, binomial);
            if (tpos == NPOS) {
                if (m_IsCrossKingdom) {
                    pos = NStr::Find(m_MainTitle, "][", NStr::eNocase, NStr::eReverseSearch);
                    if (pos != NPOS) {
                        m_MainTitle.erase (pos + 1);
                        s_TrimMainTitle (m_MainTitle);
                        tpos = s_TitleEndsInOrganism(m_MainTitle, m_Taxname);
                    }
                }
            }
        }
    }

    /* do not change unless [genus species] was at the end */
    if (tpos == NPOS) return;

    m_MainTitle.erase (tpos);
    s_TrimMainTitle (m_MainTitle);
    len1 = m_MainTitle.length();

    // find (organelle)

    if (len1 > 2 && m_MainTitle [len1 - 1] == ')') {
        pos = m_MainTitle.find_last_of ("(");
        if (pos != NPOS) {
            for ( genome = NCBI_GENOME(chloroplast); genome <= NCBI_GENOME(chromatophore); genome++ ) {
                string str = s_proteinOrganellePrefix [genome];
                if ( ! str.empty() ) {
                    string paren = "(" + str + ")";
                    if (NStr::EndsWith (m_MainTitle, paren )) {
                        opos = pos;
                        break;
                    }
                }
            }
        }
        s_TrimMainTitle (m_MainTitle);
        len1 = m_MainTitle.length();
    }

    if (opos != NPOS) {
        m_MainTitle.erase (opos);
        s_TrimMainTitle (m_MainTitle);
        len1 = m_MainTitle.length();
    }

    if ( NStr::EndsWith (m_MainTitle, ", partial")) {
        m_MainTitle.erase(m_MainTitle.length() - 9);
        s_TrimMainTitle (m_MainTitle);
    }

    // then reconstruct partial (organelle) [taxname] suffix

    if ( !x_IsComplete()) {
        m_MainTitle += ", partial";
    }

    if (m_OmitTaxonomicName) return;

    CTempString taxname = m_Taxname;

    if (m_Genome >= NCBI_GENOME(chloroplast) && m_Genome <= NCBI_GENOME(chromatophore)) {
        const char * organelle = s_proteinOrganellePrefix [m_Genome];
        if ( organelle[0] != '\0'  &&  ! taxname.empty()
            /* &&  NStr::Find (taxname, organelle) == NPOS */) {
            m_MainTitle += " (";
            m_MainTitle += organelle;
            m_MainTitle += ")";
        }
    }

    // check for special taxname, go to overlapping source feature
    if ((taxname.empty()  ||
         (!NStr::EqualNocase (taxname, "synthetic construct")  &&
          !NStr::EqualNocase (taxname, "artificial sequence")  &&
          taxname.find ("vector") == NPOS  &&
          taxname.find ("Vector") == NPOS))  &&
        !m_LocalAnnotsOnly) {
        if (m_Idx) {
            CRef<CBioseqIndex> bsx = m_Idx->GetBioseqIndex (bsh);
            if (bsx) {
                CWeakRef<CBioseqIndex> bsxp = bsx->GetBioseqForProduct();
                auto nucx = bsxp.Lock();
                if (nucx) {
                        src = x_GetSourceFeatViaCDS (bsh);
                        if (src.NotEmpty()  &&  src->IsSetTaxname()) {
                            taxname = src->GetTaxname();
                        }
                }
            }
        } else {
            src = x_GetSourceFeatViaCDS (bsh);
            if (src.NotEmpty()  &&  src->IsSetTaxname()) {
                taxname = src->GetTaxname();
            }
        }
    }

    if (m_IsCrossKingdom && ! m_FirstSuperKingdom.empty() && ! m_SecondSuperKingdom.empty()) {
        m_MainTitle += " [" + string(m_FirstSuperKingdom) + "][" + string(m_SecondSuperKingdom) + "]";
    } else if (! taxname.empty() /* && m_MainTitle.find(taxname) == NPOS */) {
        m_MainTitle += " [" + string(taxname) + "]";
    }
}

void CDeflineGenerator::x_AdjustProteinTitleSuffix (
    const CBioseq_Handle& bsh
)

{
    CBioSource::TGenome   genome;
    size_t                pos, tpos = NPOS, opos = NPOS;
    int                   len1, len2;
    CConstRef<CBioSource> src;

    if (m_Source.Empty()) return;

    if (m_Source->IsSetTaxname()) {
        m_Taxname = m_Source->GetTaxname();
    }
    if (m_Source->IsSetGenome()) {
        m_Genome = m_Source->GetGenome();
    }
    if (m_Source->IsSetOrgname()) {
        const COrgName& onp = m_Source->GetOrgname();
        if (onp.IsSetName()) {
            const COrgName::TName& nam = onp.GetName();
            if (nam.IsBinomial()) {
                const CBinomialOrgName& bon = nam.GetBinomial();
                if (bon.IsSetGenus()) {
                    m_Genus = bon.GetGenus();
                }
                if (bon.IsSetSpecies()) {
                    m_Species = bon.GetSpecies();
                }
            }
        }
    }

    s_TrimMainTitle (m_MainTitle);

    len1 = m_MainTitle.length();
    len2 = m_Taxname.length();

    // find [taxname]

    if (len1 > len2 + 4) {
        tpos = s_TitleEndsInOrganism(m_MainTitle, m_Taxname);
        if (tpos == NPOS) {
            string binomial = m_Genus;
            binomial += " ";
            binomial += m_Species;
            tpos = s_TitleEndsInOrganism(m_MainTitle, binomial);
            if (tpos == NPOS) {
                if (m_IsCrossKingdom) {
                    pos = NStr::Find(m_MainTitle, "][", NStr::eNocase, NStr::eReverseSearch);
                    if (pos != NPOS) {
                        m_MainTitle.erase (pos + 1);
                        s_TrimMainTitle (m_MainTitle);
                        tpos = s_TitleEndsInOrganism(m_MainTitle, m_Taxname);
                    }
                }
            }
        }
    }

    /* do not change unless [genus species] was at the end */
    if (tpos == NPOS) return;

    m_MainTitle.erase (tpos);
    s_TrimMainTitle (m_MainTitle);
    len1 = m_MainTitle.length();

    // find (organelle)

    if (len1 > 2 && m_MainTitle [len1 - 1] == ')') {
        pos = m_MainTitle.find_last_of ("(");
        if (pos != NPOS) {
            for ( genome = NCBI_GENOME(chloroplast); genome <= NCBI_GENOME(chromatophore); genome++ ) {
                string str = s_proteinOrganellePrefix [genome];
                if ( ! str.empty() ) {
                    string paren = "(" + str + ")";
                    if (NStr::EndsWith (m_MainTitle, paren )) {
                        opos = pos;
                        break;
                    }
                }
            }
        }
        s_TrimMainTitle (m_MainTitle);
        len1 = m_MainTitle.length();
    }

    if (opos != NPOS) {
        m_MainTitle.erase (opos);
        s_TrimMainTitle (m_MainTitle);
        len1 = m_MainTitle.length();
    }

    if ( NStr::EndsWith (m_MainTitle, ", partial")) {
        m_MainTitle.erase(m_MainTitle.length() - 9);
        s_TrimMainTitle (m_MainTitle);
    }

    // then reconstruct partial (organelle) [taxname] suffix

    if ( !x_IsComplete()) {
        m_MainTitle += ", partial";
    }

    if (m_OmitTaxonomicName) return;

    CTempString taxname = m_Taxname;

    if (m_Genome >= NCBI_GENOME(chloroplast) && m_Genome <= NCBI_GENOME(chromatophore)) {
        const char * organelle = s_proteinOrganellePrefix [m_Genome];
        if ( organelle[0] != '\0'  &&  ! taxname.empty()
            /* &&  NStr::Find (taxname, organelle) == NPOS */) {
            m_MainTitle += " (";
            m_MainTitle += organelle;
            m_MainTitle += ")";
        }
    }

    // check for special taxname, go to overlapping source feature
    if ((taxname.empty()  ||
         (!NStr::EqualNocase (taxname, "synthetic construct")  &&
          !NStr::EqualNocase (taxname, "artificial sequence")  &&
          taxname.find ("vector") == NPOS  &&
          taxname.find ("Vector") == NPOS))  &&
        !m_LocalAnnotsOnly) {
        if (m_Idx) {
            CRef<CBioseqIndex> bsx = m_Idx->GetBioseqIndex (bsh);
            if (bsx) {
                CWeakRef<CBioseqIndex> bsxp = bsx->GetBioseqForProduct();
                auto nucx = bsxp.Lock();
                if (nucx) {
                        src = x_GetSourceFeatViaCDS (bsh);
                        if (src.NotEmpty()  &&  src->IsSetTaxname()) {
                            taxname = src->GetTaxname();
                        }
                }
            }
        } else {
            src = x_GetSourceFeatViaCDS (bsh);
            if (src.NotEmpty()  &&  src->IsSetTaxname()) {
                taxname = src->GetTaxname();
            }
        }
    }

    if (m_IsCrossKingdom && ! m_FirstSuperKingdom.empty() && ! m_SecondSuperKingdom.empty()) {
        m_MainTitle += " [" + string(m_FirstSuperKingdom) + "][" + string(m_SecondSuperKingdom) + "]";
    } else if (! taxname.empty() /* && m_MainTitle.find(taxname) == NPOS */) {
        m_MainTitle += " [" + string(taxname) + "]";
    }
}

bool CDeflineGenerator::x_IsComplete() const
{
    switch (m_MICompleteness) {
    case NCBI_COMPLETENESS(complete):
        return true;
    case NCBI_COMPLETENESS(partial):
    case NCBI_COMPLETENESS(no_left):
    case NCBI_COMPLETENESS(no_right):
    case NCBI_COMPLETENESS(no_ends):
        return false;
    }
    return true;
}


static const char* s_tpaPrefixList [] = {
  "TPA:",
  "TPA_exp:",
  "TPA_inf:",
  "TPA_reasm:",
  "TPA_asm:",
  "TSA:",
  "UNVERIFIED_ORG:",
  "UNVERIFIED_ASMBLY:",
  "UNVERIFIED_CONTAM:",
  "UNVERIFIED:"
};
string CDeflineGenerator::x_GetModifiers(const CBioseq_Handle & bsh)
{
    CDefLineJoiner joiner(true);

    x_SetBioSrc (bsh);

    joiner.Add("location", m_Organelle);
    if (m_IsChromosome || !m_Chromosome.empty()) {
        joiner.Add("chromosome", m_Chromosome);
    }
    if (m_IsPlasmid || !m_Plasmid.empty()) {
        joiner.Add("plasmid name", m_Plasmid);
    }
    if (m_MICompleteness == NCBI_COMPLETENESS(complete)) {
        joiner.Add("completeness", CTempString("complete"));
    }

    // print [topology=...], if necessary
    if (bsh.CanGetInst_Topology()) {
        CSeq_inst::ETopology topology = bsh.GetInst_Topology();
        if (topology == CSeq_inst::eTopology_circular) {
            joiner.Add("topology", CTempString("circular"));
        }
    }

    // bsh modifiers retrieved from Biosource.Org-ref
    // [organism=...], etc.

    bool strain_seen = false;
    string gcode; // should be in the same scope as joiner.Join() because joiner stores CTempString

    try {
        const CBioSource* bios = sequence::GetBioSource(bsh);
        if (bios && bios->IsSetOrg()) {
            const COrg_ref & org = bios->GetOrg();
            if (org.IsSetTaxname()) {
                joiner.Add("organism", org.GetTaxname());
            }
            if (org.IsSetOrgname()) {
                const COrg_ref::TOrgname & orgname = org.GetOrgname();
                if (orgname.IsSetMod()) {
                    ITERATE(COrgName::TMod, mod_iter, orgname.GetMod()) {
                        const COrgMod & mod = **mod_iter;
                        if (mod.IsSetSubtype() && mod.IsSetSubname()) {
                            const string& subname = mod.GetSubname();
                            switch (mod.GetSubtype()) {
                            case COrgMod::eSubtype_strain:
                                if (strain_seen) {
                                    ERR_POST_X(9, Warning << __FUNCTION__ << ": "
                                        << "key 'strain' would appear multiple times, but only using the first.");
                                }
                                else {
                                    strain_seen = true;
                                    joiner.Add("strain", subname);
                                }
                                break;
                            case COrgMod::eSubtype_substrain:
                                joiner.Add("substrain", subname);
                                break;
                            case COrgMod::eSubtype_type:
                                joiner.Add("type", subname);
                                break;
                            case COrgMod::eSubtype_subtype:
                                joiner.Add("subtype", subname);
                                break;
                            case COrgMod::eSubtype_variety:
                                joiner.Add("variety", subname);
                                break;
                            case COrgMod::eSubtype_serotype:
                                joiner.Add("serotype", subname);
                                break;
                            case COrgMod::eSubtype_serogroup:
                                joiner.Add("serogroup", subname);
                                break;
                            case COrgMod::eSubtype_serovar:
                                joiner.Add("serovar", subname);
                                break;
                            case COrgMod::eSubtype_cultivar:
                                joiner.Add("cultivar", subname);
                                break;
                            case COrgMod::eSubtype_pathovar:
                                joiner.Add("pathovar", subname);
                                break;
                            case COrgMod::eSubtype_chemovar:
                                joiner.Add("chemovar", subname);
                                break;
                            case COrgMod::eSubtype_biovar:
                                joiner.Add("biovar", subname);
                                break;
                            case COrgMod::eSubtype_biotype:
                                joiner.Add("biotype", subname);
                                break;
                            case COrgMod::eSubtype_group:
                                joiner.Add("group", subname);
                                break;
                            case COrgMod::eSubtype_subgroup:
                                joiner.Add("subgroup", subname);
                                break;
                            case COrgMod::eSubtype_isolate:
                                joiner.Add("isolate", subname);
                                break;
                            case COrgMod::eSubtype_common:
                                joiner.Add("common", subname);
                                break;
                            case COrgMod::eSubtype_acronym:
                                joiner.Add("acronym", subname);
                                break;
                            case COrgMod::eSubtype_dosage: 
                                joiner.Add("dosage", subname);
                                break;
                            case COrgMod::eSubtype_nat_host: 
                                joiner.Add("nat_host", subname);
                                break;
                            case COrgMod::eSubtype_sub_species:
                                joiner.Add("sub_species", subname);
                                break;
                            case COrgMod::eSubtype_specimen_voucher:
                                joiner.Add("specimen_voucher", subname);
                                break;
                            case COrgMod::eSubtype_authority:
                                joiner.Add("authority", subname);
                                break;
                            case COrgMod::eSubtype_forma:
                                joiner.Add("forma", subname);
                                break;
                            case COrgMod::eSubtype_forma_specialis:
                                joiner.Add("forma_specialis", subname);
                                break;
                            case COrgMod::eSubtype_ecotype:
                                joiner.Add("ecotype", subname);
                                break;
                            case COrgMod::eSubtype_synonym:
                                joiner.Add("synonym", subname);
                                break;
                            case COrgMod::eSubtype_anamorph:
                                joiner.Add("anamorph", subname);
                                break;
                            case COrgMod::eSubtype_teleomorph:
                                joiner.Add("teleomorph", subname);
                                break;
                            case COrgMod::eSubtype_breed:
                                joiner.Add("breed", subname);
                                break;
                            case COrgMod::eSubtype_gb_acronym: 
                                joiner.Add("gb_acronym", subname);
                                break;
                            case COrgMod::eSubtype_gb_anamorph: 
                                joiner.Add("gb_anamorph", subname);
                                break;
                            case COrgMod::eSubtype_gb_synonym: 
                                joiner.Add("gb_synonym", subname);
                                break;
                            case COrgMod::eSubtype_culture_collection:
                                joiner.Add("culture_collection", subname);
                                break;
                            case COrgMod::eSubtype_bio_material:
                                joiner.Add("bio_material", subname);
                                break;
                            case COrgMod::eSubtype_metagenome_source:
                                joiner.Add("metagenome_source", subname);
                                break;
                            case COrgMod::eSubtype_type_material:
                                joiner.Add("type_material", subname);
                                break;
                            case COrgMod::eSubtype_nomenclature: 
                                joiner.Add("nomenclature", subname);
                                break;
                            case COrgMod::eSubtype_other: 
                                joiner.Add("note", subname);
                                break;
                            default:
                                // ignore; do nothing
                                break;
                            }
                        }
                    }
                }
                CBioSource::TGenome genome = CBioSource::eGenome_unknown;
                if (bios->CanGetGenome()) {
                    genome = bios->GetGenome();
                }

                switch ( genome ) {
                case CBioSource::eGenome_kinetoplast:
                case CBioSource::eGenome_mitochondrion:
                case CBioSource::eGenome_hydrogenosome:
                case CBioSource::eGenome_plasmid_in_mitochondrion:
                    {
                        // mitochondrial code
                        if (orgname.IsSetMgcode()) {
                            int icode = orgname.GetMgcode();
                            gcode = std::to_string(icode);
                            joiner.Add("gcode", gcode);
                        }
                    }
                    break;
                case CBioSource::eGenome_chloroplast:
                case CBioSource::eGenome_chromoplast:
                case CBioSource::eGenome_plastid:
                case CBioSource::eGenome_cyanelle:
                case CBioSource::eGenome_apicoplast:
                case CBioSource::eGenome_leucoplast:
                case CBioSource::eGenome_proplastid:
                case CBioSource::eGenome_chromatophore:
                case CBioSource::eGenome_plasmid_in_plastid:
                    {
                        // specific plant plastid code
                        if (orgname.IsSetPgcode()) {
                            int icode = orgname.GetPgcode();
                            if (icode > 0) {
                                gcode = std::to_string(icode);
                                joiner.Add("gcode", gcode);
                            }
                        } else {
                            // bacteria and plant plastids default to code 11.
                            joiner.Add("gcode", "11");
                        }
                        break;
                    }
                default:
                    {
                        if (orgname.IsSetGcode()) {
                            int icode = orgname.GetGcode();
                            if (icode > 0) {
                                gcode = std::to_string(icode);
                                joiner.Add("gcode", gcode);
                            }
                        }
                        break;
                    }
                }
            }
        }
        if ( bios && bios->IsSetSubtype() ) {
            ITERATE ( CBioSource::TSubtype, sub_iter, bios->GetSubtype() ) {
                const CSubSource& sub = **sub_iter;
                if (sub.IsSetSubtype()) {
                    if (sub.IsSetName()) {
                        const string& subname = sub.GetName();
                        switch (sub.GetSubtype()) {
                        case CSubSource::eSubtype_chromosome:
                            joiner.Add("chromosome", subname);
                            break;
                        case CSubSource::eSubtype_map:
                            joiner.Add("map", subname);
                            break;
                        case CSubSource::eSubtype_clone:
                            joiner.Add("clone", subname);
                            break;
                        case CSubSource::eSubtype_subclone:
                            joiner.Add("subclone", subname);
                            break;
                        case CSubSource::eSubtype_haplotype:
                            joiner.Add("haplotype", subname);
                            break;
                        case CSubSource::eSubtype_genotype:
                            joiner.Add("genotype", subname);
                            break;
                        case CSubSource::eSubtype_sex:
                            joiner.Add("sex", subname);
                            break;
                        case CSubSource::eSubtype_cell_line:
                            joiner.Add("cell_line", subname);
                            break;
                        case CSubSource::eSubtype_cell_type:
                            joiner.Add("cell_type", subname);
                            break;
                        case CSubSource::eSubtype_tissue_type:
                            joiner.Add("tissue_type", subname);
                            break;
                        case CSubSource::eSubtype_clone_lib:
                            joiner.Add("clone_lib", subname);
                            break;
                        case CSubSource::eSubtype_dev_stage:
                            joiner.Add("dev_stage", subname);
                            break;
                        case CSubSource::eSubtype_frequency:
                            joiner.Add("frequency", subname);
                            break;
                        case CSubSource::eSubtype_lab_host:
                            joiner.Add("lab_host", subname);
                            break;
                        case CSubSource::eSubtype_pop_variant:
                            joiner.Add("pop_variant", subname);
                            break;
                        case CSubSource::eSubtype_tissue_lib:
                            joiner.Add("tissue_lib", subname);
                            break;
                        case CSubSource::eSubtype_plasmid_name:
                            joiner.Add("plasmid_name", subname);
                            break;
                        case CSubSource::eSubtype_transposon_name:
                            joiner.Add("transposon_name", subname);
                            break;
                        case CSubSource::eSubtype_insertion_seq_name:
                            joiner.Add("insertion_seq_name", subname);
                            break;
                        case CSubSource::eSubtype_plastid_name:
                            joiner.Add("plastid_name", subname);
                            break;
                        case CSubSource::eSubtype_country:
                            joiner.Add("country", subname);
                            break;
                        case CSubSource::eSubtype_segment:
                            joiner.Add("segment", subname);
                            break;
                        case CSubSource::eSubtype_endogenous_virus_name:
                            joiner.Add("endogenous_virus_name", subname);
                            break;
                        case CSubSource::eSubtype_isolation_source:
                            joiner.Add("isolation_source", subname);
                            break;
                        case CSubSource::eSubtype_lat_lon:
                            joiner.Add("lat_lon", subname);
                            break;
                        case CSubSource::eSubtype_collection_date:
                            joiner.Add("collection_date", subname);
                            break;
                        case CSubSource::eSubtype_collected_by:
                            joiner.Add("collected_by", subname);
                            break;
                        case CSubSource::eSubtype_identified_by:
                            joiner.Add("identified_by", subname);
                            break;
                        case CSubSource::eSubtype_metagenomic:
                            joiner.Add("metagenomic", subname);
                            break;
                        case CSubSource::eSubtype_mating_type:
                            joiner.Add("mating_type", subname);
                            break;
                        case CSubSource::eSubtype_linkage_group:
                            joiner.Add("linkage_group", subname);
                            break;
                        case CSubSource::eSubtype_haplogroup:
                            joiner.Add("haplogroup", subname);
                            break;
                        case CSubSource::eSubtype_whole_replicon:
                            joiner.Add("whole_replicon", subname);
                            break;
                        case CSubSource::eSubtype_phenotype:
                            joiner.Add("phenotype", subname);
                            break;
                        case CSubSource::eSubtype_altitude:
                            joiner.Add("altitude", subname);
                            break;
                        case CSubSource::eSubtype_other:
                            joiner.Add("note", subname);
                            break;
                        default:
                            break;
                        }
                    } else {
                        switch (sub.GetSubtype()) {
                        case CSubSource::eSubtype_germline:
                            joiner.Add("germline", "true");
                            break;
                        case CSubSource::eSubtype_rearranged:
                            joiner.Add("rearranged", "true");
                            break;
                        case CSubSource::eSubtype_transgenic:
                            joiner.Add("transgenic", "true");
                            break;
                        case CSubSource::eSubtype_environmental_sample:
                            joiner.Add("environmental_sample", "true");
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
        }
        if ( bios && bios->IsSetPcr_primers() ) {
            const CBioSource_Base::TPcr_primers & primers = bios->GetPcr_primers();
            if ( primers.CanGet() ) {
                ITERATE( CBioSource_Base::TPcr_primers::Tdata, it, primers.Get() ) {

                    // bool has_fwd_seq = false;
                    // bool has_rev_seq = false;

                    if( (*it)->IsSetForward() ) {
                        const CPCRReaction_Base::TForward &forward = (*it)->GetForward();
                        if( forward.CanGet() ) {
                            ITERATE( CPCRReaction_Base::TForward::Tdata, it2, forward.Get() ) {
                                const string &fwd_name = ( (*it2)->CanGetName() ? (*it2)->GetName().Get() : kEmptyStr );
                                if( ! fwd_name.empty() ) {
                                    joiner.Add("fwd-primer-name", fwd_name);
                                }
                                const string &fwd_seq = ( (*it2)->CanGetSeq() ? (*it2)->GetSeq().Get() : kEmptyStr );
                                // NStr::ToLower( fwd_seq );
                                if( ! fwd_seq.empty() ) {
                                    joiner.Add("fwd-primer-seq", fwd_seq);
                                    // has_fwd_seq = true;
                                }
                            }
                        }
                    }
                    if( (*it)->IsSetReverse() ) {
                        const CPCRReaction_Base::TReverse &reverse = (*it)->GetReverse();
                        if( reverse.CanGet() ) {
                            ITERATE( CPCRReaction_Base::TReverse::Tdata, it2, reverse.Get() ) {
                                const string &rev_name = ((*it2)->CanGetName() ? (*it2)->GetName().Get() : kEmptyStr );
                                if( ! rev_name.empty() ) {
                                    joiner.Add("rev-primer-name", rev_name);
                                }
                                const string &rev_seq = ( (*it2)->CanGetSeq() ? (*it2)->GetSeq().Get() : kEmptyStr );
                                // NStr::ToLower( rev_seq ); // do we need this? 
                                if( ! rev_seq.empty() ) {
                                    joiner.Add("rev-primer-seq", rev_seq);
                                    // has_rev_seq = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    catch (CException &) {
        // ignore exception; it probably just means there's no org-ref
    }

    typedef SStaticPair<CMolInfo::TTech, const char*> TTechMapEntry;
    static const TTechMapEntry sc_TechArray[] = {
        // note that the text values do *NOT* precisely correspond with
        // the names in the ASN.1 schema files
        { CMolInfo::eTech_unknown, "?" },
        { CMolInfo::eTech_standard, "standard" },
        { CMolInfo::eTech_est, "EST" },
        { CMolInfo::eTech_sts, "STS" },
        { CMolInfo::eTech_survey, "survey" },
        { CMolInfo::eTech_genemap, "genetic map" },
        { CMolInfo::eTech_physmap, "physical map" },
        { CMolInfo::eTech_derived, "derived" },
        { CMolInfo::eTech_concept_trans, "concept-trans" },
        { CMolInfo::eTech_seq_pept, "seq-pept" },
        { CMolInfo::eTech_both, "both" },
        { CMolInfo::eTech_seq_pept_overlap, "seq-pept-overlap" },
        { CMolInfo::eTech_seq_pept_homol, "seq-pept-homol" },
        { CMolInfo::eTech_concept_trans_a, "concept-trans-a" },
        { CMolInfo::eTech_htgs_1, "htgs 1" },
        { CMolInfo::eTech_htgs_2, "htgs 2" },
        { CMolInfo::eTech_htgs_3, "htgs 3" },
        { CMolInfo::eTech_fli_cdna, "fli cDNA" },
        { CMolInfo::eTech_htgs_0, "htgs 0" },
        { CMolInfo::eTech_htc, "htc" },
        { CMolInfo::eTech_wgs, "wgs" },
        { CMolInfo::eTech_barcode, "barcode" },
        { CMolInfo::eTech_composite_wgs_htgs, "composite-wgs-htgs" },
        { CMolInfo::eTech_tsa, "tsa" }
    };
    typedef CStaticPairArrayMap<CMolInfo::TTech, const char*>  TTechMap;
    DEFINE_STATIC_ARRAY_MAP(TTechMap, sc_TechMap, sc_TechArray);

    // print some key-value pairs
    const CMolInfo * pMolInfo = sequence::GetMolInfo(bsh);
    if (pMolInfo != NULL) {
        const CMolInfo & molinfo = *pMolInfo;
        if (molinfo.IsSetTech()) {
            TTechMap::const_iterator find_iter = sc_TechMap.find(molinfo.GetTech());
            if (find_iter != sc_TechMap.end()) {
                joiner.Add("tech", CTempString(find_iter->second));
            }
        }
    }
    string modifiers;
    joiner.Join(&modifiers);
    m_MainTitle = (m_MainTitle.empty()) ? modifiers : modifiers + " " + m_MainTitle;
    return m_MainTitle;
}


// main method
string CDeflineGenerator::GenerateDefline (
    const CBioseq_Handle& bsh,
    TUserFlags flags
)

{
    bool capitalize = true;
    bool appendComplete = false;

    string prefix; // from a small set of compile-time constants
    string suffix;
    string final;

    // set flags from record components
    if (m_Idx) {
        x_SetFlagsIdx (bsh, flags);
    } else {
        x_SetFlags (bsh, flags);
    }

    if (flags & fShowModifiers) {
        return x_GetModifiers(bsh);
    }

    if (! m_Reconstruct) {
        // x_SetFlags set m_MainTitle from a suitable descriptor, if any;
        // now strip trailing periods, commas, semicolons, and spaces.
        size_t pos = m_MainTitle.find_last_not_of (".,;~ ");
        if (pos != NPOS) {
            m_MainTitle.erase (pos + 1);
        }
        if (! m_MainTitle.empty()) {
            capitalize = false;
        }
    }

    // adjust protein partial/organelle/taxname suffix, if necessary
    if ( m_IsAA && ! m_MainTitle.empty() ) {
        if (m_Idx) {
            x_AdjustProteinTitleSuffixIdx (bsh);
        } else {
            x_AdjustProteinTitleSuffix (bsh);
        }
    }

    // use autodef user object, if present, to regenerate title
    if (m_MainTitle.empty() && m_IsNA && (flags & fUseAutoDef) && ! m_IsTLS && ! m_IsNZ) {

        CSeqdesc_CI desc(bsh, CSeqdesc::e_User);
        while (desc && desc->GetUser().GetObjectType() != CUser_object::eObjectType_AutodefOptions) {
            ++desc;
        }
        if (desc) {
            CAutoDef autodef;
            autodef.SetOptionsObject(desc->GetUser());
            CAutoDefModifierCombo mod_combo;
            CAutoDefOptions options;
            options.InitFromUserObject(desc->GetUser());
            mod_combo.SetOptions(options);
            m_MainTitle = autodef.GetOneDefLine(&mod_combo, bsh);
            s_TrimMainTitle (m_MainTitle);
        }
    }

    // use appropriate algorithm if title needs to be generated
    if (m_MainTitle.empty()) {

        // PDB and patent records do not normally need source data
        if (m_IsPDB) {
            x_SetTitleFromPDB ();
        } else if (m_IsPatent) {
            x_SetTitleFromPatent ();
        }

        if (m_MainTitle.empty()) {
            // set fields from source information
            if (m_Idx) {
                x_SetBioSrcIdx (bsh);
            } else {
                x_SetBioSrc (bsh);
            }

            // several record types have specific methods
            if (m_IsNC) {
                x_SetTitleFromNC ();
            } else if (m_IsNM  &&  !m_LocalAnnotsOnly) {
                x_SetTitleFromNM (bsh);
            } else if (m_IsNR) {
                x_SetTitleFromNR (bsh);
            } else if (m_IsAA && m_Idx) {
                x_SetTitleFromProteinIdx (bsh);
            } else if (m_IsAA) {
                x_SetTitleFromProtein (bsh);
            } else if (m_IsSeg && (! m_IsEST_STS_GSS)) {
                x_SetTitleFromSegSeq (bsh);
            } else if (m_IsTSA || (m_IsWGS && (! m_WGSMaster)) || (m_IsTLS && (! m_TLSMaster))) {
                x_SetTitleFromWGS ();
            } else if (m_IsMap) {
                x_SetTitleFromMap ();
            }

            if (m_MainTitle.empty() && m_GpipeMode) {
                x_SetTitleFromGPipe();
            }

            if (m_MainTitle.empty()) {
                // default title using source fields
                x_SetTitleFromBioSrc();
                if (m_MICompleteness == NCBI_COMPLETENESS(complete) && !m_MainTitle.empty()) {
                    appendComplete = true;
                }
            }
        }

        /*
        if (m_MainTitle.empty()) {
            // last resort title created here
            m_MainTitle = "No definition line found";
        }
        */
    }

    // remove TPA or TSA prefix, will rely on other data in record to set
    for (int i = 0; i < sizeof (s_tpaPrefixList) / sizeof (const char*); i++) {
        string str = s_tpaPrefixList [i];
        if (NStr::StartsWith (m_MainTitle, str, NStr::eNocase)) {
            m_MainTitle.erase (0, str.length());
        }
    }

    // strip leading spaces remaining after removal of old TPA or TSA prefixes
    m_MainTitle.erase (0, m_MainTitle.find_first_not_of (' '));

    CStringUTF8 decoded = NStr::HtmlDecode (m_MainTitle);

    // strip trailing commas, semicolons, and spaces (period may be an sp.
    // species)
    size_t pos = decoded.find_last_not_of (",;~ ");
    if (pos != NPOS) {
        decoded.erase (pos + 1);
    }

    // calculate prefix
    x_SetPrefix(prefix);

    // calculate suffix
    x_SetSuffix (suffix, bsh, appendComplete);

    // produce final result
    string penult = prefix + decoded + suffix;

    x_CleanAndCompress (final, penult, m_IsAA);

    if (! m_IsPDB && ! m_IsPatent && ! m_IsAA && ! m_IsSeg) {
        if (!final.empty() && islower ((unsigned char) final[0]) && capitalize) {
            final [0] = toupper ((unsigned char) final [0]);
        }
    }

    m_Feat_Tree.Reset (NULL);
    m_Idx.Reset (NULL);

    return final;
}

string CDeflineGenerator::GenerateDefline (
    const CBioseq_Handle& bsh,
    CSeqEntryIndex& idx,
    TUserFlags flags
)

{
    m_Idx = &idx;

    return GenerateDefline(bsh, flags);
}

string CDeflineGenerator::GenerateDefline (
    const CBioseq& bioseq,
    CScope& scope,
    CSeqEntryIndex& idx,
    TUserFlags flags
)

{
    m_Idx = &idx;

    return GenerateDefline(bioseq, scope, flags);
}

string CDeflineGenerator::GenerateDefline (
    const CBioseq_Handle& bsh,
    feature::CFeatTree& ftree,
    TUserFlags flags
)

{
    m_ConstructedFeatTree = true;
    m_InitializedFeatTree = true;
    m_Feat_Tree = &ftree;

    return GenerateDefline(bsh, flags);
}

string CDeflineGenerator::GenerateDefline (
    const CBioseq& bioseq,
    CScope& scope,
    TUserFlags flags
)

{
    CBioseq_Handle bsh = scope.AddBioseq(bioseq,
                                         CScope::kPriority_Default,
                                         CScope::eExist_Get);
    return GenerateDefline(bsh, flags);
}

string CDeflineGenerator::GenerateDefline (
    const CBioseq& bioseq,
    CScope& scope,
    feature::CFeatTree& ftree,
    TUserFlags flags
)

{
    m_ConstructedFeatTree = true;
    m_InitializedFeatTree = true;
    m_Feat_Tree = &ftree;

    return GenerateDefline(bioseq, scope, flags);
}

CDeflineGenerator::CLowQualityTextFsm::CLowQualityTextFsm(void) {
    AddWord ("heterogeneous population sequenced", 1);
    AddWord ("low-quality sequence region", 2);
    AddWord ("unextendable partial coding region", 3);
    Prime ();
}

CSafeStatic<CDeflineGenerator::CLowQualityTextFsm> CDeflineGenerator::ms_p_Low_Quality_Fsa;
