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
 *   Simple application to test integrity of databases.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <algo/blast/api/version.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <util/random_gen.hpp>
#include <util/line_reader.hpp>
#include <corelib/ncbi_mask.hpp>

#include <algo/blast/blastinput/blast_input.hpp>
#include "../blast/blast_app_util.hpp"

#include <iostream>
#include <sstream>
#include <set>
#include <vector>

USING_NCBI_SCOPE;
USING_SCOPE(blast);


/////////////////////////////////////////////////////////////////////////////
///  CBlastDbCheckApplication: the main application class
class CBlastDbCheckApplication : public CNcbiApplication {
public:
    /** @inheritDoc */
    CBlastDbCheckApplication () {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }
    
private:
    /** @inheritDoc */
    virtual void Init(void);
    /** @inheritDoc */
    virtual int  Run(void);
    /** @inheritDoc */
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments

/// Verbosity levels.
enum {
    e_Silent,
    e_Brief,
    e_Summary,
    e_Details,
    e_Minutiae,
    e_Max = e_Minutiae
};

/// Types of tests (bit).
enum {
    e_IsamLookup = 1,
    e_Legacy = 2
};

static string s_VerbosityString(int v)
{
    switch(v) {
    case e_Silent:   return "Quiet";
    case e_Brief:    return "Brief";
    case e_Summary:  return "Summary";
    case e_Details:  return "Detailed";
    case e_Minutiae: return "Minutiae";
    default:
        return "";
    };
}

static string s_VerbosityText()
{
    string rv;
    
    for(int i = 0; i <= e_Max; i++) {
        string s = s_VerbosityString(i);
        
        if (rv.size()) {
            rv += ", ";
        }
        
        rv += NStr::IntToString(i) + "=" + s;
    }
    
    return rv;
}

void CBlastDbCheckApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "BLAST database integrity and validity "
                              "checking application");
    
    arg_desc->SetCurrentGroup("Input Options");

    arg_desc->AddOptionalKey
        ("db", "DbName",
         "Specify a database name.",
         CArgDescriptions::eString);
    
    arg_desc->AddDefaultKey("dbtype", "molecule_type",
                            "Molecule type of database",
                            CArgDescriptions::eString,
                            "guess");
    
    arg_desc->SetConstraint("dbtype", &(*new CArgAllow_Strings,
                                        "nucl", "prot", "guess"));
    
    arg_desc->AddOptionalKey
        ("dir", "DirName",
         "Specify a directory containing one or more databases.",
         CArgDescriptions::eString);
    arg_desc->SetDependency("dir", CArgDescriptions::eExcludes, "db");
    
    arg_desc->AddFlag
        ("recursive",
         "Specify true to recurse through all dbs in directory tree.");
    arg_desc->SetDependency("recursive", CArgDescriptions::eExcludes, "db");
    
    
    arg_desc->SetCurrentGroup("Output Options");
    
    // Describe the expected command-line arguments
    arg_desc->AddOptionalKey
        ("logfile", "LogFile",
         "If specified, output will be redirected to this file.",
         CArgDescriptions::eOutputFile);
    
    arg_desc->AddDefaultKey
        ("verbosity", "DefaultKey",
         s_VerbosityText(),
         CArgDescriptions::eInteger,
         NStr::IntToString(e_Summary));
    arg_desc->SetConstraint("verbosity", new
                            CArgAllowValuesBetween((int)e_Silent,
                                                   (int)e_Max, true));
    
// Threading and multiprocess support should not be too complex, but
// I'll defer writing them until it is more obvious that there is
// actually a need for them.
//
//     arg_desc->AddFlag
//         ("fork",
//          "If true, fork() will be used to protect main app from crashes.");
//
//     arg_desc->AddDefaultKey
//         ("threads", "NumThreads",
//          "Number of threads (or processes with -fork) to use at once.",
//          CArgDescriptions::eInteger, "1");
    
    arg_desc->SetCurrentGroup("Test Methods");
    
    arg_desc->AddFlag
        ("full",
         "If true, test every sequence (warning: may be slow).");
    arg_desc->SetDependency("full", CArgDescriptions::eExcludes, "stride");
    arg_desc->SetDependency("full", CArgDescriptions::eExcludes, "random");
    arg_desc->SetDependency("full", CArgDescriptions::eExcludes, "ends");
    
    arg_desc->AddOptionalKey
        ("stride", "StrideLength",
         "Check integrity of every Nth sequence.",
         CArgDescriptions::eInteger);
    
    arg_desc->AddOptionalKey
        ("random", "NumSequences",
         "Check this many randomly selected sequences.",
         CArgDescriptions::eInteger);
    
    arg_desc->AddOptionalKey
        ("ends", "NumSequences",
         "Check this many sequences at each end of the database.",
         CArgDescriptions::eInteger);
    
    arg_desc->AddFlag
        ("no_isam", 
         "Disable ISAM testing.");

    arg_desc->AddFlag
        ("legacy", 
         "Enable check for existence of temporary files.");
         
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


