#ifndef EFETCH__HPP
#define EFETCH__HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   EFetch request
*
*/

#include <corelib/ncbistd.hpp>
#include <objtools/eutils/api/eutils.hpp>
#include <objtools/eutils/uilist/IdList.hpp>


BEGIN_NCBI_SCOPE


/** @addtogroup EUtils
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///  CEFetch_Request
///
///  Base class for all EFetch readers
///


class NCBI_EUTILS_EXPORT CEFetch_Request : public CEUtils_Request
{
public:
    CEFetch_Request(CRef<CEUtils_ConnContext>& ctx);
    virtual ~CEFetch_Request(void);

    /// Get CGI script name (efetch.fcgi).
    virtual string GetScriptName(void) const;

    /// Get CGI script query string.
    virtual string GetQueryString(void) const;

    /// Group of ids to retrieve.
    const CEUtils_IdGroup& GetId(void) const { return m_Id; }
    CEUtils_IdGroup& GetId(void) { Disconnect(); return m_Id; }

    /// Sequential number of the first id retrieved. Default is 0 which
    /// will retrieve the first record.
    int GetRetStart(void) const { return m_RetStart; }
    void SetRetStart(int retstart) { Disconnect(); m_RetStart = retstart; }

    /// Number of items retrieved, default is 20, maximum is 10,000.
    int GetRetMax(void) const { return m_RetMax; }
    void SetRetMax(int retmax) { Disconnect(); m_RetMax = retmax; }

    /// Output format for efetch requests.
    enum ERetMode {
        eRetMode_none = 0,
        eRetMode_xml,     ///< Return data as XML
        eRetMode_html,    ///< Return data as HTML
        eRetMode_text,    ///< Return data as plain text
        eRetMode_asn      ///< Return data as text ASN.1
    };
    /// Output format. The real format can be different from the requested
    /// one depending on the request type and other arguments (e.g. rettype).
    ERetMode GetRetMode(void) const { return m_RetMode; }
    void SetRetMode(ERetMode retmode) { Disconnect(); m_RetMode = retmode; }

    /// Get serial stream format for reading data
    virtual ESerialDataFormat GetSerialDataFormat(void) const;

protected:
    // Fully implemented only for literature and taxonomy databases.
    CRef<uilist::CIdList> x_FetchIdList(int chunk_size);

private:
    typedef CEUtils_Request TParent;

    const char* x_GetRetModeName(void) const;

    CEUtils_IdGroup m_Id;
    int             m_RetStart;
    int             m_RetMax;
    ERetMode        m_RetMode;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CEFetch_Literature_Request
///
///  Literature database request.
///


class NCBI_EUTILS_EXPORT CEFetch_Literature_Request : public CEFetch_Request
{
public:
    /// Literature databases.
    enum ELiteratureDB {
        eDB_pubmed = 0,
        eDB_pmc,
        eDB_journals,
        eDB_omim
    };
    CEFetch_Literature_Request(ELiteratureDB db,
                               CRef<CEUtils_ConnContext>& ctx);

    /// Output types based on database.
    enum ERetType {
        eRetType_none = 0,
        eRetType_uilist,
        eRetType_abstract,
        eRetType_citation,
        eRetType_medline,
        eRetType_full
    };

    /// Output data type.
    ERetType GetRetType(void) const { return m_RetType; }
    void SetRetType(ERetType rettype) { Disconnect(); m_RetType = rettype; }

    /// Get CGI script query string
    virtual string GetQueryString(void) const;

    /// Get IdList using the currently set DB, WebEnv, retstart, retmax etc.
    /// The method changes rettype to uilist and retmode to xml, all other
    /// parameters are unchanged.
    /// Limit number of ids in a single request to chunk_size if it's > 0.
    CRef<uilist::CIdList> FetchIdList(int chunk_size = 10000);

private:
    typedef CEFetch_Request TParent;

    const char* x_GetRetTypeName(void) const;

    ERetType m_RetType;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CEFetch_Sequence_Request
///
///  Sequence database request.
///


class NCBI_EUTILS_EXPORT CEFetch_Sequence_Request : public CEFetch_Request
{
public:
    /// Sequence databases.
    enum ESequenceDB {
        eDB_gene = 0,
        eDB_genome,
        eDB_nucleotide,
        eDB_nuccore,
        eDB_nucest,
        eDB_nucgss,
        eDB_protein,
        eDB_popset,
        eDB_snp,
        eDB_sequences
    };
    CEFetch_Sequence_Request(ESequenceDB db,
                             CRef<CEUtils_ConnContext>& ctx);

    /// Output types based on database.
    enum ERetType {
        eRetType_none = 0,
        eRetType_native,
        eRetType_fasta,
        eRetType_gb,
        eRetType_gbc,
        eRetType_gbwithparts,
        eRetType_est,
        eRetType_gss,
        eRetType_gp,
        eRetType_gpc,
        eRetType_seqid,
        eRetType_acc,
        eRetType_chr,
        eRetType_flt,
        eRetType_rsr,
        eRetType_brief,
        eRetType_docset
    };

    /// Output data type.
    ERetType GetRetType(void) const { return m_RetType; }
    void SetRetType(ERetType rettype) { Disconnect(); m_RetType = rettype; }

    /// Strand of DNA to show.
    enum EStrand {
        eStrand_none  = 0,
        eStrand_plus  = 1,
        eStrand_minus = 2
    };

    /// Strand of DNA to show.
    EStrand GetStrand(void) const { return m_Strand; }
    void SetStrand(EStrand strand) { Disconnect(); m_Strand = strand; }

    /// Show sequence starting from this base number.
    int GetSeqStart(void) const { return m_SeqStart; }
    void SetSeqStart(int pos) { Disconnect(); m_SeqStart = pos; }
    /// Show sequence ending on this base number.
    int GetSeqStop(void) const { return m_SeqStop; }
    void SetSeqStop(int pos) { Disconnect(); m_SeqStop = pos; }

    /// Complexity level
    enum EComplexity {
        eComplexity_none      = -1,
        eComplexity_WholeBlob = 0, ///< Get the whole blob
        eComplexity_Bioseq    = 1, ///< Get bioseq (default in Entrez)
        eComplexity_BioseqSet = 2, ///< Get the minimal bioseq-set
        eComplexity_NucProt   = 3, ///< Get the minimal nuc-prot
        eComplexity_PubSet    = 4  ///< Get the minimal pub-set
    };

    /// Complexity level of the output data.
    EComplexity GetComplexity(void) const { return m_Complexity; }
    void SetComplexity(EComplexity complexity)
        { Disconnect(); m_Complexity = complexity; }

    /// Get CGI script query string
    virtual string GetQueryString(void) const;

private:
    typedef CEFetch_Request TParent;

    const char* x_GetRetTypeName(void) const;

    ERetType    m_RetType;
    EComplexity m_Complexity;
    EStrand     m_Strand;
    int         m_SeqStart;
    int         m_SeqStop;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CEFetch_Taxonomy_Request
///
///  Taxonomy database request.
///


class NCBI_EUTILS_EXPORT CEFetch_Taxonomy_Request : public CEFetch_Request
{
public:
    CEFetch_Taxonomy_Request(CRef<CEUtils_ConnContext>& ctx);

    /// Output data format.
    enum EReport {
        eReport_none    = 0,
        eReport_uilist,
        eReport_brief,
        eReport_docsum,
        eReport_xml
    };

    /// Output data format.
    EReport GetReport(void) const { return m_Report; }
    void SetReport(EReport report) { Disconnect(); m_Report = report; }

    /// Get CGI script query string
    virtual string GetQueryString(void) const;

    /// Get IdList using the currently set DB, WebEnv, retstart, retmax etc.
    /// The method changes rettype to uilist and retmode to xml, all other
    /// parameters are unchanged.
    /// Limit number of ids in a single request to chunk_size if it's > 0.
    CRef<uilist::CIdList> FetchIdList(int chunk_size = 10000);

private:
    typedef CEFetch_Request TParent;

    const char* x_GetReportName(void) const;

    EReport m_Report;
};


/* @} */


END_NCBI_SCOPE

#endif  // EFETCH__HPP
