# SPDX-License-Identifier: MIT

# Runs seqlib_check on a file whose name has characters outside most ANSI code pages. On Windows, the tool can open
# such a file only by its name in the wide command line. The file holds text, not MIDI. A tool that opens it reports
# the missing MIDI header. A tool that can't open it reports that it can't be read.
#
#     cmake -DSEQLIB_CHECK=<seqlib_check> -DFOLDER=<a folder for the file> -P check_names.cmake

set(name "${FOLDER}/音楽.mid")
file(WRITE "${name}" "not a MIDI file")
execute_process(COMMAND "${SEQLIB_CHECK}" "${name}" RESULT_VARIABLE status OUTPUT_VARIABLE out ENCODING UTF-8)
if(NOT status EQUAL 1 OR out MATCHES "can't be read" OR NOT out MATCHES "音楽\\.mid")
  message(FATAL_ERROR "seqlib_check didn't read ${name}. It exited with ${status}, and printed:\n${out}")
endif()
