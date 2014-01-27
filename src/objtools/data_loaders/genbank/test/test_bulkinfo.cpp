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
* Author:
*           Andrei Gourianov, Michael Kimelman
*
* File Description:
*           Basic test of GenBank data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
//#include <internal/asn_cache/lib/asn_cache_loader.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;
using namespace sequence;


const int g_gi_from = 156000;
const int g_gi_to   = 157000;
const int g_acc_from = 1;


/////////////////////////////////////////////////////////////////////////////
//
//  CTestApplication::
//


class CTestApplication : public CNcbiApplication
{
public:
    void TestApp_Args(CArgDescriptions& args);
    bool TestApp_Init(const CArgs& args);

    virtual void Init(void);
    virtual int Run(void);

    template<class Data>
    void Display(size_t i,
                 const char* name,
                 const Data& gis,
                 const Data& gis2,
                 const Data& gisv) const;

    typedef vector<CSeq_id_Handle> TIds;
    typedef CScope::TGIs TGis;
    typedef vector<CSeq_id_Handle> TAccs;
    typedef vector<string> TLabels;
    typedef vector<int> TTaxIds;
    typedef vector<TSeqPos> TLengths;
    typedef vector<CSeq_inst::TMol> TTypes;

    vector<CSeq_id_Handle> m_Ids;
    enum EBulkType {
        eBulk_gi,
        eBulk_acc,
        eBulk_label,
        eBulk_taxid,
        eBulk_length,
        eBulk_type
    };
    EBulkType m_Type;
    bool m_Verbose;
    bool m_Single;
    bool m_Verify;
    CScope::EForceLoad m_ForceLoad;
    CScope::EForceLabelLoad m_ForceLabelLoad;
};


void CTestApplication::Init(void)
{
    //CONNECT_Init(&GetConfig());
    //CORE_SetLOCK(MT_LOCK_cxx2c());
    //CORE_SetLOG(LOG_cxx2c());

    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    // Let test application add its own arguments
    TestApp_Args(*arg_desc);
    SetupArgDescriptions(arg_desc.release());

    if ( !TestApp_Init(GetArgs()) )
        THROW1_TRACE(runtime_error, "Cannot init test application");
}


void CTestApplication::TestApp_Args(CArgDescriptions& args)
{
    args.AddOptionalKey
        ("fromgi", "FromGi",
         "Process sequences in the interval FROM this Gi",
         CArgDescriptions::eInteger);
    args.AddOptionalKey
        ("togi", "ToGi",
         "Process sequences in the interval TO this Gi",
         CArgDescriptions::eInteger);
    args.AddOptionalKey
        ("idlist", "IdList",
         "File with list of Seq-ids to test",
         CArgDescriptions::eInputFile);
    args.AddDefaultKey
        ("type", "Type",
         "Type of bulk request",
         CArgDescriptions::eString, "gi");
    args.SetConstraint("type",
                       &(*new CArgAllow_Strings,
                         "gi", "acc", "label", "taxid", "length", "type"));
    args.AddFlag("no-force", "Do not force info loading");
    args.AddFlag("verbose", "Verbose results");
    args.AddFlag("single", "Use single id queries (non-bulk)");
    args.AddFlag("verify", "Run extra test to verify returned values");
    args.AddDefaultKey
        ("count", "Count",
         "Number of iterations to run (default: 1)",
         CArgDescriptions::eInteger, "1");

    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "test_bulkinfo", false);
}


