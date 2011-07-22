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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistl.hpp>
#include <util/xregexp/regexp.hpp>

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

#include "remove_header_conflicts.hpp"

#include "sequence_set.hpp"
#include "molecule.hpp"
#include "structure_set.hpp"
#include "cn3d_tools.hpp"
#include "molecule_identifier.hpp"
#include "messenger.hpp"
#include "chemical_graph.hpp"

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
            if (!sequence || !sequence->identifier)
                FATALMSG("Can't create Sequence object, aborting...");
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
        if (!sequence || !sequence->identifier)
            FATALMSG("Can't create Sequence object, aborting...");
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

const Sequence * SequenceSet::FindMatchingSequence(const ncbi::objects::CBioseq::TId& ids) const
{
    SequenceList::const_iterator s, se = sequences.end();
    for (s=sequences.begin(); s!=se; ++s) {
        CBioseq::TId::const_iterator i, ie = ids.end();
        for (i=ids.begin(); i!=ie; ++i)
            if ((*s)->identifier->MatchesSeqId(**i))
                return *s;
    }
    return NULL;
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
    unsigned int i;
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
    unsigned int i;
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
    str->resize(vec.size());
    for (unsigned int i=0; i<vec.size(); ++i)
        str->at(i) = LookupCharacterFromNCBIStdaaNumber(vec[i]);
}

Sequence::Sequence(SequenceSet *parent, ncbi::objects::CBioseq& bioseq) :
    StructureBase(parent), bioseqASN(&bioseq), identifier(NULL), molecule(NULL), isProtein(false)
{
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
            taxonomy = string("[") + taxid + ']';
        if (defline.size() > 0) {
            title = defline;
            // remove taxonomy repeated at end of title
            if (taxonomy.size() > 0 && NStr::EndsWith(title, taxonomy, NStr::eNocase))
                title = title.substr(0, title.size() - taxonomy.size());
            if (title[title.size() - 1] == ' ')
                title = title.substr(0, title.size() - 1);
        }
    }

    // get link to MMDB id - mainly for CDD's where Biostrucs have to be loaded separately
    int mmdbID = MoleculeIdentifier::VALUE_NOT_SET;
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
                for (unsigned int i=0; i<sequenceString.size(); ++i) {
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
            ERRORMSG("Sequence::Sequence() - sequence " << bioseq.GetId().front()->GetSeqIdString()
                << ": confused by sequence string format");
            return;
        }

        // check length
        if (bioseq.GetInst().IsSetLength() && bioseq.GetInst().GetLength() != sequenceString.length()) {
            ERRORMSG("Sequence::Sequence() - sequence string length mismatch");
            return;
        }

        // force uppercase
        for (unsigned int i=0; i<sequenceString.length(); ++i)
            sequenceString[i] = toupper((unsigned char) sequenceString[i]);

    } else {
        ERRORMSG("Sequence::Sequence() - sequence " << bioseq.GetId().front()->GetSeqIdString() << ": confused by sequence representation");
        return;
    }

    // get identifier (may be NULL if there's a problem!)
    identifier = MoleculeIdentifier::GetIdentifier(this, mmdbID, bioseq.GetId());
}

string Sequence::GetDescription(void) const
{
    string descr;
    if (taxonomy.size() > 0)
        descr = taxonomy;
    if (title.size() > 0) {
        if (descr.size() > 0)
            descr += ' ';
        descr += title;
    } else if (molecule) {
        const StructureObject *object;
        if (molecule->GetParentOfType(&object)) {
            if (object->graph->name.size() > 0) {
                if (descr.size() > 0)
                    descr += ' ';
                descr += object->graph->name;
            }
        }
    }
    return descr;
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
    CNcbiOstrstream oss;
    oss << "http://www.ncbi.nlm.nih.gov/" << (isProtein ? "protein" : "nuccore") << "/";
    // prefer gi's, since accessions can be outdated
    if (identifier->gi != MoleculeIdentifier::VALUE_NOT_SET) {
        oss << identifier->gi;
    } else if (identifier->pdbID.size() > 0) {
        oss << identifier->pdbID.c_str();
        if (identifier->pdbChain != ' ')
            oss << '_' << (char) identifier->pdbChain;
    } else {
        string label = identifier->GetLabel();
        if (label == "query" || label == "consensus")
            return;
        oss << label;
    }
    LaunchWebPage(((string) CNcbiOstrstreamToString(oss)).c_str());
}

bool Sequence::HighlightPattern(const string& prositePattern, const MoleculeHighlightMap& restrictTo) const
{
    bool retval = true;
    try {

        // update CRegexp if not the same pattern as before
        static auto_ptr < CRegexp > regexp;
        static string previousPrositePattern;
        static int nGroups;
        if (!regexp.get() || prositePattern != previousPrositePattern) {

            // convert from ProSite syntax
            string regexPattern;
            if (!Prosite2Regex(prositePattern, &regexPattern, &nGroups))
                throw "error converting ProSite to regex syntax";

            // create pattern buffer
            TRACEMSG("creating CRegexp with pattern '" << regexPattern << "'");
            regexp.reset(new CRegexp(regexPattern, CRegexp::fCompile_ungreedy));

            previousPrositePattern = prositePattern;
        }

        // do the search, finding all non-overlapping matches
        int i, start = 0;
        while (start < (int)Length()) {

            // do the search
            string ignore = regexp->GetMatch(sequenceString, start, 0, CRegexp::fMatch_default, true);
            if (regexp->NumFound() <= 0)
                break;

//            TRACEMSG("got match, num (sub)patterns: " << regexp->NumFound());
//            for (i=0; i<regexp->NumFound(); ++i)
//                TRACEMSG("    " << i << ": " << (regexp->GetResults(i)[0] + 1) << '-' << regexp->GetResults(i)[1]);

            // check to see if this entire match is within the restricted set
            bool addMatch = true;
            if (restrictTo.size() > 0) {
                MoleculeHighlightMap::const_iterator r = restrictTo.find(identifier);
                if (r != restrictTo.end()) {
                    for (i=1; i<regexp->NumFound(); ++i) {
                        for (int j=regexp->GetResults(i)[0]; j<=regexp->GetResults(i)[1]-1; ++j) {
                            if (!r->second[j]) {
                                addMatch = false;
                                break;
                            }
                        }
                        if (!addMatch)
                            break;
                    }
                } else
                    addMatch = false;
            }

            // parse the match subpatterns, highlight each subpattern range
            if (addMatch)
                for (i=1; i<regexp->NumFound(); ++i)
                    GlobalMessenger()->AddHighlights(this, regexp->GetResults(i)[0], regexp->GetResults(i)[1] - 1);

            // start next search after the end of this one
            start = regexp->GetResults(regexp->NumFound() - 1)[1];
        }

    } catch (const char *err) {
        ERRORMSG("Sequence::HighlightPattern() - " << err);
        retval = false;
    } catch (exception& e) {
        ERRORMSG("Sequence::HighlightPattern() - caught exception: " << e.what());
        retval = false;
    }

    return retval;
}

END_SCOPE(Cn3D)
