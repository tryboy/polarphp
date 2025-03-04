// This source file is part of the polarphp.org open source project
//
// Copyright (c) 2017 - 2019 polarphp software foundation
// Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://polarphp.org/LICENSE.txt for license information
// See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
//
// Created by polarboy on 2019/05/09.

#ifndef POLARPHP_PARSER_LEXER_H
#define POLARPHP_PARSER_LEXER_H

#include "polarphp/basic/adt/StringExtras.h"
#include "polarphp/ast/DiagnosticEngine.h"
#include "polarphp/parser/SourceLoc.h"
#include "polarphp/parser/SourceMgr.h"
#include "polarphp/parser/Token.h"
#include "polarphp/parser/ParsedTrivia.h"
#include "polarphp/parser/LexerState.h"
#include "polarphp/utils/SaveAndRestore.h"
#include "polarphp/parser/internal/YYLexerDefs.h"
#include "polarphp/parser/LexerFlags.h"
#include "polarphp/kernel/LangOptions.h"

#include <stack>

namespace polar::parser {

namespace internal {
bool strip_multiline_string_indentation(Lexer &lexer, std::string &str, int indentation, bool usingSpaces,
                                        bool newlineAtStart, bool newlineAtEnd);
bool convert_double_quote_str_escape_sequences(std::string &filteredStr, char quoteType, const char *iter,
                                               const char *endMark, Lexer &lexer);
}

using polar::ast::Diagnostic;
using polar::ast::DiagnosticEngine;
using polar::ast::InFlightDiagnostic;
using polar::ast::Diag;
using polar::kernel::LangOptions;
union ParserStackElement;

class Parser;

/// Given a pointer to the starting byte of a UTF8 character, validate it and
/// advance the lexer past it.  This returns the encoded character or ~0U if
/// the encoding is invalid.
///
uint32_t validate_utf8_character_and_advance(const unsigned char *&ptr, const unsigned char *end);

class Lexer final
{
private:
   struct PrincipalTag {};

   /// Nul character meaning kind.
   enum class NullCharacterKind
   {
      /// String buffer terminator.
      BufferEnd,
      /// Embedded nul character.
      Embedded,
      /// Code completion marker.
      CodeCompletion
   };

public:
   /// Create a normal lexer that scans the whole source buffer.
   ///
   /// \param Options - the language options under which to lex.  By
   ///   design, language options only affect whether a token is valid
   ///   and/or the exact token kind produced (e.g. keyword or
   ///   identifier), but not things like how many characters are
   ///   consumed.  If that changes, APIs like getLocForEndOfToken will
   ///   need to take a LangOptions explicitly.
   Lexer(const LangOptions &options, const SourceManager &sourceMgr,
         unsigned bufferId, DiagnosticEngine *diags,
         CommentRetentionMode commentRetention = CommentRetentionMode::None,
         TriviaRetentionMode triviaRetention = TriviaRetentionMode::WithoutTrivia);

   /// Create a lexer that scans a subrange of the source buffer.
   Lexer(const LangOptions &options, const SourceManager &sourceMgr,
         unsigned bufferId, DiagnosticEngine *diags,
         CommentRetentionMode commentRetention,
         TriviaRetentionMode triviaRetention, unsigned offset,
         unsigned endOffset);

   /// Create a sub-lexer that lexes from the same buffer, but scans
   /// a subrange of the buffer.
   ///
   /// \param Parent the parent lexer that scans the whole buffer
   /// \param BeginState start of the subrange
   /// \param EndState end of the subrange
   Lexer(Lexer &parent, LexerState beginState, LexerState endState);

   /// Returns true if this lexer will produce a code completion token.
   bool isCodeCompletion() const
   {
      return m_codeCompletionPtr != nullptr;
   }

