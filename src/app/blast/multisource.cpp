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
 * Author:  Kevin Bealer
 *
 * File Description:
 *   Database formatting with multiple input sources.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>

// Objects includes
#include <objects/seqloc/Seq_id.hpp>
#include <objects/blastdb/blastdb__.hpp>
#include <objects/blastdb/defline_extra.hpp>

// SeqDB and WriteDB
#include <objtools/readers/seqdb/seqdb.hpp>
#include <objtools/writers/writedb/writedb.hpp>

// Local
#include "build_db.hpp"
#include "taxid_set.hpp"
#include "multisource_util.hpp"

using namespace ncbi;


// Application to build databases from multiple input sources.

class SGiFileKey {
public:
    SGiFileKey(const string & opt,
               const string & key,
               const string & desc,
               LinkoutTypes   link_bit)
        : m_OptName (opt),
          m_Key     (key),
          m_Desc    (desc),
          m_LinkBit (link_bit)
    {
        _ASSERT(! (m_OptName.empty() ||
                   m_Key.empty()     ||
                   m_Desc.empty()));
    }
    
    void AddOption(CArgDescriptions & args) const
    {
        // Add option for linkout bit GI list.
        args.AddOptionalKey(m_OptName,
                            m_Key,
                            m_Desc,
                            CArgDescriptions::eInputFile);
    }
    
    void ReadFile(const CArgs & args, map< int, vector<string> > & link_ids) const
    {
        if (args[m_OptName].HasValue()) {
            vector<string> & v = link_ids[(int) m_LinkBit];
            CNcbiIstream & f = args[m_OptName].AsInputFile();
            
            ReadTextFile(f, v);
        }
    }
    
private:
    string         m_OptName;
    string         m_Key;
    string         m_Desc;
    LinkoutTypes   m_LinkBit;
};


class CMultisourceApplication : public CNcbiApplication {
public:
    CMultisourceApplication()
        : m_FastaFile   (0),
          m_LogFile     (& cout)
    {
    }
    
    virtual void Init();
    
    virtual int  Run ();
    
    ostream & LogFile()
    {
        return *m_LogFile;
    }
    
private:
    void x_SetupGiFileKeys();
    
    void x_FetchArgs();
    
    void x_ReadFasta(CNcbiIstream & input);
    
    CRef<CScope> x_GetScope();
    
    vector<string> m_Ids;
    
    CRef<CBuildDatabase> m_Builder;
    
    map< int, vector<string> > m_LinkIds;
    map< int, vector<string> > m_MembBits;
    
    vector<SGiFileKey> m_GiFiles;
    
    CNcbiIstream * m_FastaFile;
    
    ostream      * m_LogFile;
};


class CDbTypeConstraint : public CArgAllow_Strings {
public:
    CDbTypeConstraint()
        : CArgAllow_Strings(NStr::eNocase)
    {
        *Allow("protein"), "nucleotide", "auto", "p", "n", "a";
    }
};


