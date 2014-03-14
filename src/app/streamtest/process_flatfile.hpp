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

#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_generator.hpp>

#ifndef __process_flatfile__hpp__
#define __process_flatfile__hpp__

//  ============================================================================
class CFlatfileProcess
//  ============================================================================
    : public CScopedProcess
{
public:
    //  ------------------------------------------------------------------------
    CFlatfileProcess()
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out( 0 )
    {};

    //  ------------------------------------------------------------------------
    ~CFlatfileProcess()
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
        m_flat_set = args["flat-set"];
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
            CFlatFileConfig cfg (
                CFlatFileConfig::eFormat_GenBank,
                CFlatFileConfig::eMode_Entrez,
                CFlatFileConfig::eStyle_Normal,
                CFlatFileConfig::fShowContigFeatures,
                CFlatFileConfig::fViewNucleotides
            );

            CFlatFileGenerator gen (cfg);

            if (  m_flat_set ) {
                VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY (bit, *m_entry) {
                    const CBioseq& bioseq = *bit;

                    if (bioseq.IsSetInst()) {
                        const CSeq_inst& inst = bioseq.GetInst();
                        if (inst.IsSetMol() && inst.GetMol() == CSeq_inst::eMol_aa) {
                            continue;
                        }
                        if (inst.IsSetRepr() && inst.GetRepr() == CSeq_inst::eRepr_seg) {
                            continue;
                        }
                    }

                    gen.Generate (bioseq, *m_scope, *m_out);

                    /*
                    const CBioseq_Handle bsh = (*m_scope).GetBioseqHandle(bioseq);

                    if ( bsh.GetInst_Mol() == CSeq_inst::eMol_aa ) continue;
                    if ( bsh.GetInst_Repr() == CSeq_inst::eRepr_seg ) continue;

                    int gi = FindGi(bsh.GetBioseqCore()->GetId());
                    CRef<CSeq_loc> loc(new CSeq_loc());
                    CRef<CSeq_id> seqid(new CSeq_id(CSeq_id::e_Gi, gi));
                    loc->SetWhole(*seqid);
                    gen.Generate(*loc, *m_scope, *m_out);
                    */
                }
            } else {
                gen.Generate (m_topseh, *m_out);
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
        }
    };

protected:
    CNcbiOstream* m_out;
    bool m_flat_set;
};

#endif
