#ifndef NGALIGN_BLAST_ALIGNER__HPP
#define NGALIGN_BLAST_ALIGNER__HPP

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
 * Authors:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Na_strand.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/sseqloc.hpp>

#include <algo/align/ngalign/ngalign_interface.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_align;
    class CSeq_align_set;
    class CSeq_id;
    class CDense_seg;
END_SCOPE(objects)

BEGIN_SCOPE(blast)
    struct SSeqLoc;
    class CBlastOptionsHandle;
END_SCOPE(blast)




class CBlastArgs
{
public:
    static void s_CreateBlastArgDescriptions(CArgDescriptions& ArgDesc);
    static CRef<blast::CBlastOptionsHandle> s_ExtractBlastArgs(CArgs& Args);
    static CRef<blast::CBlastOptionsHandle> s_CreateBlastOptions(const string& Params);
private:
    static void x_ParseOptionsString(const string& Params, vector<string>& Tokens);
};


class CBlastAligner : public IAlignmentFactory
{
public:
    CBlastAligner(blast::CBlastOptionsHandle& Options, int Threshold)
        : m_BlastOptions(&Options), m_Threshold(Threshold), m_Filter(0) { ; }

    CBlastAligner(const string& Params, int Threshold)
        : m_BlastOptions(CBlastArgs::s_CreateBlastOptions(Params))
        , m_Threshold(Threshold), m_Filter(0) { ; }

    string GetName() const { return "blast_aligner"; }

    TAlignResultsRef GenerateAlignments(objects::CScope& Scope,
                                        ISequenceSet* QuerySet,
                                        ISequenceSet* SubjectSet,
                                        TAlignResultsRef AccumResults);

    void SetSoftFiltering(int Filter) { m_Filter = Filter; }

    typedef CRef<blast::CBlastOptionsHandle> TBlastOptionsRef;
    typedef CRef<CBlastAligner> TBlastAlignerRef;

    static list<TBlastAlignerRef> CreateBlastAligners(list<TBlastOptionsRef>& Options, int Threshold);
    static list<TBlastAlignerRef> CreateBlastAligners(const list<string>& Params, int Threshold);

private:

    CRef<blast::CBlastOptionsHandle> m_BlastOptions;
    int m_Threshold;
    int m_Filter;
};




class CRemoteBlastAligner : public IAlignmentFactory
{
public:
    CRemoteBlastAligner(blast::CBlastOptionsHandle& Options, int Threshold)
        : m_BlastOptions(&Options), m_Threshold(Threshold), m_Filter(0) { ; }

    CRemoteBlastAligner(const string& Params, int Threshold)
        : m_BlastOptions(CBlastArgs::s_CreateBlastOptions(Params))
        , m_Threshold(Threshold), m_Filter(0) { ; }

    string GetName() const { return "remote_blast_aligner"; }

    TAlignResultsRef GenerateAlignments(objects::CScope& Scope,
                                        ISequenceSet* QuerySet,
                                        ISequenceSet* SubjectSet,
                                        TAlignResultsRef AccumResults);


    typedef CRef<blast::CBlastOptionsHandle> TBlastOptionsRef;
    typedef CRef<CRemoteBlastAligner> TBlastAlignerRef;

    static list<TBlastAlignerRef> CreateBlastAligners(list<TBlastOptionsRef>& Options, int Threshold);
    static list<TBlastAlignerRef> CreateBlastAligners(const list<string>& Params, int Threshold);

private:

    CRef<blast::CBlastOptionsHandle> m_BlastOptions;
    int m_Threshold;
    int m_Filter;
};



END_NCBI_SCOPE

#endif