void CMultisourceApplication::Init()
{
    // Prepare command line descriptions
    //
    
    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    
    // Source database - can be any database with relevant data, such
    // as nr, nt, or nucl_dbs.
    
    // GROUP: Required Options
    
    arg_desc->SetCurrentGroup("Required Options");
    
    // Database file name.
    arg_desc->AddKey("db", "DbName", "Base name of database to build.",
                     CArgDescriptions::eString);
    
    // Database title.
    arg_desc->AddKey("title", "DbTitle", "Title for new database.",
                     CArgDescriptions::eString);
    
    
    // GROUP: Other Common Options
    
    arg_desc->SetCurrentGroup("Output Database Options");
    
    arg_desc->AddDefaultKey("type", "ProtNucl", "Specify P, N, or auto.",
                            CArgDescriptions::eString, "auto");
    
    arg_desc->SetConstraint("type", new CDbTypeConstraint);
    
    
    // GROUP: Data Sources
    
    arg_desc->SetCurrentGroup("Data Sources");
    
    arg_desc->AddOptionalKey("sourcedb", "SourceDB", "Database input source.",
                             CArgDescriptions::eString);
    
    // Fasta Data
    arg_desc->AddOptionalKey("fasta", "FastaFile",
                             "Add all sequences from this FASTA source file.",
                             CArgDescriptions::eInputFile);
    
    // Fasta Data
    arg_desc->AddOptionalKey("fasta-source", "FastaSource",
                             "Use this FASTA file to find sequences in -ids.",
                             CArgDescriptions::eInputFile);
    
    // GIs and/or Accessions to include
    arg_desc->AddOptionalKey("ids", "IdList",
                             "Identifiers of sequences to include in the database.",
                             CArgDescriptions::eInputFile);
    
    // Use remote data sources.
    arg_desc->AddDefaultKey("use-remote-fetching", "UseRemoteFetching",
                            "Use remote services to fetch sequences if other methods fail.",
                            CArgDescriptions::eBoolean, "true");
    
    
    // GROUP: TaxIDs
    
    arg_desc->SetCurrentGroup("Taxonomy IDs");
    
    // 'Global' taxid
    arg_desc->AddOptionalKey("taxid", "TaxID",
                             "Taxonomic ID to use for all sequences.",
                             CArgDescriptions::eInteger);
    
    // Map of Seq-ids or GIs to taxids.
    arg_desc->AddOptionalKey("taxid-map", "TaxIDMap",
                             "File mapping sequence ids to taxid.",
                             CArgDescriptions::eInputFile);
    
    // GROUP: Linkouts and Membership Bits
    
    arg_desc->SetCurrentGroup("Linkout and Membership Bits");
    
    // Build keys for GI files.
    x_SetupGiFileKeys();
    
    ITERATE(vector<SGiFileKey>, iter, m_GiFiles) {
        iter->AddOption(*arg_desc);
    }
    
    // GROUP: Application Options
    
    arg_desc->SetCurrentGroup("Application Options");
    
    
    // GROUP: Special Purpose Options
    
    arg_desc->SetCurrentGroup("Special Purpose Options");
    
    // Residues or bases to mask.
    arg_desc->AddOptionalKey("mask-letters", "MaskLetters",
                             "Mask these letters.",
                             CArgDescriptions::eString);
    
    // Capture output to a log file.
    arg_desc->AddOptionalKey("logfile", "File_Name",
                             "File to which the program log should be redirected",
                             CArgDescriptions::eOutputFile, CArgDescriptions::fAppend);
    
    // Make output verbose (indicate disposition of each sequence.)
    
    arg_desc->AddDefaultKey("verbose", "Verbosity",
                             "Specify true to increase the level of logging detail.",
                             CArgDescriptions::eBoolean,
                             "false");
    
    
    // End of options
    
    // Program description
    string prog_description = "Tool to build databases from multiple sources.\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description,
                              true);
    
    // Pass argument descriptions to the application
    
    SetupArgDescriptions(arg_desc.release());
}


