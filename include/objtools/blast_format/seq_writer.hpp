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

#ifndef OBJTOOLS_BLAST_FORMAT___CUSTOM_SEQ_WRITER__HPP
#define OBJTOOLS_BLAST_FORMAT___CUSTOM_SEQ_WRITER__HPP

#include <objtools/blast_format/blastdb_seqid.hpp>
#include <objtools/blast_format/blastdb_dataextract.hpp>
#include <objtools/blast_format/seq_writer.hpp>

/** @addtogroup BlastFormatting
 *
 * @{
 */

BEGIN_NCBI_SCOPE

#if 0
/// Subclass of CFastaOstream whose purpose is to override virtual functions in
/// its parent class with no-ops
class CLazyFastaOstream : public CFastaOstream
{
public:
    virtual void Write(const CSeq_entry_Handle& /* handle */,
                       const CSeq_loc* location = 0) {}
    virtual void Write(const CBioseq_Handle& /* handle */,
                       const CSeq_loc* location = 0) {}
    virtual void WriteTitle(const CBioseq_Handle& /* handle */,
                       const CSeq_loc* location = 0) {}
    virtual void WriteSequence(const CBioseq_Handle& /* handle */,
                       const CSeq_loc* location = 0) {}
};

/// Class that overrides CFastaOstream's Write methods to print an integer.
/// This is achived by hijacking the m_Flags field of CFastaOstream
class CIntStream : public CLazyFastaOstream
{
public:
    virtual void Write(const CSeq_entry_Handle& /* handle */,
                       const CSeq_loc* location = 0) { Write(); }
    virtual void Write(const CBioseq_Handle& /* handle */,
                       const CSeq_loc* location = 0) { Write(); }

private:
    /// @note m_Flags must be 'hijacked' and set by calling SetAllFlags
    inline void Write() {
        m_Out << m_Flags;
    }
};

class CCustomFastaOstream : public CFastaOstream
{
public:
    CCustomFastaOstream(const string& specification);

    virtual void Write();
};

/// Class to create a CFastaOstream instance (or a subclass of it). The
/// default implementation of the factory method creates a CFastaOstream,
/// subclasses of this object should create the appropriate CFastaOstream
/// subclasses
/// This is the Creator object in the factory method design pattern
class CCustomFastaOstreamCreator : public CFastaOstreamCreator
{
public:
    CCustomFastaOstreamCreator(CNcbiOstream& out, 
                         CSeqDB& blastdb_handle,
                         const string& specification,
                         // FIXME: use a config object, as this won't scale?
                         size_t line_length = 80, 
                         bool use_ctrl_A = false);

    virtual ~CFastaOstreamCreator() {}

    /// Factory method of this class
    virtual objects::CFastaOstream* Create();

private:
    /// Handle to a BLAST database
    CSeqDB& m_BlastDb;
    /// specification of how to customize the output
    string m_OutputSpec;
};

class CConvertor : public unary_function<const CWritable&, string>
{
public:
    string operator() (const CWritable& datum) const {
        return datum.AsString();
    }
};

#endif

class CSeqFormatter 
{
public:
    /// Constructor
    /// @param fmt_spec format specification [in]
    /// @param blastdb BLAST database from which to retrieve the data [in]
    /// @param out output stream to write the data [in]
    CSeqFormatter(const string& fmt_spec, CSeqDB& blastdb, CNcbiOstream& out);

    /// Destructor
    ~CSeqFormatter();

    /// Write the sequence data associated with the requested ID in the format
    /// specified in the constructor
    /// @param id identifier for a sequence in the BLAST database
    /// @return true if the operation succeeded, false otherwise (e.g.:
    /// sequence not found)
    bool Write(const CBlastDBSeqId& id);

private:
    /// Stream to write output
    CNcbiOstream& m_Out;
    /// The output format specification
    string m_FmtSpec;
    /// The BLAST database from which to extract data
    CSeqDB& m_BlastDb;
    /// Vector of offsets where the replacements will take place
    vector<SIZE_TYPE> m_ReplOffsets;
    /// Vector of convertor objects
    vector<IBlastDBExtract*> m_DataExtractors;


    /// Transform the sequence id into the strings specified by the format
    /// specification
    /// @param id identifier for data to retrieve [in]
    /// @param retval string representations of the datum object [out]
    void x_Transform(const CBlastDBSeqId& id, vector<string>& retval);

    /// Replace format specifiers for the data contained in data2write
    /// @param data2write data to replace in the output string [in]
    string x_Replacer(const vector<string>& data2write) const;

    /// Prohibit copy constructor
    CSeqFormatter(const CSeqFormatter& rhs);
    /// Prohibit assignment operator
    CSeqFormatter& operator=(const CSeqFormatter& rhs);

};

END_NCBI_SCOPE

/* @} */

#endif /* OBJTOOLS_BLAST_FORMAT___CUSTOM_SEQ_WRITER__HPP */

