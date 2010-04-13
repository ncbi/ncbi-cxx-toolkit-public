#ifndef OLIGOFAR_CSAM__HPP
#define OLIGOFAR_CSAM__HPP

#include "defs.hpp"

BEGIN_OLIGOFAR_SCOPES

class CSamBase 
{
    public:
        enum ESamColumns {
            eCol_qname = 0,
            eCol_flags = 1,
            eCol_rname = 2,
            eCol_pos   = 3,
            eCol_mapq  = 4,
            eCol_cigar = 5,
            eCol_mrnm  = 6,
            eCol_mpos  = 7,
            eCol_isize = 8,
            eCol_seq   = 9,
            eCol_qual  = 10,
            eCol_tags  = 11
        };
        enum FSamFlags {
            fPairedQuery   = 0x0001,
            fPairedHit     = 0x0002,
            fSeqUnmapped   = 0x0004,
            fMateUnmapped  = 0x0008,
            fSeqReverse    = 0x0010,
            fMateReverse   = 0x0020,
            fSeqIsFirst    = 0x0040,
            fSeqIsSecond   = 0x0080,
            fHitSuboptimal = 0x0100,
            fReadFailedQC  = 0x0200,
            fReadDuplicate = 0x0400
        };
        enum ESamTagType {
            eTag_int = 'i',
            eTag_char = 'A',
            eTag_float = 'f',
            eTag_string = 'Z',
            eTag_hex = 'H',
            eTag_none = 0
        };
};


END_OLIGOFAR_SCOPES

#endif
