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
 * Authors:  Josh Cherry
 *
 * File Description:
 *   Read an AGP file, build Seq-entry's, and do some validation
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/readers/fasta.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objtools/readers/agp_read.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbiexec.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CAgpconvertApplication::


class CAgpconvertApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CAgpconvertApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "AGP file converter program");

    // Describe the expected command-line arguments

    arg_desc->AddKey("template", "template_file",
                     "A template Seq-entry in ASN.1 text",
                     CArgDescriptions::eInputFile);
    arg_desc->AddFlag("fuzz100", "For gaps of length 100, "
                      "put an Int-fuzz = unk in the literal");
    arg_desc->AddOptionalKey("components", "components_file",
                             "Bioseq-set of components, used for "
                             "validation",
                             CArgDescriptions::eInputFile);
    arg_desc->AddFlag("fasta_id", "Parse object ids (col. 1) "
                      "as fasta-style ids if they contain '|'");
    arg_desc->AddOptionalKey("chromosomes", "chromosome_name_file",
                             "Mapping of col. 1 names to chromsome "
                             "names, for use as SubSource",
                             CArgDescriptions::eInputFile);
    arg_desc->AddExtra
        (1, 100, "AGP files to process",
         CArgDescriptions::eInputFile);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


// This is a ridiculous way of parsing
// strings with multiple Seq-id's, such as
// gnl|WGS:AABU|211000022280667|gb|AABU01002637
// (make a fasta stream and read it)
static void s_ParseFastaIds(const string& s, list<CRef<CSeq_id> >& ids)
{
    string fasta_input(">" + s + "\na\n");
    CNcbiIstrstream istr(fasta_input.c_str());
    CRef<CSeq_entry> ent = ReadFasta(istr);
    ids = ent->GetSeq().GetId();
}


/////////////////////////////////////////////////////////////////////////////
//  Run


