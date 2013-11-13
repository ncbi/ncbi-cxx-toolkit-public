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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Colleen Bollin
 * Created:  17/09/2013 15:38:53
 */


#ifndef _CLICKABLE_ITEM_H_
#define _CLICKABLE_ITEM_H_

#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
// #include <gui/objutils/cmd_composite.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

class CClickableText;

class CClickableText : public CObject
{
public:
    CClickableText() : m_Text(""), m_IsSelected(false), m_IsExpanded(false), m_Objdescs() {}
    CClickableText(const string& text, bool selected = false, bool expanded = false)
        : m_Text(text), m_IsSelected(selected), m_IsExpanded(expanded) {}
    ~CClickableText() {}

    const string& GetText() const { return m_Text; }
    void SetText(const string& text) { m_Text = text; }
    bool IsSelected() const { return m_IsSelected; }
    void SetSelected(bool selected = true);
    bool IsExpanded() const { return m_IsExpanded; }
    void SetExpanded(bool expanded = true, bool recurse = false); 
    void SetOwnExpanded(bool expanded = true) { m_IsExpanded = expanded; };
    const vector<CRef<CClickableText> >& GetSubitems() const { return m_Subitems; }
    vector<CRef<CClickableText> >& SetSubitems() { return m_Subitems; }
    const vector<CConstRef<CObject> >& GetObjects() const { return m_Objects; }
    vector<CConstRef<CObject> >& SetObjects() { return m_Objects; }
    const vector <string>& GetObjdescs() const { return m_Objdescs; }
    vector <string>& SetObjdescs() { return m_Objdescs; } 

protected:
    string m_Text;
    bool m_IsSelected;
    bool m_IsExpanded;
    vector<CRef<CClickableText> > m_Subitems;
    vector<CConstRef<CObject> > m_Objects;
    vector <string> m_Objdescs;
};

END_NCBI_SCOPE

#endif 
