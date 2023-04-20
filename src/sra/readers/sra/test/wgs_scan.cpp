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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Scan WGS VDB projects
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/sra/wgsresolver.hpp>
#include <sra/readers/sra/impl/wgsresolver_impl.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/seq_vector.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasnb.hpp>

#include <util/random_gen.hpp>
#include <numeric>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CWGSScanApp::


class IWGSCheck
{
public:
    virtual ~IWGSCheck() = default;
    virtual pair<string, bool> Check(CWGSDb& wgs) = 0;
};


class CWGSScanApp : public CNcbiApplication
{
protected:
    virtual void Init(void) override;
    virtual int  Run(void) override;
    virtual void Exit(void) override;

    void CheckOne(const string& name);
    void CheckMask(const string& mask);
    void CheckRange(const string& start_name, const string& end_name);
    void CheckArg(const string& arg);
    void Report(const string& name, const string& result);
    void Report(const string& name, const exception& exc);
    void Report(const string& name, const CException& exc);
    void Report(const string& name, const pair<string, bool>& result);
    
protected:
    CVDBMgr m_Mgr;
    size_t m_ErrorCount = 0;
    bool m_IgnoreAbsent = false;
    CNcbiOstream* m_Out = nullptr;
    unique_ptr<IWGSCheck> m_Check;
    map<string, size_t> m_Results;
};


class CWGSCheckDefault : public IWGSCheck
{
public:
    virtual pair<string, bool> Check(CWGSDb& /*wgs*/) override
        {
            return make_pair("exists", false);
        }
};


class CWGSCheckFeatLocIdType : public IWGSCheck
{
public:
    static const char* GetTypeName(CWGSDb_Impl::EFeatLocIdType type)
        {
            return type == CWGSDb_Impl::eFeatLocIdGi? "GI":
                type == CWGSDb_Impl::eFeatLocIdAccVer? "acc.ver": "acc";
        }
    bool CanHaveGis(CWGSDb& wgs) const
        {
            return wgs->CanHaveGis();
        }
    bool HasFeatures(CWGSDb& wgs) const
        {
            return wgs->HasFeatures();
        }
    CWGSDb_Impl::EFeatLocIdType GetFeatLocIdTypeFast(CWGSDb& wgs) const
        {
            if ( !CanHaveGis(wgs) ) {
                return CWGSDb_Impl::eFeatLocIdAccVer;
            }
            return GetFeatLocIdTypeActual(wgs);
        }
    CWGSDb_Impl::EFeatLocIdType GetFeatLocIdTypeActual(CWGSDb& wgs) const
        {
            return wgs->GetFeatLocIdType();
        }
    virtual pair<string, bool> Check(CWGSDb& wgs) override
        {
            if ( !HasFeatures(wgs) ) {
                return make_pair("no features", false);
            }
            auto fast_type = GetFeatLocIdTypeFast(wgs);
            auto real_type = GetFeatLocIdTypeActual(wgs);
            string s = (CanHaveGis(wgs)? "w/GI: ": "wo/GI: ");
            s += GetTypeName(fast_type);
            bool error = false;
            if ( fast_type != real_type ) {
                s += " != actual ";
                s += GetTypeName(real_type);
                error = true;
            }
            return make_pair(s, error);
        }
};


