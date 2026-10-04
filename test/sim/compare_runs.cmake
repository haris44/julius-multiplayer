# Runs "simtool trace" twice in two processes and fails if the traces differ.
# Usage: cmake -DSIMTOOL=... -DSAVE=... -DTICKS=... -DSTEP=... -P compare_runs.cmake
foreach(run first second)
    execute_process(COMMAND ${SIMTOOL} trace ${SAVE} ${TICKS} ${STEP}
        OUTPUT_VARIABLE ${run} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "simtool failed (${result}):\n${${run}}")
    endif()
endforeach()
if(NOT first STREQUAL second)
    message(FATAL_ERROR "Traces differ between two runs:\n--- first\n${first}\n--- second\n${second}")
endif()
string(REGEX MATCHALL "\n" lines "${first}")
list(LENGTH lines count)
message(STATUS "Identical traces (${count} samples)")
