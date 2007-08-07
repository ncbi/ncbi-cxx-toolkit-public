#!/usr/bin/env python

import sys, popen2

def get_project(root, project_name):
    url = root + "/trunk/c++/scripts/projects/" + project_name + ".lst"
    r,w,e = popen2.popen3("svn cat "+url)
    err = e.read()
    if err:
        print err
        return {}
    dirs = r.read().split('\n')
    proj_tree = {}
    for line in dirs:
        line = line.strip()
        if not line: continue
        parts = line.split(' ', 1)
        path_parts = parts[0].split('/')
        print path_parts
        cur = proj_tree
        for path_part in path_parts:
            if path_part[-1] == '$':
                path_part = path_part[:-1]
                node = {}
            else:
                node = None
            # Following code is different from setdefault in that it replaces
            # None with empty dict for non-term path_part
            # It handles the case when generic terminating path mentioned before
            # more precise one e.g.:
            # connect$
            # connect/services$
            # now it handles it so that only connect/services$ needed to include
            # connect with regular files and connect/services also.
            new_cur = cur.get(path_part, node)
            if new_cur is None and not node is None:
                new_cur = node
            cur[path_part] = new_cur
            cur = new_cur
    print proj_tree
    return proj_tree

def list_url(url):
    print "svn list "+url
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
    eff_url = url + '/' + root
    nodes = list_url(eff_url)
    for node in nodes:
        if not proj_tree is None:
            dir_node = node[-1] == '/'
            if dir_node: node = node[:-1]
            if not dir_node: continue
            if not node in proj_tree:
                dirlist.append(root + '/' + node)
            else:
                add_tree(dirlist, url, root + '/' + node, proj_tree[node])

def get_list_to_remove(root, proj_tree):
    dirlist = ["doc", "scripts/internal"]
    url = root + "/trunk/c++"
    add_tree(dirlist, url, "src", proj_tree)
    add_tree(dirlist, url, "include", proj_tree)
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
    tags = list_url(root+'/tags')
    if not project+'/' in tags:
        mktagdir = "mkdir tags/%s " % project
    else:
        mktagdir = ''
    print "svnmucc -U %s %smkdir tags/%s/current cp %s trunk/c++ %s %s" % (root, mktagdir, project, revision, target, " ".join(map(lambda x: "rm %s/%s" % (target, x), dirlist)))
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
