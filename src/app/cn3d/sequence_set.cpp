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
* Authors:  Paul Thiessen
*
* File Description:
*      Classes to hold sets of sequences
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistl.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/NCBI8na.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqblock/PDB_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>

#include <regex.h>  // regex from C-toolkit

#include "sequence_set.hpp"
#include "molecule.hpp"
#include "structure_set.hpp"
#include "cn3d_tools.hpp"
#include "molecule_identifier.hpp"
#include "messenger.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

static void UnpackSeqSet(CBioseq_set& bss, SequenceSet *parent, SequenceSet::SequenceList& seqlist)
{
    CBioseq_set::TSeq_set::iterator q, qe = bss.SetSeq_set().end();
    for (q=bss.SetSeq_set().begin(); q!=qe; ++q) {
        if (q->GetObject().IsSeq()) {

            // only store amino acid or nucleotide sequences
            if (q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_aa &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_dna &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_rna &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_na)
                continue;

            const Sequence *sequence = new Sequence(parent, q->GetObject().SetSeq());
            seqlist.push_back(sequence);

        } else { // Bioseq-set
            UnpackSeqSet(q->GetObject().SetSet(), parent, seqlist);
        }
    }
}

static void UnpackSeqEntry(CSeq_entry& seqEntry, SequenceSet *parent, SequenceSet::SequenceList& seqlist)
{
    if (seqEntry.IsSeq()) {
        const Sequence *sequence = new Sequence(parent, seqEntry.SetSeq());
        seqlist.push_back(sequence);
    } else { // Bioseq-set
        UnpackSeqSet(seqEntry.SetSet(), parent, seqlist);
    }
}

SequenceSet::SequenceSet(StructureBase *parent, SeqEntryList& seqEntries) :
    StructureBase(parent)
{
    SeqEntryList::iterator s, se = seqEntries.end();
    for (s=seqEntries.begin(); s!=se; ++s)
        UnpackSeqEntry(s->GetObject(), this, sequences);
    TRACEMSG("number of sequences: " << sequences.size());
}

#define FIRSTOF2(byte) (((byte) & 0xF0) >> 4)
#define SECONDOF2(byte) ((byte) & 0x0F)

static void StringFrom4na(const vector< char >& vec, string *str, bool isDNA)
{
    if (SECONDOF2(vec.back()) > 0)
        str->resize(vec.size() * 2);
    else
        str->resize(vec.size() * 2 - 1);

    // first, extract 4-bit values
    int i;
    for (i=0; i<vec.size(); ++i) {
        str->at(2*i) = FIRSTOF2(vec[i]);
        if (SECONDOF2(vec[i]) > 0) str->at(2*i + 1) = SECONDOF2(vec[i]);
    }

    // then convert 4-bit values to ascii characters
    for (i=0; i<str->size(); ++i) {
        switch (str->at(i)) {
            case 1: str->at(i) = 'A'; break;
            case 2: str->at(i) = 'C'; break;
            case 4: str->at(i) = 'G'; break;
            case 8: isDNA ? str->at(i) = 'T' : str->at(i) = 'U'; break;
            default:
                str->at(i) = 'X';
        }
    }
}

#define FIRSTOF4(byte) (((byte) & 0xC0) >> 6)
#define SECONDOF4(byte) (((byte) & 0x30) >> 4)
#define THIRDOF4(byte) (((byte) & 0x0C) >> 2)
#define FOURTHOF4(byte) ((byte) & 0x03)

static void StringFrom2na(const vector< char >& vec, string *str, bool isDNA)
{
    str->resize(vec.size() * 4);

    // first, extract 4-bit values
    int i;
    for (i=0; i<vec.size(); ++i) {
        str->at(4*i) = FIRSTOF4(vec[i]);
        str->at(4*i + 1) = SECONDOF4(vec[i]);
        str->at(4*i + 2) = THIRDOF4(vec[i]);
        str->at(4*i + 3) = FOURTHOF4(vec[i]);
    }

    // then convert 4-bit values to ascii characters
    for (i=0; i<str->size(); ++i) {
        switch (str->at(i)) {
            case 0: str->at(i) = 'A'; break;
            case 1: str->at(i) = 'C'; break;
            case 2: str->at(i) = 'G'; break;
            case 3: isDNA ? str->at(i) = 'T' : str->at(i) = 'U'; break;
        }
    }
}