   /// Lex a token. If \c TriviaRetentionMode is \c WithTrivia, passed pointers
   /// to trivias are populated.
   void lex(Token &result, ParsedTrivia &leadingTriviaResult, ParsedTrivia &trailingTrivialResult);
   void lex(Token &result)
   {
      ParsedTrivia leadingTrivia;
      ParsedTrivia trailingTrivia;
      lex(result, leadingTrivia, trailingTrivia);
   }

   /// Reset the lexer's buffer pointer to \p Offset bytes after the buffer
   /// start.
   void resetToOffset(size_t offset)
   {
      assert(m_bufferStart + offset <= m_bufferEnd && "offset after buffer end");
      m_yyCursor = m_bufferStart + offset;
      lexImpl();
   }

   bool isKeepingComments() const
   {
      return m_commentRetention == CommentRetentionMode::ReturnAsTokens;
   }

   const LexerFlags &getFlags() const
   {
      return m_flags;
   }

   Lexer &setLexingBinaryStrFlag(bool value)
   {
      m_flags.setLexingBinaryString(value);
      return *this;
   }

   bool isLexingBinaryStr() const
   {
      return m_flags.isLexingBinaryString();
   }

   Lexer &setCheckHeredocIndentation(bool value)
   {
      m_flags.setCheckHeredocIndentation(value);
      return *this;
   }

   bool isCheckHeredocIndentation()
   {
      return m_flags.isCheckHeredocIndentation();
   }

   unsigned int getBufferId() const
   {
      return m_bufferId;
   }

   /// peekNextToken - Return the next token to be returned by Lex without
   /// actually lexing it.
   const Token &peekNextToken() const
   {
      return m_nextToken;
   }

   /// Returns the lexer LexerState for the beginning of the given token
   /// location. After restoring the LexerState, lexer will return this token and
   /// continue from there.
   LexerState getStateForBeginningOfTokenLoc(SourceLoc sourceLoc) const;

   /// Returns the lexer LexerState for the beginning of the given token.
   /// After restoring the LexerState, lexer will return this token and continue from
   /// there.
   LexerState getStateForBeginningOfToken(const Token &token, const ParsedTrivia &leadingTrivia = {}) const
   {
      // If the token has a comment attached to it, rewind to before the comment,
      // not just the start of the token.  This ensures that we will re-lex and
      // reattach the comment to the token if rewound to this LexerState.
      SourceLoc tokenStart = token.getCommentStart();
      if (tokenStart.isInvalid()) {
         tokenStart = token.getLoc();
      }
      LexerState LexerState = getStateForBeginningOfTokenLoc(tokenStart);
      if (m_triviaRetention == TriviaRetentionMode::WithTrivia) {
         LexerState.m_leadingTrivia = leadingTrivia;
      }
      return LexerState;
   }

   LexerState getStateForEndOfTokenLoc(SourceLoc loc) const
   {
      return LexerState(getLocForEndOfToken(m_sourceMgr, loc));
   }

   bool isStateForCurrentBuffer(LexerState LexerState)
   {
      return m_sourceMgr.findBufferContainingLoc(LexerState.m_loc) == getBufferId();
   }

   /// Restore the lexer LexerState to a given one, that can be located either
   /// before or after the current position.
   void restoreState(LexerState LexerState, bool enableDiagnostics = false)
   {
      assert(LexerState.isValid());
      m_yyCursor = getBufferPtrForSourceLoc(LexerState.m_loc);
      // Don't reemit diagnostics while readvancing the lexer.
      polar::utils::SaveAndRestore<DiagnosticEngine *> diag(m_diags, enableDiagnostics ? m_diags : nullptr);
      lexImpl();
      // Restore Trivia.
      if (m_triviaRetention == TriviaRetentionMode::WithTrivia) {
         if (auto &ltrivia = LexerState.m_leadingTrivia) {
            m_leadingTrivia = std::move(*ltrivia);
         }
      }
   }

   Lexer &saveYYState();
   Lexer &restoreYYState();

