#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/seq/Pubdesc.hpp>

#include <objtools/edit/mla_updater.hpp>
#include <objtools/edit/eutils_updater.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(edit);

class CTestPubmedApplication : public CNcbiApplication
{
    void Init() override
    {
        unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
        arg_desc->SetUsageContext("", "Retrieve publications data from medarch/eutils and display in ASN");
        arg_desc->AddOptionalKey("id", "pmid", "PubMed ID to fetch", CArgDescriptions::eIntId);
        arg_desc->AddOptionalKey("source", "source", "Source of data", CArgDescriptions::eString);
        arg_desc->SetConstraint("source", &(*new CArgAllow_Strings, "medarch", "eutils"));
//      arg_desc->AddOptionalKey("url", "url", "eutils URL (eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi by default)", CArgDescriptions::eString);
        SetupArgDescriptions(arg_desc.release());
    }

    int Run() override
    {
        const CArgs& args = GetArgs();
        bool bTypeMLA = false;
        if (args["source"]) {
            string s = args["source"].AsString();
            if (s == "medarch") {
                bTypeMLA = true;
            } else if (s == "eutils") {
                bTypeMLA = false;
            }
        }
/*
        string url;
        if (args["url"]) {
            url = args["url"].AsString();
        }
*/
        if (args["id"]) {
            TIntId id = args["id"].AsIntId();
            TEntrezId pmid = ENTREZ_ID_FROM(TIntId, id);
            unique_ptr<IPubmedUpdater> upd;
            if (bTypeMLA) {
                upd.reset(new CMLAUpdater());
            } else {
                upd.reset(new CEUtilsUpdater(/*url*/));
            }
            CRef<CPub> pub(upd->GetPub(pmid));
            cout << MSerial_AsnText << *pub;
        }

        return 0;
    }
};

int main(int argc, const char* argv[])
{
    return CTestPubmedApplication().AppMain(argc, argv);
}