static void StringFromStdaa(const vector < char >& vec, string *str)
{
    static const char *stdaaMap = "-ABCDEFGHIKLMNPQRSTVWXYZU*";

    str->resize(vec.size());
    for (int i=0; i<vec.size(); ++i)
        str->at(i) = stdaaMap[vec[i]];
}

Sequence::Sequence(SequenceSet *parent, ncbi::objects::CBioseq& bioseq) :
    StructureBase(parent), bioseqASN(&bioseq), molecule(NULL), isProtein(false)
{
    int gi = MoleculeIdentifier::VALUE_NOT_SET, mmdbID = MoleculeIdentifier::VALUE_NOT_SET,
        pdbChain = MoleculeIdentifier::VALUE_NOT_SET;
    string pdbID, accession;

    // get Seq-id info
    CBioseq::TId::const_iterator s, se = bioseq.GetId().end();
    for (s=bioseq.GetId().begin(); s!=se; ++s) {
        if ((*s)->IsGi()) {
            gi = (*s)->GetGi();
        } else if ((*s)->IsPdb()) {
            pdbID = (*s)->GetPdb().GetMol().Get();
            if ((*s)->GetPdb().IsSetChain())
                pdbChain = (*s)->GetPdb().GetChain();
            else
                pdbChain = ' ';
        } else if ((*s)->IsLocal() && (*s)->GetLocal().IsStr()) {
            accession = (*s)->GetLocal().GetStr();
            // special case where local accession is actually a PDB chain + extra stuff
            if (pdbID.size() == 0 && accession.size() >= 7 &&
                    accession[4] == ' ' && accession[6] == ' ' && isalpha(accession[5])) {
                pdbID = accession.substr(0, 4);
                pdbChain = accession[5];
                accession.erase();
            }
        } else if ((*s)->IsGenbank() && (*s)->GetGenbank().IsSetAccession()) {
            accession = (*s)->GetGenbank().GetAccession();
        } else if ((*s)->IsSwissprot() && (*s)->GetSwissprot().IsSetAccession()) {
            accession = (*s)->GetSwissprot().GetAccession();
        } else if ((*s)->IsOther() && (*s)->GetOther().IsSetAccession()) {
            accession = (*s)->GetOther().GetAccession();
        } else if ((*s)->IsEmbl() && (*s)->GetEmbl().IsSetAccession()) {
            accession = (*s)->GetEmbl().GetAccession();
        } else if ((*s)->IsDdbj() && (*s)->GetDdbj().IsSetAccession()) {
            accession = (*s)->GetDdbj().GetAccession();
        }
    }
    if (gi == MoleculeIdentifier::VALUE_NOT_SET && pdbID.size() == 0 && accession.size() == 0) {
        ERRORMSG("Sequence::Sequence() - can't parse SeqId");
        return;
    }

    if (bioseq.IsSetDescr()) {
        string defline, taxid;
        CSeq_descr::Tdata::const_iterator d, de = bioseq.GetDescr().Get().end();
        for (d=bioseq.GetDescr().Get().begin(); d!=de; ++d) {

            // get "defline" from title or compound
            if ((*d)->IsTitle()) {              // prefer title over compound
                defline = (*d)->GetTitle();
            } else if (defline.size() == 0 && (*d)->IsPdb() && (*d)->GetPdb().GetCompound().size() > 0) {
                defline = (*d)->GetPdb().GetCompound().front();
            }

            // get taxonomy
            if ((*d)->IsSource()) {
                if ((*d)->GetSource().GetOrg().IsSetTaxname())
                    taxid = (*d)->GetSource().GetOrg().GetTaxname();
                else if ((*d)->GetSource().GetOrg().IsSetCommon())
                    taxid = (*d)->GetSource().GetOrg().GetCommon();
            }
        }
        if (taxid.size() > 0)
            description = string("[") + taxid + ']';
        if (defline.size() > 0) {
            if (taxid.size() > 0) description += ' ';
            description += defline;
        }
    }

    // get link to MMDB id - mainly for CDD's where Biostrucs have to be loaded separately
    if (bioseq.IsSetAnnot()) {
        CBioseq::TAnnot::const_iterator a, ae = bioseq.GetAnnot().end();
        for (a=bioseq.GetAnnot().begin(); a!=ae; ++a) {
            if (a->GetObject().GetData().IsIds()) {
                CSeq_annot::C_Data::TIds::const_iterator i, ie = a->GetObject().GetData().GetIds().end();
                for (i=a->GetObject().GetData().GetIds().begin(); i!=ie; ++i) {
                    if (i->GetObject().IsGeneral() &&
                        i->GetObject().GetGeneral().GetDb() == "mmdb" &&
                        i->GetObject().GetGeneral().GetTag().IsId()) {
                        mmdbID = i->GetObject().GetGeneral().GetTag().GetId();
                        break;
                    }
                }
                if (i != ie) break;
            }
        }
    }
    if (mmdbID != MoleculeIdentifier::VALUE_NOT_SET)
        TRACEMSG("sequence gi " << gi << ", PDB '" << pdbID << "' chain '" << (char) pdbChain <<
            "', is from MMDB id " << mmdbID);

    // get sequence string
    if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_raw && bioseq.GetInst().IsSetSeq_data()) {

        // protein formats
        if (bioseq.GetInst().GetSeq_data().IsNcbieaa()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetNcbieaa().Get();
            isProtein = true;
        } else if (bioseq.GetInst().GetSeq_data().IsIupacaa()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetIupacaa().Get();
            isProtein = true;
        } else if (bioseq.GetInst().GetSeq_data().IsNcbistdaa()) {
            StringFromStdaa(bioseq.GetInst().GetSeq_data().GetNcbistdaa().Get(), &sequenceString);
            isProtein = true;
        }

        // nucleotide formats
        else if (bioseq.GetInst().GetSeq_data().IsIupacna()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetIupacna().Get();
            // convert 'T' to 'U' for RNA
            if (bioseq.GetInst().GetMol() == CSeq_inst::eMol_rna) {
                for (int i=0; i<sequenceString.size(); ++i) {
                    if (sequenceString[i] == 'T')
                        sequenceString[i] = 'U';
                }
            }
        } else if (bioseq.GetInst().GetSeq_data().IsNcbi4na()) {
            StringFrom4na(bioseq.GetInst().GetSeq_data().GetNcbi4na().Get(), &sequenceString,
                (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna));
        } else if (bioseq.GetInst().GetSeq_data().IsNcbi8na()) {  // same repr. for non-X as 4na
            StringFrom4na(bioseq.GetInst().GetSeq_data().GetNcbi8na().Get(), &sequenceString,
                (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna));
        } else if (bioseq.GetInst().GetSeq_data().IsNcbi2na()) {
            StringFrom2na(bioseq.GetInst().GetSeq_data().GetNcbi2na().Get(), &sequenceString,
                (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna));
            if (bioseq.GetInst().IsSetLength() && bioseq.GetInst().GetLength() < sequenceString.length())
                sequenceString.resize(bioseq.GetInst().GetLength());
        }

        else {
            ERRORMSG("Sequence::Sequence() - sequence " << gi
                << ": confused by sequence string format");
            return;
        }

        // check length
        if (bioseq.GetInst().IsSetLength() && bioseq.GetInst().GetLength() != sequenceString.length()) {
            ERRORMSG("Sequence::Sequence() - sequence string length mismatch");
            return;
        }

        // force uppercase
        for (int i=0; i<sequenceString.length(); ++i)
            sequenceString[i] = toupper(sequenceString[i]);

    } else {
        ERRORMSG("Sequence::Sequence() - sequence " << gi << ": confused by sequence representation");
        return;
    }

    // get identifier (may be NULL if there's a problem!)
    identifier = MoleculeIdentifier::GetIdentifier(this, pdbID, pdbChain, mmdbID, gi, accession);
}

