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
 *   GUI application that works with project_tree_builder
 *   NCBI C++ Toolkit
 */

package ptbgui;

import java.awt.*;
import java.io.*;
import java.util.*;
import javax.swing.*;
import javax.swing.text.JTextComponent;

public class PtbguiMain extends javax.swing.JFrame {
    private static final long serialVersionUID = 1L;
    private String[]  m_OriginalArgs;
    private ArgsParser m_ArgsParser;
    private Properties m_ArgsProp;
    private Process m_Ptb;
    private BufferedReader m_PtbOut;
    private BufferedReader m_PtbErr;
    private OutputStream m_PtbIn;
    private String m_Params, m_TmpParams;
    private String m_Key3root, m_KeyCpath;
    private Map<String, String[]> m_ProjectTags;
    private SortedSet<String> m_UndefSelTags;
    private Vector<String> m_KnownTags;
    private Vector<String> m_CompositeTags;
    private KTagsDialog m_KtagsDlg;
    enum eState {
        beforePtb,
        got3rdparty,
        gotProjects,
        donePtb
    }
    private eState m_State;

    /** Creates new form PtbguiMain */
    public PtbguiMain() {
        initComponents();
        initObjects();
        resetState();
    }
    private void initObjects() {
        m_ArgsParser = new ArgsParser();
        m_ProjectTags = new HashMap<String, String[]>();
        m_UndefSelTags = new TreeSet<String>();
        m_KnownTags = new Vector<String>();
        m_CompositeTags = new Vector<String>();
        ButtonGroup group = new ButtonGroup();
        group.add(jRadioButtonStatic);
        group.add(jRadioButtonDLL);
        initCheckList(jListApps);
        initCheckList(jListLibs);
        initCheckList(jListOther);
        initCheckList(jListTags);
        initCheckList(jListUserReq);
    }
    private void resetState() {
        jTabbedPane.setSelectedIndex(0);
        jTabbedPane.setEnabledAt(0,true);
        jTabbedPane.setEnabledAt(1,true);
        jTabbedPane.setEnabledAt(2,false);
        jTabbedPane.setEnabledAt(3,false);
        jTabbedPane.setEnabledAt(4,false);
        m_State = eState.beforePtb;
        jButtonGOK.setText("Next");
        jButtonGOK.setEnabled(true);
        jButtonGOK.setVisible(true);
        jButtonGCancel.setText("Cancel");
        jButtonGCancel.setEnabled(true);
        jButtonGCancel.setVisible(true);
        ((DefaultListModel)jListApps.getModel()).clear();
        ((DefaultListModel)jListLibs.getModel()).clear();
        ((DefaultListModel)jListOther.getModel()).clear();
        ((DefaultListModel)jListTags.getModel()).clear();
        ((DefaultListModel)jListUserReq.getModel()).clear();
        showMoreAdvanced(false);
    }
    private void showMoreAdvanced(boolean show) {
        jCheckBoxNoPtb.setVisible(show);
        jCheckBoxNws.setVisible(show);
        jCheckBoxExt.setVisible(show);
        jLabel1.setVisible(show);
        jLabel2.setVisible(show);
        jTextFieldPtb.setVisible(show);
        jTextFieldRoot.setVisible(show);
        jButtonPtb.setVisible(show);
        jButtonMore.setText(show ? "< less" : "more >");
    }
    private void initData() {
        initData(m_OriginalArgs);
    }
    private void initData(String args[]) {
        if (m_OriginalArgs == null) {
            m_OriginalArgs = new String[args.length];
            for (int i=0; i<args.length; ++i) {
                m_OriginalArgs[i] = args[i];
        }
        }
        m_ArgsParser.init(args);
        setPathText(jTextFieldPtb, m_ArgsParser.getPtb(), true);
        setPathText(jTextFieldRoot, m_ArgsParser.getRoot(), true);
        setPathText(jTextFieldLst,
                m_ArgsParser.getRoot(), m_ArgsParser.getSubtree());
        setPathText(jTextFieldSolution, m_ArgsParser.getSolutionFile(), false);
        jTextFieldTags.setToolTipText(
            "Expression. For example:  (core || web) && !test");
        jTextFieldLstTags.setToolTipText(
            "When 'Use project tags' field above is empty, default tags will be used");

        if (m_ArgsParser.getArgsFile().length() > 0) {
            initData(m_ArgsParser.getArgsFile(), true);
            return;
        }
        jTextFieldTags.setText(m_ArgsParser.getProjTag());
        jTextFieldLstTags.setText("");
        jTextFieldIde.setText(m_ArgsParser.getIde());

        String arch = m_ArgsParser.getArch();
        jTextFieldArch.setText(arch);
        if (arch.equals("Win32") || arch.equals("x64")) {
            jTextFieldArch.setToolTipText("Win32 or x64");
        }

        jRadioButtonDLL.setSelected(m_ArgsParser.getDll());
        jRadioButtonStatic.setSelected(!m_ArgsParser.getDll());
        jCheckBoxNoPtb.setSelected(m_ArgsParser.m_nobuildptb);
        jCheckBoxNws.setSelected(m_ArgsParser.m_nws);
        jCheckBoxExt.setSelected(m_ArgsParser.m_ext);
        jTextFieldExt.setText(m_ArgsParser.getExtRoot());

        jLabelArgs.setText(" ");
        jButtonArgsReset.setEnabled(false);
        if (m_ArgsProp != null) {
            m_ArgsProp.clear();
        }

//        jRadioButtonDLL.setEnabled(false);
//        jRadioButtonStatic.setEnabled(false);
        adjustArch();
        initKnownTags();
        initTagsFromSubtree();
    }
    private void adjustArch() {
        File build_root = new File(m_ArgsParser.getBuildRoot());
        File[] arrFile = build_root.listFiles( new FilenameFilter()
        {
            public boolean accept(File dir, String name)
            {
                return (name.toLowerCase().matches("__configured_platform.*"));
            }
        });
        jTextFieldArch.setEditable(arrFile == null || arrFile.length==0);
    }
    private void adjustBuildType() {
        m_ArgsParser.setDll( jRadioButtonDLL.isSelected(),true);
        setPathText(jTextFieldSolution, m_ArgsParser.getSolutionFile(), false);
    }
    private void initData(String file, Boolean fromArgs) {
        try {
            if (m_ArgsProp != null) {
                m_ArgsProp.clear();
            } else {
                m_ArgsProp = new Properties();
            }
            m_ArgsProp.load(new FileInputStream(new File(file)));
            initData(m_ArgsProp, fromArgs);
            jLabelArgs.setText(file);
            jButtonArgsReset.setEnabled(!fromArgs);
        } catch (Exception e) {
            System.err.println(e.toString());
            e.printStackTrace();
            JOptionPane.showMessageDialog( this,
                "This file does not contain valid data",
                "Error", JOptionPane.ERROR_MESSAGE);
        }
    }
    private void initData(Properties prop, Boolean fromArgs) {
        String v;
        if (fromArgs) {
            v = m_ArgsParser.getRoot();
        } else {
            v = getProp(prop,"__arg_root");
            setPathText(jTextFieldRoot, getProp(prop,"__arg_root"), true);
            setPathText(jTextFieldSolution, getProp(prop,"__arg_solution"), false);
        }
        setPathText(jTextFieldLst, v, getProp(prop,"__arg_subtree"));
        jTextFieldTags.setText(getProp(prop,"__arg_projtag"));
        jTextFieldLstTags.setText("");
        jTextFieldIde.setText(getProp(prop,"__arg_ide"));
        jTextFieldArch.setText(getProp(prop,"__arg_arch"));

        jRadioButtonDLL.setSelected(getProp(prop,"__arg_dll").equals("yes"));
        jRadioButtonStatic.setSelected(!jRadioButtonDLL.isSelected());
        jCheckBoxNoPtb.setSelected(getProp(prop,"__arg_nobuildptb").equals("yes"));
        jCheckBoxNws.setSelected(getProp(prop,"__arg_nws").equals("yes"));
        jCheckBoxExt.setSelected(getProp(prop,"__arg_ext").equals("yes"));
        jTextFieldExt.setText(getProp(prop,"__arg_extroot"));
        adjustArch();
        initKnownTags();
        initTagsFromSubtree();
    }
    private void initKnownTags() {
        String from = jTextFieldRoot.getText()+
            "/src/build-system/project_tags.txt";
        int n = 0;
        m_KnownTags.clear();
        m_CompositeTags.clear();
        if (!ArgsParser.existsPath(from)) {
            return;
        }
        try {
            BufferedReader r = new BufferedReader(new InputStreamReader(
                new FileInputStream(new File(nativeFileSeparator(from)))));
            String line;
            while ((line = r.readLine()) != null) {
                if (line.length() == 0 || line.charAt(0) == '#') {
                    continue;
                }
                String[] t = line.split("=");
                if (t.length > 1) {
                    m_CompositeTags.add(line.trim());
                    continue;
                }
                t = line.split("[, ]");
                for (int i=0; i<t.length; ++i) {
                    if (t[i].trim().length() != 0) {
                        ++n;
                        m_KnownTags.add(t[i].trim());
                    }
                }
            }
        } catch (Exception e) {
            System.err.println(e.toString());
            e.printStackTrace();
        }
    }
    private void initTagsFromSubtree() {
        jTextFieldLstTags.setText("");
        String lst = jTextFieldRoot.getText() +
            File.separatorChar + jTextFieldLst.getText();
        File f = new File(nativeFileSeparator(lst));
        if (f.isFile()) {
            try {
                BufferedReader r = new BufferedReader(new InputStreamReader(
                    new FileInputStream(f)));
                String key = "#define TAGS";
                String line;
                while ((line = r.readLine()) != null) {
                    line = line.trim();
                    if (line.startsWith(key)) {
                        line = line.replaceAll(key,"");
                        line = line.replaceAll("\\[","");
                        line = line.replaceAll("\\]","");
                        line = line.trim();
//                        m_ArgsParser.setProjTagFromLst(line);
                        jTextFieldLstTags.setText(line);
                        break;
                    }
                }
            } catch (Exception e) {
                System.err.println(e.toString());
                e.printStackTrace();
            }
        }
    }
    private void updateData() {
        m_ArgsParser.setPtb(jTextFieldPtb.getText());
        if (m_ArgsProp != null && !m_ArgsProp.isEmpty()) {
            setProp(m_ArgsProp, "__arg_root", jTextFieldRoot.getText());
            setProp(m_ArgsProp, "__arg_subtree", jTextFieldLst.getText());
            setProp(m_ArgsProp, "__arg_solution", jTextFieldSolution.getText());
            setProp(m_ArgsProp, "__arg_projtag", jTextFieldTags.getText());
            setProp(m_ArgsProp, "__arg_ide", jTextFieldIde.getText());
            setProp(m_ArgsProp, "__arg_arch", jTextFieldArch.getText());

            setProp(m_ArgsProp, "__arg_dll", jRadioButtonDLL.isSelected());
            setProp(m_ArgsProp, "__arg_nobuildptb", jCheckBoxNoPtb.isSelected());
            setProp(m_ArgsProp, "__arg_nws", jCheckBoxNws.isSelected());
            setProp(m_ArgsProp, "__arg_ext", jCheckBoxExt.isSelected());
            setProp(m_ArgsProp, "__arg_extroot", jTextFieldExt.getText());

            try {
                File f = File.createTempFile("PTBconf",".ini");
                f.deleteOnExit();
                FileOutputStream fout = new FileOutputStream(f);
                Enumeration props = m_ArgsProp.propertyNames();
                while (props.hasMoreElements()) {
                    String key = props.nextElement().toString();
                    String value = m_ArgsProp.getProperty(key);
                    String line = key + "=" + value + "\n";
                    fout.write(line.getBytes());
                }
                fout.flush();
                fout.close();
                m_TmpParams = f.getPath();
            } catch (Exception e) {
                System.err.println(e.toString());
                e.printStackTrace();
            }
            m_ArgsParser.setArgsFile(m_TmpParams);
        } else {
            m_ArgsParser.setRoot(jTextFieldRoot.getText());
            m_ArgsParser.setSubtree(jTextFieldLst.getText());
            m_ArgsParser.setSolutionFile(jTextFieldSolution.getText());
            m_ArgsParser.setProjTag(jTextFieldTags.getText());

            m_ArgsParser.setArch(jTextFieldArch.getText());
            m_ArgsParser.setDll(jRadioButtonDLL.isSelected(), false);
            m_ArgsParser.m_nobuildptb = jCheckBoxNoPtb.isSelected();
            m_ArgsParser.m_nws = jCheckBoxNws.isSelected();
            m_ArgsParser.m_ext = jCheckBoxExt.isSelected();
            m_ArgsParser.setExtRoot(jTextFieldExt.getText());

            m_ArgsParser.setArgsFile(null);
        }
    }
    public static String nativeFileSeparator(String s) {
        return s.replace('/',File.separatorChar);
    }
    public static String getProp(Properties prop, String key) {
        return prop.containsKey(key) ?
            nativeFileSeparator(prop.getProperty(key).trim()) : "";
    }
    public static String portableFileSeparator(String s) {
        return s.replace(File.separatorChar,'/');
    }
    public static void setProp(Properties prop, String key, String value) {
        prop.setProperty(key,portableFileSeparator(value));
    }
    public static void setProp(Properties prop, String key, boolean value) {
        prop.setProperty(key,value ? "yes" : "no");
    }
    public static boolean copyFile(String from, String to) {
        boolean res = true;
        if (!from.equals(to)) {
            try {
                InputStream in = new FileInputStream(new File(from));
                OutputStream out = new FileOutputStream(new File(to));
                byte[] buf = new byte[1024];
                int len;
                while ((len = in.read(buf)) > 0) {
                    out.write(buf, 0, len);
                }
                in.close();
                out.close();
            } catch (Exception e) {
                res = false;
                System.err.println(e.toString());
                e.printStackTrace();
            }
        }
        return res;
    }
    private void setPathText(JTextComponent c, String path, boolean verify) {
        c.setText(path);
        if (!verify || ArgsParser.existsPath(path)) {
            c.setForeground(SystemColor.controlText);
            c.setToolTipText("");
        } else {
            c.setForeground(Color.red);
            c.setToolTipText("Path not found");
        }
    }
    private void setPathText(JTextComponent c, String root, String path) {
        c.setText(path);
        if (ArgsParser.existsPath(root + File.separator + path)) {
            c.setForeground(SystemColor.controlText);
            c.setToolTipText("");
        } else {
            c.setForeground(Color.red);
            c.setToolTipText("Path not found");
        }
    }
    private void processPtbOutput() {
        String line;
        while (isPtbRunning()) {
            try {
                Thread.sleep(3);
                while (m_PtbErr != null && m_PtbErr.ready() &&
                       (line = m_PtbErr.readLine()) != null) {
                    System.err.println(line);
                }
                while (m_PtbOut != null && m_PtbOut.ready() &&
                       (line = m_PtbOut.readLine()) != null) {
                    if (line.startsWith("*PTBGUI{* custom")) {
                        processAdditionalParams();
                        return;
                    }
                    else if (line.startsWith("*PTBGUI{* projects")) {
                        processProjects();
                        return;
                    }
                    System.out.println(line);
                }
            } catch (Exception e) {
                System.err.println(e.toString());
                e.printStackTrace();
            }
        }
        try {
            while (m_PtbErr != null && m_PtbErr.ready() &&
                   (line = m_PtbErr.readLine()) != null) {
                System.err.println(line);
            }
            while (m_PtbOut != null && m_PtbOut.ready() &&
                   (line = m_PtbOut.readLine()) != null) {
                System.out.println(line);
            }
        } catch (Exception e) {
            System.err.println(e.toString());
            e.printStackTrace();
        }
        if (m_State == eState.gotProjects || m_State == eState.got3rdparty) {
            processDone(m_Ptb.exitValue());
            return;
        }
        System.exit(m_Ptb != null ? m_Ptb.exitValue() : 1);
    }
    private void processAdditionalParams() {
        m_State = eState.got3rdparty;
        jButtonGOK.setText("Next");
        jButtonGCancel.setEnabled(true);
        int i = jTabbedPane.indexOfComponent(jPanelAdd);
        jTabbedPane.setEnabledAt(i,true);
        jTabbedPane.setSelectedIndex(i);
        jTextField3root.setText("");
        jTextField3root.setEnabled(false);
        jTextFieldCpath.setText("");
        jTextFieldCpath.setEnabled(false);
        jCheckBoxVTuneR.setEnabled(false);
        jCheckBoxVTuneR.setVisible(false);
        jCheckBoxVTuneD.setEnabled(false);
        jCheckBoxVTuneD.setVisible(false);
        jCheckBoxVTune.setSelected(false);
        jCheckBoxVTune.setEnabled(false);
        jPanelUserReq.setVisible(false);
        boolean vtune = false;
        String[] userRequests = new String[0];
        String[] enabledRequests = new String[0];

        try {
            String line;
            while (m_PtbOut != null && /*m_PtbOut.ready() &&*/
                   (line = m_PtbOut.readLine()) != null) {
                if (line.startsWith("*PTBGUI}*")) {
                    if (vtune) {
                        jCheckBoxVTune.setEnabled(true);
                        jCheckBoxVTuneR.setEnabled(true);
                        jCheckBoxVTuneD.setEnabled(true);
                        jCheckBoxVTune.setSelected(
                            jCheckBoxVTuneR.isSelected() ||
                            jCheckBoxVTuneD.isSelected());
                        jCheckBoxVTuneD.setVisible(jCheckBoxVTune.isSelected());
                        jCheckBoxVTuneR.setVisible(jCheckBoxVTune.isSelected());
                    }
                    if (userRequests.length > 0) {
                        jPanelUserReq.setVisible(true);
                        for (int r=0; r < userRequests.length; ++r) {
                            boolean sel = false;
                            for (int e=0; !sel && e < enabledRequests.length; ++e) {
                                sel = userRequests[r].equals( enabledRequests[e] );
                            }
                            addProject(jListUserReq, userRequests[r], sel);
                        }
                    }
                    return;
                }
                String[] kv = line.split("=");
                if (kv.length > 1) {
                    String k = kv[0].trim();
                    String v = kv[1].trim();
                    if (k.equals("ThirdPartyBasePath") ||
                        k.equals("XCode_ThirdPartyBasePath")) {
                        m_Key3root = k;
                        jTextField3root.setEnabled(true);
                        setPathText(jTextField3root,nativeFileSeparator(v),true);
                    }
                    else if (k.equals("ThirdParty_C_ncbi") ||
                             k.equals("XCode_ThirdParty_C_ncbi")) {
                        m_KeyCpath = k;
                        jTextFieldCpath.setEnabled(true);
                        setPathText(jTextFieldCpath,nativeFileSeparator(v),true);
                    }
                    else if (k.equals("__TweakVTuneR")) {
                        vtune = true;
                        jCheckBoxVTuneR.setSelected(v.equals("yes"));
                    }
                    else if (k.equals("__TweakVTuneD")) {
                        vtune = true;
                        jCheckBoxVTuneD.setSelected(v.equals("yes"));
                    }
                    else if (k.equals("__UserRequests")) {
                        userRequests = v.split(" ");
                    }
                    else if (k.equals("__EnabledUserRequests")) {
                        enabledRequests = v.split(" ");
                    }
                }
            }
        } catch (Exception e) {
            System.err.println(e.toString());
            e.printStackTrace();
        }
    }
    private void doneAdditionalParams() {
        if (isPtbRunning()) {
            try {
                String s;
                s = "*PTBGUI{* custom" + "\n";
                m_PtbIn.write(s.getBytes());
                if (jTextField3root.isEnabled()) {
                    s = m_Key3root+" = "+jTextField3root.getText()+"\n";
                    m_PtbIn.write(s.getBytes());
                }
                if (jTextFieldCpath.isEnabled()) {
                    s = m_KeyCpath+" = "+jTextFieldCpath.getText()+"\n";
                    m_PtbIn.write(s.getBytes());
                }
                String yn = (jCheckBoxVTune.isSelected() &&
                             jCheckBoxVTuneR.isSelected()) ? "yes" : "no";
                if (jCheckBoxVTuneR.isEnabled()) {
                    s = "__TweakVTuneR"+" = "+ yn +"\n";
                    m_PtbIn.write(s.getBytes());
                }
                yn = (jCheckBoxVTune.isSelected() &&
                      jCheckBoxVTuneD.isSelected()) ? "yes" : "no";
                if (jCheckBoxVTuneD.isEnabled()) {
                    s = "__TweakVTuneD"+" = "+ yn +"\n";
                    m_PtbIn.write(s.getBytes());
                }
                if (jPanelUserReq.isVisible()) {
                    s = "__EnabledUserRequests =";
                    DefaultListModel model = (DefaultListModel)jListUserReq.getModel();
                    for (int i =0; i< model.getSize(); ++i) {
                        JCheckBox b = (JCheckBox)model.getElementAt(i);
                        if (b.isSelected()) {
                            s += " " + b.getText();
                        }
                    }
                    s += "\n";
                    m_PtbIn.write(s.getBytes());
                }
                s = "*PTBGUI}* custom" + "\n";
                m_PtbIn.write(s.getBytes());
                m_PtbIn.flush();
            } catch (Exception e) {
                System.err.println(e.toString());
                e.printStackTrace();
            }
        }
    }
    private void initCheckList(JList list) {
        list.setModel(new DefaultListModel());
        list.setCellRenderer(new CheckListRenderer());
        list.addMouseListener(new CheckListMouseAdapter());
        list.addMouseListener(new java.awt.event.MouseAdapter() {
            public void mouseClicked(java.awt.event.MouseEvent evt) {
                jListSelectionChanged(evt);
            }
        });
    }
    private void addProject(JList list, String project, boolean selected) {
        DefaultListModel model = (DefaultListModel)list.getModel();
        JCheckBox b = new JCheckBox();
        b.setText(project);
        b.setSelected(selected);
        model.addElement(b);
    }
    private void selectProjects(JList list, boolean select) {
        DefaultListModel model = (DefaultListModel)list.getModel();
        for (int i =0; i< model.getSize(); ++i) {
            JCheckBox b = (JCheckBox)model.getElementAt(i);
            b.setSelected(select);
        }
        countSelected();
        list.repaint();
    }
    private void selectProjects(JList list,
        Vector<String> selected, Vector<String> unselected) {
        DefaultListModel model = (DefaultListModel)list.getModel();
        for (int i =0; i< model.getSize(); ++i) {
            JCheckBox b = (JCheckBox)model.getElementAt(i);
            String prj = b.getText();
            if (m_ProjectTags.containsKey(prj)) {
                String[] tags = m_ProjectTags.get(prj);
                boolean done = false;
                for (int t=0; !done && t<tags.length; ++t) {
                    if (unselected.contains(tags[t])) {
                        b.setSelected(false);
                        done = true;
                    }
                }
                for (int t=0; !done && t<tags.length; ++t) {
                    if (selected.contains(tags[t])) {
                        b.setSelected(true);
                        done = true;
                    }
                }
            }
        }
        list.repaint();
    }
    private void checkProjectSelection(JList list,
        Vector<String> selected, Vector<String> unselected) {
        DefaultListModel model = (DefaultListModel)list.getModel();
        for (int i =0; i< model.getSize(); ++i) {
            JCheckBox b = (JCheckBox)model.getElementAt(i);
            String prj = b.getText();
            if (m_ProjectTags.containsKey(prj)) {
                String[] tags = m_ProjectTags.get(prj);
                boolean hasProhibited = false;
                for (int t=0; t<tags.length; ++t) {
                    if (unselected.contains(tags[t])) {
                        if (b.isSelected()) {
                            m_UndefSelTags.add(tags[t]);
                        }
                        hasProhibited = true;
                    }
                }
                if (!hasProhibited) {
                    for (int t=0; t<tags.length; ++t) {
                        if (selected.contains(tags[t])) {
                            if (!b.isSelected()) {
                                m_UndefSelTags.add(tags[t]);
                            }
                        }
                    }
                }
            }
        }
    }
    private int getSelectedCount(JList list) {
        int count = 0;
        DefaultListModel model = (DefaultListModel)list.getModel();
        for (int i =0; i< model.getSize(); ++i) {
            JCheckBox b = (JCheckBox)model.getElementAt(i);
            if (b.isSelected()) {
                ++count;
            }
        }
        return count;
    }
    private void countSelected() {
        String t = "Applications (" +
            getSelectedCount(jListApps) + "/" +
            jListApps.getModel().getSize()+ ")";
        jLabelApps.setText(t);
        t = "Libraries (" +
            getSelectedCount(jListLibs) + "/" +
            jListLibs.getModel().getSize()+ ")";
        jLabelLibs.setText(t);
        t = "Other (" +
            getSelectedCount(jListOther) + "/" +
            jListOther.getModel().getSize()+ ")";
        jLabelOther.setText(t);
        verifyTagSelection();
    }
    private void verifyTagSelection() {
        Vector<String> selected = new Vector<String>();
        Vector<String> unselected = new Vector<String>();
        DefaultListModel model = (DefaultListModel)jListTags.getModel();
        for (int i =0; i< model.getSize(); ++i) {
            JCheckBox b = (JCheckBox)model.getElementAt(i);
            if (b.isSelected()) {
                selected.add(b.getText());
            } else {
                unselected.add(b.getText());
            }
        }
        m_UndefSelTags.clear();
        checkProjectSelection(jListApps, selected, unselected);
        checkProjectSelection(jListLibs, selected, unselected);
        checkProjectSelection(jListOther, selected, unselected);
        for (int i =0; i< model.getSize(); ++i) {
            JCheckBox b = (JCheckBox)model.getElementAt(i);
            if (m_UndefSelTags.contains(b.getText())) {
                if (b.isSelected()) {
                    b.setSelected(false);
                }
            }
        }
        CheckListRenderer r = (CheckListRenderer)(jListTags.getCellRenderer());
        r.setUndefinedSelection(m_UndefSelTags);
        jListTags.repaint();
    }
    private void jListSelectionChanged(java.awt.event.MouseEvent evt) {
        JList list = (JList) evt.getSource();
        if (list == jListTags) {
            Vector<String> selected = new Vector<String>();
            Vector<String> unselected = new Vector<String>();
            DefaultListModel model = (DefaultListModel)jListTags.getModel();

            int index = jListTags.locationToIndex(evt.getPoint());
            JCheckBox item = (JCheckBox)model.getElementAt(index);
            if (m_UndefSelTags.contains(item.getText()) && !item.isSelected()) {
                item.setSelected(true);
                jListTags.repaint();
            }
            if (item.isSelected()) {
                selected.add(item.getText());
            } else {
                unselected.add(item.getText());
            }
/*
            for (int i =0; i< model.getSize(); ++i) {
                JCheckBox b = (JCheckBox)model.getElementAt(i);
                if (b.isSelected()) {
                    selected.add(b.getText());
                } else {
                    unselected.add(b.getText());
                }
            }
 */
            selectProjects(jListApps, selected, unselected);
            selectProjects(jListLibs, selected, unselected);
            selectProjects(jListOther, selected, unselected);
        }
        countSelected();
    }
    private void writeSelected(JList list, OutputStream out) {
        DefaultListModel model = (DefaultListModel)list.getModel();
        for (int i =0; i< model.getSize(); ++i) {
            JCheckBox b = (JCheckBox)model.getElementAt(i);
            if (b.isSelected()) {
                try {
                    String s = b.getText() + "\n";
                    out.write(s.getBytes());
                } catch (Exception e) {
                    System.err.println(e.toString());
                    e.printStackTrace();
                }
            }
        }
    }
    private void processProjects() {
        m_State = eState.gotProjects;
        jButtonGOK.setText("Generate project");
        jButtonGCancel.setEnabled(true);
        int i = jTabbedPane.indexOfComponent(jPanelPrj);
        jTabbedPane.setEnabledAt(i,true);
        jTabbedPane.setSelectedIndex(i);
        SortedSet<String> alltags = new TreeSet<String>();
        try {
            String line;
            while (m_PtbOut != null && /*m_PtbOut.ready() &&*/
                   (line = m_PtbOut.readLine()) != null) {
                if (line.startsWith("*PTBGUI}*")) {
                    Iterator<String> tt = alltags.iterator();
                    while (tt.hasNext()) {
                        addProject(jListTags,tt.next(),false);
                    }
                    countSelected();
                    return;
                }
                String[] kv = line.split(",");
                if (kv.length > 2) {
                    String prj = kv[0].trim();
                    String type = kv[1].trim();
                    boolean selected = kv[2].trim().equals("select");
                    if (type.equals("lib")) {
                        addProject(jListLibs,prj,selected);
                    } else if (type.equals("app")) {
                        addProject(jListApps,prj,selected);
                    } else {
                        addProject(jListOther,prj,selected);
                    }
                    if (kv.length > 4) {
                        String[] tags = kv[4].trim().split("/");
                        for (int t=0; t<tags.length; ++t) {
                            alltags.add(tags[t]);
                        }
                        m_ProjectTags.put(prj, tags);
                    }
                }
            }
        } catch (Exception e) {
            System.err.println(e.toString());
            e.printStackTrace();
        }
    }
    private void doneProjects() {
        if (isPtbRunning()) {
            try {
                String s;
                s = "*PTBGUI{* projects" + "\n";
                m_PtbIn.write(s.getBytes());
                writeSelected(jListApps, m_PtbIn);
                writeSelected(jListLibs, m_PtbIn);
                writeSelected(jListOther, m_PtbIn);
                s = "*PTBGUI}* projects" + "\n";
                m_PtbIn.write(s.getBytes());
                m_PtbIn.flush();
            } catch (Exception e) {
                System.err.println(e.toString());
                e.printStackTrace();
            }
        }
    }
    private void processDone(int exitcode) {
        m_State = eState.donePtb;
        jButtonGOK.setVisible(false);
        jButtonGCancel.setEnabled(true);
        jButtonGCancel.setText("Finish");
        int i = jTabbedPane.indexOfComponent(jPanelDone);
        jTabbedPane.setEnabledAt(i,true);
        jTabbedPane.setSelectedIndex(i);
        if (exitcode == 0) {
            jLabelDone.setText("Configuration has completed successfully");
            jLabelSln.setText(m_ArgsParser.getSolution());
            jLabelSln.setVisible(true);
            jLabelGen.setVisible(true);
        } else {
            jLabelDone.setText("Configuration has FAILED");
            jLabelSln.setVisible(false);
            jLabelGen.setVisible(false);
        }
        File args = new File(m_ArgsParser.getSolution());
        m_Params = args.getParent() + File.separator +
            "project_tree_builder.ini.custom";
        File prm = new File(m_Params);
        jButtonSave.setEnabled(prm.exists());
    }
    private void startPtb() {
        updateData();
        String[] cmdline = m_ArgsParser.createCommandline();
        for (int i=0; i<cmdline.length; ++i) {
            System.err.print(cmdline[i]);
            System.err.print(" ");
        }
        System.err.println("");
        Runtime r = Runtime.getRuntime();
        try {
            String cwd = System.getProperty("user.dir");
            m_Ptb = r.exec(cmdline, null, new File(cwd));
            m_PtbIn = m_Ptb.getOutputStream();
            InputStream out = m_Ptb.getInputStream();
            InputStream err = m_Ptb.getErrorStream();
            m_PtbOut = new BufferedReader(new InputStreamReader(out));
            m_PtbErr = new BufferedReader(new InputStreamReader(err));
        } catch (Exception e) {
            System.err.println(e.toString());
            e.printStackTrace();
        }
    }
    private void stopPtb() {
        if (isPtbRunning()) {
            try {
                String s;
                s = "*PTBGUIabort*" + "\n";
                m_PtbIn.write(s.getBytes());
                m_PtbIn.flush();
                for (int i=0; i<5; ++i) {
                    if (isPtbRunning()) {
                        Thread.sleep(300);
                    } else {
                        break;
                    }
                }
            } catch (Exception e) {
                System.err.println(e.toString());
                e.printStackTrace();
            }
        }
        if (isPtbRunning()) {
            m_Ptb.destroy();
        }
        if (ArgsParser.existsPath(m_TmpParams)) {
            System.gc();
            (new File(m_TmpParams)).delete();
        }
    }
    private boolean isPtbRunning() {
        boolean isRunning = false;
        if (m_Ptb != null) {
            try {
                m_Ptb.exitValue();
            } catch (IllegalThreadStateException e) {
                isRunning = true;
            }
        }
        return isRunning;
    }