void CWGSScanApp::Init(void)
{
    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "wgs_scan");

    arg_desc->AddOptionalKey("type", "ScanType",
                             "Type of scan",
                             CArgDescriptions::eString);
    arg_desc->SetConstraint("type",
                            &(*new CArgAllow_Strings,
                              "feat_loc_id_type"));

    arg_desc->AddFlag("ignore_absent", "IgnoreAbsent");
    
    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");

    arg_desc->AddExtra(1, kMax_UInt,
                       "WGS project(s)",
                       CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


void CWGSScanApp::Report(const string& name, const string& result)
{
    m_Results[result] += 1;
    *m_Out << name << ": "<<result
           << endl;
}


void CWGSScanApp::Report(const string& name, const exception& exc)
{
    Report(name, string("exception: ")+exc.what());
    m_ErrorCount += 1;
}


void CWGSScanApp::Report(const string& name, const CException& exc)
{
    Report(name, string("exception: ")+exc.GetMsg());
    m_ErrorCount += 1;
}


void CWGSScanApp::Report(const string& name, const pair<string, bool>& result)
{
    if ( result.second ) {
        // error
        Report(name, "error: "+result.first);
        m_ErrorCount += 1;
    }
    else {
        Report(name, result.first);
    }
}


void CWGSScanApp::CheckOne(const string& name)
{
    CWGSDb wgs;
    try {
        wgs = CWGSDb(m_Mgr, name);
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFoundDb ) {
            if ( m_IgnoreAbsent ) {
                Report(name, "absent");
                return;
            }
        }
        Report(name, exc);
        return;
    }
    catch ( CException& exc ) {
        Report(name, exc);
        return;
    }
    catch ( exception& exc ) {
        Report(name, exc);
        return;
    }
    try {
        auto ret = m_Check->Check(wgs);
        Report(name, ret);
        return;
    }
    catch ( CException& exc ) {
        Report(name, exc);
        return;
    }
    catch ( exception& exc ) {
        Report(name, exc);
        return;
    }
}


static void make_next_name(string& name)
{
    size_t pos = name.size();
    while ( --pos > 0 ) {
        if ( isdigit(name[pos]) ) {
            continue;
        }
        if ( isalpha(name[pos]) ) {
            if ( isalpha(++name[pos]) ) {
                return;
            }
            name[pos] = 'A';
        }
        else {
            break;
        }
    }
    ++name[pos];
}


void CWGSScanApp::CheckRange(const string& start_name, const string& end_name)
{
    if ( start_name.empty() || start_name.size() != end_name.size() ) {
        ERR_POST(Fatal<<"Invalid range: "<<start_name<<"-"<<end_name);
    }
    for ( string name = start_name; name <= end_name; make_next_name(name) ) {
        CheckOne(name);
    }
}


void CWGSScanApp::CheckMask(const string& mask)
{
    string s = string(count(mask.begin(), mask.end(), '?'), 'A');
    if ( s.empty() ) {
        CheckOne(mask);
    }
    else {
        do {
            string name = mask;
            auto it_s = s.begin();
            for ( auto& c : name ) {
                if ( c == '?' ) {
                    c = *it_s++;
                }
            }
            make_next_name(s);
            CheckOne(name);
        } while ( isalpha(s[0]) );
    }
}


void CWGSScanApp::CheckArg(const string& name)
{
    auto dash = name.find('-');
    if ( dash != NPOS ) {
        CheckRange(name.substr(0, dash), name.substr(dash+1));
    }
    else if ( name.find('?') != NPOS ) {
        CheckMask(name);
    }
    else {
        CheckOne(name);
    }
}


int CWGSScanApp::Run(void)
{
    const CArgs& args = GetArgs();

    if ( args["o"] ) {
        m_Out = &args["o"].AsOutputFile();
    }
    else {
        m_Out = &cout;
    }

    m_IgnoreAbsent = args["ignore_absent"];
    if ( args["type"] ) {
        auto type = args["type"].AsString();
        if ( type == "feat_loc_id_type" ) {
            m_Check = make_unique<CWGSCheckFeatLocIdType>();
        }
    }
    if ( !m_Check ) {
        m_Check = make_unique<CWGSCheckDefault>();
    }

    m_ErrorCount = 0;

    for ( size_t i = 1, n = args.GetNExtra(); i <= n; ++i ) {
        CheckArg(args[i].AsString());
    }

    for ( auto& s : m_Results ) {
        LOG_POST("Results["<<s.first<<"] = "<<s.second);
    }
    if ( m_ErrorCount ) {
        LOG_POST("Failure. Error count: "<<m_ErrorCount);
        return 1;
    }
    else {
        LOG_POST("Success.");
        return 0;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CWGSScanApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CWGSScanApp().AppMain(argc, argv);
}
