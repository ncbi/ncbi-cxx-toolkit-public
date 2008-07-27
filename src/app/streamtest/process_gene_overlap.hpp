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
    : public CScopedProcess
{
public:
    //  ------------------------------------------------------------------------
    CGeneOverlapProcess()
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out( 0 )
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
        CRef<CSeq_entry>& se )
    //  ------------------------------------------------------------------------
    {
        CScopedProcess::SeqEntryInitialize( se );
    };

    //  ------------------------------------------------------------------------
    void SeqEntryProcess()
    //  ------------------------------------------------------------------------
    {
        try {
            FOR_ALL_FEATURES_WITHIN_SEQENTRY (fit, *m_entry) {
                const CSeq_feat& feat = *fit;
                TestFeatureGeneOverlap( feat );
                ++m_objectcount;
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
        }
    };

protected:
    //  ------------------------------------------------------------------------
    void TestFeatureGeneOverlap (
        const CSeq_feat& f )
    //  ------------------------------------------------------------------------
    {
        if ( f.GetData().Which() == CSeqFeatData::e_Gene ) {
            return;
        }
        ++m_objectcount;
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
};

#endif
