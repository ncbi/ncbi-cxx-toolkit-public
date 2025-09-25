# $Id$
# Auxiliary script for TeamCity CMake builds to generate an HTML page to view failed tests output directly in TC. CXX-14172 	

import os
import sys
import html

def generate_html(root_dir, output_file="index.html"):
    # Build a nested dictionary structure of directories and files
    tree = {}
    for dirpath, _, filenames in os.walk(root_dir):
        rel_dir = os.path.relpath(dirpath, root_dir)
        if rel_dir == ".":
            rel_dir = ""
        node = tree
        if rel_dir:
            for part in rel_dir.split(os.sep):
                node = node.setdefault(part, {})
        for f in filenames:
            if f.endswith(".txt"):
                node[f] = os.path.join(rel_dir, f) if rel_dir else f

    # Load file contents
    file_contents = {}
    for dirpath, _, filenames in os.walk(root_dir):
        for f in filenames:
            if f.endswith(".txt"):
                full_path = os.path.join(dirpath, f)
                rel_path = os.path.relpath(full_path, root_dir)
                with open(full_path, encoding="utf-8") as fh:
                    file_contents[rel_path] = html.escape(fh.read())

    # Build HTML
    html_content = """
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Text File Viewer</title>
<style>
    body { font-family: Arial, sans-serif; margin: 15px; display: flex; }
    #file-list { flex: 0; flex-basis: max-content; overflow-y: visible; max-height: 90vh; padding-right: 15px; }
    #file-content { flex: 0; flex-basis: auto; padding-left: 15px; border-left: 1px solid #ccc; }
    #content-box { 
	flex: 0;
        white-space: pre; 
        overflow: auto; 
        max-height: 90vh; 
        border: 1px solid #ddd; 
        padding: 10px; 
        background: #fcfcfc;
    }
    ul { list-style-type: none; padding-left: 15px; padding-bottom: 5px;}
    li { margin: 0px 0px; }
    .folder { font-weight: bold; cursor: pointer; }
    .hidden { display: none; }
    a { text-decoration: none; color: blue; cursor: pointer; }
    a:hover { text-decoration: underline; }
</style>
</head>
<body>
    <div id="file-list">
        <h2>Failed Tests</h2>
"""

    def render_tree(node, collapsed=True):
        """Render directory tree; all folders collapsed by default."""
        ul_class = "hidden" if collapsed else ""
        html_str = f"<ul class='{ul_class}'>\n"
        for key, val in sorted(node.items()):
            if isinstance(val, dict):
                html_str += f'<li><span class="folder" onclick="toggleFolder(this)">{key}/</span>\n'
                html_str += render_tree(val, collapsed=True)
                html_str += "</li>\n"
            else:  # file
                html_str += f'<li><a href="#" onclick="showFile(\'{val}\', event)">{key}</a></li>\n'
        html_str += "</ul>\n"
        return html_str

    # Root folders visible, but their children collapsed
    html_content += render_tree(tree, collapsed=False)

    html_content += """
    </div>
    <div id="file-content">
        <h2>Content</h2>
        <div id="content-box">Click test name to view output</div>
    </div>

<script>
    const files = {};
    let activeLink = null;

    function showFile(fname, event) {
        if (event) event.preventDefault(); // stop navigation
        document.getElementById("content-box").innerText = files[fname];

        // highlight active link
        if (activeLink) activeLink.classList.remove("active-file");
        event.target.classList.add("active-file");
        activeLink = event.target;
    }

    function toggleFolder(el) {
        let next = el.nextElementSibling;
        if (next && next.tagName.toLowerCase() === "ul") {
            next.classList.toggle("hidden");
        }
    }
</script>
</body>
</html>
"""

    # Insert file contents into JS
    insert_index = html_content.find("const files = {};") + len("const files = {};")
    file_js = "\n"
    for rel_path, content in file_contents.items():
        js_safe = content.replace("\\", "\\\\").replace("\n", "\\n").replace("'", "\\'")
        file_js += f"    files['{rel_path}'] = '{js_safe}';\n"
    html_content = html_content.replace("const files = {};", "const files = {};" + file_js)

    with open(output_file, "w", encoding="utf-8") as f:
        f.write(html_content)

    print(f"HTML page generated: {output_file}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python tc_generate_html_failed_tests.py <directory> [output_file]")
        sys.exit(1)

    directory = sys.argv[1]
    output = sys.argv[2] if len(sys.argv) > 2 else "index.html"

    generate_html(directory, output)

