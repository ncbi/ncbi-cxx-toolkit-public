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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/09/25 20:12:04  ucko
* More cleanups from Denis.
* Put utility code in the objects namespace.
* Moved utility code to {src,include}/objects/util (to become libxobjutil).
* Moved static members of CGenbankWriter to above their first use.
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/serialbase.hpp>
#include <objects/objmgr/objmgr.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// forward declarations
class CBioSource;
class CBioseq;
class CDescList; // internal
class CFeat_id;
class COrg_ref;
class CSeq_descr;
class CSeq_entry;
class CSeq_feat;
class CSeq_loc;

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
                   EVersion version = eVersion_pre127) :
        m_Stream(stream), m_Scope(scope), m_Format(format), m_Version(version)
        { SetParameters(); }

    bool Write(CSeq_entry& entry); // returns true on success

private:
    // useful constants
    static const unsigned int sm_KeywordWidth;
    static const unsigned int sm_LineWidth;
    static const unsigned int sm_DataWidth;
    static const unsigned int sm_FeatureNameIndent;
    static const unsigned int sm_FeatureNameWidth;

    void SetParameters(void); // sets formatting parameters (below)

    // The descriptions in descs are in decreasing order of specificity.
    bool Write          (const CSeq_entry& entry, const CDescList& descs);

    bool WriteLocus     (const CBioseq&    seq,   const CDescList& descs);
    bool WriteDefinition(const CBioseq&    seq,   const CDescList& descs);
    bool WriteAccession (const CBioseq&    seq,   const CDescList& descs);
    bool WriteVersion   (const CBioseq&    seq,   const CDescList& descs);
    bool WriteID        (const CBioseq&    seq,   const CDescList& descs);
    bool WriteKeywords  (const CBioseq&    seq,   const CDescList& descs);
    bool WriteSegment   (const CBioseq&    seq,   const CDescList& descs);
    bool WriteSource    (const CBioseq&    seq,   const CDescList& descs);
    bool WriteReference (const CBioseq&    seq,   const CDescList& descs);
    bool WriteComment   (const CBioseq&    seq,   const CDescList& descs);
    bool WriteFeatures  (const CBioseq&    seq,   const CDescList& descs);
    bool WriteSequence  (const CBioseq&    seq,   const CDescList& descs);

    void Wrap(const string& keyword, const string& contents,
              unsigned int indent = sm_KeywordWidth);
    void WriteFeatureLocation(const string& name, const CSeq_loc& location,
                              const CBioseq& default_seq);
    void WriteFeatureLocation(const string& name, const string& location);
    void WriteFeatureQualifier(const string& qual);
    void WriteFeatureQualifier(const string& name, const string& value,
                               bool quote);
    void WriteProteinQualifiers(const CProt_ref& prot);
    void WriteSourceQualifiers(const COrg_ref& org);
    void WriteSourceQualifiers(const CBioSource& source);

    void FormatIDPrefix(const CSeq_id& id, const CBioseq& default_seq,
                        CNcbiOstream& dest);
    void FormatFeatureInterval(const CSeq_interval& interval,
                               const CBioseq& default_seq, CNcbiOstream& dest);
    void FormatFeatureLocation(const CSeq_loc& location,
                               const CBioseq& default_seq, CNcbiOstream& dest);
    const CSeq_feat& FindFeature(const CFeat_id& id,
                                 const CBioseq& default_seq);

    CNcbiOstream&  m_Stream;
    CScope&        m_Scope;
    EFormat        m_Format;
    EVersion       m_Version;
    
    // formatting parameters; dependent on format and version
    unsigned int  m_LocusNameWidth;
    unsigned int  m_SequenceLenWidth;
    unsigned int  m_MoleculeTypeWidth;
    unsigned int  m_TopologyWidth;
    unsigned int  m_DivisionWidth;
    unsigned int  m_GenpeptBlanks;
    string        m_LengthUnit; // bases or residues
};


END_SCOPE(objects)
END_NCBI_SCOPE


#endif  /* GENBANK__HPP */
