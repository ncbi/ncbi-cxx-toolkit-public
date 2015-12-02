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
* Authors:  Eugene Vasilchenko, Aleksey Grichenko, Denis Vakatov
*
* File Description:
*   Test the functionality of annotation iterators
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbistr.hpp>
#include <util/util_exception.hpp>
#include <serial/typeinfo.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/annot_ci.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;

// Names and values of arguments
const string kTestsSection      = "Tests"; // The section must contain a list
                                           // of subsections with test settings
const string kLocation          = "loc";       // ASN.1 seq-loc
const string kAnnotType         = "type";      // annotation type
const string kAnnotType_Feat    = "feat";
const string kAnnotType_Align   = "align";
const string kAnnotType_Graph   = "graph";
const string kAnnotType_Annot   = "annot";
const string kFeatType          = "feat_type"; // Feature type (int)
const string kFeatSubtype       = "subtype";   // Feature subtype (int)
const string kResolve           = "resolve";   // resolve flag
const string kResolve_TSE       = "tse";
const string kResolve_All       = "all";
const string kResolve_None      = "none";
const string kOverlap           = "overlap";   // overlap type (int/total)
const string kOverlap_Int       = "int";
const string kOverlap_Total     = "total";
const string kSort              = "sort";      // sort (none/normal/reverse)
const string kSort_None         = "none";
const string kSort_Normal       = "normal";
const string kSort_Reverse      = "reverse";
const string kByProduct         = "product";   // search by product (bool)
const string kDepth             = "depth";     // depth: adaptive or <int>
const string kDepth_Adaptive    = "adaptive";
const string kMapped            = "mapped";    // original or mapped (bool)


template<class TObjectType> class CTestResult
{
public:
    typedef CRef<TObjectType> TObjectRef;
    typedef list<TObjectRef> TObjectList;
    static void Check(const TObjectList& obj_list,
                      const CArgs& args,
                      const string& title);
};


template<class TObjectType>
void CTestResult<TObjectType>::Check(const TObjectList& obj_list,
                                     const CArgs& args,
                                     const string& title)
{
    if ( args["dump"] ) {
        ITERATE(typename TObjectList, it, obj_list) {
            NcbiCout << MSerial_AsnText << **it;
        }
    }
    else {
        TObjectType ref;
        for (size_t i = 0; i < obj_list.size(); ++i) {
            try {
                args["results"].AsInputFile() >> MSerial_AsnText >> ref;
            }
            catch (CSerialException) {
                SetDiagStream(&NcbiCerr);
                ERR_POST(Fatal << "Test '" << title
                    << "' failed - unable to read "
                    << TObjectType::GetTypeInfo()->GetName());
            }
            bool found = false;
            ITERATE(typename TObjectList, it, obj_list) {
                if ( (*it)->Equals(ref) ) {
                    found = true;
                    break;
                }
            }
            if ( !found ) {
                SetDiagStream(&NcbiCerr);
                ERR_POST(Fatal << "Test '" << title << "' failed - invalid "
                    << TObjectType::GetTypeInfo()->GetName());
            }
        }
    }
}