void Sequence::AddMMDBAnnotTag(int mmdbID) const
{
    CBioseq::TAnnot::const_iterator a, ae = bioseqASN->GetAnnot().end();
    CSeq_annot::C_Data::TIds::const_iterator i, ie;
    bool found = false;
    for (a=bioseqASN->GetAnnot().begin(); a!=ae; ++a) {
        if ((*a)->GetData().IsIds()) {
            for (i=(*a)->GetData().GetIds().begin(), ie=(*a)->GetData().GetIds().end(); i!=ie; ++i) {
                if ((*i)->IsGeneral() && (*i)->GetGeneral().GetDb() == "mmdb" &&
                    (*i)->GetGeneral().GetTag().IsId())
                {
                    found = true;
                    TRACEMSG("mmdb link already present in sequence " << identifier->ToString());
                    if ((*i)->GetGeneral().GetTag().GetId() != mmdbID ||
                            (identifier->mmdbID != MoleculeIdentifier::VALUE_NOT_SET &&
                                identifier->mmdbID != mmdbID))
                        ERRORMSG("Sequence::AddMMDBAnnotTag() - mmdbID mismatch");
                    break;
                }
            }
        }
        if (found) break;
    }
    if (!found) {
        CRef < CSeq_id > seqid(new CSeq_id());
        seqid->SetGeneral().SetDb("mmdb");
        seqid->SetGeneral().SetTag().SetId(mmdbID);
        CRef < CSeq_annot > annot(new CSeq_annot());
        annot->SetData().SetIds().push_back(seqid);
        (const_cast<Sequence*>(this))->bioseqASN->SetAnnot().push_back(annot);
    }
}

