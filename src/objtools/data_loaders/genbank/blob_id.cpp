/*  $Id$
 * ===========================================================================
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
 * ===========================================================================
 *
 *  Author:  Eugene Vasilchenko
 *
 *  File Description: Base data reader interface
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/blob_id.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <stdio.h>   // for sscanf

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBlob_id;


CNcbiOstream& CBlob_id::Dump(CNcbiOstream& ostr) const
{
    ostr << "Blob(";
    ostr << GetSat()<<','<<GetSatKey();
    if ( !IsMainBlob() )
        ostr << ",sub="<<GetSubSat();
    ostr << ')';
    return ostr;
}


string CBlob_id::ToString(void) const
{
    CNcbiOstrstream ostr;
    Dump(ostr);
    return CNcbiOstrstreamToString(ostr);
}

/* static */
CBlob_id* CBlob_id::CreateFromString(const string& str)
{
    int sat = -1;
    int sat_key = 0;
    int sub_sat = 0;
    if (str.find(",sub=") != string::npos) {
        if( sscanf(str.c_str(), "Blob(%d,%d,sub=%d)", &sat, &sat_key, &sub_sat) != 3)
            NCBI_THROW(CLoaderException, eOtherError,
                       "\"" + str + "\" is not a valid Genbank BlobId");

    } else {
        if( sscanf(str.c_str(), "Blob(%d,%d)", &sat, &sat_key) != 2)
            NCBI_THROW(CLoaderException, eOtherError,
                       "\"" + str + "\" is not a valid Genbank BlobId");
    }
        
    CRef<CBlob_id> blobid(new CBlob_id);
    blobid->SetSat(sat);
    blobid->SetSubSat(sub_sat);
    blobid->SetSatKey(sat_key);
    return blobid;
}


bool CBlob_id::operator==(const CBlobId& id_ref) const
{
    const CBlob_id* id = dynamic_cast<const CBlob_id*>(&id_ref);
    return id && *this == *id;
}


bool CBlob_id::operator<(const CBlobId& id_ref) const
{
    const CBlob_id* id = dynamic_cast<const CBlob_id*>(&id_ref);
    if ( !id ) {
        return LessByTypeId(id_ref);
    }
    return *this < *id;
}


END_SCOPE(objects)
END_NCBI_SCOPE
