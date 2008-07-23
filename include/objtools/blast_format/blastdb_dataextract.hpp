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

/** @file blastdb_dataextract.hpp
 *  Declares classes which extract data from a BLAST database
 */

#ifndef OBJTOOLS_BLAST_FORMAT___BLASTDB_DATAEXTRACT__HPP
#define OBJTOOLS_BLAST_FORMAT___BLASTDB_DATAEXTRACT__HPP

#include <objtools/blast_format/blastdb_seqid.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>
#include <objmgr/util/sequence.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/** @addtogroup BlastFormatting
 *
 * @{
 */

/// Interface to extract data from a BLAST database given an identifier
class NCBI_XBLASTFORMAT_EXPORT IBlastDBExtract {
public:
    /// Our mandatory destructor
    virtual ~IBlastDBExtract() {}

    /// Main method of this interface
    /// @param id sequence identifier [in]
    /// @param blastdb source from which to extract the data [in]
    virtual string Extract(CBlastDBSeqId& id, CSeqDB& blastdb) = 0;
};

/// Extracts the GI for a given sequence id
/// (CSeqFormatter associates this with %g)
class NCBI_XBLASTFORMAT_EXPORT CGiExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the PIG for a given sequence id
/// (CSeqFormatter associates this with %P)
class NCBI_XBLASTFORMAT_EXPORT CPigExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the OID for a given sequence id
/// (CSeqFormatter associates this with %o)
class NCBI_XBLASTFORMAT_EXPORT COidExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(CBlastDBSeqId& id, CSeqDB& blastdb);

    /// Extracts the OID and returns it as an integer (as opposed to a string)
    /// so that other *Extractor classes can use this.
    /// @param id sequence identifier [in]
    /// @param blastdb source from which to extract the data [in]
    int ExtractOID(CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the sequence length for a given sequence id
/// (CSeqFormatter associates this with %l)
class NCBI_XBLASTFORMAT_EXPORT CSeqLenExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(CBlastDBSeqId& id, CSeqDB& blastdb);

    /// Extracts the sequence length and returns it as an integer (as opposed
    /// to a string)
    /// so that other *Extractor classes can use this.
    /// @param id sequence identifier [in]
    /// @param blastdb source from which to extract the data [in]
    TSeqPos ExtractLength(CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the taxonomy ID for a given sequence id
/// (CSeqFormatter associates this with %T)
class NCBI_XBLASTFORMAT_EXPORT CTaxIdExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(CBlastDBSeqId& id, CSeqDB& blastdb);

    /// Extracts the TaxID and returns it as an integer (as opposed to a string)
    /// so that other *Extractor classes can use this.
    /// @param id sequence identifier [in]
    /// @param blastdb source from which to extract the data [in]
    int ExtractTaxID(CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the sequence data for a given sequence id in IUPAC{AA,NA} format
/// (CSeqFormatter associates this with %s)
class NCBI_XBLASTFORMAT_EXPORT CSeqDataExtractor : public IBlastDBExtract {
public:
    /// Constructor
    /// @param range sequence range to extract, if empty, gets the entire
    /// sequence [in]
    /// @param strand All SeqLoc types will have this strand assigned;
    ///             If set to 'other', the strand will be set to 'unknown'
    ///             for protein sequences and 'both' for nucleotide [in]
    CSeqDataExtractor(TSeqRange range = TSeqRange(),
                      objects::ENa_strand strand = objects::eNa_strand_both) 
    : m_SeqRange(range), m_Strand(strand) {}
    /** @inheritDoc */
    virtual string Extract(CBlastDBSeqId& id, CSeqDB& blastdb);

protected:
    /// The range of the sequence requested
    TSeqRange m_SeqRange;
    /// The strand of the sequence requested
    objects::ENa_strand m_Strand;
};

/// Extracts the FASTA for a given sequence id
/// (CSeqFormatter associates this with %f)
class NCBI_XBLASTFORMAT_EXPORT CFastaExtractor : public CSeqDataExtractor {
public:
    /// Constructor
    /// @param line_width length of the line of output [in]
    /// @param range sequence range to extract, if empty, gets the entire
    /// sequence [in]
    /// @param strand All SeqLoc types will have this strand assigned;
    ///             If set to 'other', the strand will be set to 'unknown'
    ///             for protein sequences and 'both' for nucleotide [in]
    CFastaExtractor(TSeqPos line_width, TSeqRange range = TSeqRange(),
                    objects::ENa_strand strand = objects::eNa_strand_other,
                    bool target_only = false,
                    bool ctrl_a = false);
    /** @inheritDoc */
    virtual string Extract(CBlastDBSeqId& id, CSeqDB& blastdb);
private:
    /// The stream to write the output to (which is then converted to a string)
    stringstream m_OutputStream;
    /// Object which prints the sequence in FASTA format
    CFastaOstream m_FastaOstream;
    bool m_ShowTargetOnly;
    bool m_UseCtrlA;
};

/// Extracts the sequence title for a given sequence id
/// (CSeqFormatter associates this with %t)
class NCBI_XBLASTFORMAT_EXPORT CTitleExtractor : public IBlastDBExtract {
public:
    CTitleExtractor(bool target_only = false) : m_ShowTargetOnly(target_only) {}
    /** @inheritDoc */
    virtual string Extract(CBlastDBSeqId& id, CSeqDB& blastdb);
private:
    bool m_ShowTargetOnly;
};

/// Extracts the accession for a given sequence id
/// (CSeqFormatter associates this with %a)
class NCBI_XBLASTFORMAT_EXPORT CAccessionExtractor : public IBlastDBExtract {
public:
    CAccessionExtractor(bool target_only = false)
        : m_ShowTargetOnly(target_only) {}
    /** @inheritDoc */
    virtual string Extract(CBlastDBSeqId& id, CSeqDB& blastdb);
private:
    bool m_ShowTargetOnly;
};

/// Extracts the common taxonomy name for a given sequence id
/// (CSeqFormatter associates this with %L)
class NCBI_XBLASTFORMAT_EXPORT CCommonTaxonomicNameExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the scientific name for a given sequence id
/// (CSeqFormatter associates this with %S)
class NCBI_XBLASTFORMAT_EXPORT CScientificNameExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(CBlastDBSeqId& id, CSeqDB& blastdb);
};

END_NCBI_SCOPE

/* @} */

#endif /* OBJTOOLS_BLAST_FORMAT___BLASTDB_DATAEXTRACT__HPP */
