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
 * File Description:  SWIG interface file for gui
 *
 */

// nested classes that cause problems
%ignore *::CConvertCache_SCacheKeySort;
%ignore *::CRegularGridGen_const_iterator;
%ignore *::CGuiRegistry_CReadView::GetTopKeys;
%ignore *::CGuiRegistry_CReadView::GetKeys;
%ignore *::CGuiRegistry_SKeyInfo::access;
%ignore *::CPluginTask_STaskInfo::job_id;
%ignore *::CPluginTask_STaskInfo::status;
%{typedef CGuiRegistry::CReadView CReadView;%}  // nasty hack
%ignore *::CGuiDiagHandler_CDiagEvent;

// nested classes problem (CGuiRegistry::CReadView)
%ignore ncbi::CSettings::LoadPluginConfig;
%ignore ncbi::CPluginDlg::LoadSettings;
%ignore ncbi::CPluginDlg::SaveSettings;

// declared but not defined
%ignore ncbi::CFeatConfigFactoryDefaults::kNotDefined;
%ignore ncbi::CSettings::GetBuiltInPluginConfig();
%ignore ncbi::CSettings::ReadPluginConfig(const string& pcfile,
                                          CGuiRegistry& pcc);
%ignore ncbi::CEvent::GetRefs;
%ignore ncbi::CEvent::AddRef;
%ignore ncbi::CBookmark::SetRange;
%ignore *::CPluginValue::SetDataHandle;
%ignore ncbi::objects::CGBProjectHandle::SetProjectVersion;

// SWIG problem with non-member operators
%ignore *::operator<<(CNcbiOstream& strm, TFileType& ftype);  // app_popup.hpp
%ignore ncbi::operator<(const CPluginHandle&, const CPluginHandle&);
%ignore ncbi::operator<(CLayoutObject const &, CLayoutObject const &);


%%

gui/fltk.hpp
gui/gui.hpp
gui/types.hpp

// For CMultiColDlg; this needs to be early
gui/utils/dialog.hpp

gui/config/feat_config_inherited.hpp
// .cpp is no longer compiled
//gui/config/feat_config_list.hpp
gui/config/feat_show.hpp
gui/config/settings_set.hpp
gui/config/theme_set.hpp

gui/objects/AbstractProjectItem.hpp
gui/objects/AbstractProjectItem_.hpp
gui/objects/DataHandle.hpp
gui/objects/DataHandle_.hpp
gui/objects/FolderInfo.hpp
gui/objects/FolderInfo_.hpp
gui/objects/GBProject.hpp
gui/objects/GBProjectHandle.hpp
gui/objects/GBProjectHandle_.hpp
gui/objects/GBProject_.hpp
gui/objects/GBProject_ver2.hpp
gui/objects/GBProject_ver2_.hpp
gui/objects/GBWorkspace.hpp
gui/objects/GBWorkspace_.hpp
gui/objects/MRUCache.hpp
gui/objects/MRUCache_.hpp
gui/objects/MessageHistoryInfo.hpp
gui/objects/MessageHistoryInfo_.hpp
gui/objects/MessageStatus.hpp
gui/objects/MessageStatus_.hpp
gui/objects/NCBI_Data_Handle_module.hpp
gui/objects/NCBI_GBenchService_module.hpp
gui/objects/NCBI_GUI_Project_module.hpp
gui/objects/NCBI_Plugin_module.hpp
gui/objects/PluginArg.hpp
gui/objects/PluginArgSet.hpp
gui/objects/PluginArgSet_.hpp
gui/objects/PluginArg_.hpp
gui/objects/PluginCache.hpp
gui/objects/PluginCache_.hpp
gui/objects/PluginCommand.hpp
gui/objects/PluginCommand_.hpp
gui/objects/PluginInfo.hpp
gui/objects/PluginInfoSet.hpp
gui/objects/PluginInfoSet_.hpp
gui/objects/PluginInfo_.hpp
gui/objects/PluginLibInfo.hpp
gui/objects/PluginLibInfo_.hpp
gui/objects/PluginMRUEntry.hpp
gui/objects/PluginMRUEntry_.hpp
gui/objects/PluginMessage.hpp
gui/objects/PluginMessageChain.hpp
gui/objects/PluginMessageChain_.hpp
gui/objects/PluginMessage_.hpp
gui/objects/PluginObject.hpp
gui/objects/PluginObject_.hpp
gui/objects/PluginReply.hpp
gui/objects/PluginReplyAction.hpp
gui/objects/PluginReplyAction_.hpp
gui/objects/PluginReply_.hpp
gui/objects/PluginRequest.hpp
gui/objects/PluginRequest_.hpp
gui/objects/PluginValue.hpp
gui/objects/PluginValueConstraint.hpp
gui/objects/PluginValueConstraint_.hpp
gui/objects/PluginValueRangeConstraint.hpp
gui/objects/PluginValueRangeConstraint_.hpp
gui/objects/PluginValue_.hpp
gui/objects/ProjectAnnot.hpp
gui/objects/ProjectAnnot_.hpp
gui/objects/ProjectDescr.hpp
gui/objects/ProjectDescr_.hpp
gui/objects/ProjectFolder.hpp
gui/objects/ProjectFolder_.hpp
gui/objects/ProjectHistoryItem.hpp
gui/objects/ProjectHistoryItem_.hpp
gui/objects/ProjectItem.hpp
gui/objects/ProjectItem_.hpp
gui/objects/ViewDesc.hpp
gui/objects/ViewDesc_.hpp
gui/objects/WorkspaceFolder.hpp
gui/objects/WorkspaceFolder_.hpp
gui/objects/igbproject.hpp
gui/objects/plugin_exception.hpp

