/*
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

#ifndef __process_invert__hpp__
#define __process_invert__hpp__

//  ============================================================================
class CInvertProcess
//  ============================================================================
    : public CScopedProcess
{
public:
    //  ------------------------------------------------------------------------
    CInvertProcess()
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out( 0 )
    {};

    //  ------------------------------------------------------------------------
    ~CInvertProcess()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    void ProcessInitialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CScopedProcess::ProcessInitialize( args );

        m_out = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
    };

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
            EXPLORE_ALL_BIOSEQS_WITHIN_SEQENTRY(bsp_itr, *m_entry) {
                CBioseq& bioseq = *bsp_itr;
                CBioseq_Handle bsh = m_scope->GetBioseqHandle(bioseq);
                if (!bsh) continue;
                SAnnotSelector sel;
                for (CFeat_CI feat_ci(bsh, sel); feat_ci;  ++feat_ci) {
                    const CSeq_feat& const_feat = feat_ci->GetOriginalFeature();
                    CRef<CSeq_feat> feat(new CSeq_feat);
                    feat->Assign(const_feat);
                    bool unordered = false;
                    CSeq_loc_CI prev;
                    for (CSeq_loc_CI curr(feat->SetLocation()); curr; ++curr) {
                        if ( prev  &&  curr /* &&
                             m_scope->IsSameBioseq(curr.GetSeq_id(), prev.GetSeq_id(), CScope::eGetBioseq_Loaded) */ ) {
                            CSeq_loc_CI::TRange prev_range = prev.GetRange();
                            CSeq_loc_CI::TRange curr_range = curr.GetRange();
                            if ( curr.GetStrand() == eNa_strand_minus ) {
                                if (prev_range.GetTo() < curr_range.GetTo()) {
                                    unordered = true;
                                }
                            } else {
                                if (prev_range.GetTo() > curr_range.GetTo()) {
                                    unordered = true;
                                }
                            }
                        }
                        prev = curr;
                    }
                    if (unordered) {
                        /*
                        feat->SetPseudo(true);
                        */
                        CSeq_loc& location = feat->SetLocation();
                        if (location.Which() == CSeq_loc::e_Mix) {
                            CRef<CSeq_loc> rev_loc( new CSeq_loc );
                            CSeq_loc_mix& mix = rev_loc->SetMix();

                            ITERATE (CSeq_loc_mix::Tdata, it, location.GetMix().Get()) {
                                mix.Set().push_front(*it);
                            }

                            feat->SetLocation(*rev_loc);
                            CSeq_feat_EditHandle(feat_ci->GetSeq_feat_Handle()).Replace(*feat);
                        }
                    }
                }
            }

            *m_out << MSerial_AsnText << *m_entry << endl;
        }
        catch (CException& e) {
            ERR_POST(Error << "error processing seqentry: " << e.what());
        }
    };

protected:
    CNcbiOstream* m_out;
};

#endif