typedef set<int> TSeen;


class CBlastDbCheckLog {
public:
    CBlastDbCheckLog(ostream & outp, int max_level)
        : m_Level(max_level), m_Output(outp)
    {
    }
    
    ostream & Log(int L)
    {
        if (L <= m_Level) {
            return m_Output;
        } else {
            m_DevNull.seekp(0);
            return m_DevNull;
        }
    }
    
    int GetLevel()
    {
        return m_Level;
    }
    
private:
    CBlastDbCheckLog(const CBlastDbCheckLog &);
    CBlastDbCheckLog & operator=(const CBlastDbCheckLog &);
    
    int m_Level;
    ostream & m_Output;
    ostringstream m_DevNull;
};


//class CWrap;
//class CTestAction;


class CTestAction : public CObject {
public:
    CTestAction(CBlastDbCheckLog & log, const string & nm, int flags)
        : m_Log(log), m_TestName(nm), m_Flags(flags)
    {
    }
    
    virtual int DoTest(CSeqDB & db, TSeen & seen) = 0;
    
    ostream & Log(CSeqDB & db, int lvl)
    {
        m_Log.Log(lvl) << "  " << db.GetDBNameList() << " / " << m_TestName << ": ";
        return m_Log.Log(lvl);
    }
    
    ostream & LogMore(int lvl)
    {
        return m_Log.Log(lvl);
    }
    
    string Name()
    {
        return m_TestName;
    }
    
    bool TestOID(CSeqDB & db, TSeen & seen, int oid);
    
    int LogLevel()
    {
        return m_Log.GetLevel();
    }
    
private:
    CBlastDbCheckLog & m_Log;
    const string m_TestName;

protected:
    int m_Flags;

};

/*
class CWrap : public CObject {
public:
    virtual int DoTest(CTestAction & action, CSeqDB & db, TSeen & seen) = 0;
};
*/


// class CForkWrap : public CWrap {
// public:
//     virtual int DoTest(CTestAction & action, CSeqDB & db, TSeen & seen)
//     {
//         throw runtime_error("unimp: virtual void CForkWrap");
//     }
// };


class CTestActionList : public CObject {
public:
    void Add(CTestAction * action)
    {
        m_List.push_back(CRef<CTestAction>(action));
    }
    
    /*
    void SetWrap(CWrap * wrap)
    {
        m_Wrap = wrap;
    }*/
    
    int DoTests(CSeqDB & db, TSeen & seen)
    {
        int  tot_faults = 0;
        
        NON_CONST_ITERATE(vector< CRef<CTestAction> >, iter, m_List) {
            CTestAction & test = **iter;
            /* if (m_Wrap)  num_faults = m_Wrap->DoTest(test, db, seen);
               else  num_faults = test.DoTest(db, seen); */
            tot_faults += test.DoTest(db, seen);
        }
        
        return tot_faults;
    }
    
private:
    vector< CRef<CTestAction> > m_List;
    //CRef<CWrap> m_Wrap;
};

        
class CMetaDataTest : public CTestAction {
public:
    CMetaDataTest(CBlastDbCheckLog & log, int flags)
        : CTestAction(log, "MetaData", flags)
    {
    }
    
