#ifndef OBJECTS_FLAT___FLAT_HEAD__HPP
#define OBJECTS_FLAT___FLAT_HEAD__HPP

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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   New (early 2003) flat-file generator -- representation of "header"
*   data, which translates into a format-dependent sequence of paragraphs.
*
*/

#include <objects/flat/flat_formatter.hpp>

#include <objects/seq/Seqdesc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// Ultimately split into several paragraphs in a format-dependent manner
// (e.g., LOCUS, DEFINITION, ACCESSION, [PID,] VERSION, DBSOURCE for GenPept)
struct SFlatHead : public IFlatItem
{
public:
    // also fills in some fields of ctx, which we don't bother duplicating
    SFlatHead(SFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatHead(*this); }

    typedef CSeq_inst::TStrand   TStrand;
    typedef CSeq_inst::TTopology TTopology;

    string              m_Locus;
    TStrand             m_Strandedness;
    TTopology           m_Topology;
    string              m_Division; // XXX -- varies with format
    CConstRef<CDate>    m_UpdateDate;
    CConstRef<CDate>    m_CreateDate;
    string              m_Definition;
    CBioseq::TId        m_OtherIDs; // current
    list<string>        m_SecondaryIDs; // predecessors
    list<string>        m_DBSource; // for proteins; relatively free-form
    CConstRef<CSeqdesc> m_ProteinBlock;

    CRef<SFlatContext> m_Context;

    const char*          GetMolString(void) const;

private:
    void x_AddDate(const CDate& date);

    void x_AddDBSource(void);
    // helpers for various components
    string x_FormatDBSourceID(const CSeq_id& id);
    void x_AddPIRBlock(void);
    void x_AddSPBlock(void);
    void x_AddPRFBlock(void);
    void x_AddPDBBlock(void);
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_HEAD__HPP */
