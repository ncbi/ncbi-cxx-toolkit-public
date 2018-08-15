/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#ifndef WGS_UTILS_H
#define WGS_UTILS_H

#include <string>

#include <corelib/ncbistre.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/submit/Contact_info.hpp>

#include "wgs_enums.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace wgsparse
{

typedef void (CSeq_id::*TSetSeqIdFunc)(CTextseq_id&);
TSetSeqIdFunc FindSetTextSeqIdFunc(CSeq_id::E_Choice choice);

template <typename T> string ToStringLeadZeroes(T num, size_t width)
{
    string str_num = to_string(num);
    size_t zeroes = width > str_num.size() ? width - str_num.size() : 0;

    return string(zeroes, '0') + str_num;
}

size_t GetMaxAccessionLen(int accession_num);

bool GetInputType(const std::string& str, EInputType& type);
bool GetInputTypeFromFile(CNcbiIfstream& stream, EInputType& type);

string GetLocalOrGeneralIdStr(const CSeq_id& id);
bool IsLocalOrGeneralId(const CSeq_id& id);
bool HasTextAccession(const CSeq_id& id);

string GetSeqIdKey(const CBioseq& bioseq);
string GetSeqIdStr(const CBioseq& bioseq);
string GetSeqIdAccession(const CBioseq& bioseq);
string GetSeqIdLocalOrGeneral(const CBioseq& bioseq);

bool GetDescr(const CSeq_entry& entry, const CSeq_descr* &descrs);
bool GetNonConstDescr(CSeq_entry& entry, CSeq_descr* &descrs);
bool GetAnnot(const CSeq_entry& entry, const CBioseq::TAnnot* &annot);
bool GetNonConstAnnot(CSeq_entry& entry, CBioseq::TAnnot* &annot);

CSeqdesc* GetSeqdescr(CSeq_entry& entry, CSeqdesc::E_Choice type);

bool HasLineage(const string& lineage_str, const string& lineage);

CRef<CSeq_submit> GetSeqSubmit(CNcbiIfstream& in, EInputType type);
string GetSeqSubmitTypeName(EInputType type);
string GetIdStr(const CObject_id& obj_id);

bool IsUserObjectOfType(const CSeqdesc& descr, const string& type);
string ToString(const CSerialObject& obj);
string ToStringKey(const CSerialObject& obj);

string::size_type GetLastSlashPos(const string& str);
bool NeedToProcessId(const CSeq_id& id);

const CCit_sub* GetCitSub(const CPubdesc& pub);
CCit_sub* GetNonConstCitSub(CPubdesc& pub);
bool HasPubOfChoice(const CPubdesc& pub, CPub::E_Choice choice);
CRef<CSeqdesc> CreateCitSub(const CCit_sub& cit_sub, const CContact_info* contact);

CRef<CSeqdesc> BuildStructuredComment(const string& comment);
bool IsDigits(string::const_iterator start, string::const_iterator end);

CScope& GetScope();


class CDataChecker
{
public:
    CDataChecker(bool new_data, const list<string>& data) :
        m_new(new_data),
        m_data(data)
    {}

    bool IsStringPresent(const string& str) const
    {
        if (m_new) {
            return true; // all strings are considered to be present in a new data collection
        }

        return find(m_data.begin(), m_data.end(), str) != m_data.end();
    }

private:
    bool m_new;
    const list<string>& m_data;
};

}

#endif // WGS_UTILS_H