void CMultisourceApplication::x_FetchArgs()
{
    const CArgs& args = GetArgs();
    
    if (args["logfile"].HasValue()) {
        m_LogFile = & args["logfile"].AsOutputFile();
    }
    
    // Simple mandatory arguments.
    
    string dbname = args["db"].AsString();
    string title  = args["title"].AsString();
    
    // Source DB (if one is provided.)
    
    CRef<CSeqDBExpert> src_db;
    string src_db_name;
    
    if (args["sourcedb"].HasValue()) {
        src_db_name = args["sourcedb"].AsString();
    }
    
    // Find SeqDB database type
    
    char dbtype = toupper(args["type"].AsString()[0]);
    
    bool is_protein = false;
    bool found = true;
    
    switch(dbtype) {
    case 'A':
        // The sequence type is not specified, so try to detect it via
        // the source database (if that is specified).
        
        found = false;
        
        try {
            if (src_db_name.size()) {
                src_db.Reset(new CSeqDBExpert(src_db_name, CSeqDB::eUnknown));
                is_protein = src_db->GetSequenceType() == CSeqDB::eProtein;
                found = true;
            }
        }
        catch(CSeqDBException &) {
        }
        
        if (! found) {
            NCBI_THROW(CMultisourceException, eArg,
                       "Database type must be specified (P or N) "
                       "if source_db is not.");
        }
        break;
        
    case 'P':
        is_protein = true;
        break;
        
    case 'N':
        is_protein = false;
        break;
    }
    
    m_Builder.Reset(new CBuildDatabase(dbname, title, is_protein, m_LogFile));
    
    // Specify the source db.
    
    if (src_db_name.size()) {
        if (src_db.NotEmpty()) {
            m_Builder->SetSourceDb(src_db);
            src_db.Reset();
        } else {
            m_Builder->SetSourceDb(src_db_name);
        }
    }
    
    // Read list of accessions
    
    if (args["ids"].HasValue()) {
        ReadTextFile(args["ids"].AsInputFile(), m_Ids);
    }
    
    // Global taxid (optional)
    
    if (args["taxid"].HasValue() && args["taxid-map"].HasValue()) {
        NCBI_THROW(CMultisourceException, eArg,
                   "Cannot specify both global taxid and mapping.");
    }
    
    // Global taxid (optional)
    
    if (args["taxid"].HasValue()) {
        CRef<CTaxIdSet> taxids(new CTaxIdSet);
        taxids->SetGlobalTaxId(args["taxid"].AsInteger());
        
        m_Builder->SetTaxids(*taxids);
    }
    
    // Taxid map (optional)
    
    if (args["taxid-map"].HasValue()) {
        CRef<CTaxIdSet> taxids(new CTaxIdSet);
        taxids->SetMappingFromFile(args["taxid-map"].AsInputFile());
        
        m_Builder->SetTaxids(*taxids);
    }
    
    // Linkout id file (optional)
    
    ITERATE(vector<SGiFileKey>, iter, m_GiFiles) {
        iter->ReadFile(args, m_LinkIds);
    }
    
    bool keep_l(true), keep_m(true);
    
    m_Builder->SetKeepTaxIds(true);
    
    m_Builder->SetLinkouts(m_LinkIds, keep_l);
    m_Builder->SetMembBits(m_MembBits, keep_m);
    
    if (args["mask-letters"].HasValue()) {
        m_Builder->SetMaskLetters(args["mask-letters"].AsString());
    }
    
    if (args["fasta"].HasValue()) {
        m_FastaFile = & args["fasta"].AsInputFile();
    }
    
//  if (args["fasta-source"].HasValue()) {
//      CNcbiIstream * fs_file = & args["fasta-source"].AsInputFile();
//      CRef<CFastaSource> fs(new CFastaSource(fs_file));
//      
//      m_Builder->SetFastaSource(fs);
//  }
    
    if (args["use-remote-fetching"].HasValue()) {
        m_Builder->SetUseRemote(args["use-remote-fetching"].AsBoolean());
    }
    
    if (args["verbose"].HasValue()) {
        m_Builder->SetVerbosity(args["verbose"].AsBoolean());
    }
}