   /// Restore the lexer LexerState to a given LexerState that is located before
   /// current position.
   void backtrackToState(LexerState LexerState)
   {
      assert(getBufferPtrForSourceLoc(LexerState.m_loc) <= m_yyCursor && "can't backtrack forward");
      restoreState(LexerState);
   }

   /// Retrieve the Token referred to by \c Loc.
   ///
   /// \param SM The source manager in which the given source location
   /// resides.
   ///
   /// \param Loc The source location of the beginning of a token.
   static Token getTokenAtLocation(const SourceManager &sourceMgr, SourceLoc loc);

   /// Retrieve the source location that points just past the
   /// end of the token referred to by \c Loc.
   ///
   /// \param SM The source manager in which the given source location
   /// resides.
   ///
   /// \param Loc The source location of the beginning of a token.

   static SourceLoc getLocForEndOfToken(const SourceManager &sourceMgr, SourceLoc loc);

   /// Convert a SourceRange to the equivalent CharSourceRange
   ///
   /// \param SM The source manager in which the given source range
   /// resides.
   ///
   /// \param SR The source range
   static CharSourceRange getCharSourceRangeFromSourceRange(const SourceManager &sourceMgr, const SourceRange &range)
   {
      return CharSourceRange(sourceMgr, range.m_start, getLocForEndOfToken(sourceMgr, range.m_end));
   }

   /// Return the start location of the token that the offset in the given buffer
   /// points to.
   ///
   /// Note that this is more expensive than \c getLocForEndOfToken because it
   /// finds and re-lexes from the beginning of the line.
   ///
   /// Due to the parser splitting tokens the adjustment may be incorrect, e.g:
   ///
   /// The start of the '<' token is '<', but the lexer will produce "+<" before
   /// the parser splits it up.
   ////
   /// If the offset points to whitespace the returned source location will point
   /// to the whitespace offset.
   static SourceLoc getLocForStartOfToken(SourceManager &sourceMgr, unsigned bufferId,
                                          unsigned offset);
   static SourceLoc getLocForStartOfToken(SourceManager &sourceMgr, SourceLoc loc);

   /// Retrieve the start location of the line containing the given location.
   /// the given location.
   static SourceLoc getLocForStartOfLine(SourceManager &sourceMgr, SourceLoc loc);

   /// Retrieve the source location for the end of the line containing the
   /// given token, which is the location of the start of the next line.
   static SourceLoc getLocForEndOfLine(SourceManager &sourceMgr, SourceLoc loc);

   /// Retrieve the string used to indent the line that contains the given
   /// source location.
   ///
   /// If \c ExtraIndentation is not null, it will be set to an appropriate
   /// additional intendation for adding code in a smaller scope "within" \c Loc.
   static StringRef getIndentationForLine(SourceManager &sourceMgr, SourceLoc loc,
                                          StringRef *extraIndentation = nullptr);

   /// Determines if the given string is a valid non-operator
   /// identifier, without escaping characters.
   static bool isIdentifier(StringRef identifier);

   /// Determines if the given string is a valid operator identifier,
   /// without escaping characters.
   static bool isOperator(StringRef string);

   SourceLoc getLocForStartOfBuffer() const
   {
      return SourceLoc(polar::utils::SMLocation::getFromPointer(reinterpret_cast<const char *>(m_bufferStart)));
   }

   static SourceLoc getSourceLoc(const unsigned char *loc)
   {
      return SourceLoc(polar::utils::SMLocation::getFromPointer(reinterpret_cast<const char *>(loc)));
   }

   /// Get the token that starts at the given location.
   Token getTokenAt(SourceLoc Loc);

   void incLineNumber(int count = 1)
   {
      m_lineNumber += count;
   }

   int getLineNumber()
   {
      return m_lineNumber;
   }

   unsigned int getYYLength()
   {
      return m_yyLength;
   }

   Lexer &setYYLength(unsigned int length)
   {
      m_yyLength = length;
      return *this;
   }

   /// re2c interface methods
   const unsigned char *&getYYText()
   {
      return m_yyText;
   }