    virtual int DoTest(CSeqDB & db, TSeen & /*seen*/)
    {
        // Here I get more values than I actually have useful tests
        // for, because I want to trigger exceptions due to binary
        // file corruption in the all the myriad files.

        int num_failures = 0;

        try {
        int noid = db.GetNumOIDs();
        int nseq = db.GetNumSeqs();
        /*string t =*/ db.GetTitle();
        string d = db.GetDate();
        Uint8 tl = db.GetTotalLength();
        Uint8 vl = db.GetVolumeLength();
        
        vector<string> vols;
        db.FindVolumePaths(vols);

        int nv = vols.size();
        
        SSeqDBTaxInfo taxinfo;
        db.GetTaxInfo(9606, taxinfo);
        
        if (! d.size()) {
            Log(db, e_Brief) << "[ERROR] db has empty date string" << endl;
            num_failures++;
        }
        if (! nv) {
            Log(db, e_Brief) << "[ERROR] db has no volumes" << endl;
            num_failures++;
        }

        ITERATE(vector<string>, vol, vols) {
            
            CMaskFileName db_mask;
            db_mask.Add(CFile(*vol).GetName()+".???");

            CDir dir(CFile(*vol).GetDir());

            CDir::TEntries entries(dir.GetEntries(db_mask));
            ITERATE(CDir::TEntries, entry, entries) {
                
                // check for legacy files
                if (m_Flags & e_Legacy) {
                    CMaskFileName legacy_name;
                    legacy_name.Add("*tm");
                    if (legacy_name.Match((*entry)->GetExt())) {
                        Log(db, e_Brief) << "[ERROR] legacy file " << (*entry)->GetPath() <<
                             " exists." << endl;
                        num_failures++;
                    }
                }

                // check for zero-length files
                if ((*entry)->IsFile() && (CFile((*entry)->GetPath()).GetLength() <= 0)) {
                    Log(db, e_Brief) << "[ERROR] file " << (*entry)->GetPath() <<
                             " has zero length." << endl;
                    num_failures++;
                }
            }
        }

        if ((nseq > noid) || ((! tl) != (! noid)) || ((! vl) && tl)) {
            Log(db, e_Brief) << "[ERROR] sequence count/length mismatch" << endl;
            num_failures++;
        }
        string hs = taxinfo.scientific_name;
        if (hs != "Homo sapiens") {
            Log(db, e_Brief) << "[ERROR] tax info looks wrong (" << hs << ")" << endl;
            num_failures++;
        }
        
        string s1("a"), s2("b");
        
        for(int i = 0; i < 100000; i++) {
            string s3 = s1 + s2;
            
            if (s3.size() > 100) {
                s3 = "c";
            }
            
            if (i & 1) {
                s1 = s3;
            } else {
                s2 = s3;
            }
        }} catch(exception &e) {
            num_failures++;
            Log(db, e_Brief) << "  [ERROR] caught exception." << endl;
            Log(db, e_Details) << e.what() << endl;
        }
        
        return num_failures;
    }
};

bool CTestAction::TestOID(CSeqDB & db, TSeen & seen, int oid)
{
    ostringstream details;
    ostringstream minutiae;
    
    // If we've seen this OID before (for this db instance), assume 'true'.
    
    if (seen.find(oid) != seen.end()) {
        return true;
    }
    
    seen.insert(oid);
    
    string where;
    bool rv = true;
    
    try {
        // Getting the SeqIDs implies getting the headers.
        where = "headers";
        list< CRef<CSeq_id> > seqids = db.GetSeqIDs(oid);
        
        // These non-bioseq things are a subset of the work done for
        // GetBioseq.  By doing these first, we might have a better
        // idea where the failure actually occured than by calling
        // GetBioseq() directly.
        
        where = "sequence";
        const char * p = NULL;
        int length = db.GetSequence(oid, & p);
        if ((length == 0) || (p == NULL)) {
            throw runtime_error("sequence data is empty");
        }
        
        db.RetSequence(& p);
        
        // Finally, the bioseq, which also tests the taxinfo dbs.
        
        where = "bioseq";
        CRef<CBioseq> bs = db.GetBioseq(oid);
        if (bs.Empty()) {
            throw runtime_error("no bioseq");
        }
        
        // Reverse look up all the Seq-ids.
        
        if (m_Flags & e_IsamLookup) {
            where = "isam lookups";
            ITERATE(list< CRef<CSeq_id> >, iter, seqids) {
                int oid2(-1);
            
                ostringstream msg;
            
                if ((! db.SeqidToOid(**iter, oid2)) || (oid != oid2)) {
                    if (! db.SeqidToOid(**iter, oid2)) {
                        msg << "seqid=" << (**iter).AsFastaString();
                        throw runtime_error(msg.str());
                    } else if (oid != oid2) {
                        msg << "oid1=" << oid
                            << " oid2=" << oid2
                            << " seqid=" << (**iter).AsFastaString();
                        throw runtime_error(msg.str());
                    }
                }
            }
        }
    }
    catch(CSeqDBException & e) {
        details << "Failed during " << where
                << ", oid " << oid
                << ": " << e.what() << endl;
        
        rv = false;
    }
    catch(exception & e) {
        details << where
                << " failed; oid=" << oid
                << ": " << e.what() << endl;
        
        rv = false;
    }
    
    minutiae << "Status for OID " << oid << ": "
             << (rv ? "PASS" : "FAIL") << endl;
    
    string msg = details.str();
    string msg2 = minutiae.str();
    
    if (msg.size()) {
        Log(db, e_Details) << "    " << msg << flush;
    }
    
    if (msg2.size()) {
        Log(db, e_Minutiae) << "      " << msg2 << flush;
    }
    
    return rv;
}