gui/graph/axis.hpp
gui/graph/combo_chart.hpp
gui/graph/graph_panel.hpp
gui/graph/igraph.hpp
gui/graph/igraph_data.hpp
gui/graph/igraph_utils.hpp
gui/graph/legend.hpp
gui/graph/pie_graph.hpp
gui/graph/regular_grid.hpp
gui/graph/scatter_graph.hpp

gui/utils/bbox.hpp
gui/utils/curve.hpp
gui/utils/curve_bezier.hpp
gui/utils/curve_cubic.hpp
gui/utils/globals.hpp
gui/utils/intersect.hpp
gui/utils/math.hpp
gui/utils/matrix3.hpp
gui/utils/matrix4.hpp
gui/utils/promote.hpp
gui/utils/quat.hpp
gui/utils/vect2.hpp
gui/utils/vect3.hpp
gui/utils/vect4.hpp

gui/objutils/alignment.hpp
gui/objutils/alignment_smear.hpp
gui/objutils/alignment_smear_layout.hpp
gui/objutils/bookmark.hpp
gui/objutils/comment.hpp
gui/objutils/density_map.hpp
gui/objutils/edit_event.hpp
gui/objutils/feat_label.hpp
gui/objutils/feat_pack.hpp
gui/objutils/feature.hpp
gui/objutils/feature_conf_params.hpp
gui/objutils/filter.hpp
gui/objutils/graph.hpp
gui/objutils/histogram.hpp
gui/objutils/label.hpp
gui/objutils/layout.hpp
gui/objutils/mate_pair.hpp
gui/objutils/obj_clipboard.hpp
gui/objutils/obj_convert.hpp
gui/objutils/obj_event.hpp
gui/objutils/object_index.hpp
gui/objutils/objects.hpp
gui/objutils/pw_alignment.hpp
gui/objutils/reg_file.hpp
gui/objutils/registry.hpp
gui/objutils/seq_map.hpp
gui/objutils/seqid_dictionary.hpp
gui/objutils/sequence.hpp
gui/objutils/utils.hpp
gui/objutils/vis_range.hpp

gui/utils/accel_table.hpp
gui/utils/app_popup.hpp
gui/utils/browser_utils.hpp
// Although still in CVS, this shouldn't be #include'd
//gui/utils/button.hpp
gui/utils/clipboard.hpp
gui/utils/color_dialog.hpp
gui/utils/command.hpp
gui/utils/diag_handler.hpp
// this is first in the list
//gui/utils/dialog.hpp
gui/utils/event.hpp
gui/utils/event_handler.hpp
gui/utils/exec.hpp
gui/utils/fetch_url.hpp
gui/utils/file_browser.hpp
gui/utils/fltk_utils.hpp
gui/utils/function.hpp
gui/utils/gui_event.hpp
gui/utils/id_generator.hpp
gui/utils/ihelp_handler.hpp
gui/utils/mru_list.hpp
gui/utils/reporter.hpp
gui/utils/rgba_color.hpp
gui/utils/system_path.hpp
gui/utils/table_names.hpp
//gui/utils/utils_macosx_.hpp
gui/utils/utils_platform.hpp
//gui/utils/utils_win32_.hpp
gui/utils/view_event.hpp
gui/utils/worker_thread.hpp

%%

// Need this for return value of __Make*
%template() ncbi::CRef<ncbi::objects::CPluginInfoSet>;
%inline %{
// for creating scripting language objects from C++
inline
ncbi::CRef<ncbi::objects::CPluginMessage> __MakeCPluginMessage(unsigned long ptr)
{
    ncbi::CRef<ncbi::objects::CPluginMessage> rv;
    rv = (ncbi::objects::CPluginMessage *) ptr;
    return rv;
}
inline
ncbi::CRef<ncbi::objects::CPluginInfoSet> __MakeCPluginInfoSet(unsigned long ptr)
{
    ncbi::CRef<ncbi::objects::CPluginInfoSet> rv;
    rv = (ncbi::objects::CPluginInfoSet *) ptr;
    return rv;
}
%}

// GetArgValue is critical for gbench plugins, but it's
// template hell.  Provide the generic CObject version
// and associated template classes;
// downcasting can happen in the scripting laguage.

%template(__GetArgValue) ncbi::GetArgValue<ncbi::CObject>;


%template() ncbi::CRef<ncbi::CLayoutFeat>;
VECTOR_CREF(vector_CRef_CLayoutFeat, ncbi::CLayoutFeat);
