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
*
* File Description:
*
* ===========================================================================
*/

#ifndef __process_gene_overlap__hpp__
#define __process_gene_overlap__hpp__

//  ============================================================================
class CGeneOverlapProcess
//  ============================================================================
    : public CSeqEntryProcess
{
public:
    //  ------------------------------------------------------------------------
    CGeneOverlapProcess()
    //  ------------------------------------------------------------------------
        : m_out( 0 )
    {};

    //  ------------------------------------------------------------------------
    ~CGeneOverlapProcess()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    void ProcessInitialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        m_out = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
    };

    //  ------------------------------------------------------------------------
    void ProcessFinalize()
    //  ------------------------------------------------------------------------
    {
    }

    //  ------------------------------------------------------------------------
    virtual void SeqEntryInitialize(
        const CRef<CSeq_entry>& se )
    //  ------------------------------------------------------------------------
    {
        m_objmgr = CObjectManager::GetInstance();
        if ( !m_objmgr ) {
            /* raise hell */;
        }
        m_scope.Reset( new CScope( *m_objmgr ) );
        if ( !m_scope ) {
            /* raise hell */;
        }
        m_scope->AddDefaults();
        m_scope->AddTopLevelSeqEntry( const_cast< const CSeq_entry& >( *se ) );
    };

    //  ------------------------------------------------------------------------
    void SeqEntryProcess(
        const CRef<CSeq_entry>& se )
    //  ------------------------------------------------------------------------
    {
        switch (se->Which()) {

        case CSeq_entry::e_Seq:
            if ( se->GetSeq().IsSetAnnot() ) {
                ITERATE (CBioseq::TAnnot, it, se->GetSeq().GetAnnot()) {
                    TestFeatureGeneOverlap( **it );
                }
            }
            break;

        case CSeq_entry::e_Set: {
            const CBioseq_set& bss( se->GetSet() );
            if (bss.IsSetAnnot()) {
                ITERATE (CBioseq::TAnnot, it, bss.GetAnnot()) {
                    TestFeatureGeneOverlap( **it );
                }
            }

            if (bss.IsSetSeq_set()) {
                ITERATE (CBioseq_set::TSeq_set, it, bss.GetSeq_set()) {
                    CBioseq_set::TSeq_set::const_iterator it2 = it;
                        SeqEntryProcess( *it );
                }
            }
            break;
        }

        case CSeq_entry::e_not_set:
        default:
            break;
        }
    };

    //  ------------------------------------------------------------------------
    virtual void SeqEntryFinalize(
        const CRef<CSeq_entry>& se )
    //  ------------------------------------------------------------------------
    {
        m_scope.Release();
        m_objmgr.Release();
    };

protected:
    //  ------------------------------------------------------------------------
    void TestFeatureGeneOverlap (
    //  ------------------------------------------------------------------------
        const CSeq_feat& f )
    {
        if ( f.GetData().Which() == CSeqFeatData::e_Gene ) {
            return;
        }
        const CSeq_feat_Base::TLocation& locbase = f.GetLocation();
        CConstRef<CSeq_feat> ol = sequence::GetOverlappingGene( 
            locbase, *m_scope );
        if ( ! ol ) {
            return;
        }
        *m_out << SeqLocString( locbase ) << " -> " 
               << SeqLocString( ol->GetLocation() ) << endl;
    }

    //  ------------------------------------------------------------------------
    void TestFeatureGeneOverlap (
    //  ------------------------------------------------------------------------
        const CSeq_annot& sa )
    {
        if ( sa.IsSetData()  &&  sa.GetData().IsFtable() ) {
            ITERATE( CSeq_annot::TData::TFtable, it, sa.GetData().GetFtable() ) {
                TestFeatureGeneOverlap( **it );
            }
        }
    }

    //  ------------------------------------------------------------------------
    string SeqLocString( const CSeq_loc& loc )
    //  ------------------------------------------------------------------------
    {
        string str;
        loc.GetLabel(&str);
        return str;
    }

    //  ------------------------------------------------------------------------
    //  Data:
    //  ------------------------------------------------------------------------
protected:
    CNcbiOstream* m_out;
    CRef<CObjectManager> m_objmgr;
    CRef<CScope> m_scope;
};

#endif