   Lexer &setYYText(const unsigned char *text)
   {
      m_yyText = text;
      return *this;
   }

   const unsigned char *&getYYCursor()
   {
      return m_yyCursor;
   }

   Lexer &setYYCursor(const unsigned char *cursor)
   {
      m_yyCursor = cursor;
      return *this;
   }

   const unsigned char *&getYYLimit()
   {
      return m_artificialEof;
   }

   const unsigned char *&getYYMarker()
   {
      return m_yyMarker;
   }

   int getYYCondition()
   {
      return m_yyCondition;
   }

   Lexer &setYYCondition(YYLexerCondType cond)
   {
      m_yyCondition = cond;
      return *this;
   }

   Lexer &pushYYCondition(YYLexerCondType cond)
   {
      m_yyConditionStack.push(m_yyCondition);
      m_yyCondition = cond;
      return *this;
   }

   Lexer &popYYCondtion()
   {
      YYLexerCondType cond = m_yyConditionStack.top();
      m_yyCondition = cond;
      m_yyConditionStack.pop();
      return *this;
   }

   bool yyConditonStackEmpty()
   {
      return m_yyConditionStack.empty();
   }

   Lexer &pushHeredocLabel(std::shared_ptr<HereDocLabel> label)
   {
      m_heredocLabelStack.push(label);
      return *this;
   }

   std::shared_ptr<HereDocLabel> popHeredocLabel()
   {
      assert(!m_heredocLabelStack.empty() && "heredoc stack is empty");
      std::shared_ptr<HereDocLabel> label = m_heredocLabelStack.top();
      m_heredocLabelStack.pop();
      return label;
   }

   Lexer &setParser(Parser *parser)
   {
      m_parser = parser;
      return *this;
   }

   Lexer &setSemanticValueContainer(ParserSemantic *container)
   {
      m_valueContainer = container;
      return *this;
   }

   ParserSemantic *getSemanticValueContainer()
   {
      return m_valueContainer;
   }

   bool isInParseMode()
   {
      return m_valueContainer != nullptr;
   }

   Lexer &registerLexicalExceptionHandler(LexicalExceptionHandler handler)
   {
      m_lexicalExceptionHandler = std::move(handler);
      return *this;
   }

   bool isLexExceptionOccurred() const
   {
      return m_flags.isLexExceptionOccurred();
   }

   void clearExceptionFlag()
   {
      m_flags.setLexExceptionOccurred(false);
   }

   const std::string getCurrentExceptionMsg() const
   {
      return m_currentExceptionMsg;
   }

private:
   Lexer(const Lexer&) = delete;
   void operator=(const Lexer&) = delete;
   /// The principal constructor used by public constructors below.
   /// Don't use this constructor for other purposes, it does not initialize
   /// everything.
   Lexer(const PrincipalTag &, const LangOptions &langOpts,
         const SourceManager &sourceMgr, unsigned bufferId,
         DiagnosticEngine *diags, CommentRetentionMode commentRetention,
         TriviaRetentionMode triviaRetention);
   void initialize(unsigned offset, unsigned endOffset);
   void lexImpl();

   /// For a source location in the current buffer, returns the corresponding
   /// pointer.
   const unsigned char *getBufferPtrForSourceLoc(SourceLoc loc) const
   {
      return m_bufferStart + m_sourceMgr.getLocOffsetInBuffer(loc, m_bufferId);
   }

   InFlightDiagnostic diagnose(const unsigned char *loc, Diagnostic diag);

   template<typename ...DiagArgTypes, typename ...ArgTypes>
   InFlightDiagnostic diagnose(const unsigned char *loc, Diag<DiagArgTypes...> diagId,
                               ArgTypes &&...args)
   {
      return diagnose(loc, Diagnostic(diagId, std::forward<ArgTypes>(args)...));
   }

