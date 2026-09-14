# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

if(EXISTS "E:/test/bb/build-vs/_deps/prism-subbuild/prism-populate-prefix/src/prism-populate-stamp/prism-populate-gitclone-lastrun.txt" AND EXISTS "E:/test/bb/build-vs/_deps/prism-subbuild/prism-populate-prefix/src/prism-populate-stamp/prism-populate-gitinfo.txt" AND
  "E:/test/bb/build-vs/_deps/prism-subbuild/prism-populate-prefix/src/prism-populate-stamp/prism-populate-gitclone-lastrun.txt" IS_NEWER_THAN "E:/test/bb/build-vs/_deps/prism-subbuild/prism-populate-prefix/src/prism-populate-stamp/prism-populate-gitinfo.txt")
  message(VERBOSE
    "Avoiding repeated git clone, stamp file is up to date: "
    "'E:/test/bb/build-vs/_deps/prism-subbuild/prism-populate-prefix/src/prism-populate-stamp/prism-populate-gitclone-lastrun.txt'"
  )
  return()
endif()

# Even at VERBOSE level, we don't want to see the commands executed, but
# enabling them to be shown for DEBUG may be useful to help diagnose problems.
cmake_language(GET_MESSAGE_LOG_LEVEL active_log_level)
if(active_log_level MATCHES "DEBUG|TRACE")
  set(maybe_show_command COMMAND_ECHO STDOUT)
else()
  set(maybe_show_command "")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E rm -rf "E:/test/bb/build-vs/_deps/prism-src"
  RESULT_VARIABLE error_code
  ${maybe_show_command}
)
if(error_code)
  message(FATAL_ERROR "Failed to remove directory: 'E:/test/bb/build-vs/_deps/prism-src'")
endif()

# try the clone 3 times in case there is an odd git clone issue
set(error_code 1)
set(number_of_tries 0)
while(error_code AND number_of_tries LESS 3)
  execute_process(
    COMMAND "C:/Program Files/Git/cmd/git.exe"
            clone --no-checkout --config "advice.detachedHead=false" "https://github.com/KiritoDv/prism-processor.git" "prism-src"
    WORKING_DIRECTORY "E:/test/bb/build-vs/_deps"
    RESULT_VARIABLE error_code
    ${maybe_show_command}
  )
  math(EXPR number_of_tries "${number_of_tries} + 1")
endwhile()
if(number_of_tries GREATER 1)
  message(NOTICE "Had to git clone more than once: ${number_of_tries} times.")
endif()
if(error_code)
  message(FATAL_ERROR "Failed to clone repository: 'https://github.com/KiritoDv/prism-processor.git'")
endif()

execute_process(
  COMMAND "C:/Program Files/Git/cmd/git.exe"
          checkout "1de054450e7b3c5f777d2e3dfcb228ad120c329d" --
  WORKING_DIRECTORY "E:/test/bb/build-vs/_deps/prism-src"
  RESULT_VARIABLE error_code
  ${maybe_show_command}
)
if(error_code)
  message(FATAL_ERROR "Failed to checkout tag: '1de054450e7b3c5f777d2e3dfcb228ad120c329d'")
endif()

set(init_submodules TRUE)
if(init_submodules)
  execute_process(
    COMMAND "C:/Program Files/Git/cmd/git.exe" 
            submodule update --recursive --init 
    WORKING_DIRECTORY "E:/test/bb/build-vs/_deps/prism-src"
    RESULT_VARIABLE error_code
    ${maybe_show_command}
  )
endif()
if(error_code)
  message(FATAL_ERROR "Failed to update submodules in: 'E:/test/bb/build-vs/_deps/prism-src'")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy "E:/test/bb/build-vs/_deps/prism-subbuild/prism-populate-prefix/src/prism-populate-stamp/prism-populate-gitinfo.txt" "E:/test/bb/build-vs/_deps/prism-subbuild/prism-populate-prefix/src/prism-populate-stamp/prism-populate-gitclone-lastrun.txt"
  RESULT_VARIABLE error_code
  ${maybe_show_command}
)
if(error_code)
  message(FATAL_ERROR "Failed to copy script-last-run stamp file: 'E:/test/bb/build-vs/_deps/prism-subbuild/prism-populate-prefix/src/prism-populate-stamp/prism-populate-gitclone-lastrun.txt'")
endif()