void CheckString(const string& str,
                 const CArgs& args,
                 const string& title)
{
    if ( args["dump"] ) {
        NcbiCout << str;
    }
    else {
        list<string> errs;
        NStr::Split(str, "\n\r", errs,
            NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
        CNcbiIstream& in = args["results"].AsInputFile();
        ITERATE(list<string>, err, errs) {
            string ref;
            getline(in, ref);
            if ( ref.empty() ) {
                // Skip first EOL if any
                getline(in, ref);
            }
            if ( ref != *err ) {
                ERR_POST(Fatal << "Test '" << title << "' failed - "
                         "invalid error message(s).\nExpected:\n"<<ref<<
                         "\nReal:\n"<<*err);
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//

class CTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
private:
    void LoadEntries(void);

    typedef CConfig::TParamTree   TParamTree;
    typedef TParamTree::TTreeType TParams;

    string x_GetStringParam(const TParams& param,
                            const string& name,
                            bool optional = false);
    bool x_GetBoolParam(const TParams& param,
                        const string& name,
                        bool optional = false);
    SAnnotSelector x_InitSelector(const TParams& param);

    CRef<CObjectManager> m_ObjMgr;
    CRef<CScope>         m_Scope;
};


void CTestApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey("entries", "SeqEntriesFile",
                            "Seq-entries for the test",
                            CArgDescriptions::eInputFile,
                            "test_annot_entries.asn");
    arg_desc->AddDefaultKey("results", "ResultsFile",
                            "Resulting annotations",
                            CArgDescriptions::eInputFile,
                            "test_annot_res.asn");
    arg_desc->AddFlag("genbank", "Use GenBank loader to fetch data");
    arg_desc->AddFlag("dump", "Dump data rather than checking results");

    string prog_description = "Test for annotation iterators";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


void CTestApp::LoadEntries(void)
{
    m_Scope.Reset(new CScope(*m_ObjMgr));
    const CArgs& args = GetArgs();

    if ( args["genbank"] ) {
        CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
        m_Scope->AddDefaults();
    }

    CNcbiIstream& in = args["entries"].AsInputFile();
    for ( ; ; ) {
        try {
            CRef<CSeq_entry> entry(new CSeq_entry);
            in >> MSerial_AsnText >> *entry;
            m_Scope->AddTopLevelSeqEntry(*entry);
        }
        catch (CSerialException& e) {
            ERR_POST(Fatal <<
                "Error reading source Seq-entry: " << e.what());
        }
        catch (CEofException) {
            // Ignore EOF exceptions - it's just end of data
            break;
        }
    }
}


string CTestApp::x_GetStringParam(const TParams& param,
                                  const string& name,
                                  bool optional)
{
    const TParamTree* p = param.
        FindNode(name, TParamTree::eImmediateSubNodes);
    if ( !p  &&  !optional ) {
        ERR_POST(Fatal <<
            "Missing parameter '" << name << "' in '" << p->GetKey() << "'");
    }
    return p ? p->GetValue().value : kEmptyStr;
}


bool CTestApp::x_GetBoolParam(const TParams& param,
                              const string& name,
                              bool optional)
{
    string val = x_GetStringParam(param, name, optional);
    if ( val.empty() ) {
        _ASSERT(optional);
        return false;
    }
    return NStr::StringToBool(val);
}


SAnnotSelector CTestApp::x_InitSelector(const TParams& param)
{
    SAnnotSelector sel;
    string value = x_GetStringParam(param, kFeatType, true);
    if ( !value.empty() ) {
        sel.SetFeatType(
            SAnnotSelector::TFeatType(NStr::StringToInt(value)));
    }

    value = x_GetStringParam(param, kFeatSubtype, true);
    if ( !value.empty() ) {
        sel.SetFeatSubtype(
            SAnnotSelector::TFeatSubtype(NStr::StringToInt(value)));
    }

    value = x_GetStringParam(param, kResolve, true);
    if ( NStr::EqualNocase(value, kResolve_All) ) {
        sel.SetResolveAll();
    }
    else if ( NStr::EqualNocase(value, kResolve_TSE) ) {
        sel.SetResolveTSE();
    }
    else if ( NStr::EqualNocase(value, kResolve_None) ) {
        sel.SetResolveNone();
    }
    else if ( !value.empty() ) {
        ERR_POST(Error << "Invalid resolve mode: " << value);
    }

    value = x_GetStringParam(param, kOverlap, true);
    if ( NStr::EqualNocase(value, kOverlap_Int) ) {
        sel.SetOverlapIntervals();
    }
    else if ( NStr::EqualNocase(value, kOverlap_Total) ) {
        sel.SetOverlapTotalRange();
    }
    else if ( !value.empty() ) {
        ERR_POST(Error << "Invalid overlap mode: " << value);
    }

    SAnnotSelector::ESortOrder order = SAnnotSelector::eSortOrder_Normal;
    value = x_GetStringParam(param, kSort, true);
    if ( NStr::EqualNocase(value, kSort_None) ) {
        order = SAnnotSelector::eSortOrder_None;
    }
    else if ( NStr::EqualNocase(value, kSort_Reverse) ) {
        order = SAnnotSelector::eSortOrder_Reverse;
    }
    else if ( NStr::EqualNocase(value, kSort_Normal) ) {
        order = SAnnotSelector::eSortOrder_Normal;
    }
    else if ( !value.empty() ) {
        ERR_POST(Error << "Invalid sort order: " << value);
    }
    sel.SetSortOrder(order);

    if ( x_GetBoolParam(param, kByProduct, true) ) {
        sel.SetByProduct();
    }

    value = x_GetStringParam(param, kDepth, true);
    if ( !value.empty() ) {
        if ( NStr::EqualNocase(value, kDepth_Adaptive) ) {
            sel.SetAdaptiveDepth();
        }
        else {
            sel.SetResolveDepth(NStr::StringToInt(value));
        }
    }
    return sel;
}


int CTestApp::Run(void)
{
    // SetDiagPostFlag(eDPF_All);
    const CArgs& args = GetArgs();
    m_ObjMgr = CObjectManager::GetInstance();

    LoadEntries();
    
    auto_ptr<CConfig::TParamTree>
        app_params(CConfig::ConvertRegToTree(GetConfig()));
    _ASSERT( app_params.get() );
    const TParamTree* tests = app_params->FindNode(kTestsSection);
    if ( !tests ) {
        ERR_POST(Fatal << "[Tests] not found in config file");
    }

    // Sort subsections before iterating them
    set<string> subsections;
    TParamTree::TNodeList_CI subsection_it = tests->SubNodeBegin();
    for ( ; subsection_it != tests->SubNodeEnd(); subsection_it++) {
        subsections.insert((*subsection_it)->GetKey());
    }

    // Capture errors
    CNcbiOstrstream err_out;
    SetDiagStream(&err_out);

    ITERATE(set<string>, test_it, subsections) {
        const TParamTree* param_ptr = tests->FindNode(*test_it);
        _ASSERT(param_ptr);
        const TParams& param = *param_ptr;
        string title = param_ptr->GetValue().id;

        CSeq_loc loc;
        {{
            string loc_str = x_GetStringParam(param, kLocation);
            CNcbiIstrstream in(loc_str.c_str());
            in >> MSerial_AsnText >> loc;
        }}
        SAnnotSelector sel = x_InitSelector(param);
        bool use_mapped = x_GetBoolParam(param, kMapped, true);
        string value = x_GetStringParam(param, kAnnotType, true);

        try {
            if ( value.empty() || NStr::EqualNocase(value, kAnnotType_Feat) ) {
                typedef CTestResult<CSeq_feat> TTestResult;
                TTestResult::TObjectList feat_list;
                for (CFeat_CI it(*m_Scope, loc, sel); it; ++it) {
                    CRef<CSeq_feat> feat(new CSeq_feat);
                    feat->Assign(use_mapped ?
                        it->GetMappedFeature() : it->GetOriginalFeature());
                    feat_list.push_back(feat);
                }
                TTestResult::Check(feat_list, args, title);
            }
            else if ( NStr::EqualNocase(value, kAnnotType_Align) ) {
                typedef CTestResult<CSeq_align> TTestResult;
                TTestResult::TObjectList align_list;
                for (CAlign_CI it(*m_Scope, loc, sel); it; ++it) {
                    CRef<CSeq_align> align(new CSeq_align);
                    align->Assign(use_mapped ?
                        *it : it.GetOriginalSeq_align());
                    align_list.push_back(align);
                }
                TTestResult::Check(align_list, args, title);
            }
            else if ( NStr::EqualNocase(value, kAnnotType_Graph) ) {
                typedef CTestResult<CSeq_graph> TTestResult;
                TTestResult::TObjectList graph_list;
                for (CGraph_CI it(*m_Scope, loc, sel); it; ++it) {
                    CRef<CSeq_graph> graph(new CSeq_graph);
                    graph->Assign(use_mapped ?
                        it->GetMappedGraph() : it->GetOriginalGraph());
                    graph_list.push_back(graph);
                }
                TTestResult::Check(graph_list, args, title);
            }
            else if ( NStr::EqualNocase(value, kAnnotType_Annot) ) {
                typedef CTestResult<CSeq_annot> TTestResult;
                TTestResult::TObjectList annot_list;
                for (CAnnot_CI it(*m_Scope, loc, sel); it; ++it) {
                    CRef<CSeq_annot> annot(new CSeq_annot);
                    annot->Assign(*it->GetCompleteSeq_annot());
                    annot_list.push_back(annot);
                }
                TTestResult::Check(annot_list, args, title);
            }
        }
        catch (CException& e) {
            ERR_POST(e.what());
        }
    }
    // Check captured errors
    SetDiagStream(&NcbiCerr);
    if ( !IsOssEmpty(err_out) ) {
        CheckString(CNcbiOstrstreamToString(err_out), args, "errors messages");
    }

    LOG_POST("All tests passed");
    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestApp().AppMain(argc, argv);
}
