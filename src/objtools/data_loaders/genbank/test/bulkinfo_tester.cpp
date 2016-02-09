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
* Author:
*           Andrei Gourianov, Michael Kimelman
*
* File Description:
*           Basic test of GenBank data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "bulkinfo_tester.hpp"
#include <objmgr/seq_vector.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <util/checksum.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

USING_SCOPE(sequence);

IBulkTester::IBulkTester(void)
    : get_flags(0),
      report_all_errors(false)
{
}


IBulkTester::~IBulkTester(void)
{
}


void IBulkTester::SetParams(const TIds& ids, CScope::TGetFlags get_flags)
{
    this->ids = ids;
    this->get_flags = get_flags;
}


void IBulkTester::Display(CNcbiOstream& out, bool verify) const
{
    CMutexGuard guard(sm_DisplayMutex);
    for ( size_t i = 0; i < ids.size(); ++i ) {
        Display(out, i, verify);
    }
}


DEFINE_CLASS_STATIC_MUTEX(IBulkTester::sm_DisplayMutex);


void IBulkTester::Display(CNcbiOstream& out, size_t i, bool verify) const
{
    CMutexGuard guard(sm_DisplayMutex);
    out << GetType() << "("<<ids[i]<<") -> ";
    DisplayData(out, i);
    if ( verify && !Correct(i) ) {
        out << " actual: ";
        DisplayDataVerify(out, i);
    }
    out << endl;
}


vector<bool> IBulkTester::GetErrors(void) const
{
    vector<bool> errors(ids.size());
    for ( size_t i = 0; i < ids.size(); ++i ) {
        errors[i] = !Correct(i);
    }
    return errors;
}


class CDataTesterGi : public IBulkTester
{
public:
    typedef TGi TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "gi";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetGis(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetGi(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                if ( h ) {
                    CSeq_id_Handle id = GetId(h, eGetId_ForceGi);
                    if ( id && id.IsGi() ) {
                        data_verify[i] = id.GetGi();
                    }
                    scope.RemoveFromHistory(h);
                }
            }
        }
    bool Correct(size_t i) const
        {
            return data[i] == data_verify[i];
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterAcc : public IBulkTester
{
public:
    typedef CSeq_id_Handle TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "acc";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetAccVers(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetAccVer(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                if ( h ) {
                    data_verify[i] = GetId(h, eGetId_ForceAcc);
                    scope.RemoveFromHistory(h);
                }
            }
        }
    bool Correct(size_t i) const
        {
            if ( data[i] == data_verify[i] ) {
                return true;
            }
            if ( report_all_errors ) {
                return false;
            }
            if ( !data_verify[i] ) {
                CConstRef<CSeq_id> id = data[i].GetSeqId();
                if ( const CTextseq_id* text_id = id->GetTextseq_Id() ) {
                    if ( !text_id->IsSetVersion() ) {
                        return true;
                    }
                }
                return false;
            }
            if ( !data[i] || !data_verify[i] ||
                 data[i].Which() != data_verify[i].Which() ) {
                return false;
            }
            CRef<CSeq_id> id1(SerialClone(*data[i].GetSeqId()));
            if ( const CTextseq_id* text_id = id1->GetTextseq_Id() ) {
                const_cast<CTextseq_id*>(text_id)->ResetName();
            }
            CRef<CSeq_id> id2(SerialClone(*data_verify[i].GetSeqId()));
            if ( const CTextseq_id* text_id = id2->GetTextseq_Id() ) {
                const_cast<CTextseq_id*>(text_id)->ResetName();
            }
            return id1->Equals(*id2);
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterLabel : public IBulkTester
{
public:
    typedef string TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "label";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetLabels(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetLabel(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                if ( h ) {
                    data_verify[i] = GetLabel(h.GetId());
                    scope.RemoveFromHistory(h);
                }
            }
        }
    bool Correct(size_t i) const
        {
            if ( data[i] == data_verify[i] ) {
                return true;
            }
            if ( report_all_errors ) {
                return false;
            }
            if ( data_verify[i].find('|') == NPOS ||
                 NStr::StartsWith(data_verify[i], "gi|") ||
                 NStr::StartsWith(data_verify[i], "lcl|") ) {
                return true;
            }
            return false;
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterTaxId : public IBulkTester
{
public:
    typedef int TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "taxid";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetTaxIds(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetTaxId(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size(), -1);
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                if ( h ) {
                    data_verify[i] = GetTaxId(h);
                    scope.RemoveFromHistory(h);
                }
            }
        }
    bool Correct(size_t i) const
        {
            return data[i] == data_verify[i];
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterHash : public IBulkTester
{
public:
    typedef int TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "hash";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetSequenceHashes(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetSequenceHash(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                if ( CBioseq_Handle h = scope.GetBioseqHandle(ids[i]) ) {
                    CChecksum sum(CChecksum::eCRC32INSD);
                    CSeqVector sv(h, h.eCoding_Iupac);
                    for ( CSeqVector_CI it(sv); it; ) {
                        TSeqPos size = it.GetBufferSize();
                        sum.AddChars(it.GetBufferPtr(), size);
                        it += size;
                    }
                    data_verify[i] = sum.GetChecksum();
                    scope.RemoveFromHistory(h);
                }
            }
        }
    bool Correct(size_t i) const
        {
            return data[i] == data_verify[i] || !data[i];
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterLength : public IBulkTester
{
public:
    typedef TSeqPos TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "length";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetSequenceLengths(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetSequenceLength(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size(), kInvalidSeqPos);
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                if ( h ) {
                    data_verify[i] = h.GetBioseqLength();
                    scope.RemoveFromHistory(h);
                }
            }
        }
    bool Correct(size_t i) const
        {
            return data[i] == data_verify[i];
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterType : public IBulkTester
{
public:
    typedef CSeq_inst::EMol TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "type";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetSequenceTypes(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetSequenceType(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size(), CSeq_inst::eMol_not_set);
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                if ( h ) {
                    data_verify[i] = h.GetSequenceType();
                    scope.RemoveFromHistory(h);
                }
            }
        }
    bool Correct(size_t i) const
        {
            return data[i] == data_verify[i];
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterState : public IBulkTester
{
public:
    typedef int TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "state";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetSequenceStates(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetSequenceState(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size(), CSeq_inst::eMol_not_set);
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                data_verify[i] = h.GetState();
                if ( h ) {
                    scope.RemoveFromHistory(h);
                }
            }
        }
    bool Correct(size_t i) const
        {
            return data[i] == data_verify[i];
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


IBulkTester* IBulkTester::CreateTester(EBulkType type)
{
    switch ( type ) {
    case eBulk_gi:     return new CDataTesterGi();
    case eBulk_acc:    return new CDataTesterAcc();
    case eBulk_label:  return new CDataTesterLabel();
    case eBulk_taxid:  return new CDataTesterTaxId();
    case eBulk_hash:   return new CDataTesterHash();
    case eBulk_length: return new CDataTesterLength();
    case eBulk_type:   return new CDataTesterType();
    case eBulk_state:  return new CDataTesterState();
    default: return 0;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
