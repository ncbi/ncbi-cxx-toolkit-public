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
*          Mati Shomrat
*
* File Description:
*   Base class for the various item objects
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <serial/serialbase.hpp>

#include <objects/general/Date.hpp>

#include <objtools/format/items/item.hpp>
#include <objtools/format/items/item_base.hpp>
#include "context.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
//
// Public

const CSerialObject* CFlatItem::GetObject(void) const
{
    return m_Object;
}


bool CFlatItem::IsSetObject(void) const
{
    return m_Object; 
}


const CFFContext& CFlatItem::GetContext(void) const
{
    return *m_Context;
}


bool CFlatItem::Skip(void) const
{
    return m_Skip;
}


CFlatItem::~CFlatItem(void)
{
    m_Object.Reset();
    m_Context.Reset();
}


/////////////////////////////////////////////////////////////////////////////
//
// Protected

// constructor
CFlatItem::CFlatItem(CFFContext& ctx) :
    m_Object(0),
    m_Context(&ctx),
    m_Skip(false)
{
}


// Shared utility functions

void CFlatItem::x_SetObject(const CSerialObject& obj) 
{
    m_Object.Reset(&obj);
}


void CFlatItem::x_SetSkip(void)
{
    m_Skip = true;
    m_Object.Reset();
    m_Context.Reset();
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/12/17 20:22:46  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
