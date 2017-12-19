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

    arg_desc->AddDefaultKey("u", "ParsingMode", "Parsing mode:\n        1 - PARTIAL UPDATE, i.e. only members of project will be updated;\n        2 - ASSEMBLY UPDATE, the whole new assembly will be updated;\n        3 - SCAFFOLDS, parse brand new scaffolds for already existing\n            project;\n        4 - PARTIAL UPDATE AS COMPLETE, same as PARTIAL UPDATE\n            with master sequence generated;\n        5 - SCAFFOLDS UPDATE, parse already existing scaffolds,\n            no accessions assignment;\n        6 - EXTRA CONTIGS, add extra contigs to already existing project;\n        0 - Brand new project.", CArgDescriptions::eInteger, "0");
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

static bool OutputMaster(const CMasterInfo& info)
{
    // TODO FixMasterDates

    string fname = GetParams().GetOutputDir() + '/';
    if (info.m_master_file_name.empty()) {
        fname += GetParams().GetIdPrefix() + ToStringLeadZeroes(GetParams().GetAssemblyVersion(), 2) + ToStringLeadZeroes(0, GetMaxAccessionLen(info.m_num_of_entries));
    }
    else {
        fname += info.m_master_file_name;
    }

    if (!GetParams().IsOverrideExisting() && CFile(fname).Exists()) {
        ERR_POST_EX(0, 0, Error << "File to print out processed submission already exists: \"" << fname << "\". Override is not allowed.");
        return false;
    }

    try {
        CNcbiOfstream out(fname);

        if (GetParams().IsBinaryOutput())
            out << MSerial_AsnBinary << info.m_master_bioseq;
        else
            out << MSerial_AsnText << info.m_master_bioseq;
    }
    catch (CException& e) {
        ERR_POST_EX(0, 0, Error << "Failed to save processed submission to file: \"" << fname << "\" [" << e.GetMsg() << "]. Cannot proceed.");
        return false;
    }

    ERR_POST_EX(0, 0, Info << "Master Bioseq saved in file \"" << fname << "\".");

    return true;
}

static const int ERROR_RET = 1;

// CR Use ZZZZ prefix to mock completely new submission
int CWGSParseApp::Run(void)
{
    int ret = 0;

    if (SetParams(GetArgs())) {

        CRef<CSeq_entry> master_entry = GetMasterEntry();
        if (GetParams().GetUpdateMode() == eUpdateNew) {
            if (master_entry.NotEmpty()) {
                if (!GetParams().EnforceNew()) {
                    ERR_POST_EX(0, 0, "Incorrect parsing mode set in command line: this is not a brand new project.");
                    return ERROR_RET;
                }
                master_entry.Reset();
            }
        }
        else {
            // TODO - CR put some error 
        }

        CMasterInfo master_info;
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

        // TODO ...

        CCleanup cleanup;
        cleanup.ExtendedCleanup(*master_info.m_master_bioseq);

        // TODO ...

        if (!master_info.m_master_bioseq->GetSeq().IsNa()) {
            RemoveDupPubs(master_info.m_master_bioseq->SetDescr());
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

            // TODO
        }
        else {
            FixGbblockSource(*master_info.m_master_bioseq);
            if (!OutputMaster(master_info)) {
                ret = ERROR_RET;
            }
        }

        if (!GetParams().IsTest()) {
            // TODO
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