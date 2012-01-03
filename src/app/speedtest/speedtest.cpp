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
#include <objects/submit/Seq_submit.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <algo/align/prosplign/prosplign.hpp>

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

    void DoProcessStreamFasta ( 
        CNcbiIstream& ip, CNcbiOstream& op, CRef<CSeq_entry>& se );
    void DoProcessStreamDefline ( 
        CNcbiIstream& ip, CNcbiOstream& op, CRef<CSeq_entry>& se, CScope&i );
    int DoProcessFeatureSuggest( 
        CNcbiIstream&, CNcbiOstream&, CScope&, CRef<CSeq_entry>&, bool do_format );

    int DoProcessFeatureGeneOverlap( CNcbiIstream&, CNcbiOstream&, CScope&, CRef<CSeq_entry>&, bool do_format );
    int TestFeatureGeneOverlap( CNcbiIstream&, CNcbiOstream&, CScope&, CBioseq&, bool do_format );
    int TestFeatureGeneOverlap( CNcbiIstream&, CNcbiOstream&, CScope&, CSeq_annot&, bool do_format );
    int TestFeatureGeneOverlap( CNcbiIstream&, CNcbiOstream&, CScope&, CSeq_feat&, bool do_format );

    int PlayAroundWithSuggestIntervals( CNcbiIstream&, CNcbiOstream&, CScope&, CRef<CSeq_entry>& );
    void DumpAlignment( CNcbiOstream&, CRef<CSeq_align>& );

    void GetSeqEntry( CNcbiIstream&, CRef<CSeq_entry>& se );

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
    bool  m_hoverlap;
    bool  m_gxref;
    bool  m_ooverlap;

    bool  m_defline_only;
    bool  m_no_scope;
    bool  m_suggest;
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
         "s FASTA, S FASTA(no_scope mode), r No Defline, d Defline only, D Defline only(no_scope mode), "
         "f By Feature, t Translation, v Visit, o Ostream",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("S", &(*new CArgAllow_Strings, "S", "s", "r", "D", "d", "f", "t", "v", "o"));

    arg_desc->AddOptionalKey
        ("F", "Feature",
         "v Visit, g Gene Overlap Print, h Gene Overlap Speed, x Xref, o Operon s Suggest",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("F", &(*new CArgAllow_Strings, "v", "g", "h", "x", "o", "s"));

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
        fo.Write (*bit, 0, m_no_scope);
    }
}

/////////////////////////////////////////////////////////////////////////////

void CMytestApplication::DoProcessStreamDefline (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CRef<CSeq_entry>& se,
    CScope& scope
)
{
    CFastaOstream fo (op);
    for (CTypeConstIterator<CBioseq> bit (*se); bit; ++bit) {
//        fo.WriteTitle (scope.GetBioseqHandle(*bit));
        fo.WriteTitle (*bit, 0, m_no_scope);

    }
}

////////////////////////////////////////////////////////////////////////////////
string SeqLocString( const CSeq_loc& loc )
{
    string str;
    loc.GetLabel(&str);
    return str;
}
   
////////////////////////////////////////////////////////////////////////////////

int CMytestApplication::TestFeatureGeneOverlap (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CScope& scope,
    CSeq_feat& f,
    bool do_format )
{
    if ( f.GetData().Which() == CSeqFeatData::e_Gene ) {
        return 1;
    }
    const CSeq_feat_Base::TLocation& locbase = f.GetLocation();
    CConstRef<CSeq_feat> ol = GetOverlappingGene( locbase, scope );
    if ( ! ol ) {
        return 1;
    }
    if (do_format) {
        op << SeqLocString( locbase ) << " -> " 
           << SeqLocString( ol->GetLocation() ) << '\n';
    }
    return 1;
}

////////////////////////////////////////////////////////////////////////////////

