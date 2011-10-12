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
 *   Command line parser
 */

package ptbgui;
import java.io.File;
import java.io.IOException;
import java.util.Vector;

public class ArgsParser {
    final private String m_Undef;
    private int m_nCmd;
    private String m_Ptb, m_Root, m_Subtree;
    private String m_BuildRoot, m_BuildRootToSolution;
    private String m_SolutionPath, m_SolutionFile, m_SolutionFileExt;
    private String m_extroot, m_projtag, m_ide, m_arch, m_logfile, m_conffile;
    private String m_Unknown;
    private boolean m_dll;
    public boolean m_nobuildptb, m_ext, m_nws, m_i, m_dtdep;
    private boolean m_projtagFromLst;
    private String m_ArgsFile;

    public ArgsParser() {
        m_Undef = new String("UNDEFINED");
        m_dll = m_nobuildptb = m_ext = m_nws = false;
        m_i = m_dtdep = false;
        m_Unknown = new String();
        m_nCmd = 0;
    }
    public void init(String args[]) {
        m_projtagFromLst = false;
        m_nCmd = args.length;
        eArg dest = eArg.undefined;
        int iPositional = 0;
        for (int i = 0; i < args.length; ++i) {
            if (i == 0) {
                m_Ptb = toCanonicalPath(args[i]);
                ++iPositional;
                continue;
            }
            if (args[i].length() > 0 && args[i].charAt(0) == '-') {
                dest = toEnum(args[i]);
                switch (dest) {
                    case dll:        m_dll        = true; break;
                    case nobuildptb: m_nobuildptb = true; break;
                    case ext:        m_ext        = true; break;
                    case nws:        m_nws        = true; break;
                    case undefined:  m_Unknown.concat(args[i]); break;
                    case cfg:        --m_nCmd;            break;
                    case i:          m_i          = true; break;
                    case dtdep:      m_dtdep      = true; break;
                }
            } else {
                if (dest != eArg.undefined) {
                    switch (dest) {
                        case extroot:
                            m_extroot  = toCanonicalPath(args[i]);
                            break;
                        case projtag:  m_projtag  = args[i]; break;
                        case ide:      m_ide      = args[i]; break;
                        case arch:     m_arch     = args[i]; break;
                        case logfile:
                            m_logfile  = toCanonicalPath(args[i]);
                            break;
                        case conffile:
                            m_conffile = toCanonicalPath(args[i]);
                            break;
                        case args:
                            m_ArgsFile = toCanonicalPath(args[i]);
                            break;
                    }
                    dest = eArg.undefined;
                } else {
                    switch (iPositional) {
                    case 1:
                        m_Root = toCanonicalPath(args[i]);
                        break;
                    case 2:
                        m_Subtree = args[i];
                        break;
                    case 3:
                        parseSolutionPath(toCanonicalPath(args[i]));
                        break;
                    }
                    ++iPositional;
                }
            }
        }
    }
    private void parseSolutionPath(String solution) {
        File f = new File(solution);
        m_SolutionPath = f.getParent();
        m_SolutionFile = f.getName();
        m_SolutionFileExt = "";
        if (m_SolutionFile.endsWith(".sln")) {
            m_SolutionFileExt = ".sln";
            m_SolutionFile = m_SolutionFile.replaceFirst("[.]sln$","");
        }
        if (f.getParentFile().getName().equals("build")) {
            String t = f.getParentFile().getParentFile().getName();
            if (t.equals("static") || t.equals("dll") || t.equals("user")) {
                m_BuildRoot = f.getParentFile().getParentFile().getParent();
                m_BuildRootToSolution = t + File.separator + "build";
            }
        }
        else if (f.getParentFile().getName().equals("static")) {
            m_BuildRoot = f.getParentFile().getParent();
            m_BuildRootToSolution = "static";
        }
        else if (f.getParentFile().getName().equals("dll")) {
            m_BuildRoot = f.getParentFile().getParent();
            m_BuildRootToSolution = "dll";
        }
        if (m_BuildRoot == null) {
            m_BuildRoot = m_SolutionPath;
            m_BuildRootToSolution = ".";
        }
    }
    public String[] createCommandline() {
        Vector<String> vcmd = new Vector<String>();
        vcmd.add(m_Ptb);
        if (m_ArgsFile != null && m_ArgsFile.length() != 0) {
            vcmd.add("-args");
            vcmd.add(m_ArgsFile);
            if (m_i) {
                vcmd.add("-" + eArg.i.toString());
            }
            if (m_dtdep) {
                vcmd.add("-" + eArg.dtdep.toString());
            }
            if (m_logfile != null && m_logfile.length() != 0) {
                vcmd.add("-" + eArg.logfile.toString());
                String l = getSolution();
                if (!l.equals(m_Undef)) {
                    vcmd.add(l+"_configuration_log.txt");
                } else {
                    vcmd.add(m_logfile);
                }
            }
        } else {
            if (m_nobuildptb) {
                vcmd.add("-" + eArg.nobuildptb.toString());
            }
            if (m_dll) {
                vcmd.add("-" + eArg.dll.toString());
            }
            if (m_i) {
                vcmd.add("-" + eArg.i.toString());
            }
            if (m_dtdep) {
                vcmd.add("-" + eArg.dtdep.toString());
            }
            if (m_nws) {
                vcmd.add("-" + eArg.nws.toString());
            }
            if (m_ext) {
                vcmd.add("-" + eArg.ext.toString());
            }
            if (m_ext && m_extroot != null && m_extroot.length() != 0) {
                vcmd.add("-" + eArg.extroot.toString());
                vcmd.add(m_extroot);
            }
            if (m_projtag != null && m_projtag.length() != 0) {
                vcmd.add("-" + eArg.projtag.toString());
                vcmd.add(m_projtag);
            } /*else if (m_projtagFromLst) {
                vcmd.add("-" + eArg.projtag.toString());
                vcmd.add("\"\"");
            }*/
            if (m_ide != null && m_ide.length() != 0) {
                vcmd.add("-" + eArg.ide.toString());
                vcmd.add(m_ide);
            }
            if (m_arch != null && m_arch.length() != 0) {
                vcmd.add("-" + eArg.arch.toString());
                vcmd.add(m_arch);
            }
            if (m_logfile != null && m_logfile.length() != 0) {
                vcmd.add("-" + eArg.logfile.toString());
                String l = getSolution();
                if (!l.equals(m_Undef)) {
                    vcmd.add(l+"_configuration_log.txt");
                } else {
                    vcmd.add(m_logfile);
                }
            }
            if (m_conffile != null && m_conffile.length() != 0) {
                vcmd.add("-" + eArg.conffile.toString());
                vcmd.add(m_conffile);
            }
        }
        vcmd.add(m_Root);
        if (m_Subtree.length() > 0) {
            vcmd.add(m_Subtree);
        } else {
            vcmd.add("\"\"");
        }
        vcmd.add(getSolution());
        String[] cmd = new String[vcmd.size()];
        for (int i = 0; i < vcmd.size(); ++i) {
            cmd[i] = vcmd.get(i).toString();
        }
        return cmd;
    }
    public static String toCanonicalPath(String path) {
        File f0 = new File(path);
        if (!f0.isAbsolute()) {
            path = System.getProperty("user.dir") + File.separator + path;
        }
        File f = new File(path);
        path = f.getAbsolutePath();
        try {
            path = f.getCanonicalPath();
        } catch (IOException ex) {
        }
        return path;
    }
    public static boolean existsPath(String path) {
        if (path != null) {
            File f = new File(path);
            return f.exists();
        }
        return false;
    }
    public String getPtb() {
        return (m_Ptb != null && m_Ptb.length() > 0) ? m_Ptb : m_Undef;
    }
    public void setPtb(String ptb) {
        m_Ptb = ptb.trim();
    }
    public String getRoot() {
        return (m_Root != null && m_Root.length() > 0) ? m_Root : m_Undef;
    }
    public void setRoot(String root) {
        m_Root = root.trim();
    }
    public String getSubtree() {
        return (m_Subtree != null && m_Subtree.length() > 0) ?
            m_Subtree : m_Undef;
    }
    public void setSubtree(String subtree) {
        m_Subtree = subtree.trim();
    }
    public String getBuildRoot() {
        return (m_BuildRoot != null) ? m_BuildRoot : m_Undef;
    }
    public boolean getDll() {
        return m_dll;
    }
    public void setDll(boolean dll, boolean adjustpath) {
        if (m_dll != dll) {
            if (adjustpath) {
            if (m_dll) {
                if (!m_BuildRoot.equals(m_SolutionPath)) {
                    m_BuildRootToSolution =
                        m_BuildRootToSolution.replaceFirst("^dll", "static");
                }
                m_SolutionFile = m_SolutionFile.replaceFirst("_dll$","");
            } else {
                if (!m_BuildRoot.equals(m_SolutionPath)) {
                    m_BuildRootToSolution =
                        m_BuildRootToSolution.replaceFirst("^static", "dll");
                }
                m_SolutionFile = m_SolutionFile + "_dll";
            }
            }
            m_dll = dll;
        }
    }
    public String getSolution() {
        try {
            if (!m_BuildRoot.equals(m_SolutionPath)) {
                return m_BuildRoot           + File.separator +
                       m_BuildRootToSolution + File.separator +
                       m_SolutionFile + m_SolutionFileExt;
            } else {
                return m_SolutionPath + File.separator +
                       m_SolutionFile + m_SolutionFileExt;
            }
        } catch (Exception e) {
        }
        return m_Undef;
    }
    public String getSolutionFile() {
        return (m_SolutionFile != null && m_SolutionFile.length() > 0) ?
            m_SolutionFile : m_Undef;
    }
    public void setSolutionFile(String solution) {
        m_SolutionFile = solution.trim();
    }
    public String getExtRoot() {
        return m_extroot != null ? m_extroot : "";
    }
    public void setExtRoot(String exroot) {
        m_extroot = exroot.trim();
    }
    public String getProjTag() {
        return (m_projtag != null && m_projtag.length() != 0) ? m_projtag : "*";
    }
    public void setProjTag(String tag) {
        String t = tag.trim();
        m_projtag = (t.equals("*") || t.equals("#")) ? "" : tag;
    }
    public void setProjTagFromLst(String tag) {
        String t = tag.trim();
        m_projtag = (t.equals("*") || t.equals("#")) ? "" : tag;
        m_projtagFromLst = true;
    }
    public String getIde() {
        return m_ide != null ? m_ide : "";
    }
    public String getArch() {
        return m_arch != null ? m_arch : "";
    }
    public void setArch(String arch) {
        m_arch = arch.trim();
    }
    public void setArgsFile(String args) {
        if (args == null) {
            m_ArgsFile = "";
        } else {
            String t = args.trim();
            m_ArgsFile = existsPath(t) ? t : "" ;
        }
    }
    public String getArgsFile() {
        return (m_ArgsFile != null && existsPath(m_ArgsFile)) ? m_ArgsFile : "";
    }

    enum eArg {
        undefined,
        dll,
        nobuildptb,
        ext,
        nws,
        extroot,
        projtag,
        ide,
        arch,
        logfile,
        conffile,
        cfg,
        i,
        dtdep,
        args
    }
    private eArg toEnum(String a) {
        eArg t = eArg.undefined;
        try {
            t = eArg.valueOf(a.replace('-', ' ').trim());
        } catch (Exception e) {
            e.printStackTrace();
        }
        return t;
    }
}