CSeq_id * Sequence::CreateSeqId(void) const
{
    CSeq_id *sid = new CSeq_id();
    FillOutSeqId(sid);
    return sid;
}

void Sequence::FillOutSeqId(ncbi::objects::CSeq_id *sid) const
{
    sid->Reset();
    CBioseq::TId::const_iterator i, ie = bioseqASN->GetId().end();

    // use pdb id if present
    for (i=bioseqASN->GetId().begin(); i!=ie; ++i) {
        if ((*i)->IsPdb()) {
            sid->Assign(**i);
            return;
        }
    }

    // use gi if present
    for (i=bioseqASN->GetId().begin(); i!=ie; ++i) {
        if ((*i)->IsGi()) {
            sid->Assign(**i);
            return;
        }
    }

    // otherwise, just use the first one
    if (bioseqASN->GetId().size() > 0)
        sid->Assign(bioseqASN->GetId().front().GetObject());
    else
        ERRORMSG("Sequence::FillOutSeqId() - can't do Seq-id on sequence " << identifier->ToString());

    // dangerous to create new Seq-id's...
//    if (identifier->pdbID.size() > 0 && identifier->pdbChain != MoleculeIdentifier::VALUE_NOT_SET) {
//        sid->SetPdb().SetMol().Set(identifier->pdbID);
//        if (identifier->pdbChain != ' ') sid->SetPdb().SetChain(identifier->pdbChain);
//    } else if (identifier->gi != MoleculeIdentifier::VALUE_NOT_SET) { // use gi
//        sid->SetGi(identifier->gi);
//    } else if (identifier->accession.size() > 0) {
//        CObject_id *oid = new CObject_id();
//        oid->SetStr(identifier->accession);
//        sid->SetLocal(*oid);
}

void Sequence::AddCSeqId(SeqIdPtr *id, bool addAllTypes) const
{
    if (identifier->pdbID.size() > 0) {
        PDBSeqIdPtr pdbid = PDBSeqIdNew();
        pdbid->mol = StrSave(identifier->pdbID.c_str());
        pdbid->chain = (Uint1) identifier->pdbChain;
        ValNodeAddPointer(id, SEQID_PDB, pdbid);
        if (!addAllTypes) return;
    }
    if (identifier->gi != MoleculeIdentifier::VALUE_NOT_SET) {
        ValNodeAddInt(id, SEQID_GI, identifier->gi);
        if (!addAllTypes) return;
    }
    if (identifier->accession.size() > 0) {
        ObjectIdPtr local = ObjectIdNew();
        local->str = StrSave(identifier->accession.c_str());
        ValNodeAddPointer(id, SEQID_LOCAL, local);
        if (!addAllTypes) return;
    }
}

int Sequence::GetOrSetMMDBLink(void) const
{
    if (molecule) {
        const StructureObject *object;
        if (!molecule->GetParentOfType(&object)) return identifier->mmdbID;
        if (identifier->mmdbID != MoleculeIdentifier::VALUE_NOT_SET &&
            identifier->mmdbID != object->mmdbID)
            ERRORMSG("Sequence::GetOrSetMMDBLink() - mismatched MMDB ID: identifier says "
                << identifier->mmdbID << ", StructureObject says " << object->mmdbID);
        else
            const_cast<MoleculeIdentifier*>(identifier)->mmdbID = object->mmdbID;
    }
    return identifier->mmdbID;
}

