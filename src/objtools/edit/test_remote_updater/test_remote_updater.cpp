#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <objtools/edit/mla_updater.hpp>
#include <objtools/edit/eutils_updater.hpp>
#include <objtools/edit/remote_updater.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(edit);

class CTestRemoteUpdaterApplication : public CNcbiApplication
{
    void Init() override
    {
        unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
        arg_desc->SetUsageContext("", "Test CRemoteUpdater using medarch/eutils");
        arg_desc->AddKey("id", "pmid", "PubMed ID to fetch", CArgDescriptions::eIntId);
        arg_desc->AddOptionalKey("source", "source", "Source of data", CArgDescriptions::eString);
        arg_desc->SetConstraint("source", &(*new CArgAllow_Strings, "medarch", "eutils"));
        arg_desc->AddOptionalKey("o", "OutFile", "Output File", CArgDescriptions::eOutputFile);
        SetupArgDescriptions(arg_desc.release());
    }

    int Run() override
    {
        const CArgs& args = GetArgs();

        TEntrezId pmid = ZERO_ENTREZ_ID;
        if (args["id"]) {
            TIntId id = args["id"].AsIntId();
            pmid = ENTREZ_ID_FROM(TIntId, id);
        }
        if (pmid <= ZERO_ENTREZ_ID) {
            return 1;
        }

        bool bTypeMLA = false;
        if (args["source"]) {
            string s = args["source"].AsString();
            if (s == "medarch") {
                bTypeMLA = true;
            } else if (s == "eutils") {
                bTypeMLA = false;
            }
        }

        ostream* output = nullptr;
        if (args["o"]) {
            output = &args["o"].AsOutputFile();
        } else {
            output = &NcbiCout;
        }

        CRemoteUpdater upd(nullptr, bTypeMLA ? edit::EPubmedSource::eMLA : edit::EPubmedSource::eEUtils);
        CRef<CPub> pub(new CPub);
        pub->SetPmid().Set(pmid);
        CRef<CSeqdesc> desc(new CSeqdesc);
        desc->SetPub().SetPub().Set().push_back(pub);
        // *output << MSerial_AsnText << *desc;
        upd.UpdatePubReferences(*desc);
        *output << MSerial_AsnText << *desc;

        if (args["o"]) {
            args["o"].CloseFile();
        }

        return 0;
    }
};

int main(int argc, const char* argv[])
{
    return CTestRemoteUpdaterApplication().AppMain(argc, argv);
}