void CMultisourceApplication::x_SetupGiFileKeys()
{
    m_GiFiles.push_back(SGiFileKey("hit-in-mv",
                                   "HitInMapViewer",
                                   "File with a list of GIs for which the mapviewer has a hit.",
                                   eHitInMapviewer));
    
    m_GiFiles.push_back(SGiFileKey("annotated-in-mv",
                                   "AnnotatedInMapViewer",
                                   "File with a list of GIs for which the mapviewer has an annotation.",
                                   eAnnotatedInMapviewer));
    
    m_GiFiles.push_back(SGiFileKey("genomic-sequences",
                                   "GenomicSeqs",
                                   "File with a list of GIs for genomic sequences.",
                                   eGenomicSeq));
    
    m_GiFiles.push_back(SGiFileKey("annotated-in-ug",
                                   "AnnotatedInUnigene",
                                   "File with a list of GIs for which Unigene has an annotation.",
                                   eUnigene));
    
    m_GiFiles.push_back(SGiFileKey("annotated-in-geo",
                                   "AnnotatedInGeo",
                                   "File with a list of GIs for which Geo has an annotation.",
                                   eGeo));
    
    m_GiFiles.push_back(SGiFileKey("annotated-in-gene",
                                   "AnnotatedInGene",
                                   "File with a list of GIs for which Gene has an annotation.",
                                   eGene));
}


int CMultisourceApplication::Run()
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());
    
    // Process command line.
    
    x_FetchArgs();
    
    // Resolve All Ids.
    
    bool rv = m_Builder->Build(m_Ids, m_FastaFile);
    
    return rv ? 0 : 1;
}


// MAIN

int main(int argc, const char* argv[])
{
    try {
        CMultisourceApplication app;
        
        try {
            int rv = app.AppMain(argc, argv, 0,
                                 eDS_Default, 0);
            
            return rv;
        } catch (const CException& e) {
            app.LogFile() << e.what() << endl;
        }
    } catch (const CException& e) {
        cerr << e.what() << endl;
    }
    
    return 1;
}


/*
 * ===========================================================================
 *
 * $Log: multisource.cpp,v $
 * Revision 1.12  2007/07/03 19:48:20  bealer
 * - Remove -keep options in favor of always keeping taxids, bits and linkouts.
 *
 * Revision 1.11  2007/07/03 17:21:16  bealer
 * - Organize options into groups.
 *
 * Revision 1.10  2007/04/30 20:37:54  bealer
 * - Use default fields for arguments currently having defacto defaults.
 * - Add -verbose flag.  In verbose mode, per-sequence information is
 *   reported, otherwise only summary information is produced.
 * - Add flags for unigene, geo, and gene bits.
 * - Fix return value for Build method.
 * - Count and report the number of deflines and sequences seperately.
 *
 * Revision 1.9  2007/04/24 16:18:28  bealer
 * - Append to log file instead of overwriting it.
 *
 * Revision 1.8  2007/04/12 20:32:17  bealer
 * - Changed default values of -keep methods to true.
 * - Add -use-remote-fetching flag (defaults to true).
 * - Write a line to the log explaining that no volumes were created if
 *   that is the case.
 *
 * Revision 1.7  2007/04/09 17:37:08  bealer
 * - Support -logfile feature.
 *
 * Revision 1.6  2007/04/06 20:20:33  bealer
 * - Add -keep-taxids feature.
 *
 * Revision 1.5  2007/04/05 23:21:55  bealer
 * - Move static functions into classes where appropriate.
 * - Refactor some code to check errors more firmly.
 * - Fix some cut-and-paste comment issues.
 * - Make code more modular - more classes, less non-class code.
 * - Move more of the working code out of crowded user interface areas.
 *
 * Revision 1.4  2007/03/06 22:58:56  bealer
 * - Support for GI to TaxID map file.
 * - TaxID handling moved to new file (taxid_set.[ch]pp).
 * - Some utility code moved to new file (util.[ch]pp).
 * - CMultisourceException now in util.hpp file.
 * - Bug in taxid vs. bioseq handling fixed.
 *
 * Revision 1.3  2007/03/06 19:16:45  bealer
 * - SourceDB is now optional, allowing database builds just from FASTA.
 *
 * Revision 1.2  2007/03/06 19:00:52  bealer
 * - Allow any case for sequence type indicators.
 *
 * Revision 1.1  2006/10/31 23:03:44  bealer
 * - Multisource app - builds databases using existing DBs and ID2 queries.
 *
 * ===========================================================================
 */
