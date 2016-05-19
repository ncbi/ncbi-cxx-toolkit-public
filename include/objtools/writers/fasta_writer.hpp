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
 * Authors:  Sergiy Gotvyanskyy, Justin Foley
 *
 * File Description: Extend CFastaOstream to handle features. 
 *                   Write object as a hierarchy of FASTA objects
 *
 */

#ifndef __OBJTOOLS_WRITERS_AGP_WRITE_HPP__
#define __OBJTOOLS_WRITERS_AGP_WRITE_HPP__

#include <corelib/ncbistd.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

class CSeq_entry;
class CBioseq;
class CSeq_loc;
class CBioseq_Handle;
class CSeq_entry_Handle;


class NCBI_XOBJWRITE_EXPORT CFastaOstreamEx : public CFastaOstream
{
public:

    CFastaOstreamEx(CNcbiOstream& out);

    virtual void WriteFeature(const CSeq_feat& feat,
                              CScope& scope);

    virtual void WriteFeatureTitle(const CSeq_feat& feat,
                                   CScope& scope);

    void ResetFeatureCount(void);

protected:

    void x_WriteFeatureAttributes(const CSeq_feat& feat, 
                                  CScope& scope);

    void x_AddGeneAttributes(const CSeq_feat& feat,
                             CScope& scope,
                             string& defline);

    void x_AddProteinNameAttribute(const CSeq_feat& feat,
                                   CScope& scope,
                                   string& defline);

    void x_AddDbxrefAttribute(const CSeq_feat& feat,
                              CScope& scope,
                              string& defline);

    void x_AddReadingFrameAttribute(const CSeq_feat& feat, 
                                    string& defline);

    void x_AddncRNAClassAttribute(const CSeq_feat& feat,
                                  string& defline);

    void x_AddPseudoAttribute(const CSeq_feat& feat,
                              CScope& scope,
                              string& defline);

    void x_AddPseudoGeneAttribute(const CSeq_feat& feat,
                                  CScope& scope,
                                  string& defline);

    void x_AddRNAProductAttribute(const CSeq_feat& feat,
                                  string& defline);

    void x_AddPartialAttribute(const CSeq_feat& feat, 
                               CScope& scope,
                               string& defline);

    void x_AddExceptionAttribute(const CSeq_feat& feat, 
                                 string& defline);

    void x_AddProteinIdAttribute(const CSeq_feat& feat,
                                 CScope& scope,
                                 string& defline);
    
    void x_AddTranslationExceptionAttribute(const CSeq_feat& feat,
                                            string& defline);

    void x_AddLocationAttribute(const CSeq_feat& feat,
                                CScope& scope,
                                string& defline);

    void x_AddDeflineAttribute(const string& label,
                               const string& value,
                               string& defline);

    void x_AddDeflineAttribute(const string& label,
                               bool value,
                               string& defline);

    string x_GetCDSIdString(const CSeq_feat& cds,
                            CScope& scope);

    string x_GetRNAIdString(const CSeq_feat& rna,
                            CScope& scope);

    string x_GetProtIdString(const CSeq_feat& prot,
                             CScope& scope);

    string x_GetGeneIdString(const CSeq_feat& gene,
                             CScope& scope);


    TSeqPos m_FeatCount;
};


// this would generate CDS and genes into own files
// up to three files created: 
// filename_without_ext.fsa for nucleotide sequences
// filename_without_ext_cds_from_genomic.fna for CDS's
// filename_without_ext_rna_from_genomic.fna for RNA's
class /*NCBI_XOBJUTIL_EXPORT*/ CFastaOstreamComp
{
public:
    CFastaOstreamComp(const string& dir, const string& filename_without_ext);
    ~CFastaOstreamComp();

    enum E_FileSection
    {
        eFS_nucleotide,
        eFS_CDS,
        eFS_RNA
    };
    typedef int TFlags; ///< binary OR of CFastaOstream::EFlags

    void Write(const CSeq_entry_Handle& handle, const CSeq_loc* location = 0);

    inline
    void SetFlags(TFlags flags)
    {
        m_Flags = flags;
    }

    inline
    TFlags GetFlags() const
    {
        return m_Flags;
    }
protected:
    struct TStreams
    {
        TStreams() : m_ostream(0), m_fasta_stream(0)
        {
        }
        string         m_filename;
        CNcbiOstream*  m_ostream;
        CFastaOstream* m_fasta_stream;
    };

    void x_Write(const CBioseq_Handle& handle, const CSeq_loc* location);
    // override these method to change filename policy and output stream
    virtual void x_GetNewFilename(string& filename, E_FileSection sel);
    virtual CNcbiOstream* x_GetOutputStream(const string& filename, E_FileSection sel);
    virtual CFastaOstream* x_GetFastaOstream(CNcbiOstream& ostr, E_FileSection sel);

    TStreams& x_GetStream(E_FileSection sel);
protected:
    string m_dir;
    string m_filename_without_ext;
    TFlags m_Flags;
    vector<TStreams> m_streams;
};

END_SCOPE(objects)
END_NCBI_SCOPE


#endif // end of "include-guard"
