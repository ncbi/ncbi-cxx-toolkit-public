#ifndef OBJTOOLS_READERS___PHRAP__HPP
#define OBJTOOLS_READERS___PHRAP__HPP

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
* Authors:  Aleksey Grichenko, NCBI.
*
* File Description:
*   Reader for Phrap-format files.
*
*/

#include <corelib/ncbiobj.hpp>

// Forward declarations


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;


enum EPhrapReaderFlags {
    fPhrap_NoComplement   = 0x0001, ///< ignore "complemented" flags of traces.
                                    ///< To indicate reversed reads check for
                                    ///< seqdesc.comment = "Complemented"
    fPhrap_PackSeqData    = 0x0002, ///< use best coding to pack sequence data

    fPhrap_FeatGaps       = 0x0004, ///< add features with list of gaps
    fPhrap_FeatBaseSegs   = 0x0008, ///< add features with base segments
    fPhrap_FeatReadLocs   = 0x0010, ///< add padded read starts
    fPhrap_FeatTags       = 0x0020, ///< convert CT and RT tags to features
    fPhrap_FeatQuality    = 0x0040, ///< add quality/alignment features

    fPhrap_Descr          = 0x0080, ///< add descriptors (DS, WA)
    fPhrap_AlignAll       = 0x0100, ///< global all-in-one alignment
    fPhrap_AlignPairs     = 0x0200, ///< separate alignment for each trace
    fPhrap_AlignOptimized = 0x0300, ///< split global alignment into parts
    fPhrap_Align          = 0x0300, ///< mask for alignment flags, not a value
    fPhrap_OldVersion     = 0x1000, ///< force old ACE format
    fPhrap_NewVersion     = 0x2000, ///< force new ACE format
    fPhrap_Version        = fPhrap_OldVersion | fPhrap_NewVersion,
    fPhrap_PadsToFuzz     = 0x4000, ///< Add int-fuzz.p-m to indicate padded
                                    ///< coordinates offset.
    fPhrap_Default = fPhrap_PackSeqData
                   | fPhrap_FeatGaps
                   | fPhrap_FeatBaseSegs
                   | fPhrap_FeatTags
                   | fPhrap_FeatQuality
                   | fPhrap_Descr
                   | fPhrap_AlignOptimized
};

typedef int TPhrapReaderFlags;


NCBI_XOBJREAD_EXPORT
CRef<CSeq_entry> ReadPhrap(CNcbiIstream& in,
                           TPhrapReaderFlags flags = fPhrap_Default);


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_READERS___PHRAP__HPP */
