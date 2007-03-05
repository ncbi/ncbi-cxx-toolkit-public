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
 * Authors:  Josh Cherry
 *
 * File Description:  Simplified interface to Object Manager
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/simple/simple_om.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

CRef<CObjectManager> CSimpleOM::sm_OM;


CRef<CObjectManager> CSimpleOM::x_GetOM(void)
{
    if (!sm_OM) {
        sm_OM = CObjectManager::GetInstance();
        // Register GB loader if not already registered
        if (!sm_OM->FindDataLoader("GBLOADER")) {
            CGBDataLoader::RegisterInObjectManager(*sm_OM);
        }
    }
    return sm_OM;
};


// GetIupac, returning a string by value

string CSimpleOM::GetIupac(const CSeq_id& id, ENa_strand strand)
{
    string rv;
    GetIupac(rv, id, strand);
    return rv;    
}


string CSimpleOM::GetIupac(const string& id_string, ENa_strand strand)
{
    string rv;
    GetIupac(rv, id_string, strand);
    return rv;    
}


string CSimpleOM::GetIupac(int gi, ENa_strand strand)
{
    string rv;
    GetIupac(rv, gi, strand);
    return rv;    
}


string CSimpleOM::GetIupac(const CSeq_id_Handle& id, ENa_strand strand)
{
    string rv;
    GetIupac(rv, id, strand);
    return rv;    
}


string CSimpleOM::GetIupac(const CSeq_loc& loc, ENa_strand strand)
{
    string rv;
    GetIupac(rv, loc, strand);
    return rv;    
}


// GetIupac, writing to a string passed in by reference

void CSimpleOM::GetIupac(string& result, const CSeq_id& id, ENa_strand strand)
{
    CSeqVector vec = GetSeqVector(id, strand);
    vec.SetIupacCoding();
    vec.GetSeqData(0, vec.size(), result);
}


void CSimpleOM::GetIupac(string& result, const string& id_string, ENa_strand strand)
{
    CSeqVector vec = GetSeqVector(id_string, strand);
    vec.SetIupacCoding();
    vec.GetSeqData(0, vec.size(), result);
}


void CSimpleOM::GetIupac(string& result, int gi, ENa_strand strand)
{
    CSeqVector vec = GetSeqVector(gi, strand);
    vec.SetIupacCoding();
    vec.GetSeqData(0, vec.size(), result);
}


void CSimpleOM::GetIupac(string& result, const CSeq_id_Handle& id, ENa_strand strand)
{
    CSeqVector vec = GetSeqVector(id, strand);
    vec.SetIupacCoding();
    vec.GetSeqData(0, vec.size(), result);
}


void CSimpleOM::GetIupac(string& result, const CSeq_loc& loc, ENa_strand strand)
{
    CSeqVector vec = GetSeqVector(loc, strand);
    vec.SetIupacCoding();
    vec.GetSeqData(0, vec.size(), result);
}


// GetSeqVector

CSeqVector CSimpleOM::GetSeqVector(const CSeq_id& id, ENa_strand strand)
{
    return GetBioseqHandle(id).GetSeqVector(strand);
}


CSeqVector CSimpleOM::GetSeqVector(const string& id_string, ENa_strand strand)
{
    return GetBioseqHandle(id_string).GetSeqVector(strand);
}


CSeqVector CSimpleOM::GetSeqVector(int gi, ENa_strand strand)
{
    return GetBioseqHandle(gi).GetSeqVector(strand);
}


CSeqVector CSimpleOM::GetSeqVector(const CSeq_id_Handle& id, ENa_strand strand)
{
    return GetBioseqHandle(id).GetSeqVector(strand);
}


CSeqVector CSimpleOM::GetSeqVector(const CSeq_loc& loc, ENa_strand strand)
{
    return CSeqVector(loc, *NewScope(), CBioseq_Handle::eCoding_Ncbi, strand);
}


// GetBioseqHandle

CBioseq_Handle CSimpleOM::GetBioseqHandle(const CSeq_id& id)
{
    return NewScope()->GetBioseqHandle(id);
}


CBioseq_Handle CSimpleOM::GetBioseqHandle(const string& id_string)
{
    CSeq_id id(id_string);
    return GetBioseqHandle(id);
}


CBioseq_Handle CSimpleOM::GetBioseqHandle(int gi)
{
    CSeq_id id;
    id.SetGi(gi);
    return GetBioseqHandle(id);
}


CBioseq_Handle CSimpleOM::GetBioseqHandle(const CSeq_id_Handle& id)
{
    return NewScope()->GetBioseqHandle(id);
}


// NewScope: get a new scope, with or without default dataloaders

CRef<CScope> CSimpleOM::NewScope(bool with_defaults)
{
    CRef<CScope> scope(new CScope(*x_GetOM()));
    if (with_defaults) {
        scope->AddDefaults();
    }
    return scope;
}

// ReleaseOM: release the CRef<CObjectManager> static member

void CSimpleOM::ReleaseOM(void)
{
	sm_OM.Reset();
};

END_objects_SCOPE
END_NCBI_SCOPE
