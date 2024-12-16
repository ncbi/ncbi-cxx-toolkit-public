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
 *  PubMed Citation Match test application using EUtils
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <objects/pub/Pub.hpp>

#include <objtools/edit/eutils_updater.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(edit);

namespace
{
    NCBI_UNUSED
    void dump_cm_list(CNcbiOstream& os, const vector<SCitMatch>& cm_list)
    {
        for (const auto& cm : cm_list) {
            if (! cm.Journal.empty()) {
                os << "Journal=" << cm.Journal << endl;
            }
            if (! cm.Volume.empty()) {
                os << "Volume=" << cm.Volume << endl;
            }
            if (! cm.Page.empty()) {
                os << "Page=" << cm.Page << endl;
            }
            if (! cm.Year.empty()) {
                os << "Year=" << cm.Year << endl;
            }
            if (! cm.Author.empty()) {
                os << "Author=" << cm.Author << endl;
            }
            if (! cm.Issue.empty()) {
                os << "Issue=" << cm.Issue << endl;
            }
            if (! cm.Title.empty()) {
                os << "Title=" << cm.Title << endl;
            }
            if (cm.InPress) {
                os << "InPress=" << cm.InPress << endl;
            }
            os << endl;
        }
    }

    void read_cm_list(CNcbiIstream& is, vector<SCitMatch>& cm_list)
    {
        string                line;
        unique_ptr<SCitMatch> cm;

        while (NcbiGetlineEOL(is, line)) {
            NStr::Sanitize(line);
            if (line.empty()) {
                if (cm) {
                    cm_list.push_back(*cm);
                    cm.reset();
                } else {
                    break;
                }
            } else {
                string key, val;
                NStr::SplitInTwo(line, "=", key, val);

                if (! val.empty()) {
                    if (NStr::EqualNocase(key, "Journal")) {
                        if (! cm)
                            cm.reset(new SCitMatch);
                        cm->Journal = val;
                    } else if (NStr::EqualNocase(key, "Volume")) {
                        if (! cm)
                            cm.reset(new SCitMatch);
                        cm->Volume = val;
                    } else if (NStr::EqualNocase(key, "Page")) {
                        if (! cm)
                            cm.reset(new SCitMatch);
                        cm->Page = val;
                    } else if (NStr::EqualNocase(key, "Year")) {
                        if (! cm)
                            cm.reset(new SCitMatch);
                        cm->Year = val;
                    } else if (NStr::EqualNocase(key, "Author")) {
                        if (! cm)
                            cm.reset(new SCitMatch);
                        cm->Author = val;
                    } else if (NStr::EqualNocase(key, "Issue")) {
                        if (! cm)
                            cm.reset(new SCitMatch);
                        cm->Issue = val;
                    } else if (NStr::EqualNocase(key, "Title")) {
                        if (! cm)
                            cm.reset(new SCitMatch);
                        cm->Title = val;
                    } else if (NStr::EqualNocase(key, "InPress")) {
                        if (! cm)
                            cm.reset(new SCitMatch);
                        cm->InPress = NStr::EqualNocase(key, "true");
                    }
                }
            }
        }
        _ASSERT(! cm);
    }
}

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
        arg_desc->SetUsageContext("", "Match publications from PubMed and print Ids");
        arg_desc->AddKey("i", "InFile", "Data to match", CArgDescriptions::eInputFile);
        arg_desc->AddOptionalKey("pubmed", "source", "Always eutils", CArgDescriptions::eString, CArgDescriptions::fHidden);
        arg_desc->AddOptionalKey("url", "url", "eutils base URL (http://eutils.ncbi.nlm.nih.gov/entrez/eutils/ by default)", CArgDescriptions::eString);
        arg_desc->AddOptionalKey("o", "OutFile", "Output File", CArgDescriptions::eOutputFile);
        arg_desc->AddFlag("stats", "Also print execution statistics");
        SetupArgDescriptions(arg_desc.release());
    }

    int Run() override
    {
        const CArgs& args = GetArgs();

        vector<SCitMatch> cm_list;
        CNcbiIstream&     is = args["i"].AsInputFile();
        read_cm_list(is, cm_list);

        if (cm_list.empty()) {
            cerr << "Warning: No input data" << endl;
            return 0;
        }

        ostream* output = nullptr;
        if (args["o"]) {
            output = &args["o"].AsOutputFile();
        } else {
            output = &NcbiCout;
        }

        unique_ptr<CEUtilsUpdater> upd(new CEUtilsUpdater());

        if (args["url"]) {
            string url = args["url"].AsString();
            upd->SetBaseURL(url);
        }

        bool           bstats = args["stats"];
        unsigned       nruns  = 0;
        unsigned       ngood  = 0;
        vector<string> results;
        results.reserve(cm_list.size());
        CStopWatch sw;

        sw.Start();
        for (const SCitMatch& cm : cm_list) {
            ++nruns;
            try {
                EPubmedError err;
                TEntrezId    pmid = upd->CitMatch(cm, &err);
                if (pmid != ZERO_ENTREZ_ID) {
                    results.push_back(to_string(ENTREZ_ID_TO(TIntId, pmid)));
                    ++ngood;
                } else {
                    ostringstream oss;
                    oss << "Error: " << err;
                    results.push_back(oss.str());
                }
            } catch (const CException& e) {
                results.push_back(e.what());
            }
        }
        sw.Stop();

        for (const auto& r : results) {
            *output << r << endl;
        }

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