    /** This method is called from within the constructor to
     * initialize the form.
     * WARNING: Do NOT modify this code. The content of this method is
     * always regenerated by the Form Editor.
     */
    @SuppressWarnings("unchecked")
    // <editor-fold defaultstate="collapsed" desc="Generated Code">//GEN-BEGIN:initComponents
    private void initComponents() {

        jTabbedPane = new javax.swing.JTabbedPane();
        jPanelCmnd = new javax.swing.JPanel();
        jTextFieldTags = new javax.swing.JTextField();
        jLabel4 = new javax.swing.JLabel();
        jTextFieldLst = new javax.swing.JTextField();
        jLabel3 = new javax.swing.JLabel();
        jButtonLst = new javax.swing.JButton();
        jLabel11 = new javax.swing.JLabel();
        jButtonArgs = new javax.swing.JButton();
        jRadioButtonStatic = new javax.swing.JRadioButton();
        jRadioButtonDLL = new javax.swing.JRadioButton();
        jSeparator2 = new javax.swing.JSeparator();
        jLabel14 = new javax.swing.JLabel();
        jLabelArgs = new javax.swing.JLabel();
        jButtonArgsReset = new javax.swing.JButton();
        jButtonKTags = new javax.swing.JButton();
        jLabel10 = new javax.swing.JLabel();
        jTextFieldLstTags = new javax.swing.JTextField();
        jPanelAdvanced = new javax.swing.JPanel();
        jLabel1 = new javax.swing.JLabel();
        jTextFieldPtb = new javax.swing.JTextField();
        jButtonPtb = new javax.swing.JButton();
        jLabel5 = new javax.swing.JLabel();
        jTextFieldSolution = new javax.swing.JTextField();
        jCheckBoxNoPtb = new javax.swing.JCheckBox();
        jCheckBoxNws = new javax.swing.JCheckBox();
        jCheckBoxExt = new javax.swing.JCheckBox();
        jTextFieldExt = new javax.swing.JTextField();
        jLabel12 = new javax.swing.JLabel();
        jLabel6 = new javax.swing.JLabel();
        jTextFieldIde = new javax.swing.JTextField();
        jLabel7 = new javax.swing.JLabel();
        jTextFieldArch = new javax.swing.JTextField();
        jLabel2 = new javax.swing.JLabel();
        jTextFieldRoot = new javax.swing.JTextField();
        jButtonMore = new javax.swing.JButton();
        jPanelAdd = new javax.swing.JPanel();
        jLabel8 = new javax.swing.JLabel();
        jTextField3root = new javax.swing.JTextField();
        jLabel9 = new javax.swing.JLabel();
        jTextFieldCpath = new javax.swing.JTextField();
        jCheckBoxVTuneR = new javax.swing.JCheckBox();
        jCheckBoxVTuneD = new javax.swing.JCheckBox();
        jCheckBoxVTune = new javax.swing.JCheckBox();
        jPanelUserReq = new javax.swing.JPanel();
        jLabelUserReq = new javax.swing.JLabel();
        jScrollPane5 = new javax.swing.JScrollPane();
        jListUserReq = new javax.swing.JList();
        jPanelPrj = new javax.swing.JPanel();
        jPanel1 = new javax.swing.JPanel();
        jPanel2 = new javax.swing.JPanel();
        jLabelApps = new javax.swing.JLabel();
        jScrollPane1 = new javax.swing.JScrollPane();
        jListApps = new javax.swing.JList();
        jButtonAppsPlus = new javax.swing.JButton();
        jButtonAppsMinus = new javax.swing.JButton();
        jPanel3 = new javax.swing.JPanel();
        jLabelLibs = new javax.swing.JLabel();
        jScrollPane2 = new javax.swing.JScrollPane();
        jListLibs = new javax.swing.JList();
        jButtonLibsMinus = new javax.swing.JButton();
        jButtonLibsPlus = new javax.swing.JButton();
        jPanel4 = new javax.swing.JPanel();
        jLabelOther = new javax.swing.JLabel();
        jScrollPane3 = new javax.swing.JScrollPane();
        jListOther = new javax.swing.JList();
        jButtonOtherMinus = new javax.swing.JButton();
        jButtonOtherPlus = new javax.swing.JButton();
        jPanel5 = new javax.swing.JPanel();
        jLabelTags = new javax.swing.JLabel();
        jScrollPane4 = new javax.swing.JScrollPane();
        jListTags = new javax.swing.JList();
        jPanelDone = new javax.swing.JPanel();
        jLabelDone = new javax.swing.JLabel();
        jButtonSave = new javax.swing.JButton();
        jButtonStartOver = new javax.swing.JButton();
        jLabelGen = new javax.swing.JLabel();
        jLabelSln = new javax.swing.JLabel();
        jButtonGCancel = new javax.swing.JButton();
        jButtonGOK = new javax.swing.JButton();
        jLabel13 = new javax.swing.JLabel();

        setDefaultCloseOperation(javax.swing.WindowConstants.EXIT_ON_CLOSE);
        setTitle("The Toolkit configuration parameters");
        setCursor(new java.awt.Cursor(java.awt.Cursor.DEFAULT_CURSOR));
        addWindowListener(new java.awt.event.WindowAdapter() {
            public void windowClosing(java.awt.event.WindowEvent evt) {
                formWindowClosing(evt);
            }
        });

        jTextFieldTags.setText("jTextFieldTags");

        jLabel4.setText("Use project tags");

        jTextFieldLst.setText("jTextFieldLst");
        jTextFieldLst.addFocusListener(new java.awt.event.FocusAdapter() {
            public void focusLost(java.awt.event.FocusEvent evt) {
                jTextFieldLstFocusLost(evt);
            }
        });

        jLabel3.setText("Subtree, or LST file");

        jButtonLst.setText("...");
        jButtonLst.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonLstActionPerformed(evt);
            }
        });

        jLabel11.setText("Originally loaded from");

        jButtonArgs.setText("Load from file...");
        jButtonArgs.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonArgsActionPerformed(evt);
            }
        });

        jRadioButtonStatic.setText("Static");
        jRadioButtonStatic.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jRadioButtonStaticActionPerformed(evt);
            }
        });

        jRadioButtonDLL.setText("Dynamic");
        jRadioButtonDLL.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jRadioButtonDLLActionPerformed(evt);
            }
        });

        jLabel14.setText("Build libraries as");

        jLabelArgs.setText("l");
        jLabelArgs.setBorder(javax.swing.BorderFactory.createEtchedBorder());

        jButtonArgsReset.setText("Reset");
        jButtonArgsReset.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonArgsResetActionPerformed(evt);
            }
        });

        jButtonKTags.setText("...");
        jButtonKTags.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonKTagsActionPerformed(evt);
            }
        });

        jLabel10.setText("Default project tags");

        jTextFieldLstTags.setEditable(false);
        jTextFieldLstTags.setText("jTextFieldLstTags");

        org.jdesktop.layout.GroupLayout jPanelCmndLayout = new org.jdesktop.layout.GroupLayout(jPanelCmnd);
        jPanelCmnd.setLayout(jPanelCmndLayout);
        jPanelCmndLayout.setHorizontalGroup(
            jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanelCmndLayout.createSequentialGroup()
                .addContainerGap()
                .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                    .add(jPanelCmndLayout.createSequentialGroup()
                        .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                            .add(jPanelCmndLayout.createSequentialGroup()
                                .add(jLabel14, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 149, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                                .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.TRAILING, false)
                                    .add(org.jdesktop.layout.GroupLayout.LEADING, jRadioButtonDLL, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
                                    .add(org.jdesktop.layout.GroupLayout.LEADING, jRadioButtonStatic, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 166, Short.MAX_VALUE)))
                            .add(jSeparator2, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 624, Short.MAX_VALUE)
                            .add(jPanelCmndLayout.createSequentialGroup()
                                .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING, false)
                                    .add(jLabel11, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
                                    .add(jButtonArgs, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 167, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                                .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                                    .add(jLabelArgs, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 451, Short.MAX_VALUE)
                                    .add(jButtonArgsReset, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 87, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)))
                            .add(org.jdesktop.layout.GroupLayout.TRAILING, jPanelCmndLayout.createSequentialGroup()
                                .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.TRAILING)
                                    .add(org.jdesktop.layout.GroupLayout.LEADING, jPanelCmndLayout.createSequentialGroup()
                                        .add(jLabel4, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 149, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                                        .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                                        .add(jTextFieldTags, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 398, Short.MAX_VALUE))
                                    .add(jPanelCmndLayout.createSequentialGroup()
                                        .add(jLabel3, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 148, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                                        .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                                        .add(jTextFieldLst, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 399, Short.MAX_VALUE)))
                                .add(18, 18, 18)
                                .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                                    .add(jButtonKTags, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 55, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                                    .add(jButtonLst, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 55, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))))
                        .addContainerGap())
                    .add(jPanelCmndLayout.createSequentialGroup()
                        .add(jLabel10, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 149, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                        .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                        .add(jTextFieldLstTags, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 398, Short.MAX_VALUE)
                        .add(83, 83, 83))))
        );
        jPanelCmndLayout.setVerticalGroup(
            jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanelCmndLayout.createSequentialGroup()
                .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                    .add(jPanelCmndLayout.createSequentialGroup()
                        .addContainerGap()
                        .add(jRadioButtonStatic)
                        .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                        .add(jRadioButtonDLL))
                    .add(jPanelCmndLayout.createSequentialGroup()
                        .add(24, 24, 24)
                        .add(jLabel14)))
                .add(19, 19, 19)
                .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jLabel3)
                    .add(jTextFieldLst, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                    .add(jButtonLst, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 23, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.UNRELATED)
                .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jLabel4)
                    .add(jTextFieldTags, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                    .add(jButtonKTags, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 23, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.UNRELATED)
                .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.TRAILING)
                    .add(jLabel10)
                    .add(jTextFieldLstTags, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED, 27, Short.MAX_VALUE)
                .add(jSeparator2, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 10, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jButtonArgs, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 23, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                    .add(jButtonArgsReset))
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanelCmndLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jLabel11)
                    .add(jLabelArgs))
                .add(36, 36, 36))
        );

        jTabbedPane.addTab("Configuration", jPanelCmnd);

        jLabel1.setText("Project tree builder");

        jTextFieldPtb.setText("jTextFieldPtb");

        jButtonPtb.setText("...");
        jButtonPtb.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonPtbActionPerformed(evt);
            }
        });

        jLabel5.setText("Solution to generate");

        jTextFieldSolution.setText("jTextFieldSolution");

        jCheckBoxNoPtb.setText("Exclude 'Build PTB' step from CONFIGURE project");

        jCheckBoxNws.setText("Do not scan the whole source tree for missing project dependencies");

        jCheckBoxExt.setText("Use external libraries instead of missing in-tree ones");

        jTextFieldExt.setText("jTextFieldExt");

        jLabel12.setText("Look for missing libraries in this tree");

        jLabel6.setText("Target IDE");

        jTextFieldIde.setEditable(false);
        jTextFieldIde.setText("jTextFieldIde");

        jLabel7.setText("Target architecture");

        jTextFieldArch.setText("jTextFieldArch");

        jLabel2.setText("Source root");

        jTextFieldRoot.setText("jTextFieldRoot");
        jTextFieldRoot.addFocusListener(new java.awt.event.FocusAdapter() {
            public void focusLost(java.awt.event.FocusEvent evt) {
                jTextFieldRootFocusLost(evt);
            }
        });

        jButtonMore.setText("more >");
        jButtonMore.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonMoreActionPerformed(evt);
            }
        });

        org.jdesktop.layout.GroupLayout jPanelAdvancedLayout = new org.jdesktop.layout.GroupLayout(jPanelAdvanced);
        jPanelAdvanced.setLayout(jPanelAdvancedLayout);
        jPanelAdvancedLayout.setHorizontalGroup(
            jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanelAdvancedLayout.createSequentialGroup()
                .addContainerGap()
                .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                    .add(jPanelAdvancedLayout.createSequentialGroup()
                        .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                            .add(jLabel12, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 223, Short.MAX_VALUE)
                            .add(jLabel5, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 223, Short.MAX_VALUE)
                            .add(jLabel6, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 223, Short.MAX_VALUE)
                            .add(jLabel7, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 223, Short.MAX_VALUE))
                        .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                        .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                            .add(jPanelAdvancedLayout.createSequentialGroup()
                                .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.TRAILING, false)
                                    .add(org.jdesktop.layout.GroupLayout.LEADING, jTextFieldIde)
                                    .add(org.jdesktop.layout.GroupLayout.LEADING, jTextFieldArch, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 112, Short.MAX_VALUE))
                                .add(289, 289, 289))
                            .add(org.jdesktop.layout.GroupLayout.TRAILING, jPanelAdvancedLayout.createSequentialGroup()
                                .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.TRAILING)
                                    .add(org.jdesktop.layout.GroupLayout.LEADING, jTextFieldSolution, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 397, Short.MAX_VALUE)
                                    .add(jTextFieldExt, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 397, Short.MAX_VALUE)
                                    .add(org.jdesktop.layout.GroupLayout.LEADING, jButtonMore))
                                .addContainerGap())))
                    .add(jPanelAdvancedLayout.createSequentialGroup()
                        .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.TRAILING)
                            .add(jCheckBoxNws, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 444, Short.MAX_VALUE)
                            .add(jCheckBoxNoPtb, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 444, Short.MAX_VALUE)
                            .add(org.jdesktop.layout.GroupLayout.LEADING, jCheckBoxExt, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 444, Short.MAX_VALUE))
                        .add(190, 190, 190))
                    .add(jPanelAdvancedLayout.createSequentialGroup()
                        .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.TRAILING, false)
                            .add(org.jdesktop.layout.GroupLayout.LEADING, jLabel2, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
                            .add(org.jdesktop.layout.GroupLayout.LEADING, jLabel1, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 188, Short.MAX_VALUE))
                        .addPreferredGap(org.jdesktop.layout.LayoutStyle.UNRELATED)
                        .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                            .add(jPanelAdvancedLayout.createSequentialGroup()
                                .add(jTextFieldPtb, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 365, Short.MAX_VALUE)
                                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                                .add(jButtonPtb, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 55, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                            .add(jTextFieldRoot, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 426, Short.MAX_VALUE))
                        .addContainerGap())))
        );
        jPanelAdvancedLayout.setVerticalGroup(
            jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanelAdvancedLayout.createSequentialGroup()
                .addContainerGap()
                .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jLabel6)
                    .add(jTextFieldIde, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jLabel7)
                    .add(jTextFieldArch, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jLabel5)
                    .add(jTextFieldSolution, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.UNRELATED)
                .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jLabel12)
                    .add(jTextFieldExt, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jButtonMore)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.UNRELATED)
                .add(jCheckBoxNoPtb)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jCheckBoxNws)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jCheckBoxExt)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.UNRELATED)
                .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jLabel1)
                    .add(jTextFieldPtb, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                    .add(jButtonPtb, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 23, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanelAdvancedLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jLabel2)
                    .add(jTextFieldRoot, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                .addContainerGap(org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE))
        );

        jTabbedPane.addTab("Advanced", jPanelAdvanced);

        jLabel8.setText("Root directory of 3-rd party libraries");

        jTextField3root.setText("jTextField3root");

        jLabel9.setText("Path to the NCBI C Toolkit");

        jTextFieldCpath.setText("jTextFieldCpath");

        jCheckBoxVTuneR.setText("Release");

        jCheckBoxVTuneD.setText("Debug");

        jCheckBoxVTune.setText("Add VTune configurations:");
        jCheckBoxVTune.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jCheckBoxVTuneActionPerformed(evt);
            }
        });

        jLabelUserReq.setText("Additional requests");

        jListUserReq.setSelectionMode(javax.swing.ListSelectionModel.SINGLE_SELECTION);
        jScrollPane5.setViewportView(jListUserReq);

        org.jdesktop.layout.GroupLayout jPanelUserReqLayout = new org.jdesktop.layout.GroupLayout(jPanelUserReq);
        jPanelUserReq.setLayout(jPanelUserReqLayout);
        jPanelUserReqLayout.setHorizontalGroup(
            jPanelUserReqLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanelUserReqLayout.createSequentialGroup()
                .addContainerGap()
                .add(jPanelUserReqLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                    .add(jScrollPane5, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 122, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                    .add(jLabelUserReq, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 122, Short.MAX_VALUE))
                .addContainerGap())
        );
        jPanelUserReqLayout.setVerticalGroup(
            jPanelUserReqLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanelUserReqLayout.createSequentialGroup()
                .addContainerGap()
                .add(jLabelUserReq)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jScrollPane5, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 144, Short.MAX_VALUE)
                .addContainerGap())
        );

        org.jdesktop.layout.GroupLayout jPanelAddLayout = new org.jdesktop.layout.GroupLayout(jPanelAdd);
        jPanelAdd.setLayout(jPanelAddLayout);
        jPanelAddLayout.setHorizontalGroup(
            jPanelAddLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanelAddLayout.createSequentialGroup()
                .addContainerGap()
                .add(jPanelAddLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                    .add(jPanelUserReq, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                    .add(jPanelAddLayout.createSequentialGroup()
                        .add(jPanelAddLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING, false)
                            .add(jCheckBoxVTune, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
                            .add(jLabel9, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
                            .add(jLabel8, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 209, Short.MAX_VALUE))
                        .add(jPanelAddLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                            .add(jPanelAddLayout.createSequentialGroup()
                                .addPreferredGap(org.jdesktop.layout.LayoutStyle.UNRELATED)
                                .add(jPanelAddLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                                    .add(jTextFieldCpath, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 405, Short.MAX_VALUE)
                                    .add(jTextField3root, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 405, Short.MAX_VALUE)))
                            .add(jPanelAddLayout.createSequentialGroup()
                                .add(72, 72, 72)
                                .add(jCheckBoxVTuneR)
                                .add(18, 18, 18)
                                .add(jCheckBoxVTuneD)))))
                .addContainerGap())
        );
        jPanelAddLayout.setVerticalGroup(
            jPanelAddLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanelAddLayout.createSequentialGroup()
                .addContainerGap()
                .add(jPanelAddLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jLabel8)
                    .add(jTextField3root, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanelAddLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jLabel9)
                    .add(jTextFieldCpath, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanelAddLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jCheckBoxVTuneR)
                    .add(jCheckBoxVTuneD)
                    .add(jCheckBoxVTune))
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.UNRELATED)
                .add(jPanelUserReq, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
                .addContainerGap())
        );

        jTabbedPane.addTab("Libraries and Tools", jPanelAdd);

        jPanelPrj.setLayout(new java.awt.GridLayout(1, 0));

        jPanel2.setPreferredSize(new java.awt.Dimension(165, 280));

        jLabelApps.setText("Applications");

        jListApps.setSelectionMode(javax.swing.ListSelectionModel.SINGLE_SELECTION);
        jScrollPane1.setViewportView(jListApps);

        jButtonAppsPlus.setText("+all");
        jButtonAppsPlus.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonAppsPlusActionPerformed(evt);
            }
        });

        jButtonAppsMinus.setText("-all");
        jButtonAppsMinus.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonAppsMinusActionPerformed(evt);
            }
        });

        org.jdesktop.layout.GroupLayout jPanel2Layout = new org.jdesktop.layout.GroupLayout(jPanel2);
        jPanel2.setLayout(jPanel2Layout);
        jPanel2Layout.setHorizontalGroup(
            jPanel2Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanel2Layout.createSequentialGroup()
                .add(jButtonAppsPlus)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jButtonAppsMinus)
                .add(43, 43, 43))
            .add(jPanel2Layout.createSequentialGroup()
                .add(jLabelApps)
                .addContainerGap())
            .add(jScrollPane1, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 151, Short.MAX_VALUE)
        );
        jPanel2Layout.setVerticalGroup(
            jPanel2Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanel2Layout.createSequentialGroup()
                .add(jLabelApps)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jScrollPane1, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 215, Short.MAX_VALUE)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanel2Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jButtonAppsPlus)
                    .add(jButtonAppsMinus)))
        );

        jPanel3.setPreferredSize(new java.awt.Dimension(160, 280));

        jLabelLibs.setText("Libraries");

        jListLibs.setSelectionMode(javax.swing.ListSelectionModel.SINGLE_SELECTION);
        jScrollPane2.setViewportView(jListLibs);

        jButtonLibsMinus.setText("-all");
        jButtonLibsMinus.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonLibsMinusActionPerformed(evt);
            }
        });

        jButtonLibsPlus.setText("+all");
        jButtonLibsPlus.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonLibsPlusActionPerformed(evt);
            }
        });

        org.jdesktop.layout.GroupLayout jPanel3Layout = new org.jdesktop.layout.GroupLayout(jPanel3);
        jPanel3.setLayout(jPanel3Layout);
        jPanel3Layout.setHorizontalGroup(
            jPanel3Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanel3Layout.createSequentialGroup()
                .add(jLabelLibs)
                .addContainerGap(116, Short.MAX_VALUE))
            .add(jPanel3Layout.createSequentialGroup()
                .add(jButtonLibsPlus)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jButtonLibsMinus)
                .addContainerGap())
            .add(jScrollPane2, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 156, Short.MAX_VALUE)
        );
        jPanel3Layout.setVerticalGroup(
            jPanel3Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanel3Layout.createSequentialGroup()
                .add(jLabelLibs)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jScrollPane2, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 215, Short.MAX_VALUE)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanel3Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jButtonLibsPlus)
                    .add(jButtonLibsMinus)))
        );

        jPanel4.setPreferredSize(new java.awt.Dimension(150, 280));

        jLabelOther.setText("Other");

        jListOther.setSelectionMode(javax.swing.ListSelectionModel.SINGLE_SELECTION);
        jScrollPane3.setViewportView(jListOther);

        jButtonOtherMinus.setText("-all");
        jButtonOtherMinus.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonOtherMinusActionPerformed(evt);
            }
        });

        jButtonOtherPlus.setText("+all");
        jButtonOtherPlus.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonOtherPlusActionPerformed(evt);
            }
        });

        org.jdesktop.layout.GroupLayout jPanel4Layout = new org.jdesktop.layout.GroupLayout(jPanel4);
        jPanel4.setLayout(jPanel4Layout);
        jPanel4Layout.setHorizontalGroup(
            jPanel4Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanel4Layout.createSequentialGroup()
                .add(jLabelOther)
                .addContainerGap(118, Short.MAX_VALUE))
            .add(jPanel4Layout.createSequentialGroup()
                .add(jButtonOtherPlus)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jButtonOtherMinus)
                .addContainerGap())
            .add(jScrollPane3, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 146, Short.MAX_VALUE)
        );
        jPanel4Layout.setVerticalGroup(
            jPanel4Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanel4Layout.createSequentialGroup()
                .add(jLabelOther)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jScrollPane3, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 215, Short.MAX_VALUE)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanel4Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jButtonOtherPlus)
                    .add(jButtonOtherMinus)))
        );

        jPanel5.setPreferredSize(new java.awt.Dimension(140, 280));

        jLabelTags.setText("Tags");

        jListTags.setSelectionMode(javax.swing.ListSelectionModel.SINGLE_SELECTION);
        jScrollPane4.setViewportView(jListTags);

        org.jdesktop.layout.GroupLayout jPanel5Layout = new org.jdesktop.layout.GroupLayout(jPanel5);
        jPanel5.setLayout(jPanel5Layout);
        jPanel5Layout.setHorizontalGroup(
            jPanel5Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanel5Layout.createSequentialGroup()
                .add(jLabelTags)
                .addContainerGap(111, Short.MAX_VALUE))
            .add(jScrollPane4, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 134, Short.MAX_VALUE)
        );
        jPanel5Layout.setVerticalGroup(
            jPanel5Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanel5Layout.createSequentialGroup()
                .add(jLabelTags)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jScrollPane4, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 215, Short.MAX_VALUE)
                .add(29, 29, 29))
        );

        org.jdesktop.layout.GroupLayout jPanel1Layout = new org.jdesktop.layout.GroupLayout(jPanel1);
        jPanel1.setLayout(jPanel1Layout);
        jPanel1Layout.setHorizontalGroup(
            jPanel1Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanel1Layout.createSequentialGroup()
                .addContainerGap()
                .add(jPanel2, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 151, Short.MAX_VALUE)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanel3, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 156, Short.MAX_VALUE)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanel4, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 146, Short.MAX_VALUE)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED)
                .add(jPanel5, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 134, Short.MAX_VALUE)
                .add(29, 29, 29))
        );
        jPanel1Layout.setVerticalGroup(
            jPanel1Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanel1Layout.createSequentialGroup()
                .addContainerGap()
                .add(jPanel1Layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                    .add(jPanel5, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 264, Short.MAX_VALUE)
                    .add(jPanel4, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 264, Short.MAX_VALUE)
                    .add(org.jdesktop.layout.GroupLayout.TRAILING, jPanel3, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 264, Short.MAX_VALUE)
                    .add(jPanel2, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 264, Short.MAX_VALUE))
                .addContainerGap())
        );

        jPanelPrj.add(jPanel1);

        jTabbedPane.addTab("Projects", jPanelPrj);

        jLabelDone.setHorizontalAlignment(javax.swing.SwingConstants.CENTER);
        jLabelDone.setText("jLabelDone");

        jButtonSave.setText("Save configuration parameters into a file...");
        jButtonSave.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonSaveActionPerformed(evt);
            }
        });

        jButtonStartOver.setText("Start over");
        jButtonStartOver.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonStartOverActionPerformed(evt);
            }
        });

        jLabelGen.setHorizontalAlignment(javax.swing.SwingConstants.CENTER);
        jLabelGen.setText("Generated project file:");

        jLabelSln.setHorizontalAlignment(javax.swing.SwingConstants.CENTER);
        jLabelSln.setText("jLabelSln");

        org.jdesktop.layout.GroupLayout jPanelDoneLayout = new org.jdesktop.layout.GroupLayout(jPanelDone);
        jPanelDone.setLayout(jPanelDoneLayout);
        jPanelDoneLayout.setHorizontalGroup(
            jPanelDoneLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(jPanelDoneLayout.createSequentialGroup()
                .addContainerGap()
                .add(jPanelDoneLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                    .add(org.jdesktop.layout.GroupLayout.TRAILING, jButtonStartOver, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 305, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                    .add(org.jdesktop.layout.GroupLayout.TRAILING, jButtonSave, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 305, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                    .add(jLabelDone, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 624, Short.MAX_VALUE)
                    .add(jLabelGen, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 624, Short.MAX_VALUE)
                    .add(jLabelSln, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 624, Short.MAX_VALUE))
                .addContainerGap())
        );
        jPanelDoneLayout.setVerticalGroup(
            jPanelDoneLayout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(org.jdesktop.layout.GroupLayout.TRAILING, jPanelDoneLayout.createSequentialGroup()
                .add(29, 29, 29)
                .add(jLabelDone)
                .add(18, 18, 18)
                .add(jLabelGen)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.UNRELATED)
                .add(jLabelSln)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED, 118, Short.MAX_VALUE)
                .add(jButtonSave)
                .addPreferredGap(org.jdesktop.layout.LayoutStyle.UNRELATED)
                .add(jButtonStartOver)
                .addContainerGap())
        );

        jTabbedPane.addTab("Done", jPanelDone);

        jButtonGCancel.setText("Cancel");
        jButtonGCancel.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonGCancelActionPerformed(evt);
            }
        });

        jButtonGOK.setText("Next");
        jButtonGOK.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                jButtonGOKActionPerformed(evt);
            }
        });

        jLabel13.setText("  version 1.3.1");
        jLabel13.setEnabled(false);

        org.jdesktop.layout.GroupLayout layout = new org.jdesktop.layout.GroupLayout(getContentPane());
        getContentPane().setLayout(layout);
        layout.setHorizontalGroup(
            layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(layout.createSequentialGroup()
                .addContainerGap()
                .add(layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
                    .add(org.jdesktop.layout.GroupLayout.TRAILING, layout.createSequentialGroup()
                        .add(jLabel13)
                        .addPreferredGap(org.jdesktop.layout.LayoutStyle.RELATED, 228, Short.MAX_VALUE)
                        .add(jButtonGOK, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE, 276, org.jdesktop.layout.GroupLayout.PREFERRED_SIZE)
                        .addPreferredGap(org.jdesktop.layout.LayoutStyle.UNRELATED)
                        .add(jButtonGCancel))
                    .add(jTabbedPane, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 649, Short.MAX_VALUE))
                .addContainerGap())
        );
        layout.setVerticalGroup(
            layout.createParallelGroup(org.jdesktop.layout.GroupLayout.LEADING)
            .add(org.jdesktop.layout.GroupLayout.TRAILING, layout.createSequentialGroup()
                .addContainerGap()
                .add(jTabbedPane, org.jdesktop.layout.GroupLayout.DEFAULT_SIZE, 314, Short.MAX_VALUE)
                .add(11, 11, 11)
                .add(layout.createParallelGroup(org.jdesktop.layout.GroupLayout.BASELINE)
                    .add(jButtonGCancel)
                    .add(jButtonGOK)
                    .add(jLabel13))
                .addContainerGap())
        );

        pack();
    }// </editor-fold>//GEN-END:initComponents

    private void jButtonPtbActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonPtbActionPerformed
        JFileChooser fd = new JFileChooser();
        fd.setDialogTitle(jLabel1.getText());
        File f = new File(jTextFieldPtb.getText());
        fd.setCurrentDirectory(f.getParentFile());
        if (fd.showOpenDialog(this) == JFileChooser.APPROVE_OPTION) {
           setPathText(jTextFieldPtb, fd.getSelectedFile().getPath(),true);
        }
    }//GEN-LAST:event_jButtonPtbActionPerformed

    private void formWindowClosing(java.awt.event.WindowEvent evt) {//GEN-FIRST:event_formWindowClosing
        stopPtb();
        processPtbOutput();
    }//GEN-LAST:event_formWindowClosing

    private void jButtonLstActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonLstActionPerformed
        JFileChooser fd = new JFileChooser();
        fd.setDialogTitle(jLabel3.getText());
        String root = jTextFieldRoot.getText();
        String lst = root + File.separator + jTextFieldLst.getText();
        File flst = new File(ArgsParser.existsPath(lst) ? lst : root);
        fd.setCurrentDirectory(flst.isDirectory()? flst : flst.getParentFile());
        if (fd.showOpenDialog(this) == JFileChooser.APPROVE_OPTION) {
            String f = fd.getSelectedFile().getPath();
            if (f.startsWith(root)) {
                f = f.substring(root.length());
                if (f.startsWith("\\") || f.startsWith("/")) {
                    f = f.substring(1);
                }
                setPathText(jTextFieldLst, root, f);
            } else {
                JOptionPane.showMessageDialog( this,
                    "The file must be in the same tree",
                    "Error", JOptionPane.ERROR_MESSAGE);
            }
            initTagsFromSubtree();
        }
    }//GEN-LAST:event_jButtonLstActionPerformed

    private void jButtonArgsActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonArgsActionPerformed
        JFileChooser fd = new JFileChooser();
        fd.setDialogTitle(jLabel3.getText());
        String args = jLabelArgs.getText();
        if (ArgsParser.existsPath(args)) {
            File f = new File(args);
            fd.setCurrentDirectory(f.getParentFile());
        } else {
            fd.setCurrentDirectory(new File("."));
        }
        if (fd.showOpenDialog(this) == JFileChooser.APPROVE_OPTION) {
            initData(fd.getSelectedFile().getPath(), false);
        }
}//GEN-LAST:event_jButtonArgsActionPerformed

    private void jButtonGCancelActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonGCancelActionPerformed
        stopPtb();
        processPtbOutput();
    }//GEN-LAST:event_jButtonGCancelActionPerformed

    private void jButtonGOKActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonGOKActionPerformed
        if (m_State == eState.beforePtb) {
            startPtb();
        } else if (m_State == eState.got3rdparty) {
            doneAdditionalParams();
        } else if (m_State == eState.gotProjects) {
            doneProjects();
        }
        if (isPtbRunning()) {
            jButtonGOK.setText("Please wait...");
            jButtonGOK.setForeground(Color.red);
            jButtonGOK.paintImmediately(0,0,
                jButtonGOK.getWidth(), jButtonGOK.getHeight());
            jButtonGOK.setForeground(SystemColor.controlText);
            jButtonGCancel.setText("Stop");
        }
        processPtbOutput();
    }//GEN-LAST:event_jButtonGOKActionPerformed

    private void jButtonSaveActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonSaveActionPerformed
        JFileChooser fd = new JFileChooser();
        fd.setDialogTitle(jButtonSave.getText());
        String args = jLabelArgs.getText();
        if (ArgsParser.existsPath(args)) {
            File f = new File(args);
            fd.setCurrentDirectory(f.getParentFile());
        } else {
            fd.setCurrentDirectory(new File("."));
        }
        if (fd.showSaveDialog(this) == JFileChooser.APPROVE_OPTION) {
            try {
                String f = fd.getSelectedFile().getPath();
                copyFile(m_Params, f);
            } catch (Exception e) {
                System.err.println(e.toString());
                e.printStackTrace();
            }
        }
    }//GEN-LAST:event_jButtonSaveActionPerformed
    private void jButtonArgsResetActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonArgsResetActionPerformed
        initData();
    }//GEN-LAST:event_jButtonArgsResetActionPerformed
    private void jButtonAppsPlusActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonAppsPlusActionPerformed
        selectProjects(jListApps, true);
    }//GEN-LAST:event_jButtonAppsPlusActionPerformed
    private void jButtonAppsMinusActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonAppsMinusActionPerformed
        selectProjects(jListApps, false);
    }//GEN-LAST:event_jButtonAppsMinusActionPerformed
    private void jButtonLibsPlusActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonLibsPlusActionPerformed
        selectProjects(jListLibs, true);
    }//GEN-LAST:event_jButtonLibsPlusActionPerformed
    private void jButtonLibsMinusActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonLibsMinusActionPerformed
        selectProjects(jListLibs, false);
    }//GEN-LAST:event_jButtonLibsMinusActionPerformed
    private void jButtonOtherPlusActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonOtherPlusActionPerformed
        selectProjects(jListOther, true);
    }//GEN-LAST:event_jButtonOtherPlusActionPerformed
    private void jButtonOtherMinusActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonOtherMinusActionPerformed
        selectProjects(jListOther, false);
    }//GEN-LAST:event_jButtonOtherMinusActionPerformed
    private void jButtonStartOverActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonStartOverActionPerformed
        resetState();
        initData();
    }//GEN-LAST:event_jButtonStartOverActionPerformed
    private void jButtonMoreActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonMoreActionPerformed
        showMoreAdvanced(!jCheckBoxNoPtb.isVisible());
    }//GEN-LAST:event_jButtonMoreActionPerformed
    private void jCheckBoxVTuneActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jCheckBoxVTuneActionPerformed
        jCheckBoxVTuneD.setVisible(jCheckBoxVTune.isSelected());
        jCheckBoxVTuneR.setVisible(jCheckBoxVTune.isSelected());
    }//GEN-LAST:event_jCheckBoxVTuneActionPerformed
    private void jRadioButtonStaticActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jRadioButtonStaticActionPerformed
        adjustBuildType();
    }//GEN-LAST:event_jRadioButtonStaticActionPerformed

    private void jRadioButtonDLLActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jRadioButtonDLLActionPerformed
        adjustBuildType();
    }//GEN-LAST:event_jRadioButtonDLLActionPerformed

    private void jButtonKTagsActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonKTagsActionPerformed
        if (m_KtagsDlg == null) {
            m_KtagsDlg = new KTagsDialog(this,false);
            m_KtagsDlg.setLocationRelativeTo(this);
        }
        if (!m_KtagsDlg.isVisible()) {
            m_KtagsDlg.setTextData(m_KnownTags, m_CompositeTags);
            m_KtagsDlg.setVisible(true);
        }
    }//GEN-LAST:event_jButtonKTagsActionPerformed

    private void jTextFieldLstFocusLost(java.awt.event.FocusEvent evt) {//GEN-FIRST:event_jTextFieldLstFocusLost
        initTagsFromSubtree();
    }//GEN-LAST:event_jTextFieldLstFocusLost

    private void jTextFieldRootFocusLost(java.awt.event.FocusEvent evt) {//GEN-FIRST:event_jTextFieldRootFocusLost
        initTagsFromSubtree();
    }//GEN-LAST:event_jTextFieldRootFocusLost

    /**
    * @param args the command line arguments
    */
    public static void main(String args[]) {
        PtbguiMain ptbgui = new PtbguiMain();
        ptbgui.initData(args);
        ptbgui.setLocationRelativeTo(null);
        ptbgui.setVisible(true);
    }

    // Variables declaration - do not modify//GEN-BEGIN:variables
    private javax.swing.JButton jButtonAppsMinus;
    private javax.swing.JButton jButtonAppsPlus;
    private javax.swing.JButton jButtonArgs;
    private javax.swing.JButton jButtonArgsReset;
    private javax.swing.JButton jButtonGCancel;
    private javax.swing.JButton jButtonGOK;
    private javax.swing.JButton jButtonKTags;
    private javax.swing.JButton jButtonLibsMinus;
    private javax.swing.JButton jButtonLibsPlus;
    private javax.swing.JButton jButtonLst;
    private javax.swing.JButton jButtonMore;
    private javax.swing.JButton jButtonOtherMinus;
    private javax.swing.JButton jButtonOtherPlus;
    private javax.swing.JButton jButtonPtb;
    private javax.swing.JButton jButtonSave;
    private javax.swing.JButton jButtonStartOver;
    private javax.swing.JCheckBox jCheckBoxExt;
    private javax.swing.JCheckBox jCheckBoxNoPtb;
    private javax.swing.JCheckBox jCheckBoxNws;
    private javax.swing.JCheckBox jCheckBoxVTune;
    private javax.swing.JCheckBox jCheckBoxVTuneD;
    private javax.swing.JCheckBox jCheckBoxVTuneR;
    private javax.swing.JLabel jLabel1;
    private javax.swing.JLabel jLabel10;
    private javax.swing.JLabel jLabel11;
    private javax.swing.JLabel jLabel12;
    private javax.swing.JLabel jLabel13;
    private javax.swing.JLabel jLabel14;
    private javax.swing.JLabel jLabel2;
    private javax.swing.JLabel jLabel3;
    private javax.swing.JLabel jLabel4;
    private javax.swing.JLabel jLabel5;
    private javax.swing.JLabel jLabel6;
    private javax.swing.JLabel jLabel7;
    private javax.swing.JLabel jLabel8;
    private javax.swing.JLabel jLabel9;
    private javax.swing.JLabel jLabelApps;
    private javax.swing.JLabel jLabelArgs;
    private javax.swing.JLabel jLabelDone;
    private javax.swing.JLabel jLabelGen;
    private javax.swing.JLabel jLabelLibs;
    private javax.swing.JLabel jLabelOther;
    private javax.swing.JLabel jLabelSln;
    private javax.swing.JLabel jLabelTags;
    private javax.swing.JLabel jLabelUserReq;
    private javax.swing.JList jListApps;
    private javax.swing.JList jListLibs;
    private javax.swing.JList jListOther;
    private javax.swing.JList jListTags;
    private javax.swing.JList jListUserReq;
    private javax.swing.JPanel jPanel1;
    private javax.swing.JPanel jPanel2;
    private javax.swing.JPanel jPanel3;
    private javax.swing.JPanel jPanel4;
    private javax.swing.JPanel jPanel5;
    private javax.swing.JPanel jPanelAdd;
    private javax.swing.JPanel jPanelAdvanced;
    private javax.swing.JPanel jPanelCmnd;
    private javax.swing.JPanel jPanelDone;
    private javax.swing.JPanel jPanelPrj;
    private javax.swing.JPanel jPanelUserReq;
    private javax.swing.JRadioButton jRadioButtonDLL;
    private javax.swing.JRadioButton jRadioButtonStatic;
    private javax.swing.JScrollPane jScrollPane1;
    private javax.swing.JScrollPane jScrollPane2;
    private javax.swing.JScrollPane jScrollPane3;
    private javax.swing.JScrollPane jScrollPane4;
    private javax.swing.JScrollPane jScrollPane5;
    private javax.swing.JSeparator jSeparator2;
    private javax.swing.JTabbedPane jTabbedPane;
    private javax.swing.JTextField jTextField3root;
    private javax.swing.JTextField jTextFieldArch;
    private javax.swing.JTextField jTextFieldCpath;
    private javax.swing.JTextField jTextFieldExt;
    private javax.swing.JTextField jTextFieldIde;
    private javax.swing.JTextField jTextFieldLst;
    private javax.swing.JTextField jTextFieldLstTags;
    private javax.swing.JTextField jTextFieldPtb;
    private javax.swing.JTextField jTextFieldRoot;
    private javax.swing.JTextField jTextFieldSolution;
    private javax.swing.JTextField jTextFieldTags;
    // End of variables declaration//GEN-END:variables

}
