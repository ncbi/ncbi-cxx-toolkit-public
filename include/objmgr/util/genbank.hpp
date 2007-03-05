#ifndef GENBANK__HPP
#define GENBANK__HPP

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
* Author:  Aaron Ucko
*
* File Description:
*   Code to write Genbank/Genpept flat-file records.
*/

#include <objtools/flat/flat_ncbi_formatter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CGenbankWriter
{
public:
    enum EFormat {
        eFormat_Genbank,
        eFormat_Genpept
    };
    enum EVersion {
        // Genbank 127.0, to be released December 2001, will have a new
        // format for the LOCUS line, with more space for most fields.
        eVersion_pre127,
        eVersion_127 = 127
    };
    
    CGenbankWriter(CNcbiOstream& stream, CScope& scope,
                   EFormat format = eFormat_Genbank,
                   EVersion version = eVersion_127) :
        m_Formatter(*new CFlatTextOStream(stream), scope,
                    IFlatFormatter::eMode_Dump),
        m_Format(format)
        { }

    bool Write(const CSeq_entry& entry);

private:
    CFlatNCBIFormatter m_Formatter;
    EFormat            m_Format;
};


bool CGenbankWriter::Write(const CSeq_entry& entry)
{
    m_Formatter.Format
        (entry, m_Formatter,
         m_Format == eFormat_Genpept ? IFlatFormatter::fSkipNucleotides
         : IFlatFormatter::fSkipProteins);
    return true;
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* GENBANK__HPP */