   void formToken(TokenKindType kind, const unsigned char *tokenStart);
   void formEscapedIdentifierToken(const unsigned char *tokenStart);
   void formVariableToken(const unsigned char *tokenStart);
   void formIdentifierToken(const unsigned char *tokenStart);
   void formStringVariableToken(const unsigned char *tokenStart);
   void formErrorToken(const unsigned char *tokenStart);

   /// Advance to the end of the line.
   /// If EatNewLine is true, CurPtr will be at end of newline character.
   /// Otherwise, CurPtr will be at newline character.
   void skipToEndOfLine(bool eatNewline);

   /// Skip to the end of the line of a // comment.
   void skipSlashSlashComment(bool eatNewline);
   void skipSlashStarComment();
   void skipHashbang(bool eatNewline);
   void lexIdentifier();
   void lexBinaryNumber();
   void lexHexNumber();
   void lexLongNumber();
   void lexDoubleNumber();
   void lexSingleQuoteString();
   void lexDoubleQuoteString();
   void lexBackquote();
   void lexHeredocHeader();
   void lexHeredocBody();
   void lexNowdocBody();
   void lexHereAndNowDocEnd();
   void lexTrivia(ParsedTrivia &trivia, bool isForTrailingTrivia);
   void lexEscapedIdentifier();

   /// Returns it should be tokenize.
   bool lexUnknown(bool emitDiagnosticsIfToken);
   NullCharacterKind getNullCharacterKind(const unsigned char *ptr) const;

   bool nextLineHasHeredocEndMarker();
   bool isFoundHeredocEndMarker(std::shared_ptr<HereDocLabel> label) const
   {
      long int labelLength = label->name.size();
      return isLabelStart(*m_yyCursor) &&
            (labelLength < m_artificialEof - m_yyCursor) &&
            (StringRef(reinterpret_cast<const char *>(m_yyCursor), labelLength) == label->name);
   }

   bool isLabelStart(unsigned char c) const
   {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80;
   }

   void notifyLexicalException(StringRef msg, int code);
   template <typename... ArgTypes>
   void notifyLexicalException(int code, StringRef format, const ArgTypes&... arg)
   {
      using polar::basic::sprintable;
      size_t length = std::snprintf(nullptr, 0, format.data(), sprintable(arg)...) + 1;
      std::unique_ptr<char[]> buffer(new char[length]);
      std::snprintf(buffer.get(), length, format.data(), sprintable(arg)...);
      notifyLexicalException(StringRef(buffer.get(), length - 1), code);
   }

private:
   friend void internal::yy_token_lex(Lexer &lexer);
   friend bool internal::strip_multiline_string_indentation(Lexer &lexer, std::string &str, int indentation, bool usingSpaces,
                                                            bool newlineAtStart, bool newlineAtEnd);
   friend bool internal::convert_double_quote_str_escape_sequences(std::string &filteredStr, char quoteType, const char *iter,
                                                                   const char *endMark, Lexer &lexer);
private:
   LexerFlags m_flags;
   const LangOptions &m_langOpts;
   const SourceManager &m_sourceMgr;
   const unsigned int m_bufferId;
   DiagnosticEngine *m_diags;
   Parser *m_parser = nullptr;

   /// Pointer to the first character of the buffer, even in a lexer that
   /// scans a subrange of the buffer.
   const unsigned char *m_bufferStart;

   /// Pointer to one past the end character of the buffer, even in a lexer
   /// that scans a subrange of the buffer.  Because the buffer is always
   /// NUL-terminated, this points to the NUL terminator.
   const unsigned char *m_bufferEnd;

   /// Pointer to the artificial EOF that is located before BufferEnd.  Useful
   /// for lexing subranges of a buffer.
   const unsigned char *m_artificialEof = nullptr;

   /// If non-null, points to the '\0' character in the buffer where we should
   /// produce a code completion token.
   const unsigned char *m_codeCompletionPtr = nullptr;

   /// Points to BufferStart or past the end of UTF-8 BOM sequence if it exists.
   const unsigned char *m_contentStart;

