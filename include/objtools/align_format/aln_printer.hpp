#ifndef OBJTOOLS_ALIGN_FORMAT___ALN_PRINTER_HPP
#define OBJTOOLS_ALIGN_FORMAT___ALN_PRINTER_HPP

/* $Id:
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

File name: aln_printer.hpp

Author: Greg Boratyn

Contents: Printer for standard multiple sequence alignment formats

******************************************************************************/

#include <objects/seqalign/Seq_align.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <objmgr/scope.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(align_format)
USING_SCOPE(objects);

/// Printer for popular multiple alignmnet formats
///
class NCBI_ALIGN_FORMAT_EXPORT CMultiAlnPrinter
{
public:

    /// Alignment display type for showing nucleotice or protein-related
    /// information
    enum EAlignType {
        eNotSet = 0,
        eNucleotide,
        eProtein
    };

    /// Multiple alignmnet text formats
    enum EFormat {
        eFastaPlusGaps = 0,
        eClustal,
        ePhylipSequential,
        ePhylipInterleaved,
        eNexus,
        ePhylip = ePhylipInterleaved
    };

public:

    /// Constructor
    /// @param seqalign Alignment
    /// @param scope Scope
    ///
    CMultiAlnPrinter(const CSeq_align& seqalign, CScope& scope,
                     EAlignType type = eNotSet);

    /// Set text width (number of columns) for alignment output
    /// @param width Width
    ///
    void SetWidth(int width) {m_Width = width;}

    /// Set format for printing alignment
    /// @param format Format
    ///
    void SetFormat(EFormat format) {m_Format = format;}

    /// Set gap character
    /// @param gap Gap character
    ///
    void SetGapChar(unsigned char gap) {m_AlnVec->SetGapChar(gap);}

    /// Set end gap character
    /// @param End gap character
    ///
    void SetEndGapChar(unsigned char gap) {m_AlnVec->SetEndChar(gap);}

    /// Print alignment
    /// @param ostr Output stream
    ///
    void Print(CNcbiOstream& ostr);


protected:
    /// Forbid copy constructor
    CMultiAlnPrinter(const CMultiAlnPrinter& p);

    /// Forbid assignment operator
    CMultiAlnPrinter& operator=(const CMultiAlnPrinter& p);

    /// Print alignment in fasta + gaps format
    /// @param ostr Output stream
    void x_PrintFastaPlusGaps(CNcbiOstream& ostr);

    /// Print alignment in ClustalW format
    /// @param ostr Output stream
    void x_PrintClustal(CNcbiOstream& ostr);

    /// Print alignment in Phylip format with sequetial sequences
    /// @param ostr Output stream
    void x_PrintPhylipSequential(CNcbiOstream& ostr);

    /// Print alignment in Phylip format with interleaved sequences
    /// @param ostr Output stream
    void x_PrintPhylipInterleaved(CNcbiOstream& ostr);

    /// Print alignment in Nexus format
    /// @param ostr Output stream
    void x_PrintNexus(CNcbiOstream& ostr);


protected:
    /// Alignment manager
    CRef<CAlnVec> m_AlnVec;

    /// Alignment type
    EAlignType m_AlignType;

    /// Selected alignment format
    EFormat m_Format;

    /// Selected width of the text field
    int m_Width;
};

END_SCOPE(align_format)

END_NCBI_SCOPE

#endif /* OBJTOOLS_ALIGN_FORMAT___ALN_PRINTER_HPP */
