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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: BDB file dumper utility
 *
 */
#include <ncbi_pch.hpp>
#include <stdio.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistr.hpp>

#include <bdb/bdb_expt.hpp>
#include <bdb/bdb_types.hpp>
#include <bdb/bdb_file.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_filedump.hpp>
#include <bdb/bdb_blob.hpp>


USING_NCBI_SCOPE;



struct SBDB_FieldDescription
{
    string        field_name;
    string        type;
    unsigned int  length;
    bool          is_null;
    bool          is_primary_key;
};

class CBDB_ConfigStructureParser
{
public:
    typedef vector<SBDB_FieldDescription> TFileStructure;
public:
    CBDB_ConfigStructureParser() : m_BlobStorage(false) {};

    void ParseConfigFile(const string& fname);

    bool ParseStructureLine(const string&           line, 
                            SBDB_FieldDescription*  descr);
    
    const TFileStructure& GetStructure() const { return m_FileStructure; }
    TFileStructure& SetStructure() { return m_FileStructure; }
    
    bool IsBlobStorage() const { return m_BlobStorage; }
    
private:
    TFileStructure   m_FileStructure;
    bool             m_BlobStorage;
};



/// Parse structure file line
/// Returns TRUE if parsing is successfull
bool 
CBDB_ConfigStructureParser::ParseStructureLine(const string&           line, 
                                               SBDB_FieldDescription*  descr)
{
    list<string> tokens;
    
    NStr::Split(line, string(" \t"), tokens, NStr::eMergeDelims);
    
    descr->field_name = descr->type = "";
    descr->length = 0;
    descr->is_null = descr->is_primary_key = false;
    
    unsigned cnt = 0;
    ITERATE(list<string>, it, tokens) {
        switch (cnt) {
        case 0:     // field name
            descr->field_name = *it;
            break;
        case 1:     // field type
            {
            string s = *it;
            
            string::size_type offs1 = s.find_first_of("(");            
            if (offs1 != string::npos) {
                string::size_type offs2 = s.find_first_of(")");
                if (offs2 == string::npos || offs2 < offs1) { 
                    // TODO: parsing error here
                    return false;
                }
                const char *ls = s.c_str() + offs1;
                descr->length = ::atoi(ls);
                s.resize(offs1);
            }
            
            descr->type = s;
            }
            break;
        default: // PK or NULL or NOT NULL
            {
            int cmp = NStr::CompareNocase(*it, string("PK"));
            if (cmp == 0) {
                descr->is_primary_key = true;
            }
            else {
                cmp = NStr::CompareNocase(*it, string("key"));
                if (cmp == 0) {
                    descr->is_primary_key = true;
                }
                else {
                    cmp = NStr::CompareNocase(*it, string("NULL"));
                    if (cmp == 0) {
                        descr->is_null = true;
                    }
                }
            }
            }
            break;
        } // switch
        ++cnt;
    } // ITERATE
    
    return true;
}

void CBDB_ConfigStructureParser::ParseConfigFile(const string& fname)
{
    string line;    
    unsigned line_idx = 0;
    
    m_FileStructure.resize(0);
    
    CBDB_FieldFactory ffact;
    
    CNcbiIfstream fi(fname.c_str());
    for ( ;fi.good(); ++line_idx) {
        getline(fi, line);

        // check if it is a comment
        const char* s = line.c_str();
        for( ;isspace(*s); ++s) {}
        
        if (*s == 0 || *s == '#') // empty line or comment
            continue;
                
        SBDB_FieldDescription fdescr;
        bool parsed = ParseStructureLine(line, &fdescr);
        
        CBDB_FieldFactory::EType ft = ffact.GetType(fdescr.type);
        if (ft == CBDB_FieldFactory::eUnknown) {
            BDB_THROW(eInvalidType, fdescr.type);            
        }
        if (ft == CBDB_FieldFactory::eBlob) {  // BLOB file
            m_BlobStorage = true;
        }
                
        if (parsed) {
            m_FileStructure.push_back(fdescr);
        } else {
            NcbiCerr << "Error in line: " << line_idx << endl;
            return;
        }
        
    } // for
}


///////////////////////////////////////////////////////////////////////


/// BDB file dumper application
///
/// @internal
///
class CBDB_FileDumperApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
protected:
    /// Dump regular BDB file
    void DumpFile(const CArgs& args, 
                  CBDB_ConfigStructureParser::TFileStructure& fs);

    void DumpBlob(const CArgs& args, 
                  CBDB_ConfigStructureParser::TFileStructure& fs);
};

void CBDB_FileDumperApp::Init(void)
{

    // Setup command line arguments and parameters
        
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "BDB dumper");
    
    arg_desc->AddPositional("dbname", 
                            "BDB database name", CArgDescriptions::eString);

    arg_desc->AddPositional("confname",
                            "BDB database structure configuration file", 
                             CArgDescriptions::eString);
    
    arg_desc->AddOptionalKey("k",
                             "dbkey",
                             "File key value.(Dumps record for only this key)",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("cs",
                             "column_separator",
                             "Column separator string (default:TAB)",
                             CArgDescriptions::eString);
    
    arg_desc->AddFlag("nl", "Do NOT print field labels(names)");
    arg_desc->AddFlag("bt", "Display BLOB as text (default: HEX)");
    arg_desc->AddFlag("bf", "Display full BLOB");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
    
}

