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

#include <db/bdb/bdb_expt.hpp>
#include <db/bdb/bdb_types.hpp>
#include <db/bdb/bdb_file.hpp>
#include <db/bdb/bdb_cursor.hpp>
#include <db/bdb/bdb_filedump.hpp>
#include <db/bdb/bdb_blob.hpp>



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
    CBDB_ConfigStructureParser()
        : m_BlobStorage(false),
          m_KeyDuplicates(false) {};

    void ParseConfigFile(const string& fname);

    bool ParseStructureLine(const string&           line,
                            SBDB_FieldDescription*  descr);

    const TFileStructure& GetStructure() const { return m_FileStructure; }
    TFileStructure& SetStructure() { return m_FileStructure; }

    bool IsBlobStorage() const { return m_BlobStorage; }

    bool IsKeyDuplicates() const { return m_KeyDuplicates; }

private:
    TFileStructure   m_FileStructure;
    bool             m_BlobStorage;
    bool             m_KeyDuplicates;
};



/// Parse structure file line
/// Returns TRUE if parsing is successfull
bool
CBDB_ConfigStructureParser::ParseStructureLine(const string&           line,
                                               SBDB_FieldDescription*  descr)
{
    int cmp;
    list<string> tokens;

    NStr::Split(line, string(" \t"), tokens,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    descr->field_name = descr->type = "";
    descr->length = 0;
    descr->is_null = descr->is_primary_key = false;

    unsigned cnt = 0;
    ITERATE(list<string>, it, tokens) {
        switch (cnt) {
        case 0:     // field name
            cmp = NStr::CompareNocase(*it, string("duplicates"));

            if (cmp == 0) { // "DUPLICATES ALLOWED" ?
                ++it;
                if (it != tokens.end()) {
                    cmp = NStr::CompareNocase(*it, string("allowed"));
                    if (cmp == 0) {
                        m_KeyDuplicates = true;
                        return false;
                    }
                    cmp = NStr::CompareNocase(*it, string("yes"));
                    if (cmp == 0) {
                        m_KeyDuplicates = true;
                        return false;
                    }
                    cmp = NStr::CompareNocase(*it, string("true"));
                    if (cmp == 0) {
                        m_KeyDuplicates = true;
                        return false;
                    }
                }
            }

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
            cmp = NStr::CompareNocase(*it, string("PK"));
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

    if (!fi.is_open()) {
        string msg = "Cannot open config file: ";
        msg += fname;
        BDB_THROW(eInvalidValue, msg);
    }

    for ( ;fi.good(); ++line_idx) {
        getline(fi, line);

        // check if it is a comment
        const char* s = line.c_str();
        for(; isspace((unsigned char)(*s)); ++s) {}

        if (*s == 0 || *s == '#') // empty line or comment
            continue;

        SBDB_FieldDescription fdescr;
        bool parsed = ParseStructureLine(line, &fdescr);

        if (parsed) {
            CBDB_FieldFactory::EType ft = ffact.GetType(fdescr.type);
            if (ft == CBDB_FieldFactory::eUnknown) {
                BDB_THROW(eInvalidType, fdescr.type);
            }
            if (ft == CBDB_FieldFactory::eBlob) {  // BLOB file
                m_BlobStorage = true;
            }

            m_FileStructure.push_back(fdescr);
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

    void Dump(const CArgs& args,
              CBDB_ConfigStructureParser& parser,
              bool dump_lob_storage);
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
                            "BDB database structure configuration file.\n"
                            "(Use 'blob' or 'lob' to dump LOB storage).",
                            CArgDescriptions::eString);

    arg_desc->AddOptionalKey("k",
                             "dbkey",
                             "File key value.(Dumps record for only this key)",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("cs",
                             "column_separator",
                             "Column separator string (default:TAB)",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("q",
                             "query",
                             "BDB query string",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("bfile",
                             "blob_file",
                             "Dump BLOB to file (Use with -k)",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("ofile",
                             "output_file",
                             "All records dumped into another database (append mode)",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("nl", "Do NOT print field labels(names)");
    arg_desc->AddFlag("bt", "Display BLOB as text (default: HEX)");
    arg_desc->AddFlag("bf", "Display full BLOB");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

}



void CBDB_FileDumperApp::Dump(const CArgs&                args,
                              CBDB_ConfigStructureParser& parser,
                              bool                        dump_lob_storage)
{
    const string& db_name = args["dbname"].AsString();

    CBDB_ConfigStructureParser::TFileStructure& fs =
            parser.SetStructure();

    CBDB_FieldFactory ffact;

    auto_ptr<CBDB_File>      db_file;
    auto_ptr<CBDB_BLobFile>  db_blob_file;
    auto_ptr<CBDB_File>      db_out_file;

    CBDB_File* dump_file;

    if (dump_lob_storage) {
        // Create and configure the simple LOB dumper
        // (We dont need config for that)
        db_blob_file.reset(new CBDB_BLobFile);
        dump_file = db_blob_file.get();
        dump_file->SetFieldOwnership(true);

        CBDB_Field*  field = ffact.Create(CBDB_FieldFactory::eInt4);
        dump_file->BindKey("id", field);

    } else {

        // Create config file based dumper

        dump_lob_storage = parser.IsBlobStorage();

        if (dump_lob_storage) {
            db_blob_file.reset(new CBDB_BLobFile);
            dump_file = db_blob_file.get();
        } else {
            db_file.reset(new CBDB_File(parser.IsKeyDuplicates() ?
                                        CBDB_File::eDuplicatesEnable :
                                        CBDB_File::eDuplicatesDisable));
            dump_file = db_file.get();
        }
        dump_file->SetFieldOwnership(true);

        // ----------------------------------------
        //
        // Create description based file structure

        bool struct_created = false;

        NON_CONST_ITERATE(CBDB_ConfigStructureParser::TFileStructure, it, fs) {
            struct_created = true;
            CBDB_FieldFactory::EType ft = ffact.GetType(it->type);
            if (ft == CBDB_FieldFactory::eString ||
                ft == CBDB_FieldFactory::eLString) {
                if (it->length == 0) {
                    it->length = 4096;
                }
            }
            if (it->is_primary_key) {
                CBDB_Field*  field = ffact.Create(ft);
                dump_file->BindKey(it->field_name.c_str(),
                                   field,
                                   it->length);
            } else {
                 if (parser.IsBlobStorage()) {
                    // All DATA fields are ignored (we have only one BLOB per record)
                    // if operator requests anything but one BLOB it is treated as an
                    // error (TODO: add a warning here)
                 } else {
                     CBDB_Field* field = ffact.Create(ft);
                     dump_file->BindData(it->field_name.c_str(),
                                         field,
                                         it->length,
                      it->is_null ? eNullable : eNotNullable);
                 }
            }
        }

        if (!struct_created) {
            NcbiCerr << "Incorrect structure file (no fields)" << NcbiEndl;
            exit(1);
        }

    }


    dump_file->Open(db_name.c_str(), CBDB_File::eReadOnly);

    // ------------------------------------------
    //
    // Dump the content

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

    if (args["q"]) {  // query
        fdump.SetQuery(args["q"].AsString());
    }

    if (args["bfile"]) {
        fdump.SetBlobDumpFile(args["bfile"].AsString());
    }

    if (args["ofile"]) {
        const string& out_fname = args["ofile"].AsString();
        db_out_file.reset(new CBDB_File(parser.IsKeyDuplicates() ?
                                        CBDB_File::eDuplicatesEnable :
                                        CBDB_File::eDuplicatesDisable));
        db_out_file->SetFieldOwnership(true);
        db_out_file->DuplicateStructure(*dump_file);

        db_out_file->Open(out_fname.c_str(), CBDB_File::eReadWriteCreate);

        fdump.SetOutFile(db_out_file.get());
    }

    if (args["k"]) {
        const string& key_str = args["k"].AsString();

        CBDB_FileCursor cur(*dump_file);
        cur.SetCondition(CBDB_FileCursor::eEQ);

        const CBDB_BufferManager* key_buf = dump_file->GetKeyBuffer();
        unsigned field_count = key_buf->FieldCount();

        if (field_count > 1) {
            list<string> keys;
            NStr::Split(key_str, string("|"), keys, NStr::fSplit_NoMergeDelims);

            unsigned cnt = 0;
            ITERATE(list<string>, it, keys) {
                cur.From << *it;
                if (++cnt == field_count)
                    break;
            }
        } else {
            cur.From << key_str;
        }

        fdump.Dump(NcbiCout, cur);

    } else {

        fdump.Dump(NcbiCout, *dump_file);
    }

    NcbiCout << "Dumped records: " << fdump.GetRecordsDumped() << NcbiEndl;
}


int CBDB_FileDumperApp::Run(void)
{
    try
    {
        const CArgs &  args = GetArgs();
        const string& conf_name = args["confname"].AsString();

        CBDB_ConfigStructureParser parser;
        bool blob_store = false;

        if (NStr::CompareNocase(conf_name, "lob") == 0 ||
            NStr::CompareNocase(conf_name, "blob") == 0) {
            blob_store = true;
        } else {
            parser.ParseConfigFile(conf_name);
        }

        Dump(args, parser, blob_store);
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
    return CBDB_FileDumperApp().AppMain(argc, argv);
}
