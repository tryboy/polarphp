//===--- DiagnosticsFrontend.def - Diagnostics Text -------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This source file is part of the polarphp.org open source project
//
// Copyright (c) 2017 - 2019 polarphp software foundation
// Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://polarphp.org/LICENSE.txt for license information
// See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
//
// Created by polarboy on 2019/04/25.
//
//===----------------------------------------------------------------------===//
//
//  This file defines diagnostics emitted in processing command-line arguments
//  and setting up compilation.
//  Each diagnostic is described using one of three kinds (error, warning, or
//  note) along with a unique identifier, category, options, and text, and is
//  followed by a signature describing the diagnostic argument kinds.
//
//===----------------------------------------------------------------------===//

#if !(defined(DIAG) || (defined(ERROR) && defined(WARNING) && defined(NOTE)))
#  error Must define either DIAG or the set {ERROR,WARNING,NOTE}
#endif

#ifndef DIAG
#define DIAG(ERROR,ID,Options,Text,Signature)
#endif

#ifndef ERROR
#  define ERROR(ID,Options,Text,Signature)   \
   DIAG(ERROR,ID,Options,Text,Signature)
#endif

#ifndef WARNING
#  define WARNING(ID,Options,Text,Signature) \
   DIAG(WARNING,ID,Options,Text,Signature)
#endif

#ifndef NOTE
#  define NOTE(ID,Options,Text,Signature) \
   DIAG(NOTE,ID,Options,Text,Signature)
#endif

WARNING(warning_no_such_sdk,none,
        "no such SDK: '%0'", (StringRef))

ERROR(error_no_frontend_args, none,
      "no arguments provided to '-frontend'", ())

ERROR(error_no_such_file_or_directory,none,
      "no such file or directory: '%0'", (StringRef))

ERROR(error_unsupported_target_os, none,
      "unsupported target OS: '%0'", (StringRef))

ERROR(error_unsupported_target_arch, none,
      "unsupported target architecture: '%0'", (StringRef))

ERROR(error_unsupported_opt_for_target, none,
      "unsupported option '%0' for target '%1'", (StringRef, StringRef))

ERROR(error_argument_not_allowed_with, none,
      "argument '%0' is not allowed with '%1'", (StringRef, StringRef))

WARNING(warning_argument_not_supported_with_optimization, none,
        "argument '%0' is not supported with optimization", (StringRef))

ERROR(error_option_requires_sanitizer, none,
      "option '%0' requires a sanitizer to be enabled. Use -sanitize= to "
      "enable a sanitizer", (StringRef))

ERROR(error_option_missing_required_argument, none,
      "option '%0' is missing a required argument (%1)", (StringRef, StringRef))

ERROR(cannot_open_file,none,
      "cannot open file '%0' (%1)", (StringRef, StringRef))
ERROR(cannot_open_serialized_file,none,
      "cannot open file '%0' for diagnostics emission (%1)",
      (StringRef, StringRef))
ERROR(error_open_input_file,none,
      "error opening input file '%0' (%1)", (StringRef, StringRef))
ERROR(error_clang_importer_create_fail,none,
      "clang importer creation failed", ())
ERROR(error_missing_arg_value,none,
      "missing argument value for '%0', expected %1 argument(s)",
      (StringRef, unsigned))
ERROR(error_unknown_arg,none,
      "unknown argument: '%0'", (StringRef))
ERROR(error_invalid_arg_value,none,
      "invalid value '%1' in '%0'", (StringRef, StringRef))
WARNING(warning_cannot_multithread_batch_mode,none,
        "ignoring -num-threads argument; cannot multithread batch mode", ())
ERROR(error_unsupported_option_argument,none,
      "unsupported argument '%1' to option '%0'", (StringRef, StringRef))
ERROR(error_immediate_mode_missing_stdlib,none,
      "could not load the swift standard library", ())
ERROR(error_immediate_mode_missing_library,none,
      "could not load %select{shared library|framework}0 '%1'",
      (unsigned, StringRef))
