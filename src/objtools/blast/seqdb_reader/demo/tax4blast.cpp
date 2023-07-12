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
 * Author:  Christiam Camacho
 *
 */

/// @file tax4blast.cpp
/// Task demonstration application for SeqDB.

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objtools/blast/seqdb_reader/tax4blastsqlite.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
#endif


/// The application class
class CTax4BlastDemo : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CTax4BlastDemo() {}
    ~CTax4BlastDemo() {}
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();
};

void CTax4BlastDemo::Init()
{
    HideStdArgs(fHideConffile | fHideLogfile | fHideVersion | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
           "Taxonomy for BLAST demo program");

    arg_desc->SetCurrentGroup("Input options");
    arg_desc->AddOptionalKey("taxids", "taxonomy_ids",
    						"Comma-delimited taxonomy identifiers", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("taxidlist", "input_file", 
            "Input file for taxonomy identifiers", CArgDescriptions::eInputFile);
    arg_desc->SetDependency("taxidlist", CArgDescriptions::eExcludes, "taxids");

    arg_desc->SetCurrentGroup("Database options");
    arg_desc->AddOptionalKey("db", "path_to_sqlite_db",
    						"Path to SQLite database", CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
}

static void s_FormatResults(const int taxid, const vector<int>& desc)
{
    if (desc.empty())
        return;

    bool first = true;
    for (auto i: desc) 
        if (first) {
            cout << taxid << ": " << i;
            first = false;
        } else {
            cout << "," << i;
        }
    cout << endl;
}

int CTax4BlastDemo::Run(void)
{
    int status = 1;
    const CArgs& args = GetArgs();

    try {

        auto dbpath = args["db"].HasValue() ? args["db"].AsString() : kEmptyStr;
        unique_ptr<ITaxonomy4Blast> tb(new CTaxonomy4BlastSQLite(dbpath));
        vector<int> desc;

        if (args["taxids"].HasValue()) {
            string input = args["taxids"].AsString();
            vector<string> ids;
            NStr::Split(input, ",", ids);
            for (auto id: ids) {
                auto taxid = NStr::StringToInt(id);
                tb->GetLeafNodeTaxids(taxid, desc);
                s_FormatResults(taxid, desc);
            }
        } else {
            CNcbiIstream& input = args["taxidlist"].AsInputFile();
            while (input) {
                string line;
                NcbiGetlineEOL(input, line);
                if ( !line.empty() ) {
                    auto taxid = NStr::StringToInt(line);
                    tb->GetLeafNodeTaxids(taxid, desc);
                    s_FormatResults(taxid, desc);
                }
            }
        }

        status = 0;
    } catch (const CException& e) {
        ERR_POST(Error << e.GetMsg());
        status = 1;
    } catch (const exception& e) {
        ERR_POST(Error << e.what());
    } catch (...) {
        cerr << "Unknown exception!" << endl;
    }

    return status;
}

#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CTax4BlastDemo().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
