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
* Author:  Jonathan Kans, NCBI
*          Frank Ludwig, NCBI
*
* File Description:
*   C++ toolkit profiling module
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbitime.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/cleanup/cleanup.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(sequence);

/////////////////////////////////////////////////////////////////////////////


class CMytestApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    void DoProcess ( CNcbiIstream& ip, CNcbiOstream& op, CScope&, CRef<CSeq_entry>& se );

    void DoProcessStreamFasta ( CNcbiIstream& ip, CNcbiOstream& op, CRef<CSeq_entry>& se );

    int DoProcessFeatureGeneOverlap( CNcbiIstream&, CNcbiOstream&, CScope&, CRef<CSeq_entry>& );
    int TestFeatureGeneOverlap( CNcbiIstream&, CNcbiOstream&, CScope&, CBioseq& );
    int TestFeatureGeneOverlap( CNcbiIstream&, CNcbiOstream&, CScope&, CSeq_annot& );
    int TestFeatureGeneOverlap( CNcbiIstream&, CNcbiOstream&, CScope&, CSeq_feat& );

    // data
    bool  m_bsec;
    bool  m_ssec;

    bool  m_fidx;

    bool  m_fasta;
    bool  m_nodef;
    bool  m_featfa;
    bool  m_transl;
    bool  m_svisit;
    bool  m_ostream;

    bool  m_fvisit;
    bool  m_goverlap;
    bool  m_gxref;
    bool  m_ooverlap;
};


/////////////////////////////////////////////////////////////////////////////


void CMytestApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "C++ speed test program");

    arg_desc->AddKey
        ("i", "InputFile",
         "Input File Name",
         CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey
        ("o", "OutputFile",
         "Output File Name",
         CArgDescriptions::eOutputFile, "-");

    arg_desc->AddDefaultKey
        ("a", "ASN1Type",
         "ASN.1 Type",
         CArgDescriptions::eString, "a");
    arg_desc->SetConstraint
        ("a", &(*new CArgAllow_Strings, "a", "e", "b", "s", "m", "t", "l"));

    arg_desc->AddDefaultKey
        ("X", "Repetitions",
         "Max Repeat Count",
         CArgDescriptions::eInteger, "1");

    arg_desc->AddOptionalKey
        ("K", "Cleanup",
         "b Basic, s Serious",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("K", &(*new CArgAllow_Strings, "b", "s"));

    arg_desc->AddOptionalKey
        ("I", "Indexing",
         "f Feature Indexing",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("I", &(*new CArgAllow_Strings, "f"));

    arg_desc->AddOptionalKey
        ("S", "Sequence",
         "s FASTA, r No Defline, f By Feature, t Translation, v Visit, o Ostream",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("S", &(*new CArgAllow_Strings, "s", "r", "f", "t", "v", "o"));

    arg_desc->AddOptionalKey
        ("F", "Feature",
         "v Visit, g Gene by Overlap, x Xref, o Operon",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("F", &(*new CArgAllow_Strings, "v", "g", "x", "o"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////

void CMytestApplication::DoProcessStreamFasta (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CRef<CSeq_entry>& se
)
{
    CFastaOstream fo (op);
    for (CTypeConstIterator<CBioseq> bit (*se); bit; ++bit) {
        fo.Write (*bit);
    }
}

/////////////////////////////////////////////////////////////////////////////

string s_AsString( const CSeq_feat_Base::TLocation& loc ) 
{
    static string locstr;
    bool complement( false );

    const CSeq_id* id = loc.GetId();
    if ( ! id ) {
        return "?";
    }
    locstr = "[gi|" + NStr::IntToString( id->GetGi() ) + ":";

    switch ( loc.Which() ) {

        case CSeq_feat_Base::TLocation::e_Int: {
            const CSeq_loc_Base::TInt& intv = loc.GetInt();
            complement = ( intv.GetStrand() == eNa_strand_minus );
            if ( ! complement ) {
                locstr +=  NStr::IntToString( intv.GetFrom()+1 ) + "-" +  
                    NStr::IntToString( intv.GetTo()+1 ) + "]";
            }
            else {
                locstr +=  'c' + NStr::IntToString( intv.GetTo()+1 ) + "-" +  
                    NStr::IntToString( intv.GetFrom()+1 ) + "]";
            }
            break;
        }
        default:
            locstr = "?";
            break;
    }
    return locstr;
}

int CMytestApplication::TestFeatureGeneOverlap (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CScope& scope,
    CSeq_feat& f )
{
    if ( f.GetData().Which() == CSeqFeatData::e_Gene ) {
        return 1;
    }
    const CSeq_feat_Base::TLocation& locbase = f.GetLocation();
    CConstRef<CSeq_feat> ol;
    if ( ! ol ) {
        return 1;
    }
    op << s_AsString( locbase ) << " -> " << s_AsString( ol->GetLocation() ) << endl;
    op.flush();
    return 1;
}

int CMytestApplication::TestFeatureGeneOverlap (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CScope& scope,
    CSeq_annot& sa )
{
    int count = 0;
    if ( sa.IsSetData()  &&  sa.GetData().IsFtable() ) {
        NON_CONST_ITERATE( CSeq_annot::TData::TFtable, it, sa.SetData().SetFtable() ) {
            count += TestFeatureGeneOverlap( ip, op, scope, **it );
        }
    }
    return count;
}

int CMytestApplication::TestFeatureGeneOverlap (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CScope& scope,
    CBioseq& bs )
{
    int count = 0;
    if (bs.IsSetAnnot()) {
        NON_CONST_ITERATE (CBioseq::TAnnot, it, bs.SetAnnot()) {
            count += TestFeatureGeneOverlap( ip, op, scope, **it );
        }
    }
    return count;
}

int CMytestApplication::DoProcessFeatureGeneOverlap (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CScope& scope, 
    CRef<CSeq_entry>& se )
{
    int count = 0;
    switch (se->Which()) {

        case CSeq_entry::e_Seq:
            count += TestFeatureGeneOverlap( ip, op, scope, se->SetSeq() );
            break;

        case CSeq_entry::e_Set: {
            CBioseq_set& bss( se->SetSet() );
            if (bss.IsSetAnnot()) {
                NON_CONST_ITERATE (CBioseq::TAnnot, it, bss.SetAnnot()) {
                    count += TestFeatureGeneOverlap( ip, op, scope, **it );
                }
            }

            if (bss.IsSetSeq_set()) {
                NON_CONST_ITERATE (CBioseq_set::TSeq_set, it, bss.SetSeq_set()) {
                    CBioseq_set::TSeq_set::iterator it2 = it;
                    count += DoProcessFeatureGeneOverlap( ip, op, scope, *it );
                }
            }
            break;
        }

        case CSeq_entry::e_not_set:
        default:
            break;
    }
    return count;
}

/////////////////////////////////////////////////////////////////////////////

void CMytestApplication::DoProcess (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CScope& scope,
    CRef<CSeq_entry>& se
)

{
    if (m_bsec) {
        CCleanup Cleanup;
        Cleanup.BasicCleanup( se.GetObject() );
    }
    if (m_ssec) {
        // need to implement
    }

    if (m_fidx) {
        // need to implement
    }

    if (m_fasta) {
        DoProcessStreamFasta( ip, op, se );
    }
    if (m_nodef) {
        // need to implement
    }
    if (m_featfa) {
        // need to implement
    }
    if (m_transl) {
        // need to implement
    }
    if (m_svisit) {
        for (CTypeConstIterator<CBioseq> bit (*se); bit; ++bit) {
            if (m_ostream) {
                CFastaOstream fo (op);
            }
        }
    }
    if (m_ostream && (! m_svisit)) {
        CFastaOstream fo (op);
    }

    if (m_fvisit) {
        for (CTypeConstIterator<CBioseq> bit (*se); bit; ++bit) {
            // need to implement
        }
    }
    if (m_goverlap) {
        DoProcessFeatureGeneOverlap( ip, op, scope, se );
    }
    if (m_gxref) {
        // need to implement
    }
    if (m_ooverlap) {
        // need to implement
    }
}

int CMytestApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    CNcbiIstream& ip = args["i"].AsInputFile();
    CNcbiOstream& op = args["o"].AsOutputFile();

    string tp = args["a"].AsString();

    int mx = args["X"].AsInteger();
    if (mx < 1) {
        mx = 1;
    }

    m_bsec = false;
    m_ssec = false;

    m_fidx = false;

    m_fasta = false;
    m_nodef = false;
    m_featfa = false;
    m_transl = false;
    m_svisit = false;
    m_ostream = false;

    m_fvisit = false;
    m_goverlap = false;
    m_gxref = false;
    m_ooverlap = false;

    if (args["K"]) {
        string km = args["K"].AsString();
        if (NStr::Find (km, "b") != NPOS) {
            m_bsec = true;
        }
        if (NStr::Find (km, "s") != NPOS) {
            m_ssec = true;
        }
    }

    if (args["I"]) {
        string im = args["I"].AsString();
        if (NStr::Find (im, "f") != NPOS) {
            m_fidx = true;
        }
    }

    if (args["S"]) {
        string sm = args["S"].AsString();
        if (NStr::Find (sm, "s") != NPOS) {
            m_fasta = true;
        }
        if (NStr::Find (sm, "r") != NPOS) {
            m_nodef = true;
        }
        if (NStr::Find (sm, "f") != NPOS) {
            m_featfa = true;
        }
        if (NStr::Find (sm, "t") != NPOS) {
            m_transl = true;
        }
        if (NStr::Find (sm, "v") != NPOS) {
            m_svisit = true;
        }
        if (NStr::Find (sm, "o") != NPOS) {
            m_ostream = true;
        }
    }

    if (args["F"]) {
        string fm = args["F"].AsString();
        if (NStr::Find (fm, "v") != NPOS) {
            m_fvisit = true;
        }
        if (NStr::Find (fm, "g") != NPOS) {
            m_goverlap = true;
        }
        if (NStr::Find (fm, "x") != NPOS) {
            m_gxref = true;
        }
        if (NStr::Find (fm, "o") != NPOS) {
            m_ooverlap = true;
        }
    }

    int ct;
    CStopWatch sw;
    sw.Start();
    double lastInterval( 0 );

    // read line at a time if indicated
    if (NStr::Equal(tp, "l")) {
        string str;

        while (NcbiGetlineEOL (ip, str)) {
            if (! str.empty ()) {
                // op << str << endl;
            }
        }

        lastInterval = sw.Elapsed() - lastInterval;
        NcbiCout << "Read by line time is " << lastInterval << " seconds" << endl;
        return 0;
    }

    // otherwise read ASN.1
    auto_ptr<CObjectIStream> is (CObjectIStream::Open (eSerial_AsnText, ip));
    CRef<CSeq_entry> se(new CSeq_entry);
    *is >> *se;

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    if ( !objmgr ) {
        /* raise hell */;
    }
    CRef<CScope> scope( new CScope( *objmgr ) );
    if ( !scope ) {
        /* raise hell */;
    }
    scope->AddDefaults();
    scope->AddTopLevelSeqEntry(*se);

    lastInterval = sw.Elapsed() - lastInterval;
    NcbiCout << "ASN reading time is " << lastInterval << " seconds" << endl;

    for (ct = 0; ct < mx; ct++) {
        DoProcess (ip, op, *scope, se);
    }

    lastInterval = sw.Elapsed() - lastInterval;
    NcbiCout << "Internal processing time is " << lastInterval << " seconds" << endl;

    // write ASN.1
    /*
    if (NStr::Equal (fm, "a")) {
        auto_ptr<CObjectOStream> os (CObjectOStream::Open (eSerial_AsnText, op));
        *os << *se;
        t2.SetCurrent();
        tx = t2.DiffSecond (t1);
        NcbiCout << "elapsed time is " << tx << endl;
        return 0;
    }
    */

    return 0;
}


/////////////////////////////////////////////////////////////////////////////


void CMytestApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CMytestApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}