void Sequence::LaunchWebBrowserWithInfo(void) const
{
    string db = isProtein ? "Protein" : "Nucleotide";
    string opt = isProtein ? "GenPept" : "GenBank";
    CNcbiOstrstream oss;
    oss << "http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Search&doptcmdl=" << opt
        << "&db=" << db << "&term=";
    // prefer gi's, since accessions can be outdated
    if (identifier->gi != MoleculeIdentifier::VALUE_NOT_SET) {
        oss << identifier->gi;
    } else if (identifier->pdbID.size() > 0) {
        oss << identifier->pdbID.c_str();
        if (identifier->pdbChain != ' ')
            oss << (char) identifier->pdbChain;
    } else if (identifier->accession.size() > 0) {
        if (identifier->accession == "query" || identifier->accession == "consensus") return;
        oss << identifier->accession.c_str();
    }
    oss << '\0';
    LaunchWebPage(oss.str());
    delete oss.str();
}

static bool Prosite2Regex(const string& prosite, string *regex, int *nGroups)
{
    try {
        // check allowed characters
        static const string allowed = "-ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[],(){}<>.";
        int i;
        for (i=0; i<prosite.size(); ++i)
            if (allowed.find(toupper(prosite[i])) == string::npos) break;
        if (i != prosite.size()) throw "invalid ProSite character";
        if (prosite[prosite.size() - 1] != '.') throw "ProSite pattern must end with '.'";

        // translate into real regex syntax;
        regex->erase();
        *nGroups = 0;

        bool inGroup = false;
        for (int i=0; i<prosite.size(); ++i) {

            // handle grouping and termini
            bool characterHandled = true;
            switch (prosite[i]) {
                case '-': case '.': case '>':
                    if (inGroup) {
                        *regex += ')';
                        inGroup = false;
                    }
                    if (prosite[i] == '>') *regex += '$';
                    break;
                case '<':
                    *regex += '^';
                    break;
                default:
                    characterHandled = false;
                    break;
            }
            if (characterHandled) continue;
            if (!inGroup && (
                    (isalpha(prosite[i]) && toupper(prosite[i]) != 'X') ||
                    prosite[i] == '[' || prosite[i] == '{')) {
                *regex += '(';
                (*nGroups)++;
                inGroup = true;
            }

            // translate syntax
            switch (prosite[i]) {
                case '(':
                    *regex += '{';
                    break;
                case ')':
                    *regex += '}';
                    break;
                case '{':
                    *regex += "[^";
                    break;
                case '}':
                    *regex += ']';
                    break;
                case 'X': case 'x':
                    *regex += '.';
                    break;
                default:
                    *regex += toupper(prosite[i]);
                    break;
            }
        }
    }

    catch (const char *err) {
        ERRORMSG("Prosite2Regex() - " << err);
        return false;
    }

    return true;
}

