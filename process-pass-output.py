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


def get_idices_of_elem(list, elem):
    return [i for i, x in enumerate(list) if x == elem]


def print_orig(module):
    to_print = [l for l in module if not l.startswith("!")]
    with open(output_orig, 'w') as the_file:
        the_file.write("\n".join(to_print))


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
        exit(0)

    print("processing pass output")
    full_input = sys.stdin.readlines()
    print("read %d lines" % len(full_input))
    full_input = [l.strip() for l in full_input]

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

    print_orig(original_mod)
    print_modified(altered_mod)
    print_remarks(pass_comments)

    print("end Processing")
    pass


if __name__ == "__main__":
    main()