void 
CBDB_FileDumperApp::DumpFile(
        const CArgs& args, 
        CBDB_ConfigStructureParser::TFileStructure& fs)
{
    const string& db_name = args["dbname"].AsString();
    
    // ----------------------------------------
    //
    // Create description based file structure 


    CBDB_FieldFactory ffact;
    CBDB_File db_file;
    db_file.SetFieldOwnership(true);

    NON_CONST_ITERATE(CBDB_ConfigStructureParser::TFileStructure, it, fs) {
        CBDB_FieldFactory::EType ft = ffact.GetType(it->type);
        if (ft == CBDB_FieldFactory::eString ||
            ft == CBDB_FieldFactory::eLString) {
            if (it->length == 0) {
                it->length = 4096;
            }
        }
        CBDB_Field*    field = ffact.Create(ft);
        if (it->is_primary_key) {
            db_file.BindKey(it->field_name.c_str(), 
                               field, 
                            it->length);
        } else {
            db_file.BindData(it->field_name.c_str(), 
                               field, 
                            it->length,
              it->is_null ? eNullable : eNotNullable);
        }
    }
    
    // ------------------------------------------
    //
    // Dump the content

    db_file.Open(db_name.c_str(), CBDB_File::eReadOnly);

    CBDB_FileDumper fdump;
    if (args["nl"]) {
        fdump.SetColumnNames(CBDB_FileDumper::eDropNames);
    }
    fdump.SetValueFormatting(CBDB_FileDumper::eQuoteStrings);

    if (args["cs"]) {
        fdump.SetColumnSeparator(args["cs"].AsString());
    }

    if (args["k"]) {
        const string& key_str = args["k"].AsString();

        CBDB_FileCursor cur(db_file);
        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << key_str;

        fdump.Dump(NcbiCout, cur);

        return;
    }

    fdump.Dump(NcbiCout, db_file);
    
}

void CBDB_FileDumperApp::DumpBlob(
              const CArgs& args, 
              CBDB_ConfigStructureParser::TFileStructure& fs)
{
    const string& db_name = args["dbname"].AsString();
    
    // ----------------------------------------
    //
    // Create description based file structure 

    
    CBDB_FieldFactory ffact;
    CBDB_BLobFile  db_blob_file;
    db_blob_file.SetFieldOwnership(true);

    NON_CONST_ITERATE(CBDB_ConfigStructureParser::TFileStructure, it, fs) {
        CBDB_FieldFactory::EType ft = ffact.GetType(it->type);
        if (ft == CBDB_FieldFactory::eString ||
            ft == CBDB_FieldFactory::eLString) {
            if (it->length == 0) {
                it->length = 4096;
            }
        }
        if (it->is_primary_key) {
            CBDB_Field*  field = ffact.Create(ft);
            db_blob_file.BindKey(it->field_name.c_str(), 
                                 field, 
                                 it->length);
        } else {
            // All DATA fields are ignored (we have only one BLOB per record)
            // if operator requests anything but one BLOB it is treated as an 
            // error (TODO: add a warning here)
        }
    }
    
    // ------------------------------------------
    //
    // Dump the content

    db_blob_file.Open(db_name.c_str(), CBDB_File::eReadOnly);

    CBDB_FileDumper fdump;
    if (args["nl"]) {
        fdump.SetColumnNames(CBDB_FileDumper::eDropNames);
    }
    fdump.SetValueFormatting(CBDB_FileDumper::eQuoteStrings);

    if (args["cs"]) {
        fdump.SetColumnSeparator(args["cs"].AsString());
    }
    
    CBDB_FileDumper::TBlobFormat bformat = 0;    
    if (args["bt"]) {
        bformat |= CBDB_FileDumper::eBlobAsTxt;
    } else {
        bformat |= CBDB_FileDumper::eBlobAsHex;
    }
    if (args["bf"]) {
        bformat |= CBDB_FileDumper::eBlobAll;
    } else {
        bformat |= CBDB_FileDumper::eBlobSummary;
    }
    
    fdump.SetBlobFormat(bformat);

    if (args["k"]) {
        const string& key_str = args["k"].AsString();

        CBDB_FileCursor cur(db_blob_file);
        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << key_str;

        fdump.Dump(NcbiCout, cur);

        return;
    }

    fdump.Dump(NcbiCout, db_blob_file);
}

int CBDB_FileDumperApp::Run(void)
{
    try
    {
        CArgs args = GetArgs();
        const string& conf_name = args["confname"].AsString();
        CBDB_ConfigStructureParser parser;
        parser.ParseConfigFile(conf_name);
        
        CBDB_ConfigStructureParser::TFileStructure& fs = 
                parser.SetStructure();
        
        if (parser.IsBlobStorage()) {
            DumpBlob(args, fs);
        } else {
            DumpFile(args, fs);
        }
    }
    catch (CBDB_ErrnoException& ex)
    {
        NcbiCerr << "Error: DBD errno exception:" << ex.what();
        return 1;
    }
    catch (CBDB_LibException& ex)
    {
        NcbiCerr << "Error: DBD library exception:" << ex.what();
        return 1;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CBDB_FileDumperApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/06/23 14:11:09  kuznets
 * Added more options for BLOB dumping
 *
 * Revision 1.3  2004/06/22 18:27:31  kuznets
 * Implemented BLOB dumping(summary)
 *
 * Revision 1.2  2004/06/21 15:11:38  kuznets
 * Work in progress
 *
 * Revision 1.1  2004/06/17 16:28:24  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