class CStrideTest : public CTestAction {
public:
    CStrideTest(CBlastDbCheckLog & log, int n, int flags)
        : CTestAction(log, "Stride", flags), m_N(n)
    {
    }
    
    virtual int DoTest(CSeqDB & db, TSeen & seen)
    {
        Log(db, e_Minutiae) << "<testing every " << m_N << "th OID>" << endl;
        
        for(int oid = 0; db.CheckOrFindOID(oid); oid += m_N) {
            if (! TestOID(db, seen, oid)) {
                return 1;
            }
        }
        return 0;
    }
    
private:
    int m_N;
};


class CSampleTest : public CTestAction {
public:
    CSampleTest(CBlastDbCheckLog & log, int n, int flags)
        : CTestAction(log, "Sample", flags), m_N(n)
    {
        CTime now(CTime::eCurrent);
        time_t t = now.GetTimeT();
        m_Rng.SetSeed(t);
    }
    
    virtual int DoTest(CSeqDB & db, TSeen & seen)
    {
        int max = db.GetNumOIDs();
        
        if (! max) {
            // Technically this is still a valid DB... not any more...
            Log(db, e_Brief) << "  [ERROR] empty volume" << endl;
            return 1;
        }
        
        set<int> oids;
        
        for(int i = 0; i < m_N;) {
            int oid = m_Rng.GetRand(0, max-1);
            // for alias DBs, not all OIDs will be fine, so check it first
            if (db.CheckOrFindOID(oid)) {
                oids.insert(oid);
                i++;
            }
        }
        
        Log(db, e_Minutiae) << "<testing " << m_N
                            << " randomly selected OIDs (" << oids.size()
                            << " unique)>" << endl;
        
        ITERATE(set<int>, iter, oids) {
            if (! TestOID(db, seen, *iter)) {
                return 1;
            }
        }
        
        return 0;
    }
    
private:
    CRandom m_Rng;
    int m_N;
};


class CEndsTest : public CTestAction {
public:
    CEndsTest(CBlastDbCheckLog & log, int n, int flags)
        : CTestAction(log, "EndPoints", flags), m_N(n)
    {
    }
    
    virtual int DoTest(CSeqDB & db, TSeen & seen)
    {
        Log(db, e_Minutiae) << "<testing " << m_N << " OIDs at each end>" << endl;

        for(int oid = 0; db.CheckOrFindOID(oid); oid ++) {
            if (oid >= m_N) {
                int new_oid = db.GetNumOIDs()-m_N;
                
                if (new_oid > oid) {
                    oid = new_oid;
                }
            }
            if (! TestOID(db, seen, oid))
                return 1;
        }
        return 0;
    }
    
private:
    /// Test m_N elements from the start, m_N elements from the end
    int m_N;
};


class CAliasTest : public CObject {
public:
    CAliasTest(CBlastDbCheckLog & log, int flags)
        : m_Log (log),
          m_TestName ("AliasFileTest")
    {
    }
    
