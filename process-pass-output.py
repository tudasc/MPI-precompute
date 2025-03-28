# /bin/python3

import sys
import re

# settings: which output files to use

output_orig = "IR_orig"
output_orig_annotated = "IR_orig_annotated"
output_modified = "IR_modified"
# this can be compiled further:
output_modified_executable = "IR_modified_executable"
output_pass_remarks = "pass_comments.txt"

# markers used by the pass
marker_end_module = "END MODULE"
marker_begin_module_modified = "After Modification:"
marker_begin_module_orig = "Before Modification:"
marker_end_pass_execution = "Successfully executed the pass"

marker_text = "need for reason:"

taint_reasons = {
    # 'OTHER': 0,
    'INCLUDED': 1 << 0,
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

        # assert len(to_anno_idx) > 0
        for ii in to_anno_idx:
            module[ii] = annotation + " " + module[ii]

    return module


def remove_extra_spaces(line):
    """Replace multiple spaces with a single space."""
    return " ".join(line.split())


def remove_metadata(line):
    # removes metadata annotations such as !dbg !123
    return remove_extra_spaces(
        re.sub(r'\s*![-\w,]+(\s|$)', ' ', line)
        .rstrip(" ,")
    )


def extract_between_at_and_paren(s):
    match = re.search(r"@([^()]+)\(", s)
    return match.group(1) if match else None


def get_functions(module):
    functions = {}

    current_func = []
    has_attr_line = False
    in_func = False

    for line in module:
        if line.startswith("; Function Attrs:"):
            has_attr_line = True
            in_func = True
        if line.startswith("define"):
            in_func = True
        if in_func:
            current_func.append(line)
        if line == "}" or line.startswith("declare"):
            if not in_func:
                current_func.append(line)
                # the declare is just this line with no attributes

            assert len(current_func) > 0

            # get function name
            name = current_func[0]
            if has_attr_line:
                name = current_func[1]
            name = extract_between_at_and_paren(name)

            functions[name] = current_func
            # reset
            current_func = []
            in_func = False
            has_attr_line = False

    return functions


# the name of the copied func
def find_matching_func(name, data):
    prefix = f"{name}."
    return [key for key in data if key.startswith(prefix) and key[len(prefix):].isdigit()]


# such that one can easily view the IR diff of original function to precomputed one
def align(orig, modified):
    orig = [l for l in orig if (not l.startswith("!")) and (not "@llvm.dbg.value" in l)]
    orig = [remove_metadata(l) for l in orig]
    modified = [l for l in modified if not l.startswith("!")]
    modified = [remove_metadata(l) for l in modified]
    functions_orig = get_functions(orig)
    functions_modified = get_functions(modified)

    # match original func to precompute one
    module_orig = []
    module_modified = []

    for name, content in functions_orig.items():
        name_modified_list = find_matching_func(name, functions_modified)
        if len(name_modified_list) > 0:
            # happens with openmp tasks
            if len(name_modified_list) > 1:
                name_modified_list = sorted(name_modified_list, reverse=True)
            name_modified = name_modified_list[0]
            module_orig.extend(content)
            module_modified.extend(functions_modified[name_modified])
            module_orig.append("")
            module_modified.append("")

    return module_orig, module_modified


def print_orig_annotated(module, remarks):
    without_debug_info = [l for l in module if not l.startswith("!")]

    with_anno = annotate(without_debug_info, remarks)

    with open(output_orig_annotated, 'w') as the_file:
        the_file.write("\n".join(with_anno))


def print_aligned_orig_modified(module_orig, module_modified):
    orig, modified = align(module_orig, module_modified)
    with open(output_orig, 'w') as the_file:
        the_file.write("\n".join(orig))
    with open(output_modified, 'w') as the_file:
        the_file.write("\n".join(modified))


def print_modified(module):
    to_print = [l for l in module if not l.startswith("!")]
    with open(output_modified, 'w') as the_file:
        the_file.write("\n".join(to_print))


def print_modified_executable(module):
    with open(output_modified_executable, 'w') as the_file:
        the_file.write("\n".join(module[1:]))


def print_remarks(remarks):
    with open(output_pass_remarks, 'w') as the_file:
        the_file.write("\n".join(remarks))


def main():
    if sys.stdin.isatty():
        print("use this with a pipe")
        print("Example: ./run.sh sourcefile.cpp |& python3 ./process-pass-output.py")
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

    print_orig_annotated(original_mod, pass_comments)
    print_aligned_orig_modified(original_mod, altered_mod)
    print_modified_executable(altered_mod)
    print_remarks(pass_comments)

    print("end Processing")
    pass


if __name__ == "__main__":
    main()
