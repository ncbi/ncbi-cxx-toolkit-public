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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>


#include <corelib/ncbiapp.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include "wgs_params.hpp"
#include "wgs_id1.hpp"
#include "wgs_master.hpp"
#include "wgs_sub.hpp"
#include "wgs_utils.hpp"
#include "wgs_seqentryinfo.hpp"

USING_NCBI_SCOPE;

namespace wgsparse
{

class CWGSParseApp : public CNcbiApplication
{
public:
    CWGSParseApp()
    {
        SetVersionByBuild(0);
    }

private:
    virtual void Init(void);
    virtual int  Run(void);

    CRef<CSeq_entry> GetMasterEntry() const;

    CNcbiOstream* GetOutputStream();
};


void CWGSParseApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "Parsing multifile submissions");

    arg_desc->Delete("h"); // ?

    arg_desc->AddOptionalKey("i", "InputFiles", "The names of input files. May include (sub)directories names. Use of wildcards is allowed.", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("f", "ListInputFiles", "File with the names of input files.", CArgDescriptions::eString);

    arg_desc->AddDefaultKey("b", "Binary", "All input files are binary.", CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("p", "BinaryButMaster", "ALL output files are binary (except Master).", CArgDescriptions::eBoolean, "F");

    //arg_desc->AddOptionalKey("l", "LogFile", "Name of log file.", CArgDescriptions::eString); // -logfile instead

    arg_desc->AddDefaultKey("u", "ParsingMode", "Parsing mode:\n        1 - PARTIAL UPDATE, i.e. only members of project will be updated;\n        2 - ASSEMBLY UPDATE, the whole new assembly will be updated;\n        3 - SCAFFOLDS, parse brand new scaffolds for already existing\n            project;\n        4 - FULL UPDATE WITHIN CURRENT ASSEMBLY, all contigs are getting\n            updated with master record being generated;\n        5 - SCAFFOLDS UPDATE, parse already existing scaffolds,\n            no accessions assignment;\n        6 - EXTRA CONTIGS, add extra contigs to already existing project;\n        0 - Brand new project.", CArgDescriptions::eInteger, "0");
    arg_desc->AddKey("a", "Accession", "Prefix+version for accessions (4+2 or 7+2). Like \"AAAA55\" or \"NZ_AAAA55\".", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("d", "OutputDir", "Top-level directory for output ASN.1 and master Bioseq. Will contain the same subdirectories structure as the input one.", CArgDescriptions::eString);

    arg_desc->AddDefaultKey("t", "TaxLookup", "Perform taxonomy lookup.", CArgDescriptions::eBoolean, "T");
    arg_desc->AddDefaultKey("m", "MedlineLookup", "Perform medline lookup.", CArgDescriptions::eBoolean, "T");

    arg_desc->AddDefaultKey("w", "Override", "Override all existing output files.", CArgDescriptions::eBoolean, "F");
    arg_desc->AddOptionalKey("s", "Date", "Submission date in format DD-MMM-YYYY (like 04-JUL-1994). Will replace supplied dates in Cit-subs.", CArgDescriptions::eString);

    arg_desc->AddDefaultKey("y", "SubmissionType", "Type of input submissions. Possible values: \"Seq-submit\", \"Bioseq-set\" and \"Seq-entry\".", CArgDescriptions::eString, "Seq-submit");
    arg_desc->AddDefaultKey("c", "AccAssigned", "All accessions are already assigned.", CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("g", "NoGeneralIds", "Ignore general ids.", CArgDescriptions::eBoolean, "F");

    arg_desc->AddDefaultKey("q", "GapThreshold", "Gap threshold for converting raw na sequences to deltas.", CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("r", "DBNamesReplace", "Replace all dbnames with standard ones WGS:XXXX.", CArgDescriptions::eBoolean, "T");

    arg_desc->AddDefaultKey("x", "MolInfoReplace", "Replace supplied MolInfo.tech, Molinfo.biomol and Seq-inst.mol with correct values.", CArgDescriptions::eBoolean, "F");
    arg_desc->AddOptionalKey("n", "NucTitles", "Enter the string which will replace input nuc titles. Special substrings can be used in its contents:\n        [Seq-id]     - will be replaced with general id value;\n        [Organism]   - will be replaced with taxname from Org-ref after\n                       lookup;\n        [Chromosome] - will be replaced with chromosome value from\n                       Biosource after lookup.", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("e", "AccFile", "File name to save the list of accessions of previous assembly version. Works with only \"-u 2\" (update ASSEMBLY).", CArgDescriptions::eString);
    arg_desc->AddDefaultKey("o", "AccSort", "Assign accessions in sorted order:\n        0 - unsorted (in order of appearance in input file);\n        1 - by sequence length from longest to shortest;\n        2 - by sequence length from shortest to longest;\n        3 - by contig/scaffold ids.", CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey("k", "AccSortFile", "Assign accessions in sorted order within every single file, not the whole set. Takes effect if \"-o\" not 0.", CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("v", "CitSubDiff", "Allows differences among CitSubs throughout the data.", CArgDescriptions::eBoolean, "F");

    arg_desc->AddDefaultKey("z", "NoHTGSChanges", "Test mode for parsing scaffolds: if TRUE then does not change the contents of htgs database.", CArgDescriptions::eBoolean, "T");
    arg_desc->AddDefaultKey("h", "GeneticCode", "Propagate genetic code 1 to all cdregions regardless supplied values.", CArgDescriptions::eBoolean, "F");

    arg_desc->AddDefaultKey("j", "ScaffoldType", "Type of scaffolds to parse:\n        0 - regular genomic scaffolds;\n        1 - regular chromosomal scaffolds;\n        2 - GenCol genomic scaffolds;\n        3 - TPA genomic scaffolds;\n        4 - TPA chromosomal scaffolds.", CArgDescriptions::eInteger, "0");

    //{ "OBSOLETE: Genome-Project identifier for this WGS project.\n      Multiple GPID must be separated by commas, no blanks allowed.\n   ",
    //NULL, NULL, NULL, TRUE, 'G', ARG_STRING, 0.0, 0, NULL },

    //{ "OBSOLETE: Allow to have a mixture of far-pointer\n      accessions for scaffolds.\n   ",
    //"F", NULL, NULL, TRUE, 'M', ARG_BOOLEAN, 0.0, 0, NULL },

    arg_desc->AddDefaultKey("P", "NonConsortiumRemove", "Remove the non-consortium author names from the Cit-art pubs, provided at least one consortium author is present.", CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("S", "SecAcc", "Allow secondary accessions.", CArgDescriptions::eBoolean, "F");

    arg_desc->AddDefaultKey("N", "NewProj", "Enforce parsing as a brand new project.", CArgDescriptions::eBoolean, "F");

    arg_desc->AddOptionalKey("L", "IDsFile", "File name to save the list of contig/scaffold ids and accessions.", CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey("O", "OutFilesFile", "File name to collect the names of output files in a proper order for uploading to ID.", CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey("R", "RefsComments", "Keep references and comments on contig level. Applies to the \"EXTRA CONTIGS\" mode only.", CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("A", "CitArtsUpdate", "Upon assembly update copy all Cit-arts from the master record of previous assembly version to the new one. Applies to the \"UPDATE_ASSEMBLY\" mode only.", CArgDescriptions::eBoolean, "T");

    arg_desc->AddOptionalKey("F", "MasterFile", "The name of file containing the master record to read instead of fetching from ID.", CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("B", "BioProject", "BioProject Accession number associated with the WGS project. Multiple BPAs must be separated by commas, no blanks allowed.", CArgDescriptions::eString);

    arg_desc->AddDefaultKey("X", "DBLinkOverride", "Override any/all REJECT-level DBLink and scaffolds far pointer related data problems that are detected. Do not provide this switch unless those problems have been reviewed and deemed ok.", CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("V", "CmdLineVersion", "Trust command line assembly version.", CArgDescriptions::eBoolean, "F");

    arg_desc->AddOptionalKey("T", "BioMolType", "Specific biomol type to be used for a TSA project. Possible values (case insensitive): mRNA, rRNA, ncRNA.", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("C", "BioSampleSRA", "BioSample and/or SRA identifier(s) associated with the WGS/TSA project. Multiple ids must be separated by commas, no blanks allowed.", CArgDescriptions::eString);

    arg_desc->AddDefaultKey("Z", "Test", "Test mode: works same way as regular run except no files/directories will be created/modified.", CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("I", "OutNames", "Should the given path to input files be preserved in output names.", CArgDescriptions::eBoolean, "F");

    arg_desc->AddDefaultKey("U", "VDB", "VDB mode: no accession assignment, protein counts, descriptors moved to the master.", CArgDescriptions::eBoolean, "F");
    arg_desc->AddOptionalKey("K", "TPA", "TPA keyword for TPA projects only: \"TPA:assembly\" or \"TPA:experimental\". Case sensitive.", CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
}

static CRef<CSeq_entry> GetMasterEntryFromFile(const string& fname)
{
    CRef<CSeq_entry> entry;

    try {
        CNcbiIfstream in(fname);
        entry.Reset(new CSeq_entry);
        in >> *entry;
    }
    catch (...) {
        entry.Reset();
    }

    return entry;
}

CRef<CSeq_entry> CWGSParseApp::GetMasterEntry() const
{
    CRef<CSeq_entry> ret;
    if (GetParams().IsMasterInFile()) {
        const string fname = GetParams().GetMasterFileName();
        ret = GetMasterEntryFromFile(fname);
        if (ret.Empty()) {
            ERR_POST_EX(0, 0, "Failed to read the master record from file \"" << fname << "\".");
        }
    }
    else {
        ret = GetMasterEntryById(GetParams().GetIdPrefix(), GetParams().GetIdChoice());
    }

    return ret;
}

static bool GetPmid(const CPubdesc& pubdescr, int& pmid)
{
    if (pubdescr.IsSetPub()) {

        for (auto& pub : pubdescr.GetPub().Get()) {
            if (pub->IsPmid()) {
                pmid = pub->GetPmid();
                return true;;
            }
        }
    }
    return false;
}

static void RemoveDupPubs(CSeq_descr& descrs)
{
    if (descrs.IsSet()) {

        // Removes pubs with the same pmid
        set<int> pmids;
        CSeq_descr::Tdata& descr_list = descrs.Set();
        for (auto& descr = descr_list.begin(); descr != descr_list.end();) {

            bool increment = true;
            if ((*descr)->IsPub()) {

                int pmid = 0;
                if (GetPmid((*descr)->GetPub(), pmid)) {
                    if (!pmids.insert(pmid).second) {
                        descr = descr_list.erase(descr);
                        increment = false;
                    }
                }
            }

            if (increment) {
                ++descr;
            }
        }

        // Remove duplicate pubs
        CSeq_descr::Tdata pubs;
        for (auto& descr = descr_list.begin(); descr != descr_list.end();) {

            bool increment = true;
            if ((*descr)->IsPub()) {

                auto& dup = find_if(pubs.begin(), pubs.end(), [&descr](const CRef<CSeqdesc>& cur_pub) { return cur_pub->Equals(**descr); });
                if (dup != pubs.end()) {

                    descr = descr_list.erase(descr);
                    increment = false;
                }
                else {
                    pubs.push_back(*descr);
                }
            }

            if (increment) {
                ++descr;
            }
        }
    }
}

static void RemoveBioSourseDesc(CSeq_descr& descrs)
{
    if (descrs.IsSet()) {

        auto& descrs_list = descrs.Set();

        auto bio_src_it = find_if(descrs_list.begin(), descrs_list.end(), [](CRef<CSeqdesc>& descr) { return descr->IsSource(); });
        if (bio_src_it != descrs_list.end()) {
            descrs_list.erase(bio_src_it);
        }
    }
}

static bool OpenOutputFile(const string& fname, const string& description, CNcbiOfstream& stream)
{
    if (!GetParams().IsOverrideExisting() && CFile(fname).Exists()) {
        ERR_POST_EX(0, 0, Critical << "The " << description << " file already exists: \"" << fname << "\". Override is not allowed.");
        return false;
    }

    try {
        stream.open(fname);
    }
    catch (CException& e) {
        ERR_POST_EX(0, 0, Critical << "Failed to open " << description << " file: \"" << fname << "\" [" << e.GetMsg() << "]. Cannot proceed.");
        return false;
    }

    return true;
}

static bool OutputMaster(const CRef<CSeq_entry>& entry, const string& fname, int num_of_entries)
{
    string path_name = GetParams().GetOutputDir() + '/';
    if (fname.empty()) {
        path_name += GetParams().GetIdPrefix() + ToStringLeadZeroes(GetParams().GetAssemblyVersion(), 2) + ToStringLeadZeroes(0, GetMaxAccessionLen(num_of_entries));
    }
    else {
        path_name += fname;
    }

    CNcbiOfstream out;
    if (!OpenOutputFile(path_name, "processed submission", out)) {
        return false;
    }

    try {
        if (GetParams().IsBinaryOutput())
            out << MSerial_AsnBinary << entry;
        else
            out << MSerial_AsnText << entry;
    }
    catch (CException& e) {
        ERR_POST_EX(0, 0, Error << "Failed to save processed submission to file: \"" << fname << "\" [" << e.GetMsg() << "]. Cannot proceed.");
        return false;
    }

    ERR_POST_EX(0, 0, Info << "Master Bioseq saved in file \"" << fname << "\".");

    return true;
}

static void PrintOrderList(const list<CIdInfo>& infos, CNcbiOfstream& out)
{
    map<string, string, greater<string>> accession_to_file;
    for (auto info : infos) {
        accession_to_file[info.m_accession] = info.m_file;
    }

    set<string> written_files;
    for (auto cur_file_info : accession_to_file) {

        const char* fname = cur_file_info.second.c_str();

        if (!GetParams().IsPreserveInputPath()) {
            size_t last_slash = GetLastSlashPos(cur_file_info.second);
            if (last_slash != string::npos) {
                fname += last_slash + 1;
            }
        }

        if (written_files.insert(fname).second) {
            out << fname << ".bss\n";
        }
    }
}

static const string& GetIdHeader()
{
    static const string SCAFFOLD_ID("Scaffold id");
    static const string CONTIG_ID("Contig id");

    return GetParams().IsUpdateScaffoldsMode() ? SCAFFOLD_ID : CONTIG_ID;
}

static void PrintGenAccList(const list<CIdInfo>& infos, CNcbiOfstream& out)
{
    out << GetIdHeader() << '\t' << "Accession\n";
    out << "------------------------------------\n";
    for (auto info : infos) {
        out << info.m_dbtag << '\t' << info.m_accession << '\n';
    }
}

static void PrintGenAccOrderList(CMasterInfo& info)
{
    const string& id_acc_file = GetParams().GetIdAccFile();
    const string& load_order_file = GetParams().GetLoadOrderFile();

    if (id_acc_file.empty() && load_order_file.empty()) {
        return;
    }

    info.m_id_infos.sort();

    CNcbiOfstream out;
    if (!load_order_file.empty() && OpenOutputFile(load_order_file, "load order", out)) {
        PrintOrderList(info.m_id_infos, out);
    }

    out.close();
    if (!id_acc_file.empty() && OpenOutputFile(id_acc_file, "list of general ids and accessions", out)) {
        PrintGenAccList(info.m_id_infos, out);
    }
}

static void OutUnorderedList(const list<string>& files, CNcbiOfstream& out)
{
    for (auto file : files) {
        const char* fname = file.c_str();
        if (!GetParams().IsPreserveInputPath()) {
            size_t last_slash = GetLastSlashPos(file);
            if (last_slash != string::npos) {
                fname += last_slash + 1;
            }
        }

        out << fname << ".bss\n";
    }
}

static void PrintUnorderedList(CMasterInfo& info)
{
    const string& load_order_file = GetParams().GetLoadOrderFile();

    if (load_order_file.empty()) {
        return;
    }

    CNcbiOfstream out;
    if (!load_order_file.empty() && OpenOutputFile(load_order_file, "load order", out)) {

        OutUnorderedList(GetParams().GetInputFiles(), out);
    }
}

static bool GetAccessionIdInfo(const CSeq_id& id, string& accession, int& version)
{
    if (NeedToProcessId(id)) {
        const CTextseq_id* text_id = id.GetTextseq_Id();
        if (text_id && text_id->IsSetName()) {

            const string& cur_name = text_id->GetName();
            auto& digits = find_if(cur_name.begin(), cur_name.end(), [](char c){ return isdigit(c); });

            if (digits != cur_name.end()) {
                string version_str = cur_name.substr(digits - cur_name.begin());

                if (version_str.size() >= 2) {
                    version = NStr::StringToInt(version_str.substr(0, 2), NStr::fAllowLeadingSymbols | NStr::fAllowTrailingSymbols);
                    if (version > 0) {
                        accession = text_id->GetAccession();
                    }
                }
            }
        }
    }

    return version > 0;
}

static bool GetAccessionInfo(const CSeq_entry& entry, string& accession, int& version)
{
    if (entry.IsSeq() && entry.GetSeq().IsNa()) {

        if (entry.GetSeq().IsSetId()) {

            for (auto id : entry.GetSeq().GetId()) {
                if (GetAccessionIdInfo(*id, accession, version)) {
                    break;
                }
            }
        }
    }

    if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {

            for (auto cur_entry : entry.GetSet().GetSeq_set()) {
                if (GetAccessionInfo(*cur_entry, accession, version)) {
                    break;
                }
            }
        }
    }

    return version > 0;
}

static void GetContigNum(CConstRef<CUser_field>& field, int& contig, size_t& num_len)
{
    if (field.NotEmpty() && field->IsSetData() && field->GetData().IsStr()) {

        const string& accession = field->GetData().GetStr();

        if (NStr::StartsWith(accession, GetParams().GetIdPrefix())) {

            const char* contig_num = accession.c_str() + GetParams().GetIdPrefix().size();
            int value = NStr::StringToInt(contig_num, NStr::fAllowLeadingSymbols | NStr::fAllowTrailingSymbols);
            if (value > 0) {
                contig = value;
            }

            if (num_len == 0) {
                num_len = accession.size() - GetParams().GetIdPrefix().size();
            }
        }
    }
}

static void GetDescriptorsInfo(const CSeq_entry& entry, const vector<string>& user_obj_tags, const string& first_contig_field, const string& last_contig_field, int& first_contig, int& last_contig, size_t& num_len)
{
    if (entry.IsSeq() && entry.GetSeq().IsNa()) {

        if (entry.GetSeq().IsSetDescr() && entry.GetSeq().GetDescr().IsSet()) {

            for (auto descr : entry.GetSeq().GetDescr().Get()) {

                for (auto& tag : user_obj_tags) {
                    if (IsUserObjectOfType(*descr, tag)) {

                        const CUser_object& cur_obj = descr->GetUser();

                        {
                            auto& contig = cur_obj.GetFieldRef(first_contig_field);
                            GetContigNum(contig, first_contig, num_len);
                        }

                        {
                            auto& contig = cur_obj.GetFieldRef(last_contig_field);
                            GetContigNum(contig, last_contig, num_len);
                        }
                    }
                }
            }
        }
    }

    if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {

            for (auto cur_entry : entry.GetSet().GetSeq_set()) {
                GetDescriptorsInfo(*cur_entry, user_obj_tags, first_contig_field, last_contig_field, first_contig, last_contig, num_len);
            }
        }
    }
}

static void GetUserObjAndFieldNames(vector<string>& user_obj_tags, string& first_contig, string& last_contig)
{
    if (GetParams().IsTls()) {
        user_obj_tags.push_back("TLSProjects");
        first_contig = "TLS_accession_first";
        last_contig = "TLS_accession_last";
    }
    else if (GetParams().IsTsa()) {
        user_obj_tags.push_back("TSA-RNA-List");
        user_obj_tags.push_back("TSA-mRNA-List");
        first_contig = "TSA_accession_first";
        last_contig = "TSA_accession_last";
    }
    else {
        user_obj_tags.push_back("WGSProjects");
        first_contig = "WGS_accession_first";
        last_contig = "WGS_accession_last";
    }
}

static const CPub* GetPubOfType(const CSeqdesc& descr, CPub::E_Choice type)
{
    _ASSERT(descr.IsPub() && "Should be a publication");

    const CPub* ret = nullptr;
    if (descr.GetPub().IsSetPub() && descr.GetPub().GetPub().IsSet()) {

        const CPub_equiv::Tdata& pubs = descr.GetPub().GetPub().Get();
        auto pub = find_if(pubs.begin(), pubs.end(), [&type](const CRef<CPub>& pub) { return pub->Which() == type; });
        if (pub != pubs.end()) {
            ret = *pub;
        }
    }

    return ret;
}

// returns true if a.date > b.date (a.date is more recent)
static bool IsDateRecent(const CCit_sub& cit_sub, const CDate& date)
{
    if (cit_sub.IsSetDate()) {

        return date.Which() != CDate::e_not_set ? cit_sub.GetDate().Compare(date) == CDate::eCompare_after : true;
    }

    return false;
}

static void GetCurrentMasterPubsInfo(const CSeq_entry& entry, CRef<CDate>& cit_sub_date, CRef<CPub_equiv>& cit_sub, list<CRef<CPub_equiv>>& cit_arts)
{
    if (entry.IsSeq() && entry.GetSeq().IsNa()) {

        if (entry.GetSeq().IsSetDescr() && entry.GetSeq().GetDescr().IsSet()) {

            for (auto descr : entry.GetSeq().GetDescr().Get()) {

                if (descr->IsPub()) {

                    const CPub* cit = GetPubOfType(*descr, CPub::e_Sub);

                    if (cit) {

                        _ASSERT(cit->IsSub() && "Should contain a valid CCit_sub object");
                        const CCit_sub& cur_cit_sub = cit->GetSub();

                        if (cit_sub.Empty() || IsDateRecent(cur_cit_sub, *cit_sub_date)) {
                            cit_sub.Reset(new CPub_equiv);
                            cit_sub->Assign(descr->GetPub().GetPub());

                            cit_sub_date.Reset(new CDate);
                            if (cur_cit_sub.IsSetDate()) {
                                cit_sub_date->Assign(cur_cit_sub.GetDate());
                            }
                        }
                    }

                    if (GetParams().IsCitArtFromMaster()) {

                        cit = GetPubOfType(*descr, CPub::e_Article);

                        if (cit) {

                            _ASSERT(cit->IsArticle() && "Should contain a valid CCit_art object");

                            CRef<CPub_equiv> cit_art(new CPub_equiv);
                            cit_art->Assign(descr->GetPub().GetPub());
                            cit_arts.push_back(cit_art);
                        }
                    }
                }
            }
        }
    }

    if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {

            for (auto cur_entry : entry.GetSet().GetSeq_set()) {
                GetCurrentMasterPubsInfo(*cur_entry, cit_sub_date, cit_sub, cit_arts);
            }
        }
    }
}

static void GetCurrentMasterInfo(const CSeq_entry& entry, CCurrentMasterInfo& current_master)
{
    GetAccessionInfo(entry, current_master.m_accession, current_master.m_version);

    vector<string> user_obj_tags;
    string first_contig,
           last_contig;

    GetUserObjAndFieldNames(user_obj_tags, first_contig, last_contig);
    GetDescriptorsInfo(entry, user_obj_tags, first_contig, last_contig, current_master.m_first_contig, current_master.m_last_contig, current_master.m_num_len);

    GetCurrentMasterPubsInfo(entry, current_master.m_cit_sub_date, current_master.m_cit_sub, current_master.m_cit_arts);
}

static bool SaveAccessionsRange(int first, int last, size_t num_len)
{
    CNcbiOfstream out;
    if (!OpenOutputFile(GetParams().GetAccFile(), "", out)) {
        return false;
    }

    const string& prefix = GetParams().GetIdPrefix();
    for (int i = first; i <= last; ++i) {
        out << prefix << ToStringLeadZeroes(i, num_len) << '\n';
    }

    return true;
}

typedef bool(*SelectDescr)(const CSeqdesc&);
static void GetDescriptors(const CSeq_descr& descrs, set<string>& selected_descrs, SelectDescr select)
{
    if (descrs.IsSet()) {
        for (auto& descr : descrs.Get()) {
            if (select(*descr)) {
                const string& str = ToString(*descr);
                selected_descrs.insert(str);
            }
        }
    }
}

static bool IsDescriptorsSame(const CSeq_entry& a_entry, const CSeq_entry& b_entry, SelectDescr select)
{
    if (!a_entry.IsSeq() || !b_entry.IsSeq()) {
        return true;
    }

    set<string> a_descrs,
                b_descrs;

    if (a_entry.GetSeq().IsSetDescr()) {
        GetDescriptors(a_entry.GetSeq().GetDescr(), a_descrs, select);
    }

    if (b_entry.GetSeq().IsSetDescr()) {
        GetDescriptors(b_entry.GetSeq().GetDescr(), b_descrs, select);
    }

    return a_descrs == b_descrs;
}

static CBioSource* GetBioSourceFromDescrs(CSeq_entry& entry)
{
    CBioSource* ret = nullptr;

    if (entry.IsSeq() && entry.GetSeq().IsSetDescr() && entry.GetSeq().GetDescr().IsSet()) {
        for (auto descr : entry.SetSeq().SetDescr().Set()) {
            if (descr->IsSource()) {
                ret = &descr->SetSource();
                break;
            }
        }
    }

    return ret;
}

static void UpdateBioSource(CSeq_entry& master_bioseq, CSeq_entry& id_master_bioseq)
{
    CBioSource* new_bio_src = GetBioSourceFromDescrs(master_bioseq);
    if (new_bio_src == nullptr) {
        return;
    }

    CBioSource* old_bio_src = GetBioSourceFromDescrs(id_master_bioseq);
    if (old_bio_src == nullptr) {

        ERR_POST_EX(0, 0, (GetParams().GetSource() == eNCBI ? Error : Warning) << "Current master record in ID is lacking BioSource descriptor. Using the one from updated contigs.");

        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetSource(*new_bio_src);
        id_master_bioseq.SetSeq().SetDescr().Set().push_front(descr);
        return;
    }

    if (!new_bio_src->Equals(*old_bio_src)) {
        ERR_POST_EX(0, 0, (GetParams().GetSource() == eNCBI ? Error : Warning) <<
                    "Current master record in ID has a BioSource descriptor which differs from the BioSource obtained from the updated contigs. Using the new BioSource from the contigs.");
        old_bio_src->Assign(*new_bio_src);
    }
}

static void FixMasterDates(CMasterInfo& info, bool entry_from_id)
{
    if (info.m_master_bioseq.Empty() || info.m_id_master_bioseq.Empty() || GetParams().GetSource() == eNCBI) {
        return;
    }

    CRef<CSeqdesc> creation_date = GetSeqdescr(*info.m_id_master_bioseq, CSeqdesc::e_Create_date);

    if (creation_date.NotEmpty()) {
        if (!entry_from_id) {

            CRef<CSeqdesc> newer_creation_date = GetSeqdescr(*info.m_master_bioseq, CSeqdesc::e_Create_date);
            if (newer_creation_date.NotEmpty()) {
                newer_creation_date->Assign(*creation_date);
            }
            else {
                info.m_master_bioseq->SetDescr().Set().push_back(creation_date);
            }
        }
    }
    else if (entry_from_id && info.m_creation_date_issues == eDateNoIssues && info.m_creation_date_present) {
        creation_date.Reset(new CSeqdesc);
        creation_date->SetCreate_date(*info.m_creation_date);
        info.m_id_master_bioseq->SetDescr().Set().push_back(creation_date);
    }

    if (!entry_from_id || !info.m_update_date_present || info.m_update_date_issues != eDateNoIssues) {
        return;
    }

    CRef<CSeqdesc> update_date = GetSeqdescr(*info.m_id_master_bioseq, CSeqdesc::e_Update_date);

    if (update_date.Empty()) {
        update_date.Reset(new CSeqdesc);
        update_date->SetUpdate_date(*info.m_update_date);
        info.m_id_master_bioseq->SetDescr().Set().push_back(update_date);
    }
    else {
        bool fix_update_date = true;
        if (entry_from_id && GetParams().GetUpdateMode() == eUpdateScaffoldsUpd) {
            CCleanup cleanup;
            cleanup.ExtendedCleanup(*info.m_id_master_bioseq);
            fix_update_date = !info.m_id_master_bioseq->Equals(*info.m_master_bioseq);
        }

        if (fix_update_date) {
            update_date->SetUpdate_date(*info.m_update_date);
        }
    }

}

typedef bool(*TDescrPredicat)(const CSeqdesc&);

static void RemoveDescriptors(CSeq_entry& entry, TDescrPredicat to_remove)
{
    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {

        auto& descr_cont = descrs->Set();
        for (auto& descr = descr_cont.begin(); descr != descr_cont.end();) {
            if (to_remove(**descr)) {
                descr = descr_cont.erase(descr);
            }
            else {
                ++descr;
            }
        }
    }
}

static bool HasOneCitArt(const CPubdesc& pub)
{
    size_t num_of_articles = 0;
    if (pub.IsSetPub() && pub.GetPub().IsSet()) {

        auto& pubs = pub.GetPub().Get();
        num_of_articles = count_if(pubs.begin(), pubs.end(), [](const CRef<CPub>& cur_pub){ return cur_pub->IsArticle(); });
    }

    return num_of_articles == 1;
}

static CSeqdesc* CheckCitArtToReplace(CSeq_entry& entry, string& iso_jta, bool inpress)
{
    CSeqdesc* ret = nullptr;

    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {

        for (auto& descr : descrs->Set()) {
            if (descr->IsPub()) {

                if (HasOneCitArt(descr->GetPub())) {
                    if (ret) {
                        ret = nullptr;
                        break;
                    }

                    ret = descr;
                }
            }
        }

        if (ret && !CheckCitArtPmidPrepub(ret->GetPub().GetPub(), inpress, inpress, &iso_jta)) {
            ret = nullptr;
        }
    }
    return ret;
}

static void UpdateCitArt(CSeq_entry& id_entry, CSeq_entry& master_entry)
{
    string old_iso_jta;
    CSeqdesc* old_cit_art_descr = CheckCitArtToReplace(id_entry, old_iso_jta, true);
    if (old_cit_art_descr == nullptr) {
        return;
    }

    string new_iso_jta;
    CSeqdesc* new_cit_art_descr = CheckCitArtToReplace(master_entry, new_iso_jta, false);
    if (new_cit_art_descr == nullptr) {
        return;
    }

    if (!old_iso_jta.empty() && old_iso_jta == new_iso_jta) {
        old_cit_art_descr->SetPub().Assign(new_cit_art_descr->GetPub());
    }
}

static void GetUserObjTypesAndLastAccession(vector<string>& user_obj_tags, string& last_accession)
{
    if (GetParams().IsTls()) {
        user_obj_tags.push_back("TLSProjects");
        last_accession = "TLS_accession_last";
    }
    else if (GetParams().IsTsa()) {
        user_obj_tags.push_back("TSA-RNA-List");
        user_obj_tags.push_back("TSA-mRNA-List");
        last_accession = "TSA_accession_last";
    }
    else {
        user_obj_tags.push_back("WGSProjects");
        last_accession = "WGS_accession_last";
    }
}

static bool UpdateMasterForExtra(CSeq_entry& id_entry, CSeq_entry& master_entry)
{
    if (!id_entry.IsSeq() || !master_entry.IsSeq() || !id_entry.GetSeq().IsSetInst() || !master_entry.GetSeq().IsSetInst()) {
        return true;
    }

    CBioseq& old_bioseq = id_entry.SetSeq();
    CBioseq& new_bioseq = master_entry.SetSeq();

    TSeqPos old_length = old_bioseq.GetInst().GetLength(),
            new_length = new_bioseq.GetInst().GetLength();

    if (GetParams().GetUpdateMode() == eUpdateExtraContigs) {
        old_bioseq.SetInst().SetLength(old_length + new_length);
    }
    else if (new_length > old_length) {

        if (GetParams().GetSource() == eNCBI && !GetParams().IsDblinkOverride()) {

            ERR_POST_EX(0, 0, Critical << "Accession number range for the contigs within an assembly-version has increased from " << old_length << " to " << new_length << ".");
            return false;
        }

        ERR_POST_EX(0, 0, Warning << "Accession number range for the contigs within an assembly-version has increased from " << old_length << " to " << new_length << ".");
        old_bioseq.SetInst().SetLength(new_length);
    }

    vector<string> user_obj_types;
    string last_accession;
    GetUserObjTypesAndLastAccession(user_obj_types, last_accession);

    string accession;
    if (new_bioseq.IsSetDescr() && new_bioseq.GetDescr().IsSet()) {

        for (auto& descr : new_bioseq.GetDescr().Get()) {

            if (descr->IsUser()) {

                bool found = false;
                for (auto type : user_obj_types) {
                    if (IsUserObjectOfType(*descr, type)) {
                        found = true;
                        break;
                    }
                }

                if (found) {
                    CConstRef<CUser_field> last_acc_field = descr->GetUser().GetFieldRef(last_accession);
                    if (last_acc_field.NotEmpty() && last_acc_field->IsSetData() && last_acc_field->GetData().IsStr()) {
                        accession = last_acc_field->GetData().GetStr();
                        break;
                    }
                }
            }
        }
    }

    if (accession.empty()) {
        return true;
    }

    if (old_bioseq.IsSetDescr() && old_bioseq.GetDescr().IsSet()) {

        for (auto& descr : old_bioseq.SetDescr().Set()) {

            if (descr->IsUser()) {

                bool found = false;
                for (auto type : user_obj_types) {
                    if (IsUserObjectOfType(*descr, type)) {
                        found = true;
                        break;
                    }
                }

                if (found) {
                    if (descr->GetUser().GetFieldRef(last_accession).NotEmpty()) {
                        descr->SetUser().SetField(last_accession).SetString(accession);
                    }
                }
            }
        }
    }
    return true;
}

static void CheckOrganisms(const CSeq_entry& id_entry, const CSeq_entry& master_entry)
{
    if (!id_entry.IsSeq() || !master_entry.IsSeq()) {
        ERR_POST_EX(0, 0, Error << "Missing organism info from master record.");
        return;
    }

    const CBioseq& old_bioseq = id_entry.GetSeq();
    const CBioseq& new_bioseq = master_entry.GetSeq();

    bool found_same_bio_src = false;
    string old_taxname("NULL"),
           new_taxname("NULL");

    if (old_bioseq.IsSetDescr() && new_bioseq.IsSetDescr() && old_bioseq.GetDescr().IsSet() && new_bioseq.GetDescr().IsSet()) {

        for (auto& old_descr : old_bioseq.GetDescr().Get()) {
            if (old_descr->IsSource() && old_descr->GetSource().IsSetOrg() && old_descr->GetSource().GetOrg().IsSetTaxname()) {

                old_taxname = old_descr->GetSource().GetOrg().GetTaxname();

                for (auto& new_descr : new_bioseq.GetDescr().Get()) {
                    if (new_descr->IsSource() && new_descr->GetSource().IsSetOrg() && new_descr->GetSource().GetOrg().IsSetTaxname()) {
                        new_taxname = new_descr->GetSource().GetOrg().GetTaxname();
                        if (new_taxname == old_taxname) {
                            found_same_bio_src = true;
                            break;
                        }
                    }
                }

                if (found_same_bio_src) {
                    break;
                }
            }
        }
    }

    if (!found_same_bio_src) {
        ERR_POST_EX(0, 0, (GetParams().GetSource() == eNCBI ? Error : Warning) <<
                    "Taxname \"" << new_taxname << "\" in one or more records do not match the taxname in master record in ID \"" << old_taxname << "\".");
    }
}

static void UpdateCommonComments(CRef<CSeq_entry>& id_entry, list<string>& comments)
{
    if (id_entry.Empty() || !id_entry->IsSeq()) {
        comments.clear();
        return;
    }

    list<string*> comments_to_add;

    if (GetParams().GetUpdateMode() == eUpdateFull || GetParams().GetUpdateMode() == eUpdateExtraContigs) {
        if (!comments.empty()) {

            // selecting new master comments to be added
            auto& descrs = id_entry->SetSeq().SetDescr().Set();
            for (auto& comment : comments) {

                auto same_comment = find_if(descrs.begin(), descrs.end(), [&comment](const CRef<CSeqdesc>& descr) { return descr->IsComment() && descr->GetComment() == comment; });
                if (same_comment == descrs.end()) {
                    comments_to_add.push_back(&comment);
                }
            }
        }
    }
    else {
        comments.clear();
    }

    // adding old master entry comments
    if (id_entry->GetSeq().IsSetDescr() && id_entry->GetSeq().GetDescr().IsSet()) {

        list<string> old_comments_to_add;
        for (auto& descr : id_entry->GetSeq().GetDescr().Get()) {

            if (descr->IsComment()) {
                auto same_comment = find_if(comments.begin(), comments.end(), [&descr](const string& comment) { return descr->GetComment() == comment; });
                if (same_comment == comments.end()) {
                    old_comments_to_add.push_back(descr->GetComment());
                }
            }
        }

        comments.splice(comments.end(), old_comments_to_add);
    }

    if (!comments_to_add.empty()) {

        // adding new master comments to the old master
        auto& descrs = id_entry->SetSeq().SetDescr().Set();
        for (auto comment : comments_to_add) {
            CRef<CSeqdesc> descr(new CSeqdesc);
            descr->SetComment(*comment);
            descrs.push_back(descr);
        }
    }
}

static const int ERROR_RET = 1;

// CR Use ZZZZ prefix to mock completely new submission
int CWGSParseApp::Run(void)
{
    int ret = 0;

    if (SetParams(GetArgs())) {

        CCleanup cleanup;

        CMasterInfo master_info;
        CCurrentMasterInfo current_master;

        CRef<CSeq_entry> master_entry = GetMasterEntry();
        if (GetParams().GetUpdateMode() == eUpdateNew) {
            if (master_entry.NotEmpty()) {
                if (!GetParams().EnforceNew()) {
                    ERR_POST_EX(0, 0, Critical << "Incorrect parsing mode set in command line: this is not a brand new project.");
                    return ERROR_RET;
                }
                master_entry.Reset();
            }
        }
        else {

            if (master_entry.Empty()) {
                ERR_POST_EX(0, 0, Critical << "Failed to retrieve master sequence from ID.");
                return ERROR_RET;
            }

            cleanup.ExtendedCleanup(*master_entry);

            if (GetParams().IsUpdateScaffoldsMode() || GetParams().GetUpdateMode() == eUpdateAssembly || GetParams().GetUpdateMode() == eUpdateExtraContigs) {

                GetCurrentMasterInfo(*master_entry, current_master);
                if (current_master.m_version <= 0) {

                    ERR_POST_EX(0, 0, Critical << "Failed to get current assembly version from master sequence from ID.");
                    return ERROR_RET;
                }

                if (current_master.m_last_contig <= 0) {
                    ERR_POST_EX(0, 0, Critical << "Failed to get the range of accession from previous version master.");
                    return ERROR_RET;
                }

                if (!GetParams().IsTest() && !GetParams().GetAccFile().empty()) {
                    if (!SaveAccessionsRange(current_master.m_first_contig, current_master.m_last_contig, current_master.m_num_len)) {
                        ERR_POST_EX(0, 0, Critical << "Failed to save the range of accessions from previous version master to file \"" << GetParams().GetAccFile() << "\".");
                        return ERROR_RET;
                    }
                }

                if (GetParams().GetUpdateMode() == eUpdateAssembly) {
                    ++current_master.m_version;
                }

                master_info.m_current_master = &current_master;
            }
        }

        master_info.m_id_master_bioseq = master_entry;

        if (!CreateMasterBioseqWithChecks(master_info)) {

            switch (GetParams().GetUpdateMode()) {
                case eUpdatePartial:
                case eUpdateFull:
                case eUpdateScaffoldsNew:
                case eUpdateScaffoldsUpd:
                case eUpdateExtraContigs:
                    ERR_POST_EX(0, 0, Error << "Failed to create temporary master Bioseq.");
                    break;
                default:
                    ERR_POST_EX(0, 0, Error << "Failed to create master Bioseq.");
            }

            return ERROR_RET;
        }

        if (master_info.m_id_master_bioseq.NotEmpty()) {

            if (GetParams().GetUpdateMode() == eUpdateAssembly || GetParams().GetUpdateMode() == eUpdateFull) {
                RemoveDescriptors(*master_info.m_id_master_bioseq, [](const CSeqdesc& descr) { return IsUserObjectOfType(descr, "StructuredComment"); });
            }

            if (GetParams().GetSource() != eNCBI &&
                (GetParams().GetUpdateMode() == eUpdateAssembly || GetParams().GetUpdateMode() == eUpdateFull ||
                 GetParams().GetUpdateMode() == eUpdateExtraContigs || GetParams().GetUpdateMode() == eUpdatePartial)) {
                RemoveDescriptors(*master_info.m_id_master_bioseq, [](const CSeqdesc& descr) { return descr.IsComment(); });
            }
        }

        if (GetParams().GetSource() != eNCBI &&
            (GetParams().GetUpdateMode() == eUpdateExtraContigs || GetParams().GetUpdateMode() == eUpdateFull)) {

            UpdateCitArt(*master_info.m_id_master_bioseq, *master_info.m_master_bioseq);
        }

        // TODO ...

        cleanup.ExtendedCleanup(*master_info.m_master_bioseq);

        if (GetParams().GetUpdateMode() == eUpdateAssembly) {
            if (!IsDescriptorsSame(*master_info.m_id_master_bioseq, *master_info.m_master_bioseq, [](const CSeqdesc& descr) { return descr.IsComment(); })) {
                ERR_POST_EX(0, 0, Warning << "The new master record has altered comment.");
            }

            if (!IsDescriptorsSame(*master_info.m_id_master_bioseq, *master_info.m_master_bioseq, [](const CSeqdesc& descr) { return IsUserObjectOfType(descr, "StructuredComment"); })) {
                ERR_POST_EX(0, 0, Warning << "The new master record has altered structured comment.");
            }
        }

        if (GetParams().GetUpdateMode() == eUpdatePartial || GetParams().GetUpdateMode() == eUpdateFull || GetParams().GetUpdateMode() == eUpdateExtraContigs) {

            if (GetParams().GetUpdateMode() != eUpdatePartial && master_info.m_id_master_bioseq.NotEmpty()) {
                
                if (!UpdateMasterForExtra(*master_info.m_id_master_bioseq, *master_info.m_master_bioseq)) {
                    master_info.m_reject = true;
                    return ERROR_RET;
                }
            }

            if (master_info.m_id_master_bioseq.NotEmpty()) {
                CheckOrganisms(*master_info.m_id_master_bioseq, *master_info.m_master_bioseq);
            }

            UpdateCommonComments(master_info.m_id_master_bioseq, master_info.m_common_comments);

            // TODO
        }

        // TODO ...

        if (master_info.m_master_bioseq->GetSeq().IsNa()) {
            RemoveDupPubs(master_info.m_master_bioseq->SetDescr());
        }
        if (master_info.m_id_master_bioseq.NotEmpty() && master_info.m_id_master_bioseq->GetSeq().IsNa()) {
            RemoveDupPubs(master_info.m_id_master_bioseq->SetDescr());
        }

        if (!ParseSubmissions(master_info)) {
            return ERROR_RET;
        }

        if (!master_info.m_same_org) {
            ERR_POST_EX(0, 0, Error << "Different OrgRefs found in data. Will not populate BioSource descriptor in master record.");
            RemoveBioSourseDesc(master_info.m_master_bioseq->SetDescr());
        }

        if (GetParams().IsTest() || GetParams().GetUpdateMode() == eUpdatePartial)
            ; // do nothing
        else if (GetParams().GetUpdateMode() == eUpdateFull || GetParams().GetUpdateMode() == eUpdateExtraContigs ||
                 GetParams().GetUpdateMode() == eUpdateScaffoldsUpd) {

            if (GetParams().GetUpdateMode() == eUpdateFull) {
                UpdateBioSource(*master_info.m_master_bioseq, *master_info.m_id_master_bioseq);
            }

            FixGbblockSource(*master_info.m_id_master_bioseq);
            FixMasterDates(master_info, true);
            if (!OutputMaster(master_info.m_id_master_bioseq, master_info.m_master_file_name, master_info.m_num_of_entries)) {
                ret = ERROR_RET;
            }
        }
        else {
            FixGbblockSource(*master_info.m_master_bioseq);
            FixMasterDates(master_info, false);
            if (!OutputMaster(master_info.m_master_bioseq, master_info.m_master_file_name, master_info.m_num_of_entries)) {
                ret = ERROR_RET;
            }
        }

        if (!GetParams().IsTest()) {

            if (GetParams().IsVDBMode()) {
                PrintUnorderedList(master_info);
            }
            else {
                PrintGenAccOrderList(master_info);
            }
        }
    }

    return ret;
}

} // namespace wgsparse


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return wgsparse::CWGSParseApp().AppMain(argc, argv);
}


// TODO
// rep_locals - calculate in WGSParseSubmissions (based on WGSCheckNucBioseqs)