    int DoTest(const string & name, TSeen & /*seen*/)
    {
        // These tests are adapted from 
        // intranet/cvsutils/index.cgi/internal/blast/DBUpdates/test_alias_file.pl

        int num_failures = 0;

        const CFile f(name);
        const string dir(f.GetDir());
        const string base(f.GetBase());
        const string ext(f.GetExt());

        if (! f.Exists()) {
            Log(name, e_Brief) << "  [ERROR] alias file does not exist" << endl;
            return ++num_failures;
        }

        if (f.GetLength() <= 0) {
            Log(name, e_Brief) << "  [ERROR] alias file has zero length" << endl;
            return ++num_failures;
        }

        Int8 length;
        int oidlist, gilist, nseq, mem_bit, first_oid, last_oid, maxoid, maxlen;
        oidlist=gilist=nseq=length=mem_bit=first_oid=last_oid=maxoid=maxlen=-1;

        CNcbiIfstream is(name.c_str());
        CStreamLineReader line(is);

        do {
            ++line;
            if (NStr::StartsWith(*line, '#')) continue;  // ignore comments
            if (NStr::IsBlank(*line))  continue;  // ignore empty lines

            vector <string> tokens;
            NStr::Tokenize(*line, " \t\n\r", tokens, NStr::eMergeDelims);

            if (tokens.size() <= 1) {
                Log(name, e_Brief) << "  [ERROR] no value(s) found for keyword " 
                               << tokens[0] << endl;
                num_failures++;
                continue;
            }

            if (tokens[0] == "DBLIST") {
                // Don't check for right now
            } else if (tokens[0] == "TITLE") {
                // Don't check for right now
            } else if (tokens[0] == "MASKLIST") {
                // Don't check for right now
            } else if (tokens[0] == "OIDLIST") {
                oidlist = 0;
                num_failures += x_CheckFile(name, dir, tokens);
            } else if (tokens[0] == "GILIST") {
                gilist = 0;
                num_failures += x_CheckFile(name, dir, tokens);
            } else if (tokens[0] == "NSEQ") {
                num_failures += x_CheckNumber(name, tokens, nseq);
            } else if (tokens[0] == "LENGTH") {
                num_failures += x_CheckNumber8(name, tokens, length);
            } else if (tokens[0] == "MEMB_BIT") {
                num_failures += x_CheckNumber(name, tokens, mem_bit);
            } else if (tokens[0] == "FIRST_OID") {
                num_failures += x_CheckNumber(name, tokens, first_oid);
            } else if (tokens[0] == "LAST_OID") {
                num_failures += x_CheckNumber(name, tokens, last_oid);
            } else if (tokens[0] == "MAXOID") {
                num_failures += x_CheckNumber(name, tokens, maxoid);
            } else if (tokens[0] == "MAXLEN") {
                num_failures += x_CheckNumber(name, tokens, maxlen);
            } else {
                Log(name, e_Brief) << "  [ERROR] unknown keyword encountered: " 
                                   << tokens[0] << endl;
                num_failures++;
            }
            
        } while(!line.AtEOF());
        
        // check {first, last}_oid
        if ((first_oid+1)*(last_oid+1)==0) {
            if (first_oid+last_oid != -2) {
                Log(name, e_Brief) << "  [ERROR] FIRST_OID not paired with LAST_OID" << endl;
                num_failures++;
            }
        } else if ((nseq+1)*(nseq + first_oid -last_oid -1)!=0) {
            Log(name, e_Brief) << "  [ERROR] (FIRST_OID, LAST_OID) is not consistent" 
                               << " with NSEQ" << endl;
            num_failures++;
        }
            
        // check GILIST/OIDLIST
        if (oidlist + gilist >=0) {
            Log(name, e_Brief) << "  [ERROR] OIDLIST and GILIST cannot be present in the"
                               << " same alias file" << endl;
            num_failures++;
        }
        
        if (oidlist!=-1 && (maxoid+1)*(length+1)*(nseq+1)==0) {
            Log(name, e_Brief) << "  [ERROR] OIDLIST cannot be provided without MAXOID"
                               << " , LENGTH, or NSEQ" << endl;
            num_failures++;
        }

        // test the DB
        const string dbname(CDirEntry::MakePath(dir, base));
        try {
            CSeqDB::ESeqType dbtype = (ext[1] == 'p') ? 
                 CSeqDB::eProtein : CSeqDB::eNucleotide;
            CRef<CSeqDB> db(new CSeqDB(dbname, dbtype));
        } catch(exception &e) {
            num_failures++;
            Log(name, e_Brief) << "  [ERROR] caught exception in initializing blastdb" << endl;
            Log(name, e_Details) << e.what() << endl;
        }
        
        return num_failures;
    }

    ostream & Log(const string & name, int lvl)
    {
        m_Log.Log(lvl) << "  " << name << " / " << m_TestName << ": ";
        return m_Log.Log(lvl);
    }
    
private:
    CBlastDbCheckLog & m_Log;
    const string       m_TestName;

    int x_CheckFile(const string &name, const string &dir, const vector<string> &tokens)
    {
        int num_failures = 0;

        for (size_t i=1; i< tokens.size(); ++i) {
            CFile f(CDirEntry::MakePath(dir, tokens[i]));

            // File naming check 
            if (tokens[0] == "OIDLIST" && f.GetExt() != "msk") {
                Log(name, e_Details) << "  [WARNING] oidlist file " << tokens[i]
                               << " does not have .msk extension" << endl;
            }

            if (tokens[0] == "GILIST" && f.GetExt() != "gil") {
                Log(name, e_Details) << "  [WARNING] gilist file " << tokens[i]
                               << " does not have .gil extension" << endl;
            }

            // Existance check
            if (! f.Exists()) {
                Log(name, e_Brief) << "  [ERROR] file " << tokens[i]
                               << " referenced in keyword " << tokens[0] 
                               << " does not exist" << endl;
                num_failures++;
                continue;
            }
            // Sanity check for file size
            if (f.GetLength() <=0) {
                Log(name, e_Brief) << "  [ERROR] file " << tokens[i]
                               << " referenced in keyword " << tokens[0] 
                               << " has zero length" << endl;
                num_failures++;
                continue;
            }
        }
        return num_failures;
    };

