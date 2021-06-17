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
#include <ncbi_pch.hpp>

#include <objects/biblio/Title.hpp>

#include "utils.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace pub_report
{

string GetBestTitle(const CTitle& titles)
{
    string ret;

    if (titles.IsSet()) {
        ITERATE(CTitle::Tdata, title, titles.Get()) {
            if ((*title)->IsIso_jta()) {
                ret = (*title)->GetIso_jta();
                break;
            }
        }

        if (ret.empty()) {
            ITERATE(CTitle::Tdata, title, titles.Get()) {
                if ((*title)->IsName()) {
                    ret = (*title)->GetName();
                    break;
                }

                if ((*title)->IsJta()) {
                    ret = (*title)->GetJta();
                    break;
                }
            }
        }
    }

    return ret;
}

}