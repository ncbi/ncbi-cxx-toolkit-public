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

#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(sequence);
USING_SCOPE(feature);

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
    m_Low_Quality_Fsa = 0;

    m_Low_Quality_Fsa.AddWord ("heterogeneous population sequenced", 1);
    m_Low_Quality_Fsa.AddWord ("low-quality sequence region", 2);
    m_Low_Quality_Fsa.AddWord ("unextendable partial coding region", 3);
    m_Low_Quality_Fsa.Prime ();
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

    // reset member variables to cleared state
    m_IsNA = false;
    m_IsAA = false;

    m_IsSeg = false;
    m_IsDelta = false;

    m_IsNC = false;
    m_IsNM = false;
    m_IsNR = false;
    m_IsPatent = false;
    m_IsPDB = false;
    m_ThirdParty = false;
    m_WGSMaster = false;
    m_TSAMaster = false;

    m_MainTitle.clear();
    m_GeneralStr.clear();
    m_PatentCountry.clear();
    m_PatentNumber.clear();

    m_PatentSequence = 0;

    m_PDBChain = 0;

    m_MIBiomol = NCBI_BIOMOL(unknown);
    m_MITech = NCBI_TECH(unknown);
    m_MICompleteness = NCBI_COMPLETENESS(unknown);

    m_HTGTech = false;
    m_HTGSUnfinished = false;
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

    m_PDBCompound.clear();

    m_Source.Reset();
    m_Taxname.clear();
    m_Multispecies = false;
    m_Genome = NCBI_GENOME(unknown);

    m_Chromosome.clear();
    m_Clone.clear();
    m_has_clone = false;
    m_Map.clear();
    m_Plasmid.clear();
    m_Segment.clear();

    m_Breed.clear();
    m_Cultivar.clear();
    m_Isolate.clear();
    m_Strain.clear();

    m_IsUnverified = false;

    m_rEnzyme.clear();

    // now start setting member variables
    m_IsNA = bsh.IsNa();
    m_IsAA = bsh.IsAa();

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
                        }
                    }
                }
                break;
            }
            case NCBI_SEQID(Tpg):
            case NCBI_SEQID(Tpe):
            case NCBI_SEQID(Tpd):
                m_ThirdParty = true;
                break;
            case NCBI_SEQID(Pdb):
            {
                m_IsPDB = true;
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CPDB_seq_id& pdb_id = id->GetPdb ();
                if (pdb_id.IsSetChain()) {
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
        fPdb     = 1 << 6
    };
    int needed_desc_choices = fMolinfo | fUser | fSource | fGenbank | fEmbl;

    CSeqdesc_CI::TDescChoices desc_choices;
    desc_choices.reserve(7);
    desc_choices.push_back(CSeqdesc::e_Molinfo);
    desc_choices.push_back(CSeqdesc::e_User);
    desc_choices.push_back(CSeqdesc::e_Source);
    // Only truly needed if (m_HTGTech || m_ThirdParty), but
    // determining m_HTGTech requires a descriptor scan.
    desc_choices.push_back(CSeqdesc::e_Genbank);
    desc_choices.push_back(CSeqdesc::e_Embl);
    if (! m_Reconstruct) {
        needed_desc_choices |= fTitle;
        desc_choices.push_back(CSeqdesc::e_Title);
    }
    if (m_IsPDB) {
        needed_desc_choices |= fPdb;
        desc_choices.push_back(CSeqdesc::e_Pdb);
    }

    const list <string> *keywords = NULL;

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
            if (FIELD_IS_SET_AND_IS(user_obj, Type, Str)
                &&  user_obj.GetType().GetStr() == "Unverified" ) {
                m_IsUnverified = true;
                needed_desc_choices &= ~fUser;
            }
            break;
        }

        case CSeqdesc::e_Source:
            if ((needed_desc_choices & fSource) != 0) {
                m_Source.Reset(&desc_it->GetSource());
                // take first, then skip remainder
                needed_desc_choices &= ~fSource;
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

    if (keywords != NULL  &&  (m_HTGTech || m_ThirdParty)) {
        FOR_EACH_STRING_IN_LIST (kw_itr, *keywords) {
            const string& str = *kw_itr;
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
                default:
                    break;
            }
        }

        if (m_Source->IsSetOrgname()) {
            const COrgName& onp = m_Source->GetOrgname();
            if (onp.IsSetName()) {
                const COrgName::TName& nam = onp.GetName();
                if (nam.IsPartial()) {
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
                        break;
                    default:
                        break;
                }
            }
            ++feat_it;
        }
    } catch ( const exception& e ) {
        ERR_POST(Error << "Unable to iterate source features while constructing default definition line");
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

    pos = NStr::FindNoCase (taxname, strain, 0, taxname.size() - 1, NStr::eLast);
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
    string clnbuf;
    vector<CTempString> clnvec;
    CTextJoiner<12, CTempString> joiner;

    joiner.Add(m_Taxname);

    if (! m_Strain.empty()) {
        CTempString add(m_Strain, 0, m_Strain.find(';'));
        if (! x_EndsWithStrain (m_Taxname, add)) {
            joiner.Add(" strain ").Add(add);
        }
    }
    if (! m_Chromosome.empty()) {
        joiner.Add(" chromosome ").Add(m_Chromosome);
    }
    if (m_has_clone) {
        x_DescribeClones (clnvec, clnbuf);
        ITERATE (vector<CTempString>, it, clnvec) {
            joiner.Add(*it);
        }
    }
    if (! m_Map.empty()) {
        joiner.Add(" map ").Add(m_Map);
    }
    if (m_IsWGS  &&  ! m_Plasmid.empty()) {
        joiner.Add(" plasmid ").Add(m_Plasmid);
    }

    joiner.Join(&m_MainTitle);
    NStr::TruncateSpacesInPlace(m_MainTitle);

    if (!m_MainTitle.empty() && islower ((unsigned char) m_MainTitle[0])) {
        m_MainTitle [0] = toupper ((unsigned char) m_MainTitle [0]);
    }
}

