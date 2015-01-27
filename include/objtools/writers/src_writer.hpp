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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write source quailiers
 *
 */

#ifndef OBJTOOLS_WRITERS___SRC_WRITER__HPP
#define OBJTOOLS_WRITERS___SRC_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqtable/Seq_table.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CSrcError:
    public CLineError
//  ============================================================================
{   
protected: 
    CSrcError(const CLineError& other):CLineError(other){};
    CSrcError(
        ncbi::EDiagSev severity,
        const std::string&);

public:
    static CSrcError* Create(
        ncbi::EDiagSev severity,
        const std::string&);
};

/**
  * Used to generate tables showing qualifier-field entries occuring in the 
  * BioSources of instances of Bioseq and Seq-entry. 
  */
//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CSrcWriter:
    public CObject
//  ============================================================================
{    
public:
    typedef map<string, int> COLUMNMAP;
    typedef map<string, string> NAMEMAP;
    typedef list<string> NAMELIST;
    typedef vector<string> FIELDS;
    typedef bool (CSrcWriter::*HANDLER)(const CBioSource&, const string&, IMessageListener*);
    typedef map<string, CSrcWriter::HANDLER> HANDLERMAP;

public:
    CSrcWriter(
            unsigned int flags=0) :
        mFlags(flags),
        mDelimiter("\t") {
        xInit();
    };

    virtual ~CSrcWriter()
    {};

    /** Write a table of the specified qualifier-field entries 
      * found in the BioSource of a given Bioseq. 
      */
    virtual bool WriteBioseqHandle( 
        CBioseq_Handle,
        const FIELDS&,
        CNcbiOstream&);

    /** Write a table of the specified qualifier-field entries 
      * found in the BioSources of a vector of Bioseqs. 
      */
    virtual bool WriteBioseqHandles( 
        const vector<CBioseq_Handle>&,
        const FIELDS&,
        CNcbiOstream&,
        IMessageListener* = 0);

    /// Set the column delimiter for the output table.
    void SetDelimiter(
        const string& delimiter) {
        mDelimiter = delimiter;
    };

    /// Verify that each string in fields is a valid qualifier name.
    static bool ValidateFields(
        const FIELDS& fields,
        IMessageListener* = 0);

    /** Write a table of all qualifier-field entries occurring
      * in the BioSources for a given Seq-entry,
      * with columns appearing in a canonical order. 
      */
    virtual bool WriteSeqEntry(
        const CSeq_entry&,
        CScope&,
        CNcbiOstream&);
    
  
protected:
    void xInit();

    virtual bool xGather(CBioseq_Handle, const FIELDS&, IMessageListener* =0);
    virtual bool xGatherId(CBioseq_Handle, IMessageListener* =0); 
    virtual bool xGatherGi(CBioseq_Handle, IMessageListener* =0);
    virtual bool xGatherLocalId(CBioseq_Handle, IMessageListener* = 0);
    virtual bool xGatherDefline(CBioseq_Handle, IMessageListener* =0);
    virtual bool xHandleSourceField(const CBioSource&, const string&, IMessageListener* =0);


    virtual bool xGatherTaxname(const CBioSource&, const string&, IMessageListener* =0);
    virtual bool xGatherDivision(const CBioSource&, const string&, IMessageListener* =0);
    virtual bool xGatherGenome(const CBioSource&, const string&,  IMessageListener* =0);
    virtual bool xGatherOrigin(const CBioSource&, const string&, IMessageListener* =0);
    virtual bool xGatherSubtypeFeat(const CBioSource&, const string&, IMessageListener* =0);
    virtual bool xGatherOrgModFeat(const CBioSource&, const string&, IMessageListener* =0);
    virtual bool xGatherOrgCommon(const CBioSource&, const string&, IMessageListener* =0);
    virtual bool xGatherOrgnameLineage(const CBioSource&, const string&, IMessageListener* =0);
    virtual bool xGatherPcrPrimers(const CBioSource&, const string&, IMessageListener* =0);
    virtual bool xGatherDb(const CBioSource&, const string&, IMessageListener* =0);
    virtual bool xGatherTaxonId(const CBioSource&, const string&, IMessageListener* =0);

    virtual bool xFormatTabDelimited(const FIELDS&, CNcbiOstream&);
     
    static FIELDS xGetOrderedFieldNames(const FIELDS&);
    static HANDLER xGetHandler(const string&);
    static string xPrimerSetNames(const CPCRPrimerSet&);
    static string xPrimerSetSequences(const CPCRPrimerSet&);
    static bool xIsSubsourceTypeSuppressed(CSubSource::TSubtype);
    static bool xIsOrgmodTypeSuppressed(COrgMod::TSubtype);
    static NAMELIST xGetOrgModSubtypeNames();
    static NAMELIST xGetSubSourceSubtypeNames();
    static string xCompressFieldName(const string&);
    static FIELDS xProcessFieldNames(const FIELDS&);
 

    void xPrepareTableColumn(const string&, const string&, const string& ="");
    void xAppendColumnValue(const string&, const string&);
    bool xValueNeedsQuoting(const string&);
    string xDequotedValue(const string&);
    string xGetColStub(const string&);
    


public:
    static const FIELDS sDefaultSrcCheckFields; ///< Default fields processed by srcchk application, in their canonical order
    static const FIELDS sAllSrcCheckFields; ///< All possible fields processed by srchck application, in their canonical order

protected:
    static const FIELDS sDefaultSeqEntryFields;
    static const FIELDS sAllSeqEntryFields;
    static HANDLERMAP sHandlerMap;
    static NAMEMAP sFieldnameToColname;
    CRef<CSeq_table> mSrcTable;
    COLUMNMAP mColnameToIndex;
    unsigned int mFlags;
    string mDelimiter;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___SRC_WRITER__HPP
