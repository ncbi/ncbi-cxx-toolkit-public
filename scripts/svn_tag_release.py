#!/usr/bin/env python

import sys, os, popen2
from optparse import OptionParser

RELEASE_ROOT="release"

# ORed flags for path part
FLAG_RECURSIVE = 1
FLAG_COPY      = 2
FLAG_REMOVE    = 4

# Global
verbose = False
dry_run = False

def add_path(tree, path, is_directory=True):
    path_parts = path.split('/')
    if len(path_parts) and len(path_parts[0]) and path_parts[0][0] == '-':
        path_parts[0] = path_parts[0][1:]
        last_flag = FLAG_REMOVE
    else:
        last_flag = FLAG_COPY
    path_descr = zip([0] * len(path_parts), path_parts)
    last_part = path_parts[-1]
    if last_part[-1] == '$':
        last_part = last_part[:-1]
    else:
        last_flag |= FLAG_RECURSIVE
    path_descr[-1] = last_flag, last_part

    cur = tree
    for flag, path_part in path_descr:
        new_cur = cur[1].get(path_part, (flag, {}))
        if new_cur[0] != flag:
            new_cur = new_cur[0] | flag, new_cur[1]
        cur[1][path_part] = new_cur
        cur = new_cur

def get_project(root, project_name, project_file_name, revision):
    if project_file_name:
        filelist = file(project_file_name)
    else:
        url = root + "/trunk/c++/scripts/projects/" + project_name + ".lst"
        r,w,e = popen2.popen3("svn cat -r%s %s" % (revision, url))
        err = e.read()
        if err:
            sys.stderr.write(err+'\n')
            return None
        filelist = r.read().split('\n')
    inc_tree = FLAG_COPY, {}
    src_tree = FLAG_COPY, {}
    for line in filelist:
        line = line.strip()
        if not line: continue
        parts = line.split(' ', 1)
        path = parts[0]
        add_path(inc_tree, path)
        get_src = len(parts) <= 1 or parts[1] != "update-only"
        if get_src: add_path(src_tree, path)
    return inc_tree, src_tree

def list_url(url, revision):
#    print "svn list "+url
    r,w,e = popen2.popen3("svn list -r%s %s" % (revision, url))
    res = []
    for line in r:
        line = line.strip()
        if not line: continue
        res.append(line)
    err = e.read()
    if err:
        sys.stderr.write(err+'\n')
        return []
    return res

def add_tree(dirlist, url, root, proj_tree, revision):
    level_flag, children = proj_tree
    # Should we filter?
    if level_flag & FLAG_RECURSIVE:
        return
    eff_url = url + '/' + root
    nodes = list_url(eff_url, revision)
    for node in nodes:
        dir_node = node[-1] == '/'
        if dir_node: node = node[:-1]
        if not dir_node and level_flag & FLAG_COPY: continue
        child = children.get(node)
        if not child:
            dirlist.append(root + '/' + node)
            if verbose:
                sys.stderr.write(root + '/' + node + '\n')
        elif dir_node:
            add_tree(dirlist, url, root + '/' + node, child, revision)
        # regular file found - do nothing

def get_list_to_remove(root, proj_tree, revision):
    inc_tree, src_tree = proj_tree
    # add dirs/files, required for every build to inc_tree, src_tree
    # TODO: generalize this: keep *impl for every FLAG_COPYed dir in
    # include tree
    add_path(inc_tree, "corelib/impl$")
    add_path(inc_tree, "corelib/hash_impl$")

    # TODO: generalize this: every path part in source tree should
    # preserve Makefile.in
    add_path(src_tree, "app/Makefile.in", False)
    # This one is genuine exception
    add_path(inc_tree, "common")
    # Project tree builder
    add_path(inc_tree, "serial$")
    add_path(inc_tree, "serial/impl$")
    add_path(inc_tree, "serial/datatool$")
    add_path(inc_tree, "app/project_tree_builder$") # strange layout!
    add_path(src_tree, "serial$")
    add_path(src_tree, "serial/datatool$")
    add_path(src_tree, "app/project_tree_builder$")
#    print "Include", inc_tree
#    print "Source", src_tree
    # TODO: replace following with systematic add_path with FLAG_REMOVE
    # and use it to handle '-' dir removal in project files
    dirlist = ["doc", "scripts/internal"]
    url = root + "/trunk/c++"
    add_tree(dirlist, url, "include", inc_tree, revision)
    add_tree(dirlist, url, "src",     src_tree, revision)
    return dirlist

def main():
    global verbose
    global dry_run
    parser = OptionParser("Usage: %prog [options] project")
    parser.add_option("-v", "--verbose",
                      action="store_true", dest="verbose")
    parser.add_option("-m", "--message", dest="message",
                      help="SVN message", default="Release tagged")
    parser.add_option("-f", "--project-file", dest="proj_file", default="",
                      help="Project file (default is SVN project)")
    parser.add_option("-r", "--revision", dest="revision", default="HEAD",
                      help="Use this revision (default HEAD)")
    parser.add_option("-U", "--root-url", dest="root_url",
                      help="Root repo URL (default NCBI toolkit repo)",
                      default="https://svn.ncbi.nlm.nih.gov/repos/toolkit")
    parser.add_option("-e", "--release-name", dest="release", default="current",
                      help="Release name (default \"current\")")
    parser.add_option("-n", "--dry-run", action="store_true", dest="dry_run")
    options, args = parser.parse_args()
    if len(args) != 1:
        parser.error("Incorrect number of arguments")
    dry_run = options.dry_run
    if options.dry_run:
        verbose = True
    else:
        verbose = options.verbose
    root_url = options.root_url
    if root_url[-1] == '/':
        root_url = root_url[:-1]
    project = args[0]

    proj_tree = get_project(root_url, project, options.proj_file, options.revision)
    if not proj_tree:
        return -1
    dirlist = get_list_to_remove(root_url, proj_tree, options.revision)
    project_release_root = RELEASE_ROOT + '/' + project
    release = options.release
    distr_name = "c++"
    target = "%s/%s/%s" % (project_release_root, release, distr_name)
    mucc_cmd = "svnmucc -U %s -m \"%s\"" % (root_url, options.message)
    tags = list_url(root_url + '/' + RELEASE_ROOT, options.revision)
    if not project+'/' in tags:
        mucc_cmd += " mkdir " + project_release_root
    mucc_cmd += " mkdir %s/%s cp %s trunk/c++ %s " % (project_release_root,
                                                      release,
                                                      options.revision, target)
    if dry_run:
        mucc_cmd += " ".join(map(lambda x: "rm %s/%s" % (target, x), dirlist))
        print mucc_cmd
    else:
        tmpfname = os.tempnam()
        f = file(tmpfname, "w")
        for name in dirlist:
            f.write("rm\n%s/%s\n" % (target, name))
        f.close()
        mucc_cmd += "-X " + tmpfname
        sys.stderr.write("Release created in: %s/%s/%s\n" % (root_url, project_release_root, release))
        r,w,e = popen2.popen3(mucc_cmd)
        for line in r:
            sys.stderr.write(line)
        err = e.read()
        r.close()
        w.close()
        e.close()
        os.unlink(tmpfname)
        if err:
            sys.stderr.write("Error in svnmucc invokation: %s" % err)
            return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