// generate title for NC
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

void CDeflineGenerator::x_SetTitleFromNC (void)

{
    if (m_MIBiomol != NCBI_BIOMOL(genomic) &&
         m_MIBiomol != NCBI_BIOMOL(other_genetic)) return;

    // require taxname to be set
    if (m_Taxname.empty()) return;

    bool       has_plasmid = false, virus_or_phage = false,
               is_chromosome = false, is_plasmid = false;
    const char * orgnl, * seq_tag, * gen_tag, * pls_pfx = " ";

    CTextJoiner<6, CTempString> joiner;

    if (NStr::FindNoCase(m_Taxname, "virus") != NPOS  ||
        NStr::FindNoCase(m_Taxname, "phage") != NPOS) {
        virus_or_phage = true;
    }

    if (! m_Plasmid.empty()) {
        has_plasmid = true;
        if (NStr::FindNoCase(m_Plasmid, "plasmid") == NPOS  &&
            NStr::FindNoCase(m_Plasmid, "element") == NPOS) {
            pls_pfx = " plasmid ";
        }
    }

    orgnl = x_OrganelleName (m_Genome, has_plasmid, virus_or_phage, false);

    is_plasmid = (m_Genome == NCBI_GENOME(plasmid));
    is_chromosome = (m_Genome == NCBI_GENOME(chromosome));

    switch (m_MICompleteness) {
        case NCBI_COMPLETENESS(partial):
        case NCBI_COMPLETENESS(no_left):
        case NCBI_COMPLETENESS(no_right):
        case NCBI_COMPLETENESS(no_ends):
            seq_tag = ", partial sequence";
            gen_tag = ", genome";
            break;
        default:
            seq_tag = ", complete sequence";
            gen_tag = ", complete genome";
            break;
    }

    joiner.Add(m_Taxname);

    if (NStr::FindNoCase (m_Taxname, "plasmid") != NPOS) {
        joiner.Add(seq_tag);
    } else if (is_plasmid) {
        if (m_Plasmid.empty()) {
            joiner.Add(" unnamed plasmid").Add(seq_tag);
        } else {
            joiner.Add(pls_pfx).Add(m_Plasmid).Add(seq_tag);
        }
    } else if (! m_Plasmid.empty() ) {
        if (orgnl[0] != '\0') {
            joiner.Add(" ").Add(orgnl);
        }
        joiner.Add(pls_pfx).Add(m_Plasmid).Add(seq_tag);
    } else if ( orgnl[0] != 0 ) {
        if ( m_Chromosome.empty() ) {
            joiner.Add(" ").Add(orgnl).Add(gen_tag);
        } else if (is_chromosome) {
            joiner.Add(" chromosome ").Add(m_Chromosome).Add(seq_tag);
        } else {
            joiner.Add(" ").Add(orgnl).Add(" chromosome ").Add(m_Chromosome)
                .Add(seq_tag);
        }
    } else if (! m_Segment.empty()) {
        if (m_Segment.find ("DNA") == NPOS &&
            m_Segment.find ("RNA") == NPOS &&
            m_Segment.find ("segment") == NPOS &&
            m_Segment.find ("Segment") == NPOS) {
            joiner.Add(" segment ").Add(m_Segment).Add(seq_tag);
        } else {
            joiner.Add(" ").Add(m_Segment).Add(seq_tag);
        }
    } else if (! m_Chromosome.empty()) {
        joiner.Add(" chromosome ").Add(m_Chromosome).Add(seq_tag);
    } else {
        joiner.Add(gen_tag);
    }
    joiner.Join(&m_MainTitle);

    NStr::ReplaceInPlace (m_MainTitle, "Plasmid", "plasmid");
    NStr::ReplaceInPlace (m_MainTitle, "Element", "element");
    if (! m_MainTitle.empty()) {
        m_MainTitle[0] = toupper ((unsigned char) m_MainTitle[0]);
    }
}