bool CTestApplication::TestApp_Init(const CArgs& args)
{
    if ( args["idlist"] ) {
        CNcbiIstream& file = args["idlist"].AsInputFile();
        string line;
        while ( getline(file, line) ) {
            size_t comment = line.find('#');
            if ( comment != NPOS ) {
                line.erase(comment);
            }
            line = NStr::TruncateSpaces(line);
            if ( line.empty() ) {
                continue;
            }
            
            CSeq_id sid(line);
            CSeq_id_Handle sih = CSeq_id_Handle::GetHandle(sid);
            if ( !sih ) {
                continue;
            }
            m_Ids.push_back(sih);
        }
        
        NcbiCout << "Testing bulk info load (" <<
            m_Ids.size() << " Seq-ids from file)..." << NcbiEndl;
    }
    if ( m_Ids.empty() && (args["fromgi"] || args["togi"]) ) {
        TIntId gi_from  = args["fromgi"]? args["fromgi"].AsInteger(): g_gi_from;
        TIntId gi_to    = args["togi"]? args["togi"].AsInteger(): g_gi_to;
        TIntId delta = gi_to > gi_from? 1: -1;
        for ( TIntId gi = gi_from; gi != gi_to+delta; gi += delta ) {
            m_Ids.push_back(CSeq_id_Handle::GetGiHandle(gi));
        }
        NcbiCout << "Testing bulk info load ("
            "gi from " << gi_from << " to " << gi_to << ")..." << NcbiEndl;
    }
    if ( m_Ids.empty() ) {
        TIntId count = g_gi_to-g_gi_from+1;
        for ( TIntId i = 0; i < count; ++i ) {
            if ( i % 3 != 0 ) {
                m_Ids.push_back(CSeq_id_Handle::GetGiHandle(i+g_gi_from));
            }
            else {
                CNcbiOstrstream str;
                if ( i & 1 )
                    str << "A";
                else
                    str << "a";
                if ( i & 2 )
                    str << "A";
                else
                    str << "a";
                str << setfill('0') << setw(6) << (i/3+g_acc_from);
                string acc = CNcbiOstrstreamToString(str);
                CSeq_id seq_id(acc);
                m_Ids.push_back(CSeq_id_Handle::GetHandle(seq_id));
            }
        }
        NcbiCout << "Testing bulk info load ("
            "accessions and gi from " <<
            g_gi_from << " to " << g_gi_to << ")..." << NcbiEndl;
    }
    if ( args["type"].AsString() == "gi" ) {
        m_Type = eBulk_gi;
    }
    else if ( args["type"].AsString() == "acc" ) {
        m_Type = eBulk_acc;
    }
    else if ( args["type"].AsString() == "label" ) {
        m_Type = eBulk_label;
    }
    else if ( args["type"].AsString() == "taxid" ) {
        m_Type = eBulk_taxid;
    }
    else if ( args["type"].AsString() == "length" ) {
        m_Type = eBulk_length;
    }
    else if ( args["type"].AsString() == "type" ) {
        m_Type = eBulk_type;
    }
    m_ForceLoad = args["no-force"]? CScope::eNoForceLoad: CScope::eForceLoad;
    m_ForceLabelLoad = args["no-force"]?
        CScope::eNoForceLabelLoad: CScope::eForceLabelLoad;
    m_Verbose = args["verbose"];
    m_Single = args["single"];
    m_Verify = args["verify"];
    return true;
}


static
CRef<CScope> s_MakeScope(void)
{
    CRef<CScope> ret;
    CRef<CObjectManager> pOm = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*pOm);
    //CAsnCache_DataLoader::RegisterInObjectManager(*pOm, "/panfs/pan1.be-md.ncbi.nlm.nih.gov/gpipe_id_cache/refseq_prot_cache/", CObjectManager::eDefault, 88);
    
    ret = new CScope(*pOm);
    ret->AddDefaults();
    return ret;
}


template<class Data>
void CTestApplication::Display(size_t i,
                               const char* name,
                               const Data& gis,
                               const Data& gis2,
                               const Data& gisv) const
{
    if ( gis.empty() ) {
        return;
    }
    if ( !m_Verbose && gis2[i] == gis[i] && gisv[i] == gis[i] ) {
        return;
    }
    cout << name<<"("<<m_Ids[i]<<") -> "<<gis[i];
    if ( !m_Single && gis2[i] != gis[i] ) {
        cout << " single: "<<gis2[i];
    }
    if ( m_Verify && gisv[i] != gis[i] ) {
        cout << " extra: "<<gisv[i];
    }
    cout << endl;
}


