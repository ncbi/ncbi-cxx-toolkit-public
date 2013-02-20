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

/** @file blastdb_formatter.hpp
 *  Definition of a customizable BLAST DB information formatter interface
 */

#ifndef OBJTOOLS_BLASTDB_FORMAT___BLASTDB_FMTR__HPP
#define OBJTOOLS_BLASTDB_FORMAT___BLASTDB_FMTR__HPP

#include <objtools/blast/blastdb_format/blastdb_seqid.hpp>
#include <objtools/blast/blastdb_format/blastdb_dataextract.hpp>

BEGIN_NCBI_SCOPE

/// Customizable BLAST DB information formatter interface
class NCBI_BLASTDB_FORMAT_EXPORT CBlastDbFormatter
{
public:
    /// Constructor
    /// @param fmt_spec Output format specification, supports the flags
    /// specified in the blastdbcmd -list_outfmt command line option [in]
    CBlastDbFormatter(const string& fmt_spec);

    /// Extracts the BLAST database information for the requested BLAST DB
    /// according to the output format specification requested in the
    /// constructor
    /// @param db_init_info object defining the BLAST DB initialization
    /// information [in]
    string Write(const SSeqDBInitInfo& db_init_info);

private:
    /// The output format specification
    string m_FmtSpec;
    /// Vector of offsets where the replacements will take place
    vector<SIZE_TYPE> m_ReplOffsets;
    // Vector of replacement types, records what should be replaced in the
    // output format specifier
    vector<char> m_ReplacementTypes;    

    /// Replace format specifiers for the data contained in data2write
    /// @param data2write data to replace in the output string [in]
    string x_Replacer(const vector<string>& data2write) const;

    /// Prohibit copy constructor
    CBlastDbFormatter(const CBlastDbFormatter& rhs);
    /// Prohibit assignment operator
    CBlastDbFormatter& operator=(const CBlastDbFormatter& rhs);
};

END_NCBI_SCOPE

#endif /* OBJTOOLS_BLASTDB_FORMAT___BLASTDB_FMTR__HPP */

