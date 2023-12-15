#!/bin/bash
# wrapper to invoke clang for compilation with the as using multiple object files
compiler=clang++
#compiler= variable needs to be set on line 3 as it will be replaced with clang or clang++ depending if c or cpp wrapper is generated

 LD_PRELOAD_PREV="$LD_PRELOAD"
if [ "$DEBUG_CLANG_WRAPPER" == true ]; then
    echo "INVOKE CLANG_WRAPPER"
    echo "clang_wrapper $@"
    export LD_PRELOAD="$(clang -print-file-name=libclang_rt.asan.so):$LD_PRELOAD_PREV"
fi

#TODO add check that at least -O1 is used as we require some optimizations done in O1

USE_MPI_COMPILER_ASSISTANCE_PASS=${USE_MPI_COMPILER_ASSISTANCE_PASS:false}

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
if [ "$USE_MPI_COMPILER_ASSISTANCE_PASS" == true ] &&
    ( [ "$has_flto" == false ] ||
   [ "$has_fwhole_program_vtables" == false ] ); then
    echo "Error, need -flto and -fwhole-program-vtables for pass to work correctly"
    export LD_PRELOAD="$LD_PRELOAD_PREV"
    exit 1
fi

if [ "$USE_MPI_COMPILER_ASSISTANCE_PASS" == true ] && ( ! [[ -v MPI_COMPILER_ASSISTANCE_PASS ]] ); then
    echo "The MPI_COMPILER_ASSISTANCE_PASS environment variable is not set"
    export LD_PRELOAD="$LD_PRELOAD_PREV"
    exit 1
fi

COMPILER_INVOCATION="$compiler"
if [ "$is_to_obj" == true ]; then
    if [ "$DEBUG_CLANG_WRAPPER" == true ]; then
        echo "MODE: to obj file"
    fi
    for arg in "$@"; do
        if [ "$arg" == "-c" ]; then
            COMPILER_INVOCATION="$COMPILER_INVOCATION -c -emit-llvm"
        elif [[ "$arg" == *.o ]]; then
            # Remove the ".o" suffix and append ".bc"
            new_file="${arg%.o}.bc"
            COMPILER_INVOCATION="$COMPILER_INVOCATION $new_file"
            # create .o and update timestamp so build-systems work as intended if they use this information
            touch $arg
        else
            COMPILER_INVOCATION="$COMPILER_INVOCATION $arg"
        fi
    done
    if [ "$DEBUG_CLANG_WRAPPER" == true ]; then
        echo "$COMPILER_INVOCATION"
    fi
    $COMPILER_INVOCATION
    export LD_PRELOAD="$LD_PRELOAD_PREV"
    exit
fi


if [ "$has_o_files" == true ]; then
    if [ "$DEBUG_CLANG_WRAPPER" == true ]; then
        echo "MODE: Link .o files"
    fi
    #-x ir - : read ir from stdin
    COMPILER_INVOCATION="$compiler -x ir -"
    if [[ "$USE_MPI_COMPILER_ASSISTANCE_PASS" == true ]]; then
        COMPILER_INVOCATION="$COMPILER_INVOCATION -fpass-plugin=$MPI_COMPILER_ASSISTANCE_PASS -lprecompute"
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
    if [ "$DEBUG_CLANG_WRAPPER" == true ]; then
        echo "$LLVM_LINK_INVOCATION | $COMPILER_INVOCATION"
    fi
    $LLVM_LINK_INVOCATION | $COMPILER_INVOCATION
    export LD_PRELOAD="$LD_PRELOAD_PREV"
    exit
fi

if [ "$DEBUG_CLANG_WRAPPER" == true ]; then
    echo "MODE: direct to Binary"
fi
COMPILER_INVOCATION="$compiler"
if [[ "$USE_MPI_COMPILER_ASSISTANCE_PASS" == true ]]; then
    COMPILER_INVOCATION="$COMPILER_INVOCATION -fpass-plugin=$MPI_COMPILER_ASSISTANCE_PASS -lprecompute"
fi
for arg in "$@"; do
    COMPILER_INVOCATION="$COMPILER_INVOCATION $arg"
done
if [ "$DEBUG_CLANG_WRAPPER" == true ]; then
    echo "$COMPILER_INVOCATION"
fi
$COMPILER_INVOCATION
export LD_PRELOAD="$LD_PRELOAD_PREV"
exit