    int x_CheckNumber(const string &name, const vector<string> &tokens, int &n)
    {
        try {
            n = NStr::StringToInt(tokens[1]);
            return 0;
        } catch (...) {
            Log(name, e_Brief) 
                 << "  [ERROR] could not convert value to number for "
                 << tokens[0] << endl;
            n = -1;
            return 1;
        }
    }

    int x_CheckNumber8(const string &name, const vector<string> &tokens, Int8 &n)
    {
        try {
            n = NStr::StringToInt8(tokens[1]);
            return 0;
        } catch (...) {
            Log(name, e_Brief) 
                 << "  [ERROR] could not convert value to number for "
                 << tokens[0] << endl;
            n = -1;
            return 1;
        }
    }
};

class CTestData : public CObject {
public:
    CTestData(CBlastDbCheckLog & outp, string dbtype)
        : m_Out      (outp),
          m_DbType   (dbtype) { }

    virtual bool Test(CTestActionList & action) = 0;

protected:
    CBlastDbCheckLog & m_Out;
    string             m_DbType;
};


class CDbTest : public CTestData {
public:
    CDbTest(CBlastDbCheckLog & outp, string db, string dbtype)
        : CTestData(outp, dbtype),
          m_Db     (db) { }
    
    virtual bool Test(CTestActionList & action);
    
private:
    string m_Db;

    int x_GetVolumeList(const vector <string> &dbs, 
                                CSeqDB::ESeqType stype, 
                                set <string> &vlist,
                                set <string> &alist) const;
};

int CDbTest::x_GetVolumeList(const vector <string>  &dbs, 
                              CSeqDB::ESeqType       stype, 
                              set <string>          &vlist,
                              set <string>          &alist) const {

    int retval = 0;
    ITERATE(vector<string>, iter, dbs) {
        vector <string> paths;
        vector <string> alias;
        try {
            CSeqDB::FindVolumePaths(*iter, stype, paths, &alias);
        } catch (...) {
            m_Out.Log(e_Brief) 
                 << "  [ERROR] could not find all volume or alias "
                 << "files referenced in " << *iter << ", [skipped]" << endl;
            retval++;
            continue;
        }
        vlist.insert(paths.begin(), paths.end());
        alist.insert(alias.begin(), alias.end());
    }
    return retval;
}

bool CDbTest::Test(CTestActionList & action)
{
    TSeen seen;
    int tot_faults = 0;
    
    vector<string> dbs;
    NStr::Tokenize(m_Db, " ", dbs, NStr::eMergeDelims);
    
    CSeqDB::ESeqType seqtype = ParseMoleculeTypeString(m_DbType);

    set <string> vol_list;
    set <string> ali_list;
    
    tot_faults += x_GetVolumeList(dbs, seqtype, vol_list, ali_list);
    
    // Test volume files
    int total = vol_list.size(), passed = 0;
    m_Out.Log(e_Summary)
        << "Testing " << total << " volume(s)." << endl;
    
    ITERATE(set<string>, iter, vol_list) {
        m_Out.Log(e_Details) << " " << *iter << endl;
        int num_faults = 0;

        try {
            CRef<CSeqDB> db(new CSeqDB(*iter, seqtype));
            num_faults = action.DoTests(*db, seen);
        } catch(exception &e) {
            num_faults++;
            m_Out.Log(e_Brief) << "  [ERROR] caught exception in " << *iter << endl;
            m_Out.Log(e_Details) << e.what() << endl;
        }
        
        if (num_faults) tot_faults += num_faults;
        else passed++;
    }

    if (total == passed) {
        m_Out.Log(e_Summary)
            << " Result=SUCCESS. No errors reported for "
            << total << " volume(s)." << endl;
    } else {
        m_Out.Log(e_Summary)
            << " Result=FAILURE. "
            << (total-passed) << " errors reported in "
            << total << " volume(s)." << endl;
    }
    
    // Test alias files
    total = ali_list.size(), passed = 0;
    m_Out.Log(e_Summary)
        << "Testing " << total << " alias(es)." << endl;
    CAliasTest ali_test(m_Out, 0);

    ITERATE(set<string>, iter, ali_list) {
        m_Out.Log(e_Details) << " " << *iter << endl;
        int num_faults = ali_test.DoTest(*iter, seen);
        if (num_faults)  tot_faults += num_faults;
        else passed++;
    }
    
    if (total == passed) {
        m_Out.Log(e_Summary)
            << " Result=SUCCESS. No errors reported for "
            << total << " alias(es)." << endl;
    } else {
        m_Out.Log(e_Summary)
            << " Result=FAILURE. "
            << (total-passed) << " errors reported in "
            << total << " alias(es)." << endl;
    }
    
    // Bottom line
    if (tot_faults) {
        m_Out.Log(e_Brief) <<  endl
           << "Total errors: " << tot_faults << endl;
    }
    
    return (tot_faults == 0);
}

