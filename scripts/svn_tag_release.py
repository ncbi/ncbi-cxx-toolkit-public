#!/usr/bin/env python

import sys, popen2

# ORed flags for path part
FLAG_COPY      = 1
FLAG_RECURSIVE = 2

def add_path(tree, path_descr):
    cur = tree
    for flag, path_part in path_descr:
        new_cur = cur[1].get(path_part, (flag, {}))
        if new_cur[0] != flag:
            new_cur = new_cur[0] | flag, new_cur[1]
        cur[1][path_part] = new_cur
        cur = new_cur

def get_project(root, project_name):
    url = root + "/trunk/c++/scripts/projects/" + project_name + ".lst"
    r,w,e = popen2.popen3("svn cat "+url)
    err = e.read()
    if err:
        print err
        return None
    dirs = r.read().split('\n')
#    dirs = file("test.lst")
    inc_tree = FLAG_COPY, {}
    src_tree = FLAG_COPY, {}
    for line in dirs:
        line = line.strip()
        if not line: continue
        parts = line.split(' ', 1)
        path_parts = parts[0].split('/')
        path_descr = zip([0] * len(path_parts), path_parts)
        get_src = len(parts) <= 1 or parts[1] != "update-only"
        last_part = path_parts[-1]
        last_flag = FLAG_COPY
        if last_part[-1] == '$':
            last_part = last_part[:-1]
        else:
            last_flag |= FLAG_RECURSIVE
        path_descr[-1] = last_flag, last_part
        add_path(inc_tree, path_descr)
        if get_src: add_path(src_tree, path_descr)
    print "Include", inc_tree
    print "Source", src_tree
    return inc_tree, src_tree

def list_url(url):
#    print "svn list "+url
    r,w,e = popen2.popen3("svn list "+url)
    res = []
    for line in r:
        line = line.strip()
        if not line: continue
        res.append(line)
    err = e.read()
    if err:
        print err
        return []
    return res

def add_tree(dirlist, url, root, proj_tree):
    level_flag, children = proj_tree
    # Should we filter?
    if level_flag & FLAG_RECURSIVE:
        return
    eff_url = url + '/' + root
    nodes = list_url(eff_url)
    for node in nodes:
        dir_node = node[-1] == '/'
        if dir_node: node = node[:-1]
        if not dir_node and level_flag & FLAG_COPY: continue
        child = children.get(node)
        #TODO: handle all cases: dir/file, present/not
        if not child:
            dirlist.append(root + '/' + node)
            print root + '/' + node
        else:
            add_tree(dirlist, url, root + '/' + node, child)

def get_list_to_remove(root, proj_tree):
    inc_tree, src_tree = proj_tree
    dirlist = ["doc", "scripts/internal"]
    # TODO: add dirs/files, required for every build
    # to inc_tree, src_tree
    url = root + "/trunk/c++"
    add_tree(dirlist, url, "include", inc_tree)
    add_tree(dirlist, url, "src",     src_tree)
    return dirlist

def main(args):
    if len(args) < 2:
        print "Usage: svn_tag_release.py svn_root project [revision]"
        return 1
    root = args[0]
    if root[-1] == '/':
        root = root[:-1]
    project = args[1]
    if len(args) > 2:
        revision = args[2]
    else:
        revision = "HEAD"

    proj_tree = get_project(root, project)
    if not proj_tree:
        return -1
    dirlist = get_list_to_remove(root, proj_tree)
    target = "tags/%s/current/c++" % project
    mucc_cmd = "svnmucc -U %s" % root
    tags = list_url(root+'/tags')
    if not project+'/' in tags:
        mucc_cmd += " mkdir tags/%s " % project
    mucc_cmd += " mkdir tags/%s/current cp %s trunk/c++ %s " % (project, revision, target)
    mucc_cmd += " ".join(map(lambda x: "rm %s/%s" % (target, x), dirlist))
    print mucc_cmd
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
