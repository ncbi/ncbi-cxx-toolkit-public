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

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/** @addtogroup BlastFormatting
 *
 * @{
 */

/// Interface to extract data from a BLAST database given an identifier
class IBlastDBExtract {
public:
    /// Our mandatory destructor
    virtual ~IBlastDBExtract() {}

    /// Main method of this interface
    /// @param id sequence identifier [in]
    /// @param blastdb source from which to extract the data [in]
    virtual string Extract(const CBlastDBSeqId& id, CSeqDB& blastdb) = 0;
};

/// Extracts the GI for a given sequence id
/// (CSeqFormatter associates this with %g)
class CGiExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(const CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the PIG for a given sequence id
/// (CSeqFormatter associates this with %P)
class CPigExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(const CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the OID for a given sequence id
/// (CSeqFormatter associates this with %o)
class COidExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(const CBlastDBSeqId& id, CSeqDB& blastdb);

    /// Extracts the OID and returns it as an integer (as opposed to a string)
    /// so that other *Extractor classes can use this.
    /// @param id sequence identifier [in]
    /// @param blastdb source from which to extract the data [in]
    int ExtractOID(const CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the sequence length for a given sequence id
/// (CSeqFormatter associates this with %l)
class CSeqLenExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(const CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the taxonomy ID for a given sequence id
/// (CSeqFormatter associates this with %T)
class CTaxIdExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(const CBlastDBSeqId& id, CSeqDB& blastdb);

    /// Extracts the TaxID and returns it as an integer (as opposed to a string)
    /// so that other *Extractor classes can use this.
    /// @param id sequence identifier [in]
    /// @param blastdb source from which to extract the data [in]
    int ExtractTaxID(const CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the sequence data for a given sequence id in IUPAC{AA,NA} format
/// (CSeqFormatter associates this with %s)
class CSeqDataExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(const CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the sequence title for a given sequence id
/// (CSeqFormatter associates this with %t)
class CTitleExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(const CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the accession for a given sequence id
/// (CSeqFormatter associates this with %a)
class CAccessionExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(const CBlastDBSeqId& id, CSeqDB& blastdb);
};

/// Extracts the common taxonomy name for a given sequence id
/// (CSeqFormatter associates this with %L)
class CLineageExtractor : public IBlastDBExtract {
public:
    /** @inheritDoc */
    virtual string Extract(const CBlastDBSeqId& id, CSeqDB& blastdb);
};

END_NCBI_SCOPE

/* @} */

#endif /* OBJTOOLS_BLAST_FORMAT___BLASTDB_DATAEXTRACT__HPP */
