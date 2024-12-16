/* $Id$
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
 * Author: Vitaly Stakhovsky, NCBI
 *
 * File Description:
 *  PubMed Fetch test application: using EUtils
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/seq/Pubdesc.hpp>

#include <objtools/edit/eutils_updater.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(edit);

class CPubmedFetchApplication : public CNcbiApplication
{
public:
    CPubmedFetchApplication()
    {
        SetVersion(CVersionInfo(1, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY));
    }

    void Init() override
    {
        unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
        arg_desc->SetUsageContext("", "Retrieve publications data from PubMed and display in ASN");
        arg_desc->AddOptionalKey("id", "pmid", "PubMed ID to fetch", CArgDescriptions::eIntId);
        arg_desc->AddOptionalKey("ids", "pmids", "PubMed IDs", CArgDescriptions::eString);
        arg_desc->AddOptionalKey("idfile", "pmids", "File containing PubMed IDs", CArgDescriptions::eInputFile);
        arg_desc->AddOptionalKey("pubmed", "source", "Always eutils", CArgDescriptions::eString, CArgDescriptions::fHidden);
        arg_desc->AddOptionalKey("url", "url", "eutils base URL (http://eutils.ncbi.nlm.nih.gov/entrez/eutils/ by default)", CArgDescriptions::eString);
        arg_desc->AddOptionalKey("o", "OutFile", "Output File", CArgDescriptions::eOutputFile);
        arg_desc->AddFlag("normalize", "Normalize output deterministically for tests", CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);
        arg_desc->AddFlag("stats", "Only print execution statistics");
        SetupArgDescriptions(arg_desc.release());
    }

    int Run() override
    {
        const CArgs& args = GetArgs();

        vector<TEntrezId> pmids;

        if (args["id"]) {
            TIntId id = args["id"].AsIntId();
            if (id > 0) {
                pmids.push_back(ENTREZ_ID_FROM(TIntId, id));
            } else {
                cerr << "Warning: Invalid ID: " << id << endl;
            }
        } else if (args["ids"]) {
            vector<string> ids;
            NStr::Split(args["ids"].AsString(), ",", ids);
            for (string id_str : ids) {
                TIntId id = 0;
                if (NStr::StringToNumeric(id_str, &id, NStr::fConvErr_NoThrow) && id > 0) {
                    pmids.push_back(ENTREZ_ID_FROM(TIntId, id));
                } else {
                    cerr << "Warning: Invalid ID: " << id_str << endl;
                }
            }
        } else if (args["idfile"]) {
            CNcbiIstream& is = args["idfile"].AsInputFile();
            string        id_str;
            while (NcbiGetlineEOL(is, id_str)) {
                NStr::TruncateSpacesInPlace(id_str);
                if (id_str.empty() || id_str.front() == '#') {
                    continue;
                }
                TIntId id = 0;
                if (NStr::StringToNumeric(id_str, &id, NStr::fConvErr_NoThrow) && id > 0) {
                    pmids.push_back(ENTREZ_ID_FROM(TIntId, id));
                } else {
                    cerr << "Warning: Invalid ID: " << id_str << endl;
                }
            }
        } else {
            cerr << "Error: One of -id, -ids, or -idfile must be specified." << endl;
            return 1;
        }

        if (pmids.empty()) {
            cerr << "Warning: No PMIDs specified" << endl;
            return 0;
        }

        auto normalize = args["normalize"].AsBoolean() ? CEUtilsUpdater::ENormalize::On : CEUtilsUpdater::ENormalize::Off;

        ostream* output = nullptr;
        if (args["o"]) {
            output = &args["o"].AsOutputFile();
        } else {
            output = &NcbiCout;
        }

        unique_ptr<CEUtilsUpdater> upd(new CEUtilsUpdater(normalize));

        if (args["url"]) {
            string url = args["url"].AsString();
            upd->SetBaseURL(url);
        }

        bool       bstats = args["stats"];
        unsigned   nruns  = 0;
        unsigned   ngood  = 0;
        CStopWatch sw;

        sw.Start();
        for (TEntrezId pmid : pmids) {
            ++nruns;
            try {
                EPubmedError err;
                CRef<CPub>   pub(upd->GetPubmedEntry(pmid, &err));
                if (pub) {
                    if (! bstats) {
                        *output << MSerial_AsnText << *pub;
                    }
                    ++ngood;
                } else {
                    cerr << "Error: " << err << endl;
                }
            } catch (const CException& e) {
                cerr << "CException: " << e.what() << endl;
            }
        }
        sw.Stop();

        if (bstats) {
            *output << " * Number of runs: " << nruns << endl;
            *output << " *     successful: " << ngood << endl;
            *output << " * Elapsed time:   " << sw << endl;
        }

        if (args["o"]) {
            args["o"].CloseFile();
        }

        return 0;
    }
};

int main(int argc, const char* argv[])
{
    return CPubmedFetchApplication().AppMain(argc, argv);
}
