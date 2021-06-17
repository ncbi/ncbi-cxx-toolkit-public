/* $Id$
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
 * Author:  Andrei Gourianov
 *
 * File Description:
 *   Render list of checkboxes
 */

package ptbgui;

import java.awt.*;
import java.util.*;
import javax.swing.*;

public class CheckListRenderer extends JCheckBox implements ListCellRenderer {
    private static final long serialVersionUID = 1L;
    private SortedSet<String> m_undefSel;
    public void setUndefinedSelection(SortedSet<String> undefSel) {
        if (m_undefSel == null) {
            m_undefSel = new TreeSet<String>();
        } else {
            m_undefSel.clear();
        }
        m_undefSel.addAll(undefSel);
    }
    public Component getListCellRendererComponent(
           JList list, Object value, int index,
           boolean isSelected, boolean hasFocus)
{
    setEnabled(list.isEnabled());
    setFont(list.getFont());
    JCheckBox b= (JCheckBox)value;
    String s = b.getText();
    setText(s);
    boolean selected = b.isSelected();
    setSelected(selected);
    if (selected) {
        setBackground(SystemColor.textHighlight);
        setForeground(SystemColor.textHighlightText);
    } else {
        if (m_undefSel != null && m_undefSel.contains(s)) {
            Color n = list.getBackground();
            Color h = SystemColor.textHighlight;
            Color u = new Color(
                    (n.getRed() + h.getRed())/2,
                    (n.getGreen() + h.getGreen())/2,
                    (n.getBlue() + h.getBlue())/2);
            setBackground(u);
            setForeground(list.getForeground());
        } else {
            setBackground(list.getBackground());
            setForeground(list.getForeground());
        }
    }
    return this;
}
}