int CTestApplication::Run(void)
{
    int run_count = GetArgs()["count"].AsInteger();
    for ( int run_i = 0; run_i < run_count; ++run_i ) {
        size_t count = m_Ids.size();

        TGis gis, gis2, gisv;
        TAccs accs, accs2, accsv;
        TLabels labels, labels2, labelsv;
        TTaxIds taxids, taxids2, taxidsv;
        TLengths lengths, lengths2, lengthsv;
        TTypes types, types2, typesv;

        if ( !m_Single ) {
            CRef<CScope> scope = s_MakeScope();
            switch ( m_Type ) {
            case eBulk_gi:
                gis = scope->GetGis(m_Ids, m_ForceLoad);
                break;
            case eBulk_acc:
                accs = scope->GetAccVers(m_Ids, m_ForceLoad);
                break;
            case eBulk_label:
                labels = scope->GetLabels(m_Ids, m_ForceLoad);
                break;
            case eBulk_taxid:
                taxids = scope->GetTaxIds(m_Ids, m_ForceLoad);
                break;
            case eBulk_length:
                lengths = scope->GetSequenceLengths(m_Ids, m_ForceLoad);
                break;
            case eBulk_type:
                types = scope->GetSequenceTypes(m_Ids, m_ForceLoad);
                break;
            }
            for ( size_t i = 0; i < count; ++i ) {
                _ASSERT(!scope->GetBioseqHandle(m_Ids[i], CScope::eGetBioseq_Loaded));
            }
        }

        {
            CRef<CScope> scope = s_MakeScope();

            switch ( m_Type ) {
            case eBulk_gi:    gis2.resize(count); break;
            case eBulk_acc:   accs2.resize(count); break;
            case eBulk_label: labels2.resize(count); break;
            case eBulk_taxid: taxids2.resize(count); break;
            case eBulk_length: lengths2.resize(count); break;
            case eBulk_type: types2.resize(count); break;
            }
            for ( size_t i = 0; i < count; ++i ) {
                switch ( m_Type ) {
                case eBulk_gi:
                    gis2[i] = scope->GetGi(m_Ids[i]);
                    break;
                case eBulk_acc:
                    accs2[i] = scope->GetAccVer(m_Ids[i]);
                    break;
                case eBulk_label:
                    labels2[i] = scope->GetLabel(m_Ids[i], m_ForceLabelLoad);
                    break;
                case eBulk_taxid:
                    taxids2[i] = scope->GetTaxId(m_Ids[i], m_ForceLoad);
                    break;
                case eBulk_length:
                    lengths2[i] = scope->GetSequenceLength(m_Ids[i], m_ForceLoad);
                    break;
                case eBulk_type:
                    types2[i] = scope->GetSequenceType(m_Ids[i], m_ForceLoad);
                    break;
                }
            }
            for ( size_t i = 0; i < count; ++i ) {
                _ASSERT(!scope->GetBioseqHandle(m_Ids[i], CScope::eGetBioseq_Loaded));
            }
        }

        if ( m_Single ) {
            switch ( m_Type ) {
            case eBulk_gi:    gis = gis2; break;
            case eBulk_acc:   accs = accs2; break;
            case eBulk_label: labels = labels2; break;
            case eBulk_taxid: taxids = taxids2; break;
            case eBulk_length: lengths = lengths2; break;
            case eBulk_type: types = types2; break;
            }
        }

        if ( m_Verify ) {
            CRef<CScope> scope = s_MakeScope();

            switch ( m_Type ) {
            case eBulk_gi:    gisv.resize(count); break;
            case eBulk_acc:   accsv.resize(count); break;
            case eBulk_label: labelsv.resize(count); break;
            case eBulk_taxid: taxidsv.resize(count); break;
            case eBulk_length: lengthsv.resize(count); break;
            case eBulk_type: typesv.resize(count); break;
            }
            for ( size_t i = 0; i < count; ++i ) {
                CBioseq_Handle h = scope->GetBioseqHandle(m_Ids[i]);
                switch ( m_Type ) {
                case eBulk_gi:
                    if ( h ) {
                        CSeq_id_Handle id = GetId(h, eGetId_ForceGi);
                        if ( id && id.IsGi() ) {
                            gisv[i] = id.GetGi();
                        }
                    }
                    break;
                case eBulk_acc:
                    if ( h ) {
                        accsv[i] = GetId(h, eGetId_ForceAcc);
                    }
                    break;
                case eBulk_label:
                    if ( h ) {
                        labelsv[i] = GetLabel(h.GetId());
                    }
                    break;
                case eBulk_taxid:
                    taxidsv[i] = h? GetTaxId(h): 0;
                        break;
                case eBulk_length:
                    lengthsv[i] = h? h.GetBioseqLength(): kInvalidSeqPos;
                        break;
                case eBulk_type:
                    typesv[i] = h? h.GetSequenceType(): CSeq_inst::eMol_not_set;
                        break;
                }
                if ( h ) {
                    scope->RemoveFromHistory(h);
                }
            }
        }
        else {
            switch ( m_Type ) {
            case eBulk_gi:    gisv = gis2; break;
            case eBulk_acc:   accsv = accs2; break;
            case eBulk_label: labelsv = labels2; break;
            case eBulk_taxid: taxidsv = taxids2; break;
            case eBulk_length: lengthsv = lengths2; break;
            case eBulk_type: typesv = types2; break;
            }
        }

        for ( size_t i = 0; i < count; ++i ) {
            Display(i, "gi", gis, gis2, gisv);
            Display(i, "acc", accs, accs2, accsv);
            Display(i, "label", labels, labels2, labelsv);
            Display(i, "taxid", taxids, taxids2, taxidsv);
            Display(i, "length", lengths, lengths2, lengthsv);
 Display(i, "type", types, types2, typesv);
        }

        switch ( m_Type ) {
        case eBulk_gi:    _ASSERT(gis == gis2); break;
        case eBulk_acc:   _ASSERT(accs == accs2); break;
        case eBulk_label: _ASSERT(labels == labels2); break;
        case eBulk_taxid: _ASSERT(taxids == taxids2); break;
        case eBulk_length: _ASSERT(lengths == lengths2); break;
        case eBulk_type: _ASSERT(types == types2); break;
        }
        switch ( m_Type ) {
        case eBulk_gi:    _ASSERT(gis == gisv); break;
        case eBulk_acc:   _ASSERT(accs == accsv); break;
        case eBulk_label: _ASSERT(labels == labelsv); break;
        case eBulk_taxid: _ASSERT(taxids == taxidsv); break;
        case eBulk_length: _ASSERT(lengths == lengthsv); break;
        case eBulk_type: _ASSERT(types == typesv); break;
        }
    }
    LOG_POST("Passed");
    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//
//  MAIN
//


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv);
}
