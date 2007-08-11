#!/usr/bin/env python
# TODO: Use optparse to implement 3 options: -m message_for_svn
# -f file_for_project and -r revision_to_tag
import sys, popen2

RELEASE_ROOT="release"

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
        sys.stderr.write(err+'\n')
        return None
    filelist = r.read().split('\n')
#    filelist = file("netschedule.lst")
    inc_tree = FLAG_COPY, {}
    src_tree = FLAG_COPY, {}
    for line in filelist:
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
        sys.stderr.write(err+'\n')
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
        if not child:
            dirlist.append(root + '/' + node)
            sys.stderr.write(root + '/' + node + '\n')
        elif dir_node:
            add_tree(dirlist, url, root + '/' + node, child)
        # regular file found - do nothing

def get_list_to_remove(root, proj_tree):
    inc_tree, src_tree = proj_tree
    # add dirs/files, required for every build to inc_tree, src_tree
    add_path(inc_tree, [(0, "corelib"), (FLAG_COPY, "impl")])
    add_path(inc_tree, [(0, "corelib"), (FLAG_COPY, "hash_impl")])
    add_path(inc_tree, [(FLAG_COPY | FLAG_RECURSIVE, "common")])
    add_path(src_tree, [(0, "app"), (0, "Makefile.in")])
#    print "Include", inc_tree
#    print "Source", src_tree
    dirlist = ["doc", "scripts/internal"]
    url = root + "/trunk/c++"
    add_tree(dirlist, url, "include", inc_tree)
    add_tree(dirlist, url, "src",     src_tree)
    return dirlist

def main(args):
    if len(args) < 2:
        sys.stderr.write("Usage: svn_tag_release.py svn_root project [revision]\n")
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
    project_release_root = RELEASE_ROOT + '/' + project
    target = "%s/current/c++" % project_release_root
    mucc_cmd = "svnmucc -U %s" % root
    tags = list_url(root + '/' + RELEASE_ROOT)
    if not project+'/' in tags:
        mucc_cmd += " mkdir " + project_release_root
    mucc_cmd += " mkdir %s/current cp %s trunk/c++ %s " % (project_release_root, revision, target)
    mucc_cmd += " ".join(map(lambda x: "rm %s/%s" % (target, x), dirlist))
    print mucc_cmd
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
