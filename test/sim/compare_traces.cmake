# Runs simtool with two argument lists and checks that the outputs are the same or different.
# Arguments are separated with "|" (CMake lists cannot be passed through -D on the test command line).
# Usage: cmake -DSIMTOOL=... -DARGS_A="trace|x.sav|100" -DARGS_B="--mp|trace|x.sav|100" -DEXPECT=same|different
#        -P compare_traces.cmake
string(REPLACE "|" ";" args_a "${ARGS_A}")
string(REPLACE "|" ";" args_b "${ARGS_B}")
execute_process(COMMAND ${SIMTOOL} ${args_a} OUTPUT_VARIABLE out_a RESULT_VARIABLE result_a)
execute_process(COMMAND ${SIMTOOL} ${args_b} OUTPUT_VARIABLE out_b RESULT_VARIABLE result_b)
if(NOT result_a EQUAL 0 OR NOT result_b EQUAL 0)
    message(FATAL_ERROR "simtool failed: ${result_a} / ${result_b}\n${out_a}\n${out_b}")
endif()
if(EXPECT STREQUAL "same" AND NOT out_a STREQUAL out_b)
    message(FATAL_ERROR "Outputs differ but should be the same:\n--- ${ARGS_A}\n${out_a}\n--- ${ARGS_B}\n${out_b}")
elseif(EXPECT STREQUAL "different" AND out_a STREQUAL out_b)
    message(FATAL_ERROR "Outputs are the same but should differ:\n--- ${ARGS_A}\n${out_a}")
endif()
message(STATUS "Outputs are ${EXPECT}, as expected")
