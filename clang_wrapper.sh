#!/bin/bash

# wrapper to invoke clang for compilation with the ass using multiple object files

echo "INVOKE CLANG_WRAPPER"
echo "clang_wrapper $@"

USE_MPI_COMPILER_ASSISTANCE_PASS=${MPI_COMPILER_ASSISTANCE_PASS:false}

compiler=clang++

is_to_obj=false
has_o_option=false
has_o_files=false
has_flto=false
has_fwhole_program_vtables=false
for arg in "$@"; do
    # Check if the current argument is "-c"
    if [ "$arg" == "-c" ]; then
        is_to_obj=true
    elif [ "$arg" == "-o" ]; then
        has_o_option=true
    elif [[ "$arg" == *.o ]]; then
        has_o_files=true
    elif [ "$arg" == "-flto" ]; then
        has_flto=true
    elif [ "$arg" == "-fwhole-program-vtables" ]; then
        has_fwhole_program_vtables=true
    fi
done

# check if necessary flags are given
if [ "$has_flto" == false ] ||
   [ "$has_fwhole_program_vtables" == false ]; then
    echo "Error, need -flto and -fwhole-program-vtables for pass to work correctly"
    exit 1
fi

if ! [[ -v MPI_COMPILER_ASSISTANCE_PASS ]]; then
    echo "The MPI_COMPILER_ASSISTANCE_PASS environment variable is not set"
    exit 1
fi

COMPILER_INVOCATION="$compiler"
if [ "$is_to_obj" == true ]; then
    echo "MODE: to obj file"
    for arg in "$@"; do
        if [ "$arg" == "-c" ]; then
            COMPILER_INVOCATION="$COMPILER_INVOCATION -c -emit-llvm"
        elif [[ "$arg" == *.o ]]; then
            # Remove the ".o" suffix and append ".bc"
            new_file="${arg%.o}.bc"
            COMPILER_INVOCATION="$COMPILER_INVOCATION $new_file"
        else
            COMPILER_INVOCATION="$COMPILER_INVOCATION $arg"
        fi
    done
    echo "$COMPILER_INVOCATION"
    $COMPILER_INVOCATION
    exit
fi


if [ "$has_o_files" == true ]; then
    echo "MODE: Link .o files"
    #-x ir - : read ir from stdin
    COMPILER_INVOCATION="$compiler -x ir -"
    if [[ "$USE_MPI_COMPILER_ASSISTANCE_PASS" == true ]]; then
        COMPILER_INVOCATION="$COMPILER_INVOCATION -fpass-plugin=$MPI_COMPILER_ASSISTANCE_PASS"
    fi
    LLVM_LINK_INVOCATION="llvm-link"
    for arg in "$@"; do
        if [[ "$arg" == *.o ]]; then
            # Remove the ".o" suffix and append ".bc"
            new_file="${arg%.o}.bc"
            # remove from compiler invocation and add to file list
            #COMPILER_INVOCATION="$COMPILER_INVOCATION $new_file"
            LLVM_LINK_INVOCATION="$LLVM_LINK_INVOCATION $new_file"
        elif [[ "$arg" == *.so ]]; then
            # in our mode we cannot enter .o and .so files so we need to tell it to link it with -l
            basefilename=$(basename "$arg")
            # Use parameter expansion to remove file extensions
            lib_fname="${basefilename%%.*}"
            # Use parameter expansion to remove "lib" from the beginning
            lib_name="${lib_fname#lib}"
            # Use dirname to get the directory part (will at least result in ".")
            directory=$(dirname "$arg")
            COMPILER_INVOCATION="$COMPILER_INVOCATION -L$directory -l$lib_name"
        else
            COMPILER_INVOCATION="$COMPILER_INVOCATION $arg"
        fi
    done
    echo "$LLVM_LINK_INVOCATION | $COMPILER_INVOCATION"
    $LLVM_LINK_INVOCATION | $COMPILER_INVOCATION
    exit
fi


echo "MODE: direct to Binary"
COMPILER_INVOCATION="$compiler"
if [[ "$USE_MPI_COMPILER_ASSISTANCE_PASS" == true ]]; then
    COMPILER_INVOCATION="$COMPILER_INVOCATION -fpass-plugin=$MPI_COMPILER_ASSISTANCE_PASS"
fi
for arg in "$@"; do
    COMPILER_INVOCATION="$COMPILER_INVOCATION $arg"
done
echo "$COMPILER_INVOCATION"
$COMPILER_INVOCATION
exit

