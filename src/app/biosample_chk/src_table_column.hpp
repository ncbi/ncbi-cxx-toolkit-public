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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   check biosource and structured comment descriptors against biosample database
 *
 */

#ifndef BIOSAMPLE_CHK__SRC_TABLE_COLUMN__HPP
#define BIOSAMPLE_CHK__SRC_TABLE_COLUMN__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>

#include <objmgr/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

const string kOrganismName = "Organism Name";
const string kTaxId = "Tax ID";

class CSrcTableColumnBase : public CObject
{
public:
    // default is to do no command.  Subclasses will almost always override this
    virtual void AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue ) { }
    virtual void ClearInBioSource(
        objects::CBioSource & in_out_bioSource ) { }
    virtual string GetFromBioSource(
        const objects::CBioSource & in_out_bioSource ) const { return ""; }
    virtual string GetLabel() const { return ""; }
};

class CSrcTableOrganismNameColumn : public CSrcTableColumnBase
{
public:
    virtual void AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue );
    virtual void ClearInBioSource(
        objects::CBioSource & in_out_bioSource );
    virtual string GetFromBioSource(
        const objects::CBioSource & in_out_bioSource ) const;
    virtual string GetLabel() const { return kOrganismName; }
};


class CSrcTableTaxonIdColumn : public CSrcTableColumnBase
{
public:
    virtual void AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue );
    virtual void ClearInBioSource(
        objects::CBioSource & in_out_bioSource );
    virtual string GetFromBioSource(
        const objects::CBioSource & in_out_bioSource ) const;
    virtual string GetLabel() const { return kTaxId; }
};


class CSrcTableGenomeColumn : public CSrcTableColumnBase
{
public:
    virtual void AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue );
    virtual void ClearInBioSource(
        objects::CBioSource & in_out_bioSource );
    virtual string GetFromBioSource(
        const objects::CBioSource & in_out_bioSource ) const;
    virtual string GetLabel() const { return "Organism Name"; }
};

class CSrcTableSubSourceColumn : public CSrcTableColumnBase
{
public:
  CSrcTableSubSourceColumn(objects::CSubSource::TSubtype subtype) { m_Subtype = subtype; };
    virtual void AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue );
    virtual void ClearInBioSource(
        objects::CBioSource & in_out_bioSource );
    virtual string GetFromBioSource(
        const objects::CBioSource & in_out_bioSource ) const;
    virtual string GetLabel() const;
private:
    objects::CSubSource::TSubtype m_Subtype;
};

class CSrcTableOrgModColumn : public CSrcTableColumnBase
{
public:
  CSrcTableOrgModColumn(objects::COrgMod::TSubtype subtype) { m_Subtype = subtype; };
    virtual void AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue );
    virtual void ClearInBioSource(
        objects::CBioSource & in_out_bioSource );
    virtual string GetFromBioSource(
        const objects::CBioSource & in_out_bioSource ) const;
    virtual string GetLabel() const;
private:
    objects::COrgMod::TSubtype m_Subtype;
};


class CSrcTableFwdPrimerSeqColumn : public CSrcTableColumnBase
{
public:
    CSrcTableFwdPrimerSeqColumn() { };
    virtual void AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue );
    virtual void ClearInBioSource(
        objects::CBioSource & in_out_bioSource );
    virtual string GetFromBioSource(
        const objects::CBioSource & in_out_bioSource ) const;
    virtual string GetLabel() const { return "fwd-primer-seq"; }
};


class CSrcTableRevPrimerSeqColumn : public CSrcTableColumnBase
{
public:
    CSrcTableRevPrimerSeqColumn() { };
    virtual void AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue );
    virtual void ClearInBioSource(
        objects::CBioSource & in_out_bioSource );
    virtual string GetFromBioSource(
        const objects::CBioSource & in_out_bioSource ) const;
    virtual string GetLabel() const { return "rev-primer-seq"; }
};


class CSrcTableFwdPrimerNameColumn : public CSrcTableColumnBase
{
public:
    CSrcTableFwdPrimerNameColumn() { };
    virtual void AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue );
    virtual void ClearInBioSource(
        objects::CBioSource & in_out_bioSource );
    virtual string GetFromBioSource(
        const objects::CBioSource & in_out_bioSource ) const;
    virtual string GetLabel() const { return "fwd-primer-name"; }
};


class CSrcTableRevPrimerNameColumn : public CSrcTableColumnBase
{
public:
    CSrcTableRevPrimerNameColumn() { };
    virtual void AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue );
    virtual void ClearInBioSource(
        objects::CBioSource & in_out_bioSource );
    virtual string GetFromBioSource(
        const objects::CBioSource & in_out_bioSource ) const;
    virtual string GetLabel() const { return "rev-primer-name"; }
};


class CSrcTableColumnBaseFactory
{
public:
    static CRef<CSrcTableColumnBase> Create(string sTitle);
};


typedef vector< CRef<CSrcTableColumnBase> > TSrcTableColumnList;

TSrcTableColumnList GetSourceFields(const CBioSource& src);


END_SCOPE(objects)
END_NCBI_SCOPE

#endif //SRC_TABLE_COLUMN
