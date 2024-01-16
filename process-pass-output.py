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

marker_text = "need for reason:"

taint_reasons = {
    # 'OTHER': 0,
    'ANALYSIS': 1 << 0,
    'COMPUTE_TAG': 1 << 1,
    'COMPUTE_DEST': 1 << 2,
    'CONTROL_FLOW': 1 << 3,
    'CONTROL_FLOW_RETURN_VALUE_NEEDED': 1 << 4,
    'CONTROL_FLOW_CALLEE_NEEDED': 1 << 5,
    'CONTROL_FLOW_EXCEPTION_NEEDED': 1 << 6,
    'CONTROL_FLOW_ONLY_PRESENCE_NEEDED': 1 << 7,
}


def get_idices_of_elem(list, elem):
    return [i for i, x in enumerate(list) if x == elem]


def get_annotation_string(reason):
    anno = ""
    if reason == 0:
        return "OTHER "

    for key, val in taint_reasons.items():
        if reason & val:
            anno = anno + key + " "
    return anno


# TODO a call with no return value is not unique as it can occur multiple times
# but this is visible in multiple annotations in the result output

def annotate(module, remarks):
    idx_annotations = [i for i, x in enumerate(remarks) if x.startswith(marker_text)]

    function_defines = [(i, v) for i, v in enumerate(module) if v.startswith("define ")]
    function_defines.append((len(module) - 1, "END OF MODULE"))
    # is sorted by i already

    for idx in idx_annotations:
        integer_part = remarks[idx].split(':')[-1].strip()
        assert integer_part != ""
        reason = int(integer_part)
        annotation = get_annotation_string(reason)
        # the next line
        to_annotate = remarks[idx + 2]
        assert to_annotate != ""

        # only the part before the debug symbols amd attributes
        if "!" in to_annotate:
            pos = to_annotate.find("!")
            to_annotate = to_annotate[0:pos]
        # and also before attributes
        if "#" in to_annotate:
            pos = to_annotate.find("#")
            to_annotate = to_annotate[0:pos]
        assert to_annotate != ""

        to_anno_idx = [i for i, v in enumerate(module) if to_annotate.strip() in v]
        # print(to_annotate.strip())
        # print(remarks[idx + 1].strip())
        # print(" ")
        # assert len(to_anno_idx) > 0
        # sometimes the names in the IR change??

        if not len(to_anno_idx) == 1:
            func = remarks[idx + 1].strip()
            assert func != ""
            match_func_idx = [i for i, v in enumerate(function_defines) if "@" + func + "(" in v[1]]
            assert len(match_func_idx) == 1
            to_anno_idx = [i for i in to_anno_idx if
                           function_defines[match_func_idx[0]][0] < i < function_defines[match_func_idx[0] + 1][0]]

        #assert len(to_anno_idx) > 0
        for ii in to_anno_idx:
            module[ii] = annotation + " " + module[ii]

    return module


def print_orig(module, remarks):
    without_debug_info = [l for l in module if not l.startswith("!")]

    with_anno = annotate(without_debug_info, remarks)

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

    if len(end_idx) > 1:
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
