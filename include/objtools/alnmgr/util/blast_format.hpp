/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_format.hpp

Author: Ilya Dondoshansky

Contents: Functions needed for formatting of BLAST results

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_FORMAT__
#define __BLAST_FORMAT__

#include <corelib/ncbistd.hpp>
#include <objtools/alnmgr/util/showalign.hpp>

#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_seqsrc.h>

USING_NCBI_SCOPE;
USING_SCOPE(blast);

typedef list<objects::CDisplaySeqalign::SeqlocInfo*> TSeqLocInfo; 
typedef vector<TSeqLocInfo> TSeqLocInfoVector;

#define BLAST_NUM_DESCRIPTIONS 500
#define BLAST_NUM_ALIGNMENTS 250
#define BLAST_ALIGN_VIEW 0

/** Options for formatting BLAST results 
 */
class NCBI_XBLAST_EXPORT CBlastFormatOptions : public CObject
{
public:

    /// Constructor
    CBlastFormatOptions(EProgram program, CNcbiOstream &ostr) THROWS((CBlastException));
    /// Destructor
    virtual ~CBlastFormatOptions();

    /// Accessors/Mutators for individual options
    int GetDescriptions() const;
    void SetDescriptions(int d);
    int GetAlignments() const;
    void SetAlignments(int a);
    bool GetHtml() const;
    void SetHtml(bool h);
    int GetAlignView() const;
    void SetAlignView(int a);
    CNcbiOstream* GetOstream() const;

protected:
   Uint1 m_align_view;
   Uint4 m_align_options;
   Uint4 m_print_options;
   bool m_believe_query;
   bool m_html;
   CNcbiOstream *m_ostr;
   Int4 m_descriptions;
   Int4 m_alignments;
   Boolean m_ungapped; /**< Should this be here????? */

private:    
    /// Prohibit copy c-tor 
    CBlastFormatOptions(const CBlastFormatOptions& bfo);
    /// Prohibit assignment operator
    CBlastFormatOptions& operator=(const CBlastFormatOptions& bfo);
};

inline int CBlastFormatOptions::GetDescriptions() const
{
    return m_descriptions;
}

inline void CBlastFormatOptions::SetDescriptions(int d)
{
    m_descriptions = d;
}

inline int CBlastFormatOptions::GetAlignments() const
{
    return m_alignments;
}

inline void CBlastFormatOptions::SetAlignments(int a)
{
    m_alignments = a;
}

inline bool CBlastFormatOptions::GetHtml() const
{
    return m_html;
}

inline void CBlastFormatOptions::SetHtml(bool h)
{
    m_html = h;
}

inline int CBlastFormatOptions::GetAlignView() const
{
    return m_align_view;
}

inline void CBlastFormatOptions::SetAlignView(int a)
{
    m_align_view = a;
}

inline CNcbiOstream* CBlastFormatOptions::GetOstream() const
{
    return m_ostr;
}

int
BLAST_FormatResults(TSeqAlignVector &seqalign, 
    EProgram program, TSeqLocVector &query,
    TSeqLocInfoVector& maskv, const CBlastFormatOptions* format_options, 
    bool is_ooframe);

#endif /* !__BLAST_FORMAT__ */

