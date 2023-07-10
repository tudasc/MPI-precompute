#!/bin/bash

STYLEARG="--style=llvm"

format_file() {
  file="${1}"
  if [ -f $file ]; then
#	echo format $file
	clang-format -i ${STYLEARG} ${1}

  fi
}



for file in `find mpi_compiler_assistance_matching_pass/ tests/ openmpi-patch/ -name "*.h" -or -name "*.hpp" -or -name "*.c" -or -name "*.cpp" -not -name "json.hpp"` ; do
# format everything but the json.hpp
	if [[ "$file" != "mpi_compiler_assistance_matching_pass/json.hpp" ]]; then
		format_file "${file}"
	fi
    done

