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

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/api/blast_types.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

typedef list<objects::CDisplaySeqalign::SeqlocInfo*> TSeqLocInfo; 
typedef vector<TSeqLocInfo> TSeqLocInfoVector;

#define BLAST_NUM_DESCRIPTIONS 500
#define BLAST_NUM_ALIGNMENTS 250
#define BLAST_ALIGN_VIEW 0

/** Formatting flags (copy of those in txalign.h */
#define ALIGN_LOCUS_NAME	((Uint4)0x00000100)	//<display the locus name
#define ALIGN_MASTER			((Uint4)0x00000002)	//<display the alignment as multiple pairwise alignment
#define ALIGN_MISMATCH		((Uint4)0x00000004)	//<display the mismatched residue of the sequence
#define ALIGN_MATRIX_VAL	((Uint4)0x00000008)	//<display the matrix of the alignment
#define ALIGN_HTML			((Uint4)0x00000010)	//<display the format in a HTML page
#define ALIGN_HTML_RELATIVE	((Uint4)0x00002000)	//<the HTML (if enabled by ALIGN_HTML) should be relative
#define ALIGN_SHOW_RULER	((Uint4)0x00000020)	//<display the ruler for the text alignment
#define ALIGN_COMPRESS		((Uint4)0x00000040)	//<make the space for label smaller
#define ALIGN_END_NUM		((Uint4)0x00000080)	//<show the number at the end
#define ALIGN_FLAT_INS		((Uint4)0x00000001)	//<flat the insertions in multiple pairwise alignment
#define ALIGN_SHOW_GI		((Uint4)0x00000200)	//<show the gi in the defline.
#define ALIGN_SHOW_NO_OF_SEGS ((Uint4)0x00000400)//<show the number of (sum statistics) segments in the one-line descriptions?
#define ALIGN_BLASTX_SPECIAL  ((Uint4)0x00000800)	//<display the BLASTX results as protein alignment
#define	ALIGN_SHOW_QS		((Uint4)0x00001000)	//<show the results as query-subect
#define ALIGN_SPLIT_ANNOT	((Uint4)0x00004000)	//<for Seq-annot from the same alignment, split the the display into individual panel
#define ALIGN_SHOW_STRAND	((Uint4)0x00008000)	//<for displaying the stradn even in the compact form
#define ALIGN_BLUNT_END		((Uint4)0x00010000)	//<showing the blunt-end for the end gaps
#define ALIGN_DO_NOT_PRINT_TITLE	((Uint4)0x00020000)	//< do not print title before list of deflines
#define ALIGN_CHECK_BOX		((Uint4)0x00040000)	//< place checkbox before the line (HTML only)
#define ALIGN_CHECK_BOX_CHECKED	((Uint4)0x00080000)	//< make default value for checkboxes ON (HTML only)
#define ALIGN_NEW_GIF		((Uint4)0x00100000)	//< print new.gif near new alignments (HTML only)
#define ALIGN_NO_ENTREZ		((Uint4)0x00200000)	//< Use dumpgnl syntax instead of ENTREZ.
#define ALIGN_NO_DUMPGNL	((Uint4)0x00400000)	//< No dumpgnl output, even if GNL.
#define ALIGN_TARGET_IN_LINKS	((Uint4)0x00800000)	//< Put TARGET in Entrez links
#define ALIGN_SHOW_LINKOUT ((Uint4)0x01000000)	//<print linkout info
#define ALIGN_BL2SEQ_LINK	((Uint4) 0x02000000)	//< Add link to Blast 2 Sequences
#define ALIGN_GET_SEQUENCE ((Uint4)0x04000000)	//<get sequence ability

/** Options for formatting BLAST results 
 */
class NCBI_XALNUTIL_EXPORT CBlastFormatOptions : public CObject
{
public:

    /// Constructor
    CBlastFormatOptions(EProgram program, CNcbiOstream &ostr);
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
    EProgram program, const TSeqLocVector &query,
    TSeqLocInfoVector& maskv, const CBlastFormatOptions* format_options, 
    bool is_ooframe);

END_SCOPE(blast)

END_NCBI_SCOPE

#endif /* !__BLAST_FORMAT__ */

