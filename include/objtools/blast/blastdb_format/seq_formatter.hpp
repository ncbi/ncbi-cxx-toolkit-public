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
 * Author: Amelia Fong
 *
 */

/** @file seq_formatter.hpp
 *  Definition of a blastdb formatter interfaces
 */

#ifndef OBJTOOLS_BLASTDB_FORMAT___SEQ_FORMATTER__HPP
#define OBJTOOLS_BLASTDB_FORMAT___SEQ_FORMATTER__HPP

#include <objtools/blast/blastdb_format/blastdb_seqid.hpp>
#include <objtools/blast/blastdb_format/blastdb_dataextract.hpp>

BEGIN_NCBI_SCOPE

/// Configuration object for CBlastDB_Formatter classes
struct CBlastDB_FormatterConfig {
    /// Default constructor
    CBlastDB_FormatterConfig() {
        m_SeqRange = TSeqRange();
        m_Strand = objects::eNa_strand_other;
        m_UseCtrlA = false;
        m_FiltAlgoId = -1;
        m_FmtAlgoId = -1;
    }


    /// The range of the sequence to retrieve, if empty, the
    /// entire sequence will be retrived
    TSeqRange m_SeqRange;
    /// All SeqLoc types will have this strand assigned; If set to 'other', the
    /// strand will be set to 'unknown' for protein sequences and 'both' for
    /// nucleotide
    objects::ENa_strand m_Strand;
    /// Determines whether Ctrl-A characters should be used as defline
    /// separators
    bool m_UseCtrlA;
    /// Filtering algorithm ID to mask the FASTA
    int m_FiltAlgoId;
    /// Filtering algorithm ID for outfmt %m
    int m_FmtAlgoId;
};

class NCBI_BLASTDB_FORMAT_EXPORT CBlastDB_Formatter
{
public:
    CBlastDB_Formatter() {}

    virtual int Write(CSeqDB::TOID oid, const CBlastDB_FormatterConfig & config, string target_id = kEmptyStr) = 0;
    virtual void DumpAll(const CBlastDB_FormatterConfig & config) = 0;
    virtual ~CBlastDB_Formatter() {}

private:
    /// Prohibit copy constructor
    CBlastDB_Formatter(const CBlastDB_Formatter& rhs);
    /// Prohibit assignment operator
    CBlastDB_Formatter& operator=(const CBlastDB_Formatter& rhs);

};

/// Customizable sequence formatter interface
class NCBI_BLASTDB_FORMAT_EXPORT CBlastDB_SeqFormatter : public CBlastDB_Formatter
{
public:
    /// Constructor
    /// @param fmt_spec format specification [in]
    /// @param blastdb BLAST database from which to retrieve the data [in]
    /// @param out output stream to write the data [in]
    CBlastDB_SeqFormatter(const string& fmt_spec, CSeqDB& blastdb, CNcbiOstream& out);

    int Write(CSeqDB::TOID oid, const CBlastDB_FormatterConfig & config, string target_id = kEmptyStr);
    void DumpAll(const CBlastDB_FormatterConfig & config);

private:
    /// Fields not in defline
    enum EOtherFields {
    	e_seq = 0,
    	e_mask,
    	e_hash,
    	e_max_other_fields
    };

    /// Stream to write output
    CNcbiOstream& m_Out;
    /// The output format specification
    string m_FmtSpec;
    /// The BLAST database from which to extract data
    CSeqDB& m_BlastDb;
    /// Vector of offsets where the replacements will take place
    vector<string> m_Seperators;
    /// Vector of convertor objects
    vector<char> m_ReplTypes;
    bool m_GetDefline;
    CBlastDeflineUtil::BlastDeflineFields m_DeflineFields;
    /// Bit Mask for other fields
    unsigned int m_OtherFields;
    bool m_UseLongSeqIds;
    /// Build data for write
    void x_Print(CSeqDB::TOID oid, vector<string> & defline_data, vector<string> & other_fields);

    void x_DataRequired();

    void x_GetSeq(CSeqDB::TOID oid, const CBlastDB_FormatterConfig & config, string & seq);
    string x_GetSeqHash(CSeqDB::TOID oid);
    string x_GetSeqMask(CSeqDB::TOID oid, int algo_id);

    /// Prohibit copy constructor
    CBlastDB_SeqFormatter(const CBlastDB_SeqFormatter& rhs);
    /// Prohibit assignment operator
    CBlastDB_SeqFormatter& operator=(const CBlastDB_SeqFormatter& rhs);

};

/// Fasta formatter  interface
class NCBI_BLASTDB_FORMAT_EXPORT CBlastDB_FastaFormatter : public CBlastDB_Formatter
{
public:
    /// Constructor
    /// @param blastdb BLAST database from which to retrieve the data [in]
    /// @param out output stream to write the data [in]
    /// @param width fasta line width [in]
    CBlastDB_FastaFormatter(CSeqDB& blastdb, CNcbiOstream& out, TSeqPos width = 80);

    void DumpAll(const CBlastDB_FormatterConfig  & config);

    /// Retun 0 if Sucess otherwise -1
    int Write(CSeqDB::TOID oid, const CBlastDB_FormatterConfig & config, string target_id = kEmptyStr);

private:

    /// The BLAST database from which to extract data
    CSeqDB& m_BlastDb;
    CNcbiOstream& m_Out;
    CFastaOstream m_fasta;
    bool m_UseLongSeqIds;

    /// Prohibit copy constructor
    CBlastDB_FastaFormatter(const CBlastDB_FastaFormatter& rhs);
    /// Prohibit assignment operator
    CBlastDB_FastaFormatter& operator=(const CBlastDB_FastaFormatter& rhs);
};

class NCBI_BLASTDB_FORMAT_EXPORT CBlastDB_BioseqFormatter : public CBlastDB_Formatter
{
public:
    /// Constructor
    /// @param blastdb BLAST database from which to retrieve the data [in]
    /// @param out output stream to write the data [in]
    CBlastDB_BioseqFormatter(CSeqDB& blastdb, CNcbiOstream& out);

    void DumpAll(const CBlastDB_FormatterConfig  & config);

    /// Retun 0 if Sucess otherwise -1
    int Write(CSeqDB::TOID oid, const CBlastDB_FormatterConfig & config, string target_id = kEmptyStr);

private:

    /// The BLAST database from which to extract data
    CSeqDB& m_BlastDb;
    CNcbiOstream& m_Out;

    /// Prohibit copy constructor
    CBlastDB_BioseqFormatter(const CBlastDB_BioseqFormatter& rhs);
    /// Prohibit assignment operator
    CBlastDB_BioseqFormatter& operator=(const CBlastDB_BioseqFormatter& rhs);

};

END_NCBI_SCOPE

#endif /* OBJTOOLS_BLASTDB_FORMAT___SEQ_FORMATTER__HPP */

