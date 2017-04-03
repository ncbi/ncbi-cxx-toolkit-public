#ifndef SRA__READER__BAM__BAM2SAM__HPP
#define SRA__READER__BAM__BAM2SAM__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Interface to SAMTOOLS library.
 *
 */

#include <corelib/ncbistd.hpp>
#include <util/range.hpp>
#include <bam.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(samtools)

struct SBamAlignment
{
    int ref_seq_index;
    ncbi::CRange<TSeqPos> ref_range;
    ncbi::CRange<TSeqPos> short_range;
    int flags;
    int quality;
    string short_seq_id;
    vector<Uint4> CIGAR;
    string short_sequence;

    bool operator<(const SBamAlignment& a) const;
    bool operator==(const SBamAlignment& a) const;
    bool operator!=(const SBamAlignment& a) const
        {
            return !(*this == a);
        }
};
ostream& operator<<(ostream& out, const SBamAlignment& a);

class CBamFile
{
public:
    enum EIndexType {
        eWithIndex,
        eWithoutIndex
    };
    
    CBamFile(void);
    explicit
    CBamFile(const string& path,
             EIndexType index_type = eWithIndex);
    ~CBamFile(void);

    void Open(const string& path,
              EIndexType index_type = eWithIndex);
    void Close(void);
    
    typedef int TRefSeqIndex;
    typedef unsigned TSeqPos;

    TRefSeqIndex GetRefSeqIndex(const string& id);
    
    size_t Count(TRefSeqIndex, TSeqPos from, TSeqPos to,
                 size_t limit_count = 0);
    size_t Count(const string& id, TSeqPos from, TSeqPos to,
                 size_t limit_count = 0)
        {
            return Count(GetRefSeqIndex(id), from, to, limit_count);
        }

    typedef vector<SBamAlignment> TAlignments;

    void Fetch(TRefSeqIndex, TSeqPos from, TSeqPos to,
               TAlignments& alns, size_t limit_count = 0);
    void Fetch(const string& id, TSeqPos from, TSeqPos to,
               TAlignments& alns, size_t limit_count = 0) 
        {
            Fetch(GetRefSeqIndex(id), from, to, alns, limit_count);
        }

private:
    bamFile         m_File;
    bam_index_t*    m_Index;
    bam_header_t*   m_Header;

private:
    CBamFile(const CBamFile&);
    void operator=(const CBamFile&);
};

END_SCOPE(samtools)
END_NCBI_SCOPE

#endif // SRA__READER__BAM__BAM2SAM__HPP