class CDirTest : public CTestData {
public:
    CDirTest(CBlastDbCheckLog & outp,
             string dir,
             string dbtype,
             int threads,
             bool recurse)
        : CTestData(outp, dbtype),
          m_Dir     (dir),
          m_Threads (threads),
          m_Recurse (recurse) { }
    
    virtual bool Test(CTestActionList & action);
    
private:
    string         m_Dir;
    vector<SSeqDBInitInfo> m_DBs;
    
    int    m_Threads;
    bool   m_Recurse;

    int x_GetVolumeList(CSeqDB::ESeqType stype, set <string> &vlist, set <string> &alist) const;
};

int CDirTest::x_GetVolumeList(CSeqDB::ESeqType stype, set <string> &vlist, set <string> &alist) const {

    int retval = 0;
    ITERATE(vector<SSeqDBInitInfo>, iter, m_DBs) {
        if (iter->m_MoleculeType != stype) continue;
        vector <string> paths;
        vector <string> alias;
        try {
            CSeqDB::FindVolumePaths(iter->m_BlastDbName, iter->m_MoleculeType, paths, &alias);
        } catch (...) {
            m_Out.Log(e_Brief) 
                 << "  [ERROR] could not find all volume or alias "
                 << "files referenced in " << iter->m_BlastDbName << ", [skipped]" << endl;
            retval++;
            continue;
        }
        vlist.insert(paths.begin(), paths.end());
        alist.insert(alias.begin(), alias.end());
    }
    return retval;
   
};

bool CDirTest::Test(CTestActionList & action)
{
    int tot_faults = 0;
    TSeen seen;
    
    m_Out.Log(e_Summary)
        << "Finding database volumes." << endl;
    
    m_DBs = FindBlastDBs(m_Dir, m_DbType, m_Recurse, true);

    set <string> prot_list;
    set <string> nucl_list;
    set <string> ali_list;
    
    tot_faults += x_GetVolumeList(CSeqDB::eProtein, prot_list, ali_list);
    tot_faults += x_GetVolumeList(CSeqDB::eNucleotide, nucl_list, ali_list);
    
    // Test volumes
    int total = prot_list.size() + nucl_list.size(), passed = 0;
    m_Out.Log(e_Summary)
        << "Testing " << total << " volume(s)." << endl;

    ITERATE(set<string>, iter, prot_list) {
        m_Out.Log(e_Details) << " " << *iter << endl;
        int num_faults = 0;

        try {
            CRef<CSeqDB> db(new CSeqDB(*iter, CSeqDB::eProtein));
            num_faults = action.DoTests(*db, seen);
        } catch(exception &e) {
            num_faults++;
            m_Out.Log(e_Brief) << "  [ERROR] caught exception in " << *iter << endl;
            m_Out.Log(e_Details) << e.what() << endl;
        }
        
        if (num_faults) tot_faults += num_faults;
        else passed++;
    }
    
    ITERATE(set<string>, iter, nucl_list) {
        m_Out.Log(e_Details) << " " << *iter << endl;
        int num_faults = 0;

        try {
            CRef<CSeqDB> db(new CSeqDB(*iter, CSeqDB::eNucleotide));
            num_faults = action.DoTests(*db, seen);
        } catch(exception &e) {
            num_faults++;
            m_Out.Log(e_Brief) << "  [ERROR] caught exception in " << *iter << endl;
            m_Out.Log(e_Details) << e.what() << endl;
        }
        
        if (num_faults) tot_faults += num_faults;
        else  passed++;
    }

    if (total == passed) {
        m_Out.Log(e_Summary)
            << " Result=SUCCESS. No errors reported for "
            << total << " volumes." << endl;
    } else {
        m_Out.Log(e_Summary)
            << " Result=FAILURE. "
            << (total-passed) << " errors reported in "
            << total << " volumes." << endl;
    }

    // Test alias files
    total = ali_list.size(), passed = 0;
    m_Out.Log(e_Summary)
        << "Testing " << total << " alias(es)." << endl;
    CAliasTest ali_test(m_Out, 0);

    ITERATE(set<string>, iter, ali_list) {
        m_Out.Log(e_Details) << " " << *iter << endl;
        int num_faults = ali_test.DoTest(*iter, seen);
        if (num_faults)  tot_faults += num_faults;
        else passed++;
    }
    
    if (total == passed) {
        m_Out.Log(e_Summary)
            << " Result=SUCCESS. No errors reported for "
            << total << " alias(es)." << endl;
    } else {
        m_Out.Log(e_Summary)
            << " Result=FAILURE. "
            << (total-passed) << " errors reported in "
            << total << " alias(es)." << endl;
    }
    
    // Bottom line
    if (tot_faults) {
        m_Out.Log(e_Brief) <<  endl
           << "Total errors: " << tot_faults << endl;
    }
    
    return (tot_faults == 0);
}


