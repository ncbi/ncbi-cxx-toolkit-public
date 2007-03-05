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
#include <util/regexp.hpp>

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
    } else {
        string label = identifier->GetLabel();
        if (label == "query" || label == "consensus")
            return;
        oss << label;
    }
    LaunchWebPage(((string) CNcbiOstrstreamToString(oss)).c_str());
}

static bool Prosite2Regex(const string& prosite, string *regex, int *nGroups)
{
    try {
        // check allowed characters
        static const string allowed = "-ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[],(){}<>.";
        unsigned int i;
        for (i=0; i<prosite.size(); ++i)
            if (allowed.find(toupper((unsigned char) prosite[i])) == string::npos) break;
        if (i != prosite.size()) throw "invalid ProSite character";
        if (prosite[prosite.size() - 1] != '.') throw "ProSite pattern must end with '.'";

        // translate into real regex syntax;
        regex->erase();
        *nGroups = 0;

        bool inGroup = false;
        for (unsigned int i=0; i<prosite.size(); ++i) {

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
                    (isalpha((unsigned char) prosite[i]) && toupper((unsigned char) prosite[i]) != 'X') ||
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
                    *regex += toupper((unsigned char) prosite[i]);
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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.79  2006/07/26 22:21:07  thiessen
* adjust for 28-letter ncbistdaa
*
* Revision 1.78  2006/05/30 22:21:29  thiessen
* fix warning
*
* Revision 1.77  2006/05/30 21:41:21  thiessen
* add pattern search within selection
*
* Revision 1.76  2006/02/16 23:13:39  thiessen
* better handling of non-standard fasta deflines
*
* Revision 1.75  2005/11/04 20:45:32  thiessen
* major reorganization to remove all C-toolkit dependencies
*
* Revision 1.74  2005/10/27 22:53:03  thiessen
* better handling of sequence descriptions
*
* Revision 1.73  2005/10/26 18:36:05  thiessen
* minor fixes
*
* Revision 1.72  2005/10/22 02:50:34  thiessen
* deal with memory issues, mostly in ostrstream->string conversion
*
* Revision 1.71  2005/10/19 17:28:19  thiessen
* migrate to wxWidgets 2.6.2; handle signed/unsigned issue
*
* Revision 1.70  2005/06/03 16:26:44  lavr
* Explicit (unsigned char) casts in ctype routines
*
* Revision 1.69  2004/09/27 23:35:17  thiessen
* don't abort on sequence import if gi/acc mismatch, but only on original load
*
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
