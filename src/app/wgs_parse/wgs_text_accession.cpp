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

#include <corelib/ncbistr.hpp>

#include "wgs_text_accession.hpp"

USING_NCBI_SCOPE;

namespace wgsparse
{

using namespace std;

CTextAccessionContainer::CTextAccessionContainer() :
    m_number(0),
    m_valid(false)
{
}

CTextAccessionContainer::CTextAccessionContainer(const string& accession)
{
    Set(accession);
}

void CTextAccessionContainer::Set(const string& accession)
{
    m_number = 0;
    m_prefix.clear();
    m_valid = true;

    m_accession = accession;

    size_t idx = 0;
    for (; idx < m_accession.size(); ++idx) {
        if (isdigit(m_accession[idx])) {
            break;
        }

        m_prefix += m_accession[idx];
    }

    if (m_prefix.empty() || idx == m_accession.size()) {
        m_valid = false;
    }
    else {
        for (; idx < m_accession.size(); ++idx) {
            if (!isdigit(m_accession[idx])) {
                break;
            }

            m_number *= 10;
            m_number += m_accession[idx] - '0';
        }

        if (idx != m_accession.size()) {
            m_valid = false;
        }
    }

    if (!m_valid) {
        m_prefix.clear();
        m_number = 0;
    }
}

bool CTextAccessionContainer::IsValid() const {
    return m_valid;
}

const string& CTextAccessionContainer::GetAccession() const
{
    return m_accession;
}

const string& CTextAccessionContainer::GetPrefix() const
{
    return m_prefix;
}

size_t CTextAccessionContainer::GetNumber() const
{
    return m_number;
}

void CTextAccessionContainer::swap(CTextAccessionContainer& other)
{
    m_accession.swap(other.m_accession);
    m_prefix.swap(other.m_prefix);
    ::swap(m_number, other.m_number);
    ::swap(m_valid, other.m_valid);
}

bool CTextAccessionContainer::operator<(const CTextAccessionContainer& other) const
{
    if (GetPrefix() == other.GetPrefix()) {
        return GetNumber() < other.GetNumber();
    }
    return GetPrefix() < other.GetPrefix();
}

}
