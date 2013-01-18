#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <algorithm>

#include <corelib/ncbistre.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <algo/align/util/blast_tabular.hpp>
#include <algo/align/util/hit_comparator.hpp>
#include <algo/align/util/compartment_finder.hpp>

#include <algo/align/prosplign/compartments.hpp>
//#include <objects/seqalign/seqalign__.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(prosplign);
//USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CCompartApplication::


class CCompartApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
};


/////////////////////////////////////////////////////////////////////////////

void CCompartApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "makes compartments from protein BLAST m8");

    CCompartOptions::SetupArgDescriptions(arg_desc.get());

    arg_desc->AddDefaultKey("ifmt", "InputFormat",
                            "Format for input",
                            CArgDescriptions::eString,
                            "");
    arg_desc->SetConstraint("ifmt",
                            &(*new CArgAllow_Strings,
                              "tabular", "seq-align", "seq-align-set", "seq-annot"));

    arg_desc->AddFlag("hits",
                      "print hits");
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



void PrintCompartments(int& last_id, const string& query, const string& subj, const TCompartmentStructs& compartments, ostream& ostr)
{
    ITERATE(TCompartmentStructs, i, compartments) {
        ostr << ++last_id << '\t' << subj << '\t' << query << '\t';
        ostr << i->from+1 << '\t' << i->to+1 << '\t' << (i->strand?'+':'-') << '\t';
        ostr << i->covered_aa << '\t' << i->score << '\n';
    }
}

void DoCompartments(const CSplign::THitRefs& one_query_subj_pair_hitrefs,
                    const CCompartOptions& compart_options,
                    bool hits, int& last_id, const string& query, const string& subj)
{
    auto_ptr<CCompartmentAccessor<THit> > comps_ptr =
        CreateCompartmentAccessor(one_query_subj_pair_hitrefs, compart_options);
    if (comps_ptr.get() == NULL)
        return;

    if (!hits) {
        TCompartments compart_list = FormatAsAsn(comps_ptr.get(), compart_options);
        TCompartmentStructs compartments = MakeCompartments(compart_list, compart_options);
        PrintCompartments(last_id, query, subj, compartments, cout);
    } else {
        CCompartmentAccessor<THit>& comps = *comps_ptr;

        THitRefs comphits;
        if(comps.GetFirst(comphits)) {
            do {
                ITERATE(THitRefs, h, comphits) {
                    cout << **h << endl;
                }
            } while (comps.GetNext(comphits));
        }
    }
}

string GetSeqIdString(const CSeq_id& id)
{
    return id.IsGi() ? id.AsFastaString() : id.GetSeqIdString(true);
}

int PdbBadRank(const CRef<CSeq_id>& id)
{
    return (id && id->IsPdb()) ? kMax_Int-1 : CSeq_id::FastaAARank(id);
}

int CCompartApplication::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();

    CCompartOptions compart_options(args);
    string ifmt = args["ifmt"].AsString();
    bool hits = args["hits"];
    
    int last_id = 0;

    string buf;
    string old_q, old_s;
    CSplign::THitRefs one_query_subj_pair_hitrefs;

    while(getline(cin, buf)) {
        CSplign::THitRef hit (new CSplign::THit (buf.c_str(), PdbBadRank));
        if (hit->GetQueryStrand() == false) {
            NCBI_THROW(CException, eUnknown, "Reverse strand on protein sequence: "+buf);
        }
        string new_q = GetSeqIdString(*hit->GetQueryId());
        string new_s = GetSeqIdString(*hit->GetSubjId());
        if (new_q != old_q || new_s != old_s) {
            DoCompartments(one_query_subj_pair_hitrefs, compart_options, hits, last_id, old_q, old_s);    
            
            one_query_subj_pair_hitrefs.clear();
            old_q = new_q;
            old_s = new_s;
        }
        one_query_subj_pair_hitrefs.push_back(hit);
    }

    DoCompartments(one_query_subj_pair_hitrefs, compart_options, hits, last_id, old_q, old_s);    
    
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    SetDiagFilter(eDiagFilter_All, "!AddRule()");
    return CCompartApplication().AppMain(argc, argv);
}