ERROR(error_immediate_mode_primary_file,none,
      "immediate mode is incompatible with -primary-file", ())
ERROR(error_missing_frontend_action,none,
      "no frontend action was selected", ())
ERROR(error_invalid_source_location_str,none,
      "invalid source location string '%0'", (StringRef))
ERROR(error_no_source_location_scope_map,none,
      "-dump-scope-maps argument must be 'expanded' or a list of "
      "source locations", ())

NOTE(note_valid_swift_versions, none,
     "valid arguments to '-swift-version' are %0", (StringRef))

ERROR(error_mode_cannot_emit_dependencies,none,
      "this mode does not support emitting dependency files", ())
ERROR(error_mode_cannot_emit_reference_dependencies,none,
      "this mode does not support emitting reference dependency files", ())
ERROR(error_mode_cannot_emit_header,none,
      "this mode does not support emitting Objective-C headers", ())
ERROR(error_mode_cannot_emit_loaded_module_trace,none,
      "this mode does not support emitting the loaded module trace", ())
ERROR(error_mode_cannot_emit_module,none,
      "this mode does not support emitting modules", ())
ERROR(error_mode_cannot_emit_module_doc,none,
      "this mode does not support emitting module documentation files", ())
ERROR(error_mode_cannot_emit_interface,none,
      "this mode does not support emitting parseable interface files", ())

WARNING(emit_reference_dependencies_without_primary_file,none,
        "ignoring -emit-reference-dependencies (requires -primary-file)", ())

ERROR(error_bad_module_name,none,
      "module name \"%0\" is not a valid identifier"
      "%select{|; use -module-name flag to specify an alternate name}1",
      (StringRef, bool))
ERROR(error_stdlib_module_name,none,
      "module name \"%0\" is reserved for the standard library"
      "%select{|; use -module-name flag to specify an alternate name}1",
      (StringRef, bool))

ERROR(error_stdlib_not_found,Fatal,
      "unable to load standard library for target '%0'", (StringRef))

ERROR(error_unable_to_load_supplementary_output_file_map, none,
      "unable to load supplementary output file map '%0': %1",
      (StringRef, StringRef))

ERROR(error_missing_entry_in_supplementary_output_file_map, none,
      "supplementary output file map '%0' is missing an entry for '%1' "
      "(this likely indicates a compiler issue; please file a bug report)",
      (StringRef, StringRef))

ERROR(error_repl_requires_no_input_files,none,
      "REPL mode requires no input files", ())
ERROR(error_mode_requires_one_input_file,none,
      "this mode requires a single input file", ())
ERROR(error_mode_requires_an_input_file,none,
      "this mode requires at least one input file", ())
ERROR(error_mode_requires_one_sil_multi_sib,none,
      "this mode requires .sil for primary-file and only .sib for other inputs",
      ())

ERROR(error_no_output_filename_specified,none,
      "an output filename was not specified for a mode which requires an "
      "output filename", ())

ERROR(error_implicit_output_file_is_directory,none,
      "the implicit output file '%0' is a directory; explicitly specify a "
      "filename using -o", (StringRef))

ERROR(error_if_any_output_files_are_specified_they_all_must_be,none,
      "if any output files are specified, they all must be", ())

ERROR(error_primary_file_not_found,none,
      "primary file '%0' was not found in file list '%1'",
      (StringRef, StringRef))

ERROR(error_cannot_have_input_files_with_file_list,none,
      "cannot have input files with file list", ())

ERROR(error_cannot_have_primary_files_with_primary_file_list,none,
      "cannot have primary input files with primary file list", ())

ERROR(error_cannot_have_supplementary_outputs,none,
      "cannot have '%0' with '%1'", (StringRef, StringRef))

ERROR(error_duplicate_input_file,none,
      "duplicate input file '%0'", (StringRef))

ERROR(repl_must_be_initialized,none,
      "variables currently must have an initial value when entered at the "
      "top level of the REPL", ())