bool Sequence::HighlightPattern(const string& prositePattern) const
{
    // setup regex syntax
    reg_syntax_t newSyntax = RE_CONTEXT_INDEP_ANCHORS | RE_CONTEXT_INVALID_OPS | RE_INTERVALS |
        RE_LIMITED_OPS | RE_NO_BK_BRACES | RE_NO_BK_PARENS | RE_NO_EMPTY_RANGES;
    reg_syntax_t oldSyntax = re_set_syntax(newSyntax);

    bool retval = true;
    try {
        // allocate structures
        static re_pattern_buffer *patternBuffer = NULL;
        static re_registers *registers = NULL;
        if (!patternBuffer) {
            // new pattern initialized to zero
            patternBuffer = (re_pattern_buffer *) calloc(1, sizeof(re_pattern_buffer));
            if (!patternBuffer) throw "can't allocate pattern buffer";
            patternBuffer->fastmap = (char *) calloc(256, sizeof(char));
            if (!patternBuffer->fastmap) throw "can't allocate fastmap";
            registers = (re_registers *) calloc(1, sizeof(re_registers));
            if (!registers) throw "can't allocate registers";
            patternBuffer->regs_allocated = REGS_UNALLOCATED;
        }

        // update pattern buffer if not the same pattern as before
        static string previousPrositePattern;
        static int nGroups;
        int i;
        if (prositePattern != previousPrositePattern) {

            // convert from ProSite syntax
            string regexPattern;
            if (!Prosite2Regex(prositePattern, &regexPattern, &nGroups))
                throw "error converting ProSite to regex syntax";

            // create pattern buffer
            TRACEMSG("compiling pattern '" << regexPattern << "'");
            const char *error = re_compile_pattern(regexPattern.c_str(), regexPattern.size(), patternBuffer);
            if (error) throw error;

            // optimize pattern buffer
            int err = re_compile_fastmap(patternBuffer);
            if (err) throw "re_compile_fastmap internal error";

            previousPrositePattern = prositePattern;
        }

        // do the search, finding all non-overlapping matches
        int start = 0;
        while (start < Length()) {

            int result = re_search(patternBuffer, sequenceString.c_str(),
                Length(), start, Length(), registers);
            if (result == -1)
                break;
            else if (result == -2)
                throw "re_search internal error";

            // re_search gives the longest hit, but we want the shortest; so try to find the
            // shortest hit within the hit already found by limiting the length of the string
            // allowed to be included in the search. (This isn't very efficient! but
            // the regex API doesn't have an option for finding the shortest hit...)
            int stringSize = start + 1;
            while (stringSize <= Length()) {
                result = re_search(patternBuffer, sequenceString.c_str(),
                    stringSize, start, stringSize, registers);
                if (result >= 0) break;
                ++stringSize;
            }

            // parse the match registers, highlight ranges
//            TESTMSG("found match starting at " << identifier->ToString() << " loc " << result+1);
            int lastMatched = result;
            for (i=1; i<registers->num_regs; ++i) {
                int from = registers->start[i], to = registers->end[i] - 1;
                if (from >= 0 && to >= 0) {
                    if (to > lastMatched) lastMatched = to;

                    // highlight this ranage
//                    TESTMSG("register " << i << ": from " << from+1 << " to " << to+1);
                    GlobalMessenger()->AddHighlights(this, from, to);
                }
            }

            start = lastMatched + 1;
        }

    } catch (const char *err) {
        ERRORMSG("Sequence::HighlightPattern() - " << err);
        retval = false;
    }

    // cleanup
    re_set_syntax(oldSyntax);

    return retval;
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.68  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.67  2004/05/21 17:29:51  thiessen
* allow conversion of mime to cdd data
*
* Revision 1.66  2004/03/15 18:27:12  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.65  2004/03/01 22:56:09  thiessen
* convert 'T' to 'U' for RNA with IUPACna
*
* Revision 1.64  2004/02/19 17:05:06  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.63  2004/01/05 17:09:16  thiessen
* abort import and warn if same accession different gi
*
* Revision 1.62  2003/11/26 20:37:54  thiessen
* prefer gi for URLs
*
* Revision 1.61  2003/11/20 22:08:49  thiessen
* update Entrez url
*
* Revision 1.60  2003/09/03 18:14:01  thiessen
* hopefully fix Seq-id issues
*
* Revision 1.59  2003/08/30 14:01:15  thiessen
* use existing CSeq_id instead of creating new one
*
* Revision 1.58  2003/07/14 18:37:08  thiessen
* change GetUngappedAlignedBlocks() param types; other syntax changes
*
* Revision 1.57  2003/03/13 14:26:18  thiessen
* add file_messaging module; split cn3d_main_wxwin into cn3d_app, cn3d_glcanvas, structure_window, cn3d_tools
*
* Revision 1.56  2003/02/03 19:20:05  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.55  2002/12/12 15:04:11  thiessen
* fix for report launch on nucleotides
*
* Revision 1.54  2002/12/09 16:25:04  thiessen
* allow negative score threshholds
*
* Revision 1.53  2002/11/22 19:54:29  thiessen
* fixes for wxMac/OSX
*
* Revision 1.52  2002/11/19 21:19:44  thiessen
* more const changes for objects; fix user vs default style bug
*
* Revision 1.51  2002/11/06 00:18:10  thiessen
* fixes for new CRef/const rules in objects
*
* Revision 1.50  2002/10/11 17:21:39  thiessen
* initial Mac OSX build
*
* Revision 1.49  2002/09/13 14:21:45  thiessen
* finish hooking up browser launch on unix
*
* Revision 1.48  2002/09/06 18:56:48  thiessen
* add taxonomy to description
*
* Revision 1.47  2002/09/05 17:14:14  thiessen
* create new netscape window on unix
*
* Revision 1.46  2002/07/12 13:24:10  thiessen
* fixes for PSSM creation to agree with cddumper/RPSBLAST
*
* Revision 1.45  2002/01/24 20:08:17  thiessen
* fix local id problem
*
* Revision 1.44  2002/01/19 02:34:46  thiessen
* fixes for changes in asn serialization API
*
* Revision 1.43  2002/01/11 15:49:01  thiessen
* update for Mac CW7
*
* Revision 1.42  2001/11/30 14:59:55  thiessen
* add netscape launch for Mac
*
* Revision 1.41  2001/11/30 14:02:05  thiessen
* progress on sequence imports to single structures
*
* Revision 1.40  2001/11/27 16:26:09  thiessen
* major update to data management system
*
* Revision 1.39  2001/09/27 15:37:59  thiessen
* decouple sequence import and BLAST
*
* Revision 1.38  2001/08/21 01:10:31  thiessen
* fix lit. launch for nucleotides
*
* Revision 1.37  2001/08/09 19:07:13  thiessen
* add temperature and hydrophobicity coloring
*
* Revision 1.36  2001/07/24 15:02:59  thiessen
* use ProSite syntax for pattern searches
*
* Revision 1.35  2001/07/23 20:09:23  thiessen
* add regex pattern search
*
* Revision 1.34  2001/07/19 19:14:38  thiessen
* working CDD alignment annotator ; misc tweaks
*
* Revision 1.33  2001/07/10 16:39:55  thiessen
* change selection control keys; add CDD name/notes dialogs
*
* Revision 1.32  2001/06/21 02:02:34  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.31  2001/06/18 21:48:27  thiessen
* add [PACC] to Entrez query for PDB id's
*
* Revision 1.30  2001/06/02 17:22:46  thiessen
* fixes for GCC
*
* Revision 1.29  2001/05/31 18:47:09  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.28  2001/05/31 14:32:03  thiessen
* better netscape launch for unix
*
* Revision 1.27  2001/05/25 01:38:16  thiessen
* minor fixes for compiling on SGI
*
* Revision 1.26  2001/05/24 13:32:32  thiessen
* further tweaks for GTK
*
* Revision 1.25  2001/05/15 23:48:37  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.24  2001/05/02 16:35:15  thiessen
* launch entrez web page on sequence identifier
*
* Revision 1.23  2001/04/20 18:03:22  thiessen
* add ncbistdaa parsing
*
* Revision 1.22  2001/04/18 15:46:53  thiessen
* show description, length, and PDB numbering in status line
*
* Revision 1.21  2001/03/28 23:02:17  thiessen
* first working full threading
*
* Revision 1.20  2001/02/16 00:40:01  thiessen
* remove unused sequences from asn data
*
* Revision 1.19  2001/02/13 01:03:57  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.18  2001/02/08 23:01:50  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.17  2001/01/25 20:21:18  thiessen
* fix ostrstream memory leaks
*
* Revision 1.16  2001/01/09 21:45:00  thiessen
* always use pdbID as title if known
*
* Revision 1.15  2001/01/04 18:20:52  thiessen
* deal with accession seq-id
*
* Revision 1.14  2000/12/29 19:23:39  thiessen
* save row order
*
* Revision 1.13  2000/12/28 20:43:09  vakatov
* Do not use "set" as identifier.
* Do not include <strstream>. Use "CNcbiOstrstream" instead of "ostrstream".
*
* Revision 1.12  2000/12/21 23:42:15  thiessen
* load structures from cdd's
*
* Revision 1.11  2000/12/20 23:47:48  thiessen
* load CDD's
*
* Revision 1.10  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.9  2000/11/17 19:48:13  thiessen
* working show/hide alignment row
*
* Revision 1.8  2000/11/11 21:15:54  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.7  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.6  2000/09/03 18:46:49  thiessen
* working generalized sequence viewer
*
* Revision 1.5  2000/08/30 23:46:27  thiessen
* working alignment display
*
* Revision 1.4  2000/08/30 19:48:41  thiessen
* working sequence window
*
* Revision 1.3  2000/08/28 23:47:19  thiessen
* functional denseg and dendiag alignment parsing
*
* Revision 1.2  2000/08/28 18:52:42  thiessen
* start unpacking alignments
*
* Revision 1.1  2000/08/27 18:52:22  thiessen
* extract sequence information
*
*/
