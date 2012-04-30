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
 *   Read an AGP file, build Seq-entry's or Seq-submit's,
 *   and do some validation
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <serial/serial.hpp>
#include <serial/objostr.hpp>

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
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include <algorithm>

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
                     "A template Seq-entry or Seq-submit in ASN.1 text",
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
    arg_desc->AddFlag("no_asnval",
                      "Do not validate using asnval");
    arg_desc->AddFlag("no_testval",
                      "Equivalent to -no_asnval, for backward compatibility");
    arg_desc->AddOptionalKey("outdir", "output_directory",
                             "Directory for output files "
                             "(defaults to current directory)",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("ofs", "ofs",
                             "Output filename suffix "
                             "(default is \".ent\" for Seq-entry "
                             "or \".sqn\" for Seq-submit",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("stdout", "Write to stdout rather than files.  "
                      "Implies -no_asnval.");
    arg_desc->AddFlag("gap-info",
                      "Set Seq-gap (gap type and linkage) in delta sequence");


    arg_desc->AddOptionalKey("dl", "definition_line",
                             "Definition line (title descriptor)",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("nt", "tax_id",
                             "NCBI Taxonomy Database ID",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("on", "org_name",
                             "Organism name",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("sn", "strain_name",
                             "Strain name",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("cm", "chromosome",
                             "Chromosome (for BioSource.subtype)",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("cn", "clone",
                             "Clone (for BioSource.subtype)",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("cl", "clone_lib",
                             "Clone library (for BioSource.subtype)",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("sc", "source_comment",
                             "Source comment (for BioSource.subtype = other)",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("ht", "haplotype",
                             "Haplotype (for BioSource.subtype)",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("sex", "sex",
                             "Sex/gender (for BioSource.subtype)",
                             CArgDescriptions::eString);


    arg_desc->AddExtra
        (1, 32766, "AGP files to process",
         CArgDescriptions::eInputFile);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


static int s_GetTaxid(const COrg_ref& org_ref) {
    int taxid = 0;
    int count = 0;
    ITERATE (COrg_ref::TDb, db_tag, org_ref.GetDb()) {
        if ((*db_tag)->GetDb() == "taxon") {
            count++;
            taxid = (*db_tag)->GetTag().GetId();
        }
    }
    if (count != 1) {
        throw runtime_error("found " + NStr::IntToString(count) + " taxids; "
                            "expected exactly one");
    }
    return taxid;
}


// Helper for removing old-name OrgMod's
struct SIsOldName
{
    bool operator()(CRef<COrgMod> mod)
    {
        return mod->GetSubtype() == COrgMod::eSubtype_old_name;
    }
};


/////////////////////////////////////////////////////////////////////////////
//  Run


int CAgpconvertApplication::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();

    CSeq_entry ent_templ;
    CRef<CSeq_submit> submit_templ;  // may not be used
    bool output_seq_submit = false;  // whether the output should be a
                                     // Seq-submit (rather than a Seq-entry)
    bool use_gap_info;               // Whether to incorporate some info
                                     // from the AGP file as Seq-gap's

    try {
        // a Seq-entry?
        args["template"].AsInputFile() >> MSerial_AsnText >> ent_templ;
    } catch (...) {
        // a Seq-submit or Submit-block?
        submit_templ.Reset(new CSeq_submit);
        CNcbiIfstream istr(args["template"].AsString().c_str());
        try {
            // a Seq-submit?
            istr >> MSerial_AsnText >> *submit_templ;
            if (!submit_templ->GetData().IsEntrys()
                || submit_templ->GetData().GetEntrys().size() != 1) {
                throw runtime_error("Seq-submit template must contain "
                                    "exactly one Seq-entry");
            }
        } catch(...) {
            // a Submit-block?
            CRef<CSubmit_block> submit_block(new CSubmit_block);
            istr.seekg(0);
            istr >> MSerial_AsnText >> *submit_block;

            // Build a Seq-submit containing this plus a bogus Seq-entry
            submit_templ->SetSub(*submit_block);
            CRef<CSeq_entry> ent(new CSeq_entry);
            CRef<CSeq_id> dummy_id(new CSeq_id("lcl|dummy_id"));
            ent->SetSeq().SetId().push_back(dummy_id);
            ent->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
            ent->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
            submit_templ->SetData().SetEntrys().push_back(ent);

            output_seq_submit = true;
        }
        ent_templ.Assign(*submit_templ->GetData().GetEntrys().front());
        // The template may contain a set rather than a seq.
        // That's OK if it contains only one na entry, which we'll use.
        if (ent_templ.IsSet()) {
            unsigned int num_nuc_ents = 0;
            CSeq_entry tmp;
            ITERATE (CBioseq_set::TSeq_set, ent_iter,
                     ent_templ.GetSet().GetSeq_set()) {
                if ((*ent_iter)->GetSeq().GetInst().IsNa()) {
                    ++num_nuc_ents;
                    tmp.Assign(**ent_iter);
                    // Copy any descriptors from the set to the sequence
                    ITERATE (CBioseq_set::TDescr::Tdata, desc_iter,
                             ent_templ.GetSet().GetDescr().Get()) {
                        CRef<CSeqdesc> desc(new CSeqdesc);
                        desc->Assign(**desc_iter);
                        tmp.SetSeq().SetDescr().Set().push_back(desc);
                    }
                }
            }
            if (num_nuc_ents == 1) {
                ent_templ.Assign(tmp);
            } else {
                throw runtime_error("template contains "
                                    + NStr::IntToString(num_nuc_ents)
                                    + " nuc. Seq-entrys; should contain 1");
            }
        }
        // incorporate any Seqdesc's that follow in the file
        while (true) {
            try {
                CRef<CSeqdesc> desc(new CSeqdesc);
                istr >> MSerial_AsnText >> *desc;
                ent_templ.SetSeq().SetDescr().Set().push_back(desc);
            } catch (...) {
                break;
            }
        }
        if (!output_seq_submit) {
            // Take Seq-submit.sub.cit and put it in the Bioseq
            CRef<CPub> pub(new CPub);
            pub->SetSub().Assign(submit_templ->GetSub().GetCit());
            CRef<CSeqdesc> pub_desc(new CSeqdesc);
            pub_desc->SetPub().SetPub().Set().push_back(pub);
            ent_templ.SetSeq().SetDescr().Set().push_back(pub_desc);
        }
    }

    ent_templ.SetSeq().ResetAnnot();  // don't use any annots in the template

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

    // user may specify directory for output files
    string outdir;
    if (args["outdir"]) {
        outdir = args["outdir"].AsString();
        CDirEntry dir_ent(outdir);
        if (!dir_ent.Exists()) {
            throw runtime_error(outdir + " does not exist");
        }
        if (!dir_ent.IsDir()) {
            throw runtime_error(outdir + " is not a directory");
        }
    }

    // if requested, load a file of mappings of
    // object identifiers to chromsome names
    map<string, string> chr_names;
    if (args["chromosomes"]) {
        // Make sure there's not already a chromosome in the template
        ITERATE (CSeq_descr::Tdata, desc,
                 ent_templ.GetSeq().GetDescr().Get()) {
            if ((*desc)->IsSource() && (*desc)->GetSource().IsSetSubtype()) {
                ITERATE (CBioSource::TSubtype, sub_type,
                         (*desc)->GetSource().GetSubtype()) {
                    if ((*sub_type)->GetSubtype() ==
                        CSubSource::eSubtype_chromosome) {
                        throw runtime_error("-chromosomes given but template "
                                            "contains a chromosome SubSource");
                    }
                }
            }
        }

        // Load from file
        CNcbiIstream& istr = args["chromosomes"].AsInputFile();
        string line;
        while (!istr.eof()) {
            NcbiGetlineEOL(istr, line);
            if (line.empty()) {
                continue;
            }
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

    // Deal with any descriptor info from command line
    if (args["dl"]) {
        const string& dl = args["dl"].AsString();
        ITERATE (CSeq_descr::Tdata, desc,
                 ent_templ.GetSeq().GetDescr().Get()) {
            if ((*desc)->IsTitle()) {
                throw runtime_error("-dl given but template contains a title");
            }
        }
        CRef<CSeqdesc> title_desc(new CSeqdesc);
        title_desc->SetTitle(dl);
        ent_templ.SetSeq().SetDescr().Set().push_back(title_desc);
    }
    if (args["nt"] || args["on"] || args["sn"] || args["cm"]
        || args["cn"] || args["cl"] || args["sc"]
        || args["ht"] || args["sex"]) {
        // consistency checks
        ITERATE (CSeq_descr::Tdata, desc,
                 ent_templ.GetSeq().GetDescr().Get()) {
            if ((*desc)->IsSource()) {
                throw runtime_error("BioSource specified on command line but "
                                    "template contains BioSource");
            }
        }
        if (args["chromosomes"]) {
            throw runtime_error("-chromosomes cannot be given with "
                                "-nt, -on, -sn, -cm, -cn, -cl, -sc, -ht or -sex");
        }

        // build a BioSource desc and add it to template
        CRef<CSeqdesc> source_desc(new CSeqdesc);
        CRef<CSubSource> sub_source;
        if (args["cm"]) {
            sub_source = new CSubSource;
            sub_source->SetSubtype(CSubSource::eSubtype_chromosome);
            sub_source->SetName(args["cm"].AsString());
            source_desc->SetSource().SetSubtype().push_back(sub_source);
        }
        if (args["cn"]) {
            sub_source = new CSubSource;
            sub_source->SetSubtype(CSubSource::eSubtype_clone);
            sub_source->SetName(args["cn"].AsString());
            source_desc->SetSource().SetSubtype().push_back(sub_source);
        }
        if (args["cl"]) {
            sub_source = new CSubSource;
            sub_source->SetSubtype(CSubSource::eSubtype_clone_lib);
            sub_source->SetName(args["cl"].AsString());
            source_desc->SetSource().SetSubtype().push_back(sub_source);
        }
        if (args["sc"]) {
            sub_source = new CSubSource;
            sub_source->SetSubtype(CSubSource::eSubtype_other);
            sub_source->SetName(args["sc"].AsString());
            source_desc->SetSource().SetSubtype().push_back(sub_source);
        }
        if (args["ht"]) {
            sub_source = new CSubSource;
            sub_source->SetSubtype(CSubSource::eSubtype_haplotype);
            sub_source->SetName(args["ht"].AsString());
            source_desc->SetSource().SetSubtype().push_back(sub_source);
        }
        if (args["sex"]) {
            sub_source = new CSubSource;
            sub_source->SetSubtype(CSubSource::eSubtype_sex);
            sub_source->SetName(args["sex"].AsString());
            source_desc->SetSource().SetSubtype().push_back(sub_source);
        }
        if (args["sn"]) {
            if (!args["on"]) {
                throw runtime_error("-sn requires -on");
            }
        }
        if (args["on"] || args["nt"]) {

            CTaxon1 cl;
            if (!cl.Init()) {
                throw runtime_error("failure contacting taxonomy server");
            }

            CConstRef<CTaxon2_data> on_result;
            CRef<CTaxon2_data> nt_result;
            COrg_ref inp_orgref;

            if (args["on"]) {
                const string& inp_taxname = args["on"].AsString();
                inp_orgref.SetTaxname(inp_taxname);

                if (args["sn"]) {
                    CRef<COrgMod> mod(new COrgMod);
                    mod->SetSubtype(COrgMod::eSubtype_strain);
                    mod->SetSubname(args["sn"].AsString());
                    inp_orgref.SetOrgname().SetMod().push_back(mod);
                }

                on_result = cl.LookupMerge(inp_orgref);

                if (!on_result) {
                    throw runtime_error("taxonomy server lookup failed");
                }
                if (!on_result->GetIs_species_level()) {
                    throw runtime_error("supplied name is not species-level");
                }
                if (inp_orgref.GetTaxname() != inp_taxname) {
                    cerr << "** Warning: taxname returned by server ("
                         << on_result->GetOrg().GetTaxname()
                         << ") differs from that supplied with -on ("
                         << inp_taxname << ")" << endl;
                    // an old-name OrgMod will have been added
                    COrgName::TMod& mod = inp_orgref.SetOrgname().SetMod();
                    mod.erase(remove_if(mod.begin(), mod.end(), SIsOldName()),
                              mod.end());
                    if (mod.empty()) {
                        inp_orgref.SetOrgname().ResetMod();
                    }
                }

                if (args["sn"]) {
                    const string& inp_strain_name = args["sn"].AsString();
                    vector<string> strain_names;
                    ITERATE (COrgName::TMod, mod,
                             inp_orgref.GetOrgname().GetMod()) {
                        if ((*mod)->GetSubtype() == COrgMod::eSubtype_strain) {
                            strain_names.push_back((*mod)->GetSubname());
                        }
                    }
                    if (!(strain_names.size() == 1
                          && strain_names[0] == inp_strain_name)) {
                        cerr << "** Warning: strain name " << inp_strain_name
                             << " provided but server lookup yielded";
                        if (strain_names.empty()) {
                            cerr << " no strain name" << endl;
                        } else {
                            for (unsigned int i = 0;
                                 i < strain_names.size();
                                 ++i) {
                                if (i > 0) {
                                    cerr << " and";
                                }
                                cerr << " " << strain_names[i];
                            }
                            cerr << endl;
                        }
                    }
                }
            }

            if (args["nt"]) {
                int inp_taxid = args["nt"].AsInteger();
                nt_result = cl.GetById(inp_taxid);
                if (!nt_result->GetIs_species_level()) {
                    throw runtime_error("taxid " + NStr::IntToString(inp_taxid)
                                        + " is not species-level");
                }
                nt_result->SetOrg().ResetSyn();  // lose any synonyms
                int db_taxid = s_GetTaxid(nt_result->GetOrg());
                if (db_taxid != inp_taxid) {
                    cerr << "** Warning: taxid returned by server ("
                         << NStr::IntToString(db_taxid)
                         << ") differs from that supplied with -nt ("
                         << inp_taxid << ")" << endl;
                }
                if (args["on"]) {
                    int on_taxid = s_GetTaxid(on_result->GetOrg());
                    if (on_taxid != db_taxid) {
                        throw runtime_error("taxid from name lookup ("
                                            + NStr::IntToString(on_taxid)
                                            + ") differs from that from "
                                            + "taxid lookup ("
                                            + NStr::IntToString(db_taxid)
                                            + ")");
                    }
                }
            }

            if (args["on"]) {
                source_desc->SetSource().SetOrg().Assign(inp_orgref);
            } else {
                source_desc->SetSource().SetOrg().Assign(nt_result->GetOrg());
            }
        }
        ent_templ.SetSeq().SetDescr().Set().push_back(source_desc);
    }

    // Should we make Seq-gap's?
    use_gap_info = args["gap-info"];

    // Preliminary stuff for -stdout
    bool write_stdout = args["stdout"];
    if (write_stdout && output_seq_submit) {
        throw runtime_error("can't write Seq-submit to stdout");
    }
    string kBssBegin = "Bioseq-set ::= {\n  seq-set {\n";
    string kBssEnd = "\n  }\n}\n";
    auto_ptr<CObjectOStream> obj_ostr;
    if (write_stdout) {
        cout << kBssBegin;
        obj_ostr.reset(CObjectOStream::Open(eSerial_AsnText, cout));
    }

    // Iterate over AGP files
    bool any_written = false;
    for (unsigned int i = 1; i <= args.GetNExtra(); ++i) {

        CNcbiIstream& istr = args[i].AsInputFile();
        CRef<CBioseq_set> big_set = AgpRead(istr, eAgpRead_ParseId,
                                            use_gap_info);

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
                    ids.clear();
                    CSeq_id::ParseFastaIds(ids, id_str);
                    id_str.clear();
                    // need version, no db name from id general
                    CSeq_id::TLabelFlags flags = CSeq_id::fLabel_GeneralDbIsContent|CSeq_id::fLabel_Version;
                    ids.front()->GetLabel(&id_str, CSeq_id::eContent, flags);
                } else {
                    cerr << "** ID " << id_str <<
                        " contains a '|'; consider using the -fasta_id option"
                         << endl;
                }
            }
            CSeq_entry new_entry;
            new_entry.Assign(ent_templ);
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
            if (args["chromosomes"]
                && chr_names.find(col1) != chr_names.end()) {
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
                                        NStr::UIntToString(source_descs.size()) +
                                        "Source Desc's; expected exactly one");
                }
                CSeqdesc& source_desc = *source_descs[0];
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
            if (!write_stdout) {
                string outfpath;
                if (!output_seq_submit) {
                    // write a Seq-entry
                    string suffix;
                    if (!args["ofs"]) {
                        suffix = "ent";
                    } else {
                        suffix = args["ofs"].AsString();
                    }
                    outfpath = CDirEntry::MakePath(outdir, id_str, suffix);
                    CNcbiOfstream ostr(outfpath.c_str());
                    ostr << MSerial_AsnText << new_entry;
                } else {
                    // write a Seq-submit
                    string suffix;
                    if (!args["ofs"]) {
                        suffix = "sqn";
                    } else {
                        suffix = args["ofs"].AsString();
                    }
                    outfpath = CDirEntry::MakePath(outdir, id_str, suffix);
                    CSeq_submit new_submit;
                    new_submit.Assign(*submit_templ);
                    new_submit.SetData().SetEntrys().front().Reset(&new_entry);
                    CNcbiOfstream ostr(outfpath.c_str());
                    ostr << MSerial_AsnText << new_submit;
                }

                if (!args["no_asnval"] && !args["no_testval"]) {
                    // verify using asnval
                    string cmd = "asnval -Q 2 -o stdout -i \""
                        + outfpath + "\"";
                    cout << cmd << endl;
                    CExec::SpawnLP(CExec::eWait, "asnval", "-Q", "2",
                                   "-o", "stdout", "-i", outfpath.c_str(), NULL);
                }
            } else {
                // write_stdout
                if (any_written) {
                    cout << ",\n";
                }
                any_written = true;
                obj_ostr->WriteObject(&new_entry, new_entry.GetThisTypeInfo());
                obj_ostr->Flush();
            }
        }
    }

    if (write_stdout) {
        obj_ostr->Flush();
        cout << kBssEnd;
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