ERROR(error_doing_code_completion,none,
      "compiler is in code completion mode (benign diagnostic)", ())

ERROR(verify_encountered_fatal,none,
      "fatal error encountered while in -verify mode", ())

ERROR(error_parse_input_file,none,
      "error parsing input file '%0' (%1)", (StringRef, StringRef))

ERROR(error_write_index_unit,none,
      "writing index unit file: %0", (StringRef))
ERROR(error_create_index_dir,none,
      "creating index directory: %0", (StringRef))
ERROR(error_write_index_record,none,
      "writing index record file: %0", (StringRef))
ERROR(error_index_failed_status_check,none,
      "failed file status check: %0", (StringRef))
ERROR(error_index_inputs_more_than_outputs,none,
      "index output filenames do not match input source files", ())

ERROR(error_wrong_number_of_arguments,none,
      "wrong number of '%0' arguments (expected %1, got %2)",
      (StringRef, int, int))

ERROR(error_formatting_multiple_file_ranges,none,
      "file ranges don't support multiple input files", ())

ERROR(error_formatting_invalid_range,none,
      "file range is invalid", ())

WARNING(stats_disabled,none,
        "compiler was not built with support for collecting statistics", ())

WARNING(tbd_only_supported_in_whole_module,none,
        "TBD generation is only supported when the whole module can be seen",
        ())

ERROR(symbol_in_tbd_not_in_ir,none,
      "symbol '%0' (%1) is in TBD file, but not in generated IR",
      (StringRef, StringRef))
ERROR(symbol_in_ir_not_in_tbd,none,
      "symbol '%0' (%1) is in generated IR file, but not in TBD file",
      (StringRef, StringRef))

ERROR(tbd_validation_failure,none,
      "please file a radar or open a bug on bugs.swift.org with this code, and "
      "add -Xfrontend -validate-tbd-against-ir=none to squash the errors", ())

ERROR(redundant_prefix_compilation_flag,none,
      "invalid argument '-D%0'; did you provide a redundant '-D' in your "
      "build settings?", (StringRef))

ERROR(invalid_conditional_compilation_flag,none,
      "conditional compilation flags must be valid Swift identifiers "
      "(rather than '%0')", (StringRef))

WARNING(cannot_assign_value_to_conditional_compilation_flag,none,
        "conditional compilation flags do not have values in Swift; they are "
        "either present or absent (rather than '%0')", (StringRef))

ERROR(error_optimization_remark_pattern, none, "%0 in '%1'",
      (StringRef, StringRef))

ERROR(error_invalid_debug_prefix_map, none,
      "invalid argument '%0' to -debug-prefix-map; it must be of the form "
      "'original=remapped'", (StringRef))

ERROR(invalid_vfs_overlay_file,none,
      "invalid virtual overlay file '%0'", (StringRef))

WARNING(parseable_interface_scoped_import_unsupported,none,
        "scoped imports are not yet supported in parseable module interfaces",
        ())
ERROR(error_extracting_version_from_parseable_interface,none,
      "error extracting version from parseable module interface", ())
ERROR(unsupported_version_of_parseable_interface,none,
      "unsupported version of parseable module interface '%0': '%1'",
      (StringRef, polar::utils::VersionTuple))
ERROR(error_extracting_flags_from_parseable_interface,none,
      "error extracting flags from parseable module interface", ())
ERROR(missing_dependency_of_parseable_module_interface,none,
      "missing dependency '%0' of parseable module interface '%1': %2",
      (StringRef, StringRef, StringRef))
ERROR(error_extracting_dependencies_from_cached_module,none,
      "error extracting dependencies from cached module '%0'",
      (StringRef))
ERROR(unknown_forced_module_loading_mode,none,
      "unknown value for SWIFT_FORCE_MODULE_LOADING variable: '%0'",
      (StringRef))

#ifndef DIAG_NO_UNDEF
# if defined(DIAG)
#  undef DIAG
# endif
# undef NOTE
# undef WARNING
# undef ERROR
#endif
