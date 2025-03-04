// This source file is part of the polarphp.org open source project
//
// Copyright (c) 2017 - 2019 polarphp software foundation
// Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://polarphp.org/LICENSE.txt for license information
// See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
//
// Created by polarboy on 2019/06/26.

#ifndef POLARPHP_PARSER_INTERNAL_YY_LEXER_EXTRAS_H
#define POLARPHP_PARSER_INTERNAL_YY_LEXER_EXTRAS_H

#include <cstddef>
#include <string>

#include "polarphp/syntax/internal/TokenEnumDefs.h"
#include "polarphp/basic/adt/SmallVector.h"

/// forward declare class
namespace polar::parser {
class Lexer;
class Token;
} // polar::parser

namespace polar::basic {
class StringRef;
} // polar::basic

namespace polar::ast {
class DiagnosticEngine;
} // polar::ast

namespace polar::parser::internal {

using polar::syntax::internal::TokenKindType;
using polar::basic::SmallVectorImpl;
using polar::basic::StringRef;
using polar::ast::DiagnosticEngine;

bool encode_to_utf8(unsigned c,
                    SmallVectorImpl<char> &result);
unsigned count_leading_ones(unsigned char c);
bool is_start_of_utf8_character(unsigned char c);
void strip_underscores(unsigned char *str, int &length);
size_t count_str_newline(const unsigned char *str, size_t length);
void handle_newlines(Lexer &lexer, const unsigned char *str, size_t length);
void handle_newline(Lexer &lexer, unsigned char c);
TokenKindType token_kind_map(unsigned char c);
size_t convert_single_quote_str_escape_sequences(char *iter, char *endMark, Lexer &lexer);
bool convert_double_quote_str_escape_sequences(std::string &filteredStr, char quoteType, const char *iter,
                                               const char *endMark, Lexer &lexer);
void diagnose_embedded_null(DiagnosticEngine *diags, const unsigned char *ptr);
bool advance_to_end_of_line(const unsigned char *&m_yyCursor, const unsigned char *bufferEnd,
                            const unsigned char *codeCompletionPtr = nullptr,
                            DiagnosticEngine *diags = nullptr);
bool skip_to_end_of_slash_star_comment(const unsigned char *&m_yyCursor,
                                       const unsigned char *bufferEnd,
                                       const unsigned char *codeCompletionPtr = nullptr,
                                       DiagnosticEngine *diags = nullptr);
bool is_valid_identifier_continuation_code_point(uint32_t c);
bool is_valid_identifier_start_code_point(uint32_t c);
bool advance_if(const unsigned char *&ptr, const unsigned char *end,
                bool (*predicate)(uint32_t));
bool advance_if_valid_start_of_identifier(const unsigned char *&ptr,
                                          const unsigned char *end);
bool advance_if_valid_continuation_of_identifier(const unsigned char *&ptr,
                                                 const unsigned char *end);
bool advance_if_valid_start_of_operator(const unsigned char *&ptr,
                                        const unsigned char *end);
bool advance_if_valid_continuation_of_operator(const unsigned char *&ptr,
                                               const unsigned char *end);
const char *next_newline(const char *str, const char *end, size_t &newlineLen);
bool strip_multiline_string_indentation(Lexer &lexer, std::string &str, int indentation, bool usingSpaces,
                                        bool newlineAtStart, bool newlineAtEnd);
} // polar::parser::internal

#endif // POLARPHP_PARSER_INTERNAL_YY_LEXER_EXTRAS_H