// generate title for NM
static void x_FlyCG_PtoR (
    string& s
)

{
    // s =~ s/\b(CG\d*-)P([[:alpha:]])\b/$1R$2/g, more or less.
    SIZE_TYPE pos = 0, len = s.size();
    while ((pos = NStr::FindCase (s, "CG", pos)) != NPOS) {
        if (pos > 0  &&  !isspace((unsigned char)s[pos - 1]) ) {
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

// generate title for PDB
void CDeflineGenerator::x_SetTitleFromPDB (void)

{
    if (isprint ((unsigned char) m_PDBChain)) {
        string chain(1, (char) m_PDBChain);
        CTextJoiner<4, CTempString> joiner;
        joiner.Add("Chain ").Add(chain).Add(", ").Add(m_PDBCompound);
        joiner.Join(&m_MainTitle);
    } else {
        m_MainTitle = m_PDBCompound;
    }
}

void CDeflineGenerator::x_SetTitleFromGPipe (void)

{
    string clnbuf;
    vector<CTempString> clnvec;
    CTextJoiner<12, CTempString> joiner;

    joiner.Add(m_Taxname);

    bool       has_plasmid = false, virus_or_phage = false;
    const char * orgnl, * pls_pfx = " ";

    if (NStr::FindNoCase(m_Taxname, "virus") != NPOS  ||
        NStr::FindNoCase(m_Taxname, "phage") != NPOS) {
        virus_or_phage = true;
    }

    if (! m_Plasmid.empty()) {
        has_plasmid = true;
        if (NStr::FindNoCase(m_Plasmid, "plasmid") == NPOS  &&
            NStr::FindNoCase(m_Plasmid, "element") == NPOS) {
            pls_pfx = " plasmid ";
        }
    }

    orgnl = x_OrganelleName (m_Genome, has_plasmid, virus_or_phage, false);

    if ( orgnl[0] != 0 && NStr::FindNoCase (m_Taxname, "plasmid") != NPOS) {
        joiner.Add(orgnl);
    }

    if (! m_Strain.empty()) {
        CTempString add(m_Strain, 0, m_Strain.find(';'));
        if (! x_EndsWithStrain (m_Taxname, add)) {
            joiner.Add(" strain ").Add(add);
        }
    }
    if (! m_Chromosome.empty()) {
        joiner.Add(" chromosome ").Add(m_Chromosome);
    }
    if (m_has_clone) {
        x_DescribeClones (clnvec, clnbuf);
        ITERATE (vector<CTempString>, it, clnvec) {
            joiner.Add(*it);
        }
    }
    if (! m_Map.empty()) {
        joiner.Add(" map ").Add(m_Map);
    }
    if (! m_Plasmid.empty()) {
        joiner.Add(pls_pfx).Add(m_Plasmid);
    }
    if (m_MICompleteness ==  NCBI_COMPLETENESS(complete)) {
        joiner.Add(", complete sequence");
    }

    joiner.Join(&m_MainTitle);
    NStr::TruncateSpacesInPlace(m_MainTitle);

    if (!m_MainTitle.empty() && islower ((unsigned char) m_MainTitle[0])) {
        m_MainTitle [0] = toupper ((unsigned char) m_MainTitle [0]);
    }
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

        CMappedFeat mapped_gene = GetBestGeneForCds (mapped_cds, m_Feat_Tree);
        if (mapped_gene) {
            const CSeq_feat& gene_feat = mapped_gene.GetOriginalFeature();
            gene_ref = &gene_feat.GetData().GetGene();
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
            CConstRef<CSeq_feat> src_feat
                = GetOverlappingSource (*cds_loc, scope);
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
        int next_state = m_Low_Quality_Fsa.GetNextState (current_state, ch);
        if (m_Low_Quality_Fsa.IsMatchFound (next_state)) {
            return true;
        }
        current_state = next_state;
    }


    return false;
}

/*
static const char* s_proteinOrganellePrefix [] = {
  "",
  "",
  "chloroplast",
  "chromoplast",
  "kinetoplast",
  "mitochondrion",
  "plastid",
  "macronuclear",
  "extrachromosomal",
  "plasmid",
  "",
  "",
  "cyanelle",
  "proviral",
  "virus",
  "nucleomorph",
  "apicoplast",
  "leucoplast",
  "protoplast",
  "endogenous virus",
  "hydrogenosome",
  "chromosome",
  "chromatophore"
};
*/

static const char* s_proteinOrganellePrefix [] = {
  "",
  "",
  "chloroplast",
  "",
  "",
  "mitochondrion",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
};

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
    bool                  partial = false;

    // gets longest protein on Bioseq, parts set, or seg set, even if not
    // full-length

    prot_feat = x_GetLongestProtein (bsh);

    if (prot_feat) {
        prot = &prot_feat->GetData().GetProt();
    }

    switch (m_MICompleteness) {
        case NCBI_COMPLETENESS(partial):
        case NCBI_COMPLETENESS(no_left):
        case NCBI_COMPLETENESS(no_right):
        case NCBI_COMPLETENESS(no_ends):
            partial = true;
            break;
        default:
            break;
    }

    const CMappedFeat& mapped_cds = GetMappedCDSForProduct (bsh);

    if (prot) {
        const CProt_ref& prp = *prot;
        const char* prefix = "";
        FOR_EACH_NAME_ON_PROT (prp_itr, prp) {
            const string& str = *prp_itr;
            m_MainTitle += prefix;
            m_MainTitle += str;
            if (! m_AllProtNames) {
                break;
            }
            prefix = "; ";
        }

        if (! m_MainTitle.empty()) {
            // strip trailing periods, commas, and spaces
            SIZE_TYPE pos = m_MainTitle.find_last_not_of (".,;~ ");
            if (pos != string::npos) {
                m_MainTitle.erase (pos + 1);
            }

            if ((NStr::EqualNocase (m_MainTitle, "hypothetical protein")  ||
                 NStr::EqualNocase (m_MainTitle, "uncharacterized protein"))
                &&  !m_LocalAnnotsOnly ) {
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
        }
        if (m_MainTitle.empty()) {
            if (prp.IsSetDesc()) {
                m_MainTitle = prp.GetDesc();
            }
        }
        if (m_MainTitle.empty()) {
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

    if (m_MainTitle.empty()) {
        m_MainTitle = "unnamed protein product";
        if (!m_LocalAnnotsOnly) {
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
    if (pos != string::npos) {
        m_MainTitle.erase (pos + 1);
    }

    if (partial /* && m_MainTitle.find(", partial") == NPOS */) {
        m_MainTitle += ", partial";
    }

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

    if (! taxname.empty() /* && m_MainTitle.find(taxname) == NPOS */) {
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
    string       locus, product, clnbuf;
    vector<CTempString> clnvec;
    CTextJoiner<13, CTempString> joiner;

    if (m_Taxname.empty()) {
        m_Taxname = "Unknown";
    }

    if ( !m_LocalAnnotsOnly ) {
        cds_found = x_GetSegSeqInfoViaCDS(locus, product, completeness, bsh);
    }

    joiner.Add(m_Taxname);

    if ( !cds_found) {
        if (! m_Strain.empty()
            &&  ! x_EndsWithStrain (m_Taxname, m_Strain) ) {
            joiner.Add(" strain ").Add(m_Strain);
        } else if (! m_Clone.empty()
                   /* && m_Clone.find(" clone ") != NPOS */) {
            x_DescribeClones (clnvec, clnbuf);
            ITERATE (vector<CTempString>, it, clnvec) {
                joiner.Add(*it);
            }
        } else if (! m_Isolate.empty() ) {
            joiner.Add(" isolate ").Add(m_Isolate);
        }
    }
    if (! product.empty()) {
        joiner.Add(" ").Add(product);
    }
    if (! locus.empty()) {
        joiner.Add(" (").Add(locus).Add(")");
    }
    if ((! product.empty()) || (! locus.empty())) {
        joiner.Add(" gene, ").Add(completeness).Add(" cds");
    }
    joiner.Join(&m_MainTitle);
    NStr::TruncateSpacesInPlace(m_MainTitle);
}

// generate title for TSA or non-master WGS
void CDeflineGenerator::x_SetTitleFromWGS (void)

{
    string clnbuf;
    vector<CTempString> clnvec;
    CTextJoiner<14, CTempString> joiner;

    joiner.Add(m_Taxname);

    if (! m_Strain.empty()) {
        if (! x_EndsWithStrain (m_Taxname, m_Strain)) {
            joiner.Add(" strain ");
            joiner.Add(m_Strain.substr (0, m_Strain.find(';')));
        }
    } else if (! m_Breed.empty()) {
        joiner.Add(" breed ").Add(m_Breed.substr (0, m_Breed.find(';')));
    } else if (! m_Cultivar.empty()) {
        joiner.Add(" cultivar ");
        joiner.Add(m_Cultivar.substr (0, m_Cultivar.find(';')));
    }
    if (! m_Chromosome.empty()) {
        joiner.Add(" chromosome ").Add(m_Chromosome);
    }
    if (! m_Clone.empty()) {
        x_DescribeClones (clnvec, clnbuf);
        ITERATE (vector<CTempString>, it, clnvec) {
            joiner.Add(*it);
        }
    }
    if (! m_Map.empty()) {
        joiner.Add(" map ").Add(m_Map);
    }
    if (! m_Plasmid.empty()) {
        if (m_IsWGS) {
            joiner.Add(" plasmid ").Add(m_Plasmid);
        }
    }
    if (! m_GeneralStr.empty()  &&  m_GeneralStr != m_Chromosome
        &&  (! m_IsWGS  ||  m_GeneralStr != m_Plasmid)) {
        joiner.Add(" ").Add(m_GeneralStr);
    }

    joiner.Join(&m_MainTitle);
    NStr::TruncateSpacesInPlace(m_MainTitle);

    if (islower ((unsigned char) m_MainTitle[0])) {
        m_MainTitle [0] = toupper ((unsigned char) m_MainTitle [0]);
    }
}

// generate title for optical map
void CDeflineGenerator::x_SetTitleFromMap (void)

{
    string clnbuf;
    vector<CTempString> clnvec;
    CTextJoiner<14, CTempString> joiner;

    joiner.Add(m_Taxname);

    if (! m_Strain.empty()) {
        if (! x_EndsWithStrain (m_Taxname, m_Strain)) {
            joiner.Add(" strain ");
            joiner.Add(m_Strain.substr (0, m_Strain.find(';')));
        }
    }
    if (! m_Chromosome.empty()) {
        joiner.Add(" chromosome ").Add(m_Chromosome);
    }

    if (! m_rEnzyme.empty()) {
        joiner.Add(", ").Add(m_rEnzyme).Add(" whole genome map");
    }

    joiner.Join(&m_MainTitle);
    NStr::TruncateSpacesInPlace(m_MainTitle);

    if (islower ((unsigned char) m_MainTitle[0])) {
        m_MainTitle [0] = toupper ((unsigned char) m_MainTitle [0]);
    }
}

// generate TPA or TSA prefix
const char * CDeflineGenerator::x_SetPrefix (void)

{
    const char * prefix = kEmptyCStr;

    if (m_IsUnverified) {
        if (m_MainTitle.find ("UNVERIFIED") == NPOS) {
            prefix = "UNVERIFIED: ";
        }
    } else if (m_IsTSA) {
        prefix = "TSA: ";
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
    } else if (m_Multispecies) {
        prefix = "MULTISPECIES: ";
    }

    return prefix;
}

// generate suffix if not already present
void CDeflineGenerator::x_SetSuffix (
    string& suffix,
    const CBioseq_Handle& bsh
)

{
    switch (m_MITech) {
        case NCBI_TECH(htgs_0):
            if (m_MainTitle.find ("LOW-PASS") == NPOS) {
                suffix = ", LOW-PASS SEQUENCE SAMPLING";
            }
            break;
        case NCBI_TECH(htgs_1):
        case NCBI_TECH(htgs_2):
        {
            if (m_HTGSDraft && m_MainTitle.find ("WORKING DRAFT") == NPOS) {
                suffix = ", WORKING DRAFT SEQUENCE";
            } else if ( !m_HTGSDraft && !m_HTGSCancelled &&
                       m_MainTitle.find ("SEQUENCING IN") == NPOS) {
                suffix = ", *** SEQUENCING IN PROGRESS ***";
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
                    // suffix += (", 1 " + un + "ordered piece");
                } else {
                    suffix += (", " + NStr::IntToString (pieces)
                               + " " + un + "ordered pieces");
                }
            } else {
                // suffix += ", in " + un + "ordered pieces";
            }
            break;
        }
        case NCBI_TECH(htgs_3):
            if (m_MainTitle.find ("complete sequence") == NPOS) {
                suffix = ", complete sequence";
            }
            break;
        case NCBI_TECH(est):
            if (m_MainTitle.find ("mRNA sequence") == NPOS) {
                suffix = ", mRNA sequence";
            }
            break;
        case NCBI_TECH(sts):
            if (m_MainTitle.find ("sequence tagged site") == NPOS) {
                suffix = ", sequence tagged site";
            }
            break;
        case NCBI_TECH(survey):
            if (m_MainTitle.find ("genomic survey sequence") == NPOS) {
                suffix = ", genomic survey sequence";
            }
            break;
        case NCBI_TECH(wgs):
            if (m_WGSMaster) {
                if (m_MainTitle.find ("whole genome shotgun sequencing project")
                    == NPOS){
                    suffix = ", whole genome shotgun sequencing project";
                }            
            } else if (m_MainTitle.find ("whole genome shotgun sequence")
                       == NPOS) {
                string orgnl = x_OrganelleName (m_Genome, false, false, true);
                if (! orgnl.empty()  &&  m_MainTitle.find(orgnl) == NPOS) {
                    suffix = " " + orgnl;
                }
                suffix += ", whole genome shotgun sequence";
            }
            break;
        case NCBI_TECH(tsa):
            if (m_MIBiomol == NCBI_BIOMOL(mRNA)) {
                if (m_TSAMaster) {
                    if (m_MainTitle.find
                        ("transcriptome shotgun assembly")
                        == NPOS) {
                        suffix = ", transcriptome shotgun assembly";
                    }            
                } else {
                    if (m_MainTitle.find ("RNA sequence") == NPOS) {
                        switch (m_MIBiomol) {
                            case NCBI_BIOMOL(mRNA):
                                suffix = ", mRNA sequence";
                                break;
                            case NCBI_BIOMOL(rRNA):
                                suffix = ", rRNA sequence";
                                break;
                            case NCBI_BIOMOL(ncRNA):
                                suffix = ", ncRNA sequence";
                                break;
                            case NCBI_BIOMOL(pre_RNA):
                            case NCBI_BIOMOL(snRNA):
                            case NCBI_BIOMOL(scRNA):
                            case NCBI_BIOMOL(cRNA):
                            case NCBI_BIOMOL(snoRNA):
                            case NCBI_BIOMOL(transcribed_RNA):
                                suffix = ", transcribed RNA sequence";
                                break;
                            default:
                                break;
                        }
                    }            
                }
            }
            break;
        default:
            break;
    }
}

// main method
string CDeflineGenerator::GenerateDefline (
    const CBioseq_Handle& bsh,
    TUserFlags flags
)

{
    const char * prefix; // from a small set of compile-time constants
    string suffix;

    // set flags from record components
    x_SetFlags (bsh, flags);

    if (! m_Reconstruct) {
        // x_SetFlags set m_MainTitle from a suitable descriptor, if any;
        // now strip trailing periods, commas, semicolons, and spaces.
        size_t pos = m_MainTitle.find_last_not_of (".,;~ ");
        if (pos != string::npos) {
            m_MainTitle.erase (pos + 1);
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
            x_SetBioSrc (bsh);

            // several record types have specific methods
            if (m_IsNC) {
                x_SetTitleFromNC ();
            } else if (m_IsNM  &&  !m_LocalAnnotsOnly) {
                x_SetTitleFromNM (bsh);
            } else if (m_IsNR) {
                x_SetTitleFromNR (bsh);
            } else if (m_IsAA) {
                x_SetTitleFromProtein (bsh);
            } else if (m_IsSeg && (! m_IsEST_STS_GSS)) {
                x_SetTitleFromSegSeq (bsh);
            } else if (m_IsTSA || (m_IsWGS && (! m_WGSMaster))) {
                x_SetTitleFromWGS ();
            } else if (m_IsMap) {
                x_SetTitleFromMap ();
            }
        }

        if (m_MainTitle.empty() && m_GpipeMode) {
            x_SetTitleFromGPipe ();
        }

        if (m_MainTitle.empty()) {
            // default title using source fields
            x_SetTitleFromBioSrc ();
        }

        if (m_MainTitle.empty()) {
            // last resort title created here
            //m_MainTitle = "No definition line found";
        }
    }

    // remove TPA or TSA prefix, will rely on other data in record to set
    switch (CTempString (m_MainTitle, 0, 11).find (':')) {
    case 3:
        if (NStr::StartsWith (m_MainTitle, "TPA", NStr::eNocase)  ||
            NStr::StartsWith (m_MainTitle, "TSA", NStr::eNocase)) {
            m_MainTitle.erase (0, 4);
        }
        break;

    case 7:
        if (NStr::StartsWith (m_MainTitle, "TPA_exp", NStr::eNocase)  ||
            NStr::StartsWith (m_MainTitle, "TPA_inf", NStr::eNocase)  ||
            NStr::StartsWith (m_MainTitle, "TPA_asm", NStr::eNocase)) {
            m_MainTitle.erase (0, 8);
        }
        break;

    case 9:
        if (NStr::StartsWith (m_MainTitle, "TPA_reasm", NStr::eNocase)) {
            m_MainTitle.erase (0, 10);
        }
        break;

    case 10:
        if (NStr::StartsWith (m_MainTitle, "UNVERIFIED", NStr::eNocase)) {
            m_MainTitle.erase (0, 11);
        }
        break;

    default:
        break;
    }

    // strip leading spaces remaining after removal of old TPA or TSA prefixes
    m_MainTitle.erase (0, m_MainTitle.find_first_not_of (' '));

    // strip trailing commas, semicolons, and spaces (period may be an sp.
    // species)
    size_t pos = m_MainTitle.find_last_not_of (",;~ ");
    if (pos != string::npos) {
        m_MainTitle.erase (pos + 1);
    }

    // calculate prefix
    prefix = x_SetPrefix();

    // calculate suffix
    x_SetSuffix (suffix, bsh);

    return prefix + m_MainTitle + suffix;
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