/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)


int CBlastDbCheckApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();
    int status = 0;
    
    try {
        // Do run
        
        // Stream to result output
        // (NOTE: "x_lg" is just a workaround for bug in SUN WorkShop 5.1 compiler)
        ostream* lg = args["logfile"] ? &args["logfile"].AsOutputFile() : &cout;
        
        // Get verbosity as well
        const int verbosity = args["verbosity"].AsInteger();
        
        CBlastDbCheckLog output(*lg, verbosity);
        
        output.Log(e_Summary) << "Writing messages to ";
        
        if (args["logfile"]) {
            output.Log(e_Summary)
                << "file  (" << args["logfile"].AsString() << ")";
        } else {
            output.Log(e_Summary) << "<stdout>";
        }
        
        output.Log(e_Summary)
            << " at verbosity ("
            << s_VerbosityString(verbosity) << ")" << endl;
        
        // Get data source (db or directory) object.
        
        CRef<CTestData> data;
        
        string db(args["db"] ? args["db"].AsString() : "");
        string dir(args["dir"] ? args["dir"].AsString() : "");
        string dbtype(args["dbtype"].AsString());
        bool recurse = !! args["recursive"];
        //int threads = args["threads"].AsInteger();
        
        if ((db == "") == (dir == "")) {
            output.Log(e_Brief)
                << "error: Must specify exactly one of -dir or -db." << endl;
            
            return 1;
        } else if (db != "") {
            data.Reset(new CDbTest(output, db, dbtype));
        } else {
            data.Reset(new CDirTest(output, dir, dbtype, 1, recurse));
        }
        
        // Set up testing modifiers
        
        int flags = 0;

        if (!args["no_isam"])  flags |= e_IsamLookup;
        output.Log(e_Summary)
            << "ISAM testing is " << (args["no_isam"] ? "DIS" : "EN") << "ABLED." << endl;
        
        if (args["legacy"]) flags |= e_Legacy;
        output.Log(e_Summary)
            << "Legacy testing is " << (args["legacy"] ? "EN" : "DIS") << "ABLED." << endl;

        //bool fork1 = !! args["fork"];
        
        // Build test actions
        
        CRef<CTestActionList> tests(new CTestActionList());
        
        tests->Add(new CMetaDataTest(output, flags));
        
        bool default_set = false;

        if (args["full"]) {
            output.Log(e_Summary)
                << "Using `full' mode: every OID will be tested." << endl;
            tests->Add(new CStrideTest(output, 1, flags));
            default_set = true;
        }
            
        if (args["stride"]) {
            int stride = args["stride"].HasValue() ? args["stride"].AsInteger() : 10000;
            output.Log(e_Summary)
                << "Testing every " << stride << "-th OID." << endl;
            tests->Add(new CStrideTest(output, stride, flags));
            default_set = true;
        }
        
        if (args["random"]) {
            int random_sample = args["random"].HasValue() ? args["random"].AsInteger(): 200;
            output.Log(e_Summary)
                << "Testing " << random_sample << " randomly sampled OIDs." << endl;
            tests->Add(new CSampleTest(output, random_sample, flags));
            default_set = true;
        }
        
        if (args["ends"]) {
            int end_amt = args["ends"].HasValue() ? args["ends"].AsInteger() : 200;
            output.Log(e_Summary)
                << "Testing first " << end_amt
                << " and last " << end_amt << " OIDs." << endl;
            tests->Add(new CEndsTest(output, end_amt, flags));
            default_set = true;
        }
        
        if (!default_set) {
            int random_sample = 200;
            output.Log(e_Summary)
                << "By default, testing " << random_sample << " randomly sampled OIDs." << endl;
            tests->Add(new CSampleTest(output, random_sample, flags));
        }
        
        //if (fork1) {
        //    output.Log(e_Summary)
        //        << "Using fork() before each action for safety." << endl;
        //    
        //    tests->SetWrap(new CForkWrap);
        //}
        
        output.Log(e_Summary) << endl;
        
        bool okay = data->Test(*tests);
        
        status = okay ? 0 : 1;
    } CATCH_ALL(status)
    return status;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBlastDbCheckApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBlastDbCheckApplication().AppMain(argc, argv, 0, eDS_Default, 0);
#endif /* SKIP_DOXYGEN_PROCESSING */
}
