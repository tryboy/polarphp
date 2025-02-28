# This source file is part of the polarphp.org open source project
#
# Copyright (c) 2017 - 2019 polarphp software foundation
# Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
# Licensed under Apache License v2.0 with Runtime Library Exception
#
# See https://polarphp.org/LICENSE.txt for license information
# See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
#
# Created by polarboy on 2018/08/26.

add_subdirectory(bytecode)
add_subdirectory(codegen)
add_subdirectory(global)
add_subdirectory(basic)
add_subdirectory(markup)
add_subdirectory(kernel)
add_subdirectory(utils)
add_subdirectory(vm)
add_subdirectory(runtime)
add_subdirectory(sema)
add_subdirectory(driver)
add_subdirectory(serialization)
add_subdirectory(reflection)
add_subdirectory(syntax)

# setup compiler sources
set(POLAR_PARSER_SOURCES)

# process language grammer files
include(ProcessLangGrammer)

polar_collect_files(
   TYPE_BOTH
   DIR ast
   OUTPUT_VAR POLAR_AST_SOURCES)
polar_merge_list(POLAR_PARSER_SOURCES POLAR_AST_SOURCES)

polar_collect_files(
   TYPE_BOTH
   DIR parser
   OUTPUT_VAR POLAR_PARSER_SOURCES)

polar_merge_list(POLAR_PARSER_SOURCES POLAR_PARSER_SOURCES)
polar_merge_list(POLAR_PARSER_SOURCES POLAR_HEADERS)

polar_add_library(PolarParser SHARED
   ${POLAR_PARSER_SOURCES}
   LINK_LIBS PolarUtils PolarKernel PolarSyntax)

# for YYParserDefs.h
target_include_directories(PolarParser PRIVATE ${POLAR_MAIN_INCLUDE_DIR}/polarphp/parser/internal)
