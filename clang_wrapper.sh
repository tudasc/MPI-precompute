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

if [ "$USE_COMPILER_PASS" == 1 ]; then
USE_COMPILER_PASS=true
fi

USE_COMPILER_PASS=${USE_COMPILER_PASS:false}

is_to_obj=false
has_o_option=false
has_o_files=false
has_flto=false
has_fwhole_program_vtables=false
has_opt_lvl=false
has_src_file=false
has_multiple_src_file=false
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
    elif [ "$arg" == "-O1" ] || [ "$arg" == "-O2" ] || [ "$arg" == "-O3" ]; then
        has_opt_lvl=true
    elif [[ "$arg" == *.c ]] || [[ "$arg" == *.cpp ]]  || [[ "$arg" == *.cc ]] || [[ "$arg" == *.cxx ]]; then
        if [ "$has_src_file" == true ]; then
            has_multiple_src_file=true
        fi
        has_src_file=true
    fi
done

# check if necessary flags are given
if [ "$USE_COMPILER_PASS" == true ] &&
    ( [ "$has_flto" == false ] ||
   [ "$has_fwhole_program_vtables" == false ] || [ "$has_opt_lvl" == false ] ); then
    echo "Error, need -flto and -fwhole-program-vtables and at least -O1 for pass to work correctly"
    export LD_PRELOAD="$LD_PRELOAD_PREV"
    exit 1
fi

if [ "$USE_COMPILER_PASS" == true ] && ( ! [[ -v COMPILER_PASS ]] ); then
    echo "The COMPILER_PASS environment variable is not set"
    export LD_PRELOAD="$LD_PRELOAD_PREV"
    exit 1
fi

COMPILER_INVOCATION="$compiler"
if [ "$is_to_obj" == true ]; then
    if [ "$DEBUG_CLANG_WRAPPER" == true ]; then
        echo "MODE: to obj file"
    fi
    if [ $has_multiple_src_file"" == true ]; then
      echo "ERROR linking multiple src files directly into one object file is not supported"
      echo "Compile one by one and link afterwards"
      exit 1
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
        #elif [[ "$arg" == "-fsanitize=thread" ]]; then
        #    # remove the arg, as tsan instrumentation will be done when linking to one bc file
        #    COMPILER_INVOCATION=$COMPILER_INVOCATION
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
    if [[ "$USE_COMPILER_PASS" == true ]]; then
        COMPILER_INVOCATION="$COMPILER_INVOCATION -fpass-plugin=$COMPILER_PASS -lprecompute"
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
if [ $has_multiple_src_file"" == true ]; then
      echo "ERROR linking multiple src files directly into one binary file is not supported"
      echo "Compile one by one and link afterwards"
      exit 1
fi
COMPILER_INVOCATION="$compiler"
if [[ "$USE_COMPILER_PASS" == true ]]; then
    COMPILER_INVOCATION="$COMPILER_INVOCATION -fpass-plugin=$COMPILER_PASS -lprecompute"
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

