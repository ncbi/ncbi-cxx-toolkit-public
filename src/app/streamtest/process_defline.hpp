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

#include <objmgr/util/create_defline.hpp>

#ifndef __process_defline__hpp__
#define __process_defline__hpp__

//  ============================================================================
class CDeflineProcess
//  ============================================================================
    : public CScopedProcess
{
public:
    //  ------------------------------------------------------------------------
    CDeflineProcess()
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out (0)
        , m_flags (0)
        , m_skip_virtual (false)
        , m_skip_segmented (false)
        , m_do_indexed (false)
        , m_gpipe_mode (false)
    {};

    //  ------------------------------------------------------------------------
    CDeflineProcess(bool use_indexing)
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out (0)
        , m_flags (0)
        , m_skip_virtual (false)
        , m_skip_segmented (false)
        , m_do_indexed (use_indexing)
        , m_gpipe_mode (false)
    {};

    //  ------------------------------------------------------------------------
    CDeflineProcess(bool use_indexing, bool gpipe_mode)
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out (0)
        , m_flags (0)
        , m_skip_virtual (false)
        , m_skip_segmented (false)
        , m_do_indexed (use_indexing)
        , m_gpipe_mode (gpipe_mode)
    {};

    //  ------------------------------------------------------------------------
    ~CDeflineProcess()
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

        string options = args["options"].AsString();
        if ( options == "ignore_existing" ) {
            m_flags |= CDeflineGenerator::fIgnoreExisting;
        }
        if (m_gpipe_mode) {
            m_flags |= CDeflineGenerator::fIgnoreExisting;
            m_flags |= CDeflineGenerator::fGpipeMode;
        }

        string skip = args["skip"].AsString();
        if ( skip == "virtual" ) {
            m_skip_virtual = true;
        }
        if ( skip == "segmented" ) {
            m_skip_segmented = true;
        }
        if ( skip == "both" ) {
            m_skip_virtual = true;
            m_skip_segmented = true;
        }

        m_debug = args["debug"];

        m_timer = CStopWatch();
        m_timer.Start();
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
    void x_FastaSeqIdWrite(const CBioseq& bioseq)
    //  ------------------------------------------------------------------------
    {
        string gi_string;
        string accn_string;

        FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, bioseq) {
            const CSeq_id& sid = **sid_itr;
            TSEQID_CHOICE chs = sid.Which();
            switch (chs) {
                case NCBI_SEQID(Gi):
                {
                    const string str = sid.AsFastaString();
                    gi_string = str;
                    break;
                }
                default:
                    break;
            }
        }

        FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, bioseq) {
            const CSeq_id& sid = **sid_itr;
            TSEQID_CHOICE chs = sid.Which();
            switch (chs) {
                case NCBI_SEQID(Other):
                case NCBI_SEQID(Genbank):
                case NCBI_SEQID(Embl):
                case NCBI_SEQID(Ddbj):
                case NCBI_SEQID(Tpg):
                case NCBI_SEQID(Tpe):
                case NCBI_SEQID(Tpd):
                {
                    const string str = sid.AsFastaString();
                    accn_string = str;
                    break;
                }
                 default:
                   break;
            }
        }

        if (gi_string.empty() || accn_string.empty()) {
            CSeq_id::WriteAsFasta (*m_out, bioseq);
        } else {
            *m_out << gi_string << "|" << accn_string;
        }
    }

    //  ------------------------------------------------------------------------
    void IndexedProcess()
    //  ------------------------------------------------------------------------
    {
        try {
            CDeflineGenerator gen (m_topseh);

            VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY (bit, *m_entry) {
                const CBioseq& bioseq = *bit;

                bool okay = true;
                if (m_skip_virtual) {
                    if (bioseq.IsSetInst()) {
                        const CSeq_inst& inst = bioseq.GetInst();
                        if (inst.IsSetRepr()) {
                            TSEQ_REPR repr = inst.GetRepr();
                            if (repr == CSeq_inst::eRepr_virtual) {
                                okay = false;
                            }
                        }
                    }
                }
                if (m_skip_segmented) {
                    CSeq_entry* se;
                    se = bioseq.GetParentEntry();
                    if (se) {
                        se = se->GetParentEntry();
                        if (se) {
                            if (se->IsSet()) {
                                const CBioseq_set& seqset = se->GetSet();
                                if (seqset.IsSetClass()) {
                                    CBioseq_set::EClass mclass = seqset.GetClass();
                                    if (mclass == CBioseq_set::eClass_segset ||
                                        mclass == CBioseq_set::eClass_parts) {
                                        okay = false;
                                    }
                                }
                            }
                        }
                    }
                }
 
                if (okay) {
                    const string& title = gen.GenerateDefline (bioseq, *m_scope, m_flags);

                    size_t total, resident, shared;
                    if ( m_debug && GetMemoryUsage ( &total, &resident, &shared) ) {
                        *m_out << "Tot " << total << ", Res " << resident;
                        *m_out << ", Shr " << shared << ", Sec " << m_timer.Restart() << " - ";
                    }
                    *m_out << ">";
                    x_FastaSeqIdWrite (bioseq);
                    *m_out << " ";
                    *m_out << title << endl;
                    ++m_objectcount;
                }
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
        }
    };

    //  ------------------------------------------------------------------------
    void UnindexedProcess()
    //  ------------------------------------------------------------------------
    {
        try {
            CDeflineGenerator gen;

            VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY (bit, *m_entry) {
                const CBioseq& bioseq = *bit;

                bool okay = true;
                if (m_skip_virtual) {
                    if (bioseq.IsSetInst()) {
                        const CSeq_inst& inst = bioseq.GetInst();
                        if (inst.IsSetRepr()) {
                            TSEQ_REPR repr = inst.GetRepr();
                            if (repr == CSeq_inst::eRepr_virtual) {
                                okay = false;
                            }
                        }
                    }
                }
                if (m_skip_segmented) {
                    CSeq_entry* se;
                    se = bioseq.GetParentEntry();
                    if (se) {
                        se = se->GetParentEntry();
                        if (se) {
                            if (se->IsSet()) {
                                const CBioseq_set& seqset = se->GetSet();
                                if (seqset.IsSetClass()) {
                                    CBioseq_set::EClass mclass = seqset.GetClass();
                                    if (mclass == CBioseq_set::eClass_segset ||
                                        mclass == CBioseq_set::eClass_parts) {
                                        okay = false;
                                    }
                                }
                            }
                        }
                    }
                }
 
                if (okay) {
                    const string& title = gen.GenerateDefline (bioseq, *m_scope, m_flags);

                    *m_out << ">";
                    x_FastaSeqIdWrite (bioseq);
                    *m_out << " ";
                    *m_out << title << endl;
                    ++m_objectcount;
                }
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
        }
    };

    //  ------------------------------------------------------------------------
    void SeqEntryProcess()
    //  ------------------------------------------------------------------------
    {
        if (m_do_indexed) {
            IndexedProcess ();
        } else {
            UnindexedProcess ();
        }
    };

protected:
    CNcbiOstream* m_out;
    CDeflineGenerator::TUserFlags m_flags;
    bool m_skip_virtual;
    bool m_skip_segmented;
    bool m_do_indexed;
    bool m_gpipe_mode;
    bool m_debug;
    CStopWatch m_timer;
};

#endif