int CAgpconvertApplication::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();

    CSeq_entry templ;
    args["template"].AsInputFile() >> MSerial_AsnText >> templ;

    // if validating against a file containing
    // sequence components, load it and make
    // a mapping of ids to lengths
    map<string, TSeqPos> comp_lengths;
    if (args["components"]) {
        CBioseq_set seq_set;
        args["components"].AsInputFile() >> MSerial_AsnText >> seq_set;
        ITERATE (CBioseq_set::TSeq_set, ent, seq_set.GetSeq_set()) {
            TSeqPos length = (*ent)->GetSeq().GetInst().GetLength();
            ITERATE (CBioseq::TId, id, (*ent)->GetSeq().GetId()) {
                comp_lengths[(*id)->AsFastaString()] = length;
            }
        }
    }

    // if requested, load a file of mappings of 
    // object identifiers to chromsome names
    map<string, string> chr_names;
    if (args["chromosomes"]) {
        CNcbiIstream& istr = args["chromosomes"].AsInputFile();
        string line;
        while (!istr.eof()) {
            NcbiGetlineEOL(istr, line);
            list<string> split_line;
            NStr::Split(line, " \t", split_line);
            if (split_line.size() != 2) {
                throw runtime_error("line of chromosome file does not have "
                                    "two columns: " + line);
            }
            string id = split_line.front();
            string chr = split_line.back();
            if (chr_names.find(id) != chr_names.end()
                && chr_names[id] != chr) {
                throw runtime_error("inconsistent chromosome for " + id +
                                    " in chromsome file");
            }
            chr_names[id] = chr;
        }
    }

    for (unsigned int i = 1; i <= args.GetNExtra(); ++i) {

        CNcbiIstream& istr = args[i].AsInputFile();
        CRef<CBioseq_set> big_set = AgpRead(istr);

        ITERATE (CBioseq_set::TSeq_set, ent, big_set->GetSeq_set()) {

            // insert sequence instance and id into a copy of template
            const CBioseq& seq = (*ent)->GetSeq();
            CRef<CSeq_id> id(new CSeq_id);
            id->Assign(*seq.GetFirstId());
            string col1;
            if (id->GetLocal().IsStr()) {
                col1 = id->GetLocal().GetStr();
            } else {
                col1 = NStr::IntToString(id->GetLocal().GetId());
            }
            string id_str = col1;
            list<CRef<CSeq_id> > ids;
            ids.push_back(id);
            if (NStr::Find(id_str, "|") != NPOS) {
                if (args["fasta_id"]) {
                    // parse the id as a fasta id
                    s_ParseFastaIds(id_str, ids);
                    id_str = ids.front()->GetSeqIdString(true);
                } else {
                    cerr << "** ID " << id_str <<
                        " contains a '|'; consider using the -fasta_id option"
                         << endl;
                }
            }
            CSeq_entry new_entry;
            new_entry.Assign(templ);
            new_entry.SetSeq().SetInst().Assign(seq.GetInst());
            new_entry.SetSeq().ResetId();
            ITERATE (list<CRef<CSeq_id> >, an_id, ids) {
                new_entry.SetSeq().SetId().push_back(*an_id);
            }

            // if requested, put an Int-fuzz = unk for
            // all literals of length 100
            if (args["fuzz100"]) {
                NON_CONST_ITERATE (CDelta_ext::Tdata, delta,
                         new_entry.SetSeq().SetInst()
                         .SetExt().SetDelta().Set()) {
                    if ((*delta)->IsLiteral() &&
                        (*delta)->GetLiteral().GetLength() == 100) {
                        (*delta)->SetLiteral().SetFuzz().SetLim();
                    }
                }
            }

            // if requested, verify against known sequence components
            if (args["components"]) {
                bool failure = false;
                ITERATE (CDelta_ext::Tdata, delta,
                         new_entry.GetSeq().GetInst()
                         .GetExt().GetDelta().Get()) {
                    if ((*delta)->IsLoc()) {
                        string comp_id_str = 
                            (*delta)->GetLoc().GetInt().GetId().AsFastaString();
                        if (comp_lengths.find(comp_id_str)
                            == comp_lengths.end()) {
                            failure = true;
                            cout << "** Component " << comp_id_str << 
                                " of entry " << id_str << " not found" << endl;
                        } else {
                            TSeqPos to = (*delta)->GetLoc().GetInt().GetTo();
                            if (to >= comp_lengths[comp_id_str]) {
                                failure = true;
                                cout << "** Component " << comp_id_str <<
                                    " of entry " << id_str << " not long enough"
                                     << endl;
                                cout << "** Length is " << 
                                    comp_lengths[comp_id_str] <<
                                    "; requested \"to\" is " << to << endl;
                            }
                        }
                    }
                }

                if (failure) {
                    cout << "** Not writing entry " << id_str
                         << " due to failed validation" << endl;
                    continue;
                }
            }

            // if requested, set chromosome name in source subtype
            if (args["chromosomes"]) {
                CRef<CSubSource> sub_source(new CSubSource);
                sub_source->SetSubtype(CSubSource::eSubtype_chromosome);
                sub_source->SetName(chr_names[col1]);
                vector<CRef<CSeqdesc> > source_descs;
                ITERATE (CSeq_descr::Tdata, desc,
                         new_entry.GetSeq().GetDescr().Get()) {
                    if ((*desc)->IsSource()) {
                        source_descs.push_back(*desc);
                    }
                }
                if (source_descs.size() != 1) {
                    throw runtime_error("found " +
                                        NStr::IntToString(source_descs.size()) +
                                        "Source Desc's; expected exactly one");
                }
                CSeqdesc& source_desc = *source_descs[0];
                if (source_desc.GetSource().IsSetSubtype()) {
                    cout << "** warning: overriding BioSource subtype "
                        "from template for " << col1 << endl;
                    source_desc.SetSource().SetSubtype().clear();
                }
                source_desc.SetSource().SetSubtype().push_back(sub_source);
            }
        
            // set create and update dates to today
            CRef<CDate> date(new CDate);
            date->SetToTime(CurrentTime(), CDate::ePrecision_day);
            CRef<CSeqdesc> update_date(new CSeqdesc);
            update_date->SetUpdate_date(*date);
            new_entry.SetSeq().SetDescr().Set().push_back(update_date);
            CRef<CSeqdesc> create_date(new CSeqdesc);
            create_date->SetCreate_date(*date);
            new_entry.SetSeq().SetDescr().Set().push_back(create_date);

            // write the entry in asn text
            string outfname = id_str + ".ent";
            CNcbiOfstream ostr(outfname.c_str());
            ostr << MSerial_AsnText << new_entry;
            ostr.close();  // testval reads this file

            // verify using testval
            string cmd = "testval -e -q 2 -i \"" + outfname + "\"";
            cout << cmd << endl;
            CExec::System(cmd.c_str());
        }
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CAgpconvertApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAgpconvertApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/02 18:37:34  jcherry
 * Initial version
 *
 * ===========================================================================
 */
