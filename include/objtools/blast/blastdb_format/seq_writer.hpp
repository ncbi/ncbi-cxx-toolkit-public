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
 * Author: Christiam Camacho
 *
 */

/** @file seq_writer.hpp
 *  Definition of a customizable sequence writer interface
 */

#ifndef OBJTOOLS_BLASTDB_FORMAT___SEQ_WRITER__HPP
#define OBJTOOLS_BLASTDB_FORMAT___SEQ_WRITER__HPP

#include <objtools/blast/blastdb_format/blastdb_seqid.hpp>
#include <objtools/blast/blastdb_format/blastdb_dataextract.hpp>

BEGIN_NCBI_SCOPE

/// Configuration object for CSeqFormatter
struct NCBI_DEPRECATED CSeqFormatterConfig {
    /// Default constructor
    CSeqFormatterConfig() {
        m_LineWidth = 80;
        m_SeqRange = TSeqRange();
        m_Strand = objects::eNa_strand_other;
        m_TargetOnly = false;
        m_UseCtrlA = false;
        m_FiltAlgoId = -1;
        m_FmtAlgoId = -1;
    }

    /// length of the line of output (applicable only to FASTA output)
    TSeqPos m_LineWidth;
    /// The range of the sequence to retrieve, if empty, the
    /// entire sequence will be retrived
    TSeqRange m_SeqRange;
    /// All SeqLoc types will have this strand assigned; If set to 'other', the
    /// strand will be set to 'unknown' for protein sequences and 'both' for
    /// nucleotide
    objects::ENa_strand m_Strand;
    /// Determines whether only the GI indicated as 'target' is retrieved when
    /// multiple GIs are associated with a given OID
    bool m_TargetOnly;
    /// Determines whether Ctrl-A characters should be used as defline
    /// separators
    bool m_UseCtrlA;
    /// Filtering algorithm ID to mask the FASTA
    int m_FiltAlgoId;
    /// Filtering algorithm ID for outfmt %m
    int m_FmtAlgoId;
};

/// Customizable sequence writer interface
class NCBI_BLASTDB_FORMAT_EXPORT NCBI_DEPRECATED CSeqFormatter 
{
public:
    /// Constructor
    /// @param fmt_spec format specification [in]
    /// @param blastdb BLAST database from which to retrieve the data [in]
    /// @param out output stream to write the data [in]
    CSeqFormatter(const string& fmt_spec, CSeqDB& blastdb, CNcbiOstream& out,
                  CSeqFormatterConfig config = CSeqFormatterConfig());

    /// Write the sequence data associated with the requested ID in the format
    /// specified in the constructor
    /// @param id identifier for a sequence in the BLAST database
    /// @throws CException derived classes on error
    void Write(CBlastDBSeqId& id);

    /// Full database FASTA dump
    /// This is an optimized version that does not support range and mask retrieval
    /// @throws CExcpetion derived classes on error
    void DumpAll(CSeqDB& blastdb, CSeqFormatterConfig config = CSeqFormatterConfig());

    /// Set range, strand and filter algo for each seq id
    void SetConfig(TSeqRange range, objects::ENa_strand strand, int filt_algo_id);
private:
    /// Stream to write output
    CNcbiOstream& m_Out;
    /// The output format specification
    string m_FmtSpec;
    /// The BLAST database from which to extract data
    CSeqDB& m_BlastDb;
    /// Vector of offsets where the replacements will take place
    vector<SIZE_TYPE> m_ReplOffsets;
    /// Data extractor
    CBlastDBExtractor m_DataExtractor;
    /// Vector of convertor objects
    vector<char> m_ReplTypes;
    /// Fasta output?
    bool m_Fasta;

    /// Build data for write
    /// @param data2write data to replace in the output string [in]
    void x_Builder(vector<string>& data2write);

    /// Replace format specifiers for the data contained in data2write
    /// @param data2write data to replace in the output string [in]
    string x_Replacer(const vector<string>& data2write) const;

    /// Specifies whether or not data (e.g., Bioseq) is required.
    bool x_RequireData() const;

    /// Prohibit copy constructor
    CSeqFormatter(const CSeqFormatter& rhs);
    /// Prohibit assignment operator
    CSeqFormatter& operator=(const CSeqFormatter& rhs);

};

END_NCBI_SCOPE

#endif /* OBJTOOLS_BLASTDB_FORMAT___SEQ_WRITER__HPP */

