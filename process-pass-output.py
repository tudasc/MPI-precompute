# /bin/python3

import sys

# settings: which output files to use

output_orig = "IR_orig.txt"
output_modified = "IR_modified.txt"
output_pass_remarks = "pass_comments.txt"

# markers used by the pass
marker_end_module = "END MODULE"
marker_begin_module_modified = "After Modification:"
marker_begin_module_orig = "Before Modification:"
marker_end_pass_execution = "Successfully executed the pass"

marker_need_control = "need for control flow:"
marker_need_dest = "need for dest compute:"
marker_need_tag = "need for tag compute:"
marker_need_other = "need for other reason:"


def get_idices_of_elem(list, elem):
    return [i for i, x in enumerate(list) if x == elem]


def annotate(module, remarks, marker, annotation):
    idx_annotations = get_idices_of_elem(remarks, marker)

    function_defines = [(i, v) for i, v in enumerate(module) if v.startswith("define ")]
    function_defines.append((len(module) - 1, "END OF MODULE"))
    # is sorted by i already

    for idx in idx_annotations:
        # the next line
        to_annotate = remarks[idx + 2]
        if "!" in to_annotate:
            pos = to_annotate.find("!")
            to_annotate = to_annotate[0:pos]
        to_anno_idx = [i for i, v in enumerate(module) if to_annotate in v]
        if not len(to_anno_idx) == 1:
            func = remarks[idx + 1]
            match_func_idx = [i for i, v in enumerate(function_defines) if func in v[1]]
            assert len(match_func_idx) == 1
            to_anno_idx = [i for i in to_anno_idx if
                           function_defines[match_func_idx[0]][0] < i < function_defines[match_func_idx[0] + 1][0]]

        for idx in to_anno_idx:
            module[idx] = annotation + " " + module[idx]

    return module


def print_orig(module, remarks):
    without_debug_info = [l for l in module if not l.startswith("!")]

    with_anno = annotate(without_debug_info, remarks, marker_need_control, "CONTROL")
    with_anno = annotate(with_anno, remarks, marker_need_tag, "TAG")
    with_anno = annotate(with_anno, remarks, marker_need_dest, "DEST")
    with_anno = annotate(with_anno, remarks, marker_need_other, "OTHER")

    with open(output_orig, 'w') as the_file:
        the_file.write("\n".join(with_anno))


def print_modified(module):
    to_print = [l for l in module if not l.startswith("!")]
    with open(output_modified, 'w') as the_file:
        the_file.write("\n".join(to_print))


def print_remarks(remarks):
    with open(output_pass_remarks, 'w') as the_file:
        the_file.write("\n".join(remarks))


def main():
    if sys.stdin.isatty():
        print("use this with a pipe")
        print("Example: ./run.sh sourcefile.cpp |& python ./process-pass-output.py")
        exit(0)

    print("processing pass output")
    full_input = sys.stdin.readlines()
    print("read %d lines" % len(full_input))
    full_input = [l.rstrip() for l in full_input]

    end_idx = get_idices_of_elem(full_input, marker_end_module)

    assert (len(end_idx) == 2)
    begin_mod_idx = get_idices_of_elem(full_input, marker_begin_module_orig)
    assert (len(begin_mod_idx) == 1)

    original_mod = full_input[begin_mod_idx[0]:end_idx[0]]

    success_marker_idx = get_idices_of_elem(full_input, marker_end_pass_execution)
    assert len(success_marker_idx) == 1
    if success_marker_idx:
        begin_mod_idx = get_idices_of_elem(full_input, marker_begin_module_modified)
        assert len(begin_mod_idx) == 1
        altered_mod = full_input[begin_mod_idx[0]:end_idx[1]]
    else:
        altered_mod = []
        begin_mod_idx = [len(full_input)]

    pass_comments = full_input[end_idx[0]:begin_mod_idx[0]]

    print_orig(original_mod, pass_comments)
    print_modified(altered_mod)
    print_remarks(pass_comments)

    print("end Processing")
    pass


if __name__ == "__main__":
    main()