   /// current token text
   const unsigned char *m_yyText = nullptr;

   /// Pointer to the next not consumed character.
   const unsigned char *m_yyCursor = nullptr;

   /// backup pointer
   const unsigned char *m_yyMarker = nullptr;

   /// The token semantic value
   ParserSemantic *m_valueContainer = nullptr;

   YYLexerCondType m_yyCondition = COND_NAME(ST_IN_SCRIPTING);
   unsigned m_heredocIndentation;
   /// current token length
   std::size_t m_yyLength;
   unsigned int m_lineNumber;

   LexicalEventHandler m_eventHandler = nullptr;
   LexicalExceptionHandler m_lexicalExceptionHandler = nullptr;

   Token m_nextToken;

   const CommentRetentionMode m_commentRetention;
   const TriviaRetentionMode m_triviaRetention;

   /// The current leading trivia for the next token.
   ///
   /// This is only preserved if this Lexer was constructed with
   /// `TriviaRetentionMode::WithTrivia`.
   ParsedTrivia m_leadingTrivia;

   /// The current trailing trivia for the next token.
   ///
   /// This is only preserved if this Lexer was constructed with
   /// `TriviaRetentionMode::WithTrivia`.
   ParsedTrivia m_trailingTrivia;
   std::string m_currentExceptionMsg;
   std::stack<YYLexerCondType> m_yyConditionStack;
   std::stack<std::shared_ptr<HereDocLabel>> m_heredocLabelStack;
   std::stack<LexerState> m_yyStateStack;
};

/// Given an ordered token \param Array , get the iterator pointing to the first
/// token that is not before \param Loc .
template<typename ArrayTy, typename Iterator = typename ArrayTy::iterator>
Iterator token_lower_bound(ArrayTy &array, SourceLoc loc)
{
   return std::lower_bound(array.begin(), array.end(), loc,
                           [](const Token &t, SourceLoc l) {
      return t.getLoc().getOpaquePointerValue() < l.getOpaquePointerValue();
   });
}

/// Given an ordered token array \param AllTokens , get the slice of the array
/// where front() locates at \param StartLoc and back() locates at \param EndLoc .
ArrayRef<Token> slice_token_array(ArrayRef<Token> allTokens, SourceLoc startLoc,
                                  SourceLoc endLoc);

template <typename DF>
void tokenize(const LangOptions &langOpts, const SourceManager &sourceMgr,
              unsigned bufferId, unsigned offset, unsigned endOffset,
              DiagnosticEngine * diags,
              CommentRetentionMode commentRetention,
              TriviaRetentionMode triviaRetention, DF &&destFunc, std::function<void(Lexer &)> prepareLexFunc = nullptr)
{
   assert(triviaRetention != TriviaRetentionMode::WithTrivia && "string interpolation with trivia is not implemented yet");

   if (offset == 0 && endOffset == 0) {
      endOffset = sourceMgr.getRangeForBuffer(bufferId).getByteLength();
   }

   Lexer lexer(langOpts, sourceMgr, bufferId, diags, commentRetention, triviaRetention, offset,
               endOffset);

   if (prepareLexFunc) {
      prepareLexFunc(lexer);
   }

   Token token;
   ParsedTrivia leadingTrivia;
   ParsedTrivia trailingTrivia;
   do {
      lexer.lex(token, leadingTrivia, trailingTrivia);
      destFunc(lexer, token, leadingTrivia, trailingTrivia);
   } while (token.getKind() != TokenKindType::END);
}

/// Lex and return a vector of tokens for the given buffer.
std::vector<Token> tokenize(const LangOptions &langOpts,
                            const SourceManager &sourceMgr, unsigned bufferId,
                            unsigned offset = 0, unsigned endOffset = 0,
                            DiagnosticEngine *diags = nullptr,
                            bool keepComments = true);

} // polar::parser

#endif // POLARPHP_PARSER_LEXER_H