int CMytestApplication::DoProcessFeatureSuggest (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CScope& scope,
    CRef<CSeq_entry>& se,
    bool do_format )
{
    CProSplign prosplign;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

int CMytestApplication::TestFeatureGeneOverlap (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CScope& scope,
    CSeq_annot& sa,
    bool do_format )
{
    int count = 0;
    if ( sa.IsSetData()  &&  sa.GetData().IsFtable() ) {
        NON_CONST_ITERATE( CSeq_annot::TData::TFtable, it, sa.SetData().SetFtable() ) {
            count += TestFeatureGeneOverlap( ip, op, scope, **it, do_format );
        }
    }
    return count;
}

////////////////////////////////////////////////////////////////////////////////

int CMytestApplication::TestFeatureGeneOverlap (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CScope& scope,
    CBioseq& bs,
    bool do_format )
{
    int count = 0;
    if (bs.IsSetAnnot()) {
        NON_CONST_ITERATE (CBioseq::TAnnot, it, bs.SetAnnot()) {
            count += TestFeatureGeneOverlap( ip, op, scope, **it, do_format );
        }
    }
    return count;
}

////////////////////////////////////////////////////////////////////////////////

int CMytestApplication::DoProcessFeatureGeneOverlap (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CScope& scope, 
    CRef<CSeq_entry>& se,
    bool do_format )
{
    int count = 0;
    switch (se->Which()) {

        case CSeq_entry::e_Seq:
            count += TestFeatureGeneOverlap( ip, op, scope, se->SetSeq(), do_format );
            break;

        case CSeq_entry::e_Set: {
            CBioseq_set& bss( se->SetSet() );
            if (bss.IsSetAnnot()) {
                NON_CONST_ITERATE (CBioseq::TAnnot, it, bss.SetAnnot()) {
                    count += TestFeatureGeneOverlap( ip, op, scope, **it, do_format );
                }
            }

            if (bss.IsSetSeq_set()) {
                NON_CONST_ITERATE (CBioseq_set::TSeq_set, it, bss.SetSeq_set()) {
                    count += DoProcessFeatureGeneOverlap( ip, op, scope, *it, do_format );
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
            return;
    }
    if (m_defline_only) {
        DoProcessStreamDefline( ip, op, se, scope );
            return;
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
        DoProcessFeatureGeneOverlap( ip, op, scope, se, true );
    }
    if (m_hoverlap) {
        DoProcessFeatureGeneOverlap( ip, op, scope, se, false );
    }
    if (m_gxref) {
        // need to implement
    }
    if (m_ooverlap) {
        // need to implement
    }
}

//  ============================================================================
void
CMytestApplication::DumpAlignment(
    CNcbiOstream& Os,
    CRef<CSeq_align>& align )
//  ============================================================================
{
    Os << MSerial_AsnText << *align << endl;
}

//  ============================================================================
int 
CMytestApplication::PlayAroundWithSuggestIntervals( 
    CNcbiIstream& is,
    CNcbiOstream& os, 
    CScope& scope,
    CRef<CSeq_entry>& se )
//  ============================================================================
{
    CSeq_entry_Handle entry;
    try {
        entry = scope.GetSeq_entryHandle( *se );
    } catch ( CException& ) {}

    if ( !entry ) {  // add to scope if not already in it
        entry = scope.AddTopLevelSeqEntry( *se );
    }
    CBioseq_set::TClass clss = entry.GetSet().GetClass();
    if (clss != CBioseq_set::eClass_nuc_prot) {
        return 1;
    }
    CRef<CSeq_loc> nucloc;
    list< CConstRef<CSeq_id> > proteins;

    CSeq_inst::TMol mol_type = CSeq_inst::eMol_not_set;
    CBioseq_CI seq_iter(entry, mol_type, CBioseq_CI::eLevel_Mains);
    for ( ; seq_iter; ++seq_iter ) {
        const CBioseq_Handle& bs = *seq_iter;
        if (bs.IsNa()) {
//            const CSeq_id& nucid = ( *bs.GetSeqId() );
//            nucloc.Reset( new CSeq_loc( nucid, 0, (int)bs.GetInst_Length(),eNa_strand_unknown ) );
            nucloc.Reset( bs.GetRangeSeq_loc(0, bs.GetInst_Length() ) );
//            nucloc.Reset( bs.GetRangeSeq_loc(0, 0 ) );
        }
        else if (bs.IsAa()) {
            proteins.push_back( bs.GetSeqId() );
        }
    }

    CProSplign prosplign;
    list< CConstRef<CSeq_id> >::iterator it = proteins.begin();
    for ( ; it != proteins.end(); ++it ) {
        CRef<CSeq_align> alignment = prosplign.FindAlignment(
            scope, 
            **it, 
            *nucloc );
        DumpAlignment( os, alignment );
    }
    os.flush();
    return 0;
}


////////////////////////////////////////////////////////////////////////////////
void CMytestApplication::GetSeqEntry( 
    CNcbiIstream& ip, 
    CRef<CSeq_entry>& se )
{
    string asntype = GetArgs()["a"].AsString();
    auto_ptr<CObjectIStream> is (CObjectIStream::Open (eSerial_AsnText, ip));
    is->SetStreamPos( 0 );

    if ( asntype == "a" || asntype == "m" ) {
        try {
            CRef<CSeq_submit> sub(new CSeq_submit());
            *is >> *sub;
            se.Reset( sub->SetData().SetEntrys().front() );
            return;
        }
        catch( ... ) {
            is->SetStreamPos( 0 );
        }
    }
    if ( asntype == "a" || asntype == "e" ) {
        try {
            *is >> *se;
            return;
        }
        catch( ... ) {
            is->SetStreamPos( 0 );
        }
    }
    if ( asntype == "a" || asntype == "b" ) {
        try {
		    CRef<CBioseq> bs( new CBioseq );
	        *is >> *bs;
            se->SetSeq( bs.GetObject() );
            return;
        }
        catch( ... ) {
            is->SetStreamPos( 0 );
        }
    }
    if ( asntype == "a" || asntype == "s" ) {
        try {
		    CRef<CBioseq_set> bss( new CBioseq_set );
	        *is >> *bss;
            se->SetSet( bss.GetObject() );
            return;
        }
        catch( ... ) {
//          is->SetStreamPos( 0 );
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
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
    m_hoverlap = false;
    m_gxref = false;
    m_ooverlap = false;

    m_defline_only = false;
    m_suggest = false;

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
        if (NStr::Find (sm, "S") != NPOS) {
            m_fasta = true;
            m_no_scope = true;
        }
        if (NStr::Find (sm, "s") != NPOS) {
            m_fasta = true;
            m_no_scope = false;
        }
        if (NStr::Find (sm, "r") != NPOS) {
            m_nodef = true;
        }
        if (NStr::Find (sm, "D") != NPOS) {
            m_defline_only = true;
            m_no_scope = true;
        }
        if (NStr::Find (sm, "d") != NPOS) {
            m_defline_only = true;
            m_no_scope = false;
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
        if (NStr::Find (fm, "h") != NPOS) {
            m_hoverlap = true;
        }
        if (NStr::Find (fm, "x") != NPOS) {
            m_gxref = true;
        }
        if (NStr::Find (fm, "o") != NPOS) {
            m_ooverlap = true;
        }
        if (NStr::Find (fm, "s") != NPOS) {
            m_suggest = true;
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

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    if ( !objmgr ) {
        /* raise hell */;
    }
    CRef<CScope> scope( new CScope( *objmgr ) );
    if ( !scope ) {
        /* raise hell */;
    }
    scope->AddDefaults();

    // otherwise read ASN.1
    CRef<CSeq_entry> se(new CSeq_entry);
    GetSeqEntry( ip, se );
    if ( ! se ) {
        return 1;
    }

    if ( m_suggest ) {
        int iRet = PlayAroundWithSuggestIntervals( ip, op, *scope, se );
        lastInterval = sw.Elapsed() - lastInterval;
        NcbiCout << "Internal processing time is " << lastInterval << " seconds" << endl;
        return iRet;        
    }

    scope->AddTopLevelSeqEntry(const_cast<const CSeq_entry&>(*se));

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

