// This source file is part of the polarphp.org open source project
//
// Copyright (c) 2017 - 2019 polarphp software foundation
// Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://polarphp.org/LICENSE.txt for license information
// See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
//
// Created by polarboy on 2018/10/25.

#include "ExtraFuncs.h"
#include "polarphp/utils/RawOutStream.h"
#include "polarphp/utils/ErrorHandling.h"
#include "polarphp/utils/WithColor.h"
#include "CLI/CLI.hpp"
#include <cassert>

namespace polar {
namespace filechecker {

using polar::utils::WithColor;
using polar::utils::RawStringOutStream;

CLI::App *sg_commandParser = nullptr;
std::vector<std::string> sg_checkPrefixes{};
std::vector<std::string> sg_defines{};
std::vector<std::string> sg_implicitCheckNot;

CLI::App &retrieve_command_parser()
{
   assert(sg_commandParser != nullptr);
   return *sg_commandParser;
}

static std::map<std::string, DumpInputValue> sg_allowDumpOpts{
   {"help", DumpInputValue::Help},
   {"never", DumpInputValue::Never},
   {"fail", DumpInputValue::Fail},
   {"always", DumpInputValue::Always},
   {"default", DumpInputValue::Default},
};

std::string dump_input_checker(const std::string &value)
{
   auto iter = sg_allowDumpOpts.find(value);
   if (iter == sg_allowDumpOpts.end()) {
      return std::string("dump input option value invalid");
   }
   return std::string();
}

DumpInputValue get_dump_input_type(const std::string &opt)
{
   auto iter = sg_allowDumpOpts.find(opt);
   if (iter == sg_allowDumpOpts.end()) {
      return DumpInputValue::Default;
   }
   return iter->second;
}

void dump_command_line(int argc, char **argv)
{
   polar::utils::error_stream() << "FileCheck command line: ";
   for (int i = 0; i < argc; i++) {
      polar::utils::error_stream() << " " << argv[i];
   }
   polar::utils::error_stream() << "\n";
}

MarkerStyle get_marker(FileCheckDiag::MatchType matchTy) {
   switch (matchTy) {
   case FileCheckDiag::MatchFoundAndExpected:
      return MarkerStyle('^', RawOutStream::Colors::GREEN);
   case FileCheckDiag::MatchFoundButExcluded:
      return MarkerStyle('!', RawOutStream::Colors::RED, "error: no match expected");
   case FileCheckDiag::MatchFoundButWrongLine:
      return MarkerStyle('!', RawOutStream::Colors::RED, "error: match on wrong line");
   case FileCheckDiag::MatchFoundButDiscarded:
      return MarkerStyle('!', RawOutStream::Colors::CYAN,
                         "discard: overlaps earlier match");
   case FileCheckDiag::MatchNoneAndExcluded:
      return MarkerStyle('X', RawOutStream::Colors::GREEN);
   case FileCheckDiag::MatchNoneButExpected:
      return MarkerStyle('X', RawOutStream::Colors::RED, "error: no match found");
   case FileCheckDiag::MatchFuzzy:
      return MarkerStyle('?', RawOutStream::Colors::MAGENTA, "poutStreamsible intended match");
   }
   polar::utils::unreachable_internal("unexpected match type");
}

void dump_input_annotation_help(RawOutStream &outStream)
{
   outStream << "The following description was requested by -dump-input=help to\n"
             << "explain the input annotations printed by -dump-input=always and\n"
             << "-dump-input=fail:\n\n";

   // Labels for input lines.
   outStream << "  - ";
   WithColor(outStream, RawOutStream::Colors::SAVEDCOLOR, true) << "L:";
   outStream << "     labels line number L of the input file\n";

   // Labels for annotation lines.
   outStream << "  - ";
   WithColor(outStream, RawOutStream::Colors::SAVEDCOLOR, true) << "T:L";
   outStream << "    labels the only match result for a pattern of type T from "
             << "line L of\n"
             << "           the check file\n";
   outStream << "  - ";
   WithColor(outStream, RawOutStream::Colors::SAVEDCOLOR, true) << "T:L'N";
   outStream << "  labels the Nth match result for a pattern of type T from line "
             << "L of\n"
             << "           the check file\n";

   // Markers on annotation lines.
   outStream << "  - ";
   WithColor(outStream, RawOutStream::Colors::SAVEDCOLOR, true) << "^~~";
   outStream << "    marks good match (reported if -v)\n"
             << "  - ";
   WithColor(outStream, RawOutStream::Colors::SAVEDCOLOR, true) << "!~~";
   outStream << "    marks bad match, such as:\n"
             << "           - CHECK-NEXT on same line as previous match (error)\n"
             << "           - CHECK-NOT found (error)\n"
             << "           - CHECK-DAG overlapping match (discarded, reported if "
             << "-vv)\n"
             << "  - ";
   WithColor(outStream, RawOutStream::Colors::SAVEDCOLOR, true) << "X~~";
   outStream << "    marks search range when no match is found, such as:\n"
             << "           - CHECK-NEXT not found (error)\n"
             << "           - CHECK-NOT not found (success, reported if -vv)\n"
             << "           - CHECK-DAG not found after discarded matches (error)\n"
             << "  - ";
   WithColor(outStream, RawOutStream::Colors::SAVEDCOLOR, true) << "?";
   outStream << "      marks fuzzy match when no match is found\n";

   // Colors.
   outStream << "  - colors ";
   WithColor(outStream, RawOutStream::Colors::GREEN, true) << "success";
   outStream << ", ";
   WithColor(outStream, RawOutStream::Colors::RED, true) << "error";
   outStream << ", ";
   WithColor(outStream, RawOutStream::Colors::MAGENTA, true) << "fuzzy match";
   outStream << ", ";
   WithColor(outStream, RawOutStream::Colors::CYAN, true, false) << "discarded match";
   outStream << ", ";
   WithColor(outStream, RawOutStream::Colors::CYAN, true, true) << "unmatched input";
   outStream << "\n\n"
             << "If you are not seeing color above or in input dumps, try: -color\n";
}

/// Get an abbreviation for the check type.
std::string get_check_type_abbreviation(FileCheckType type)
{
   switch (type) {
   case FileCheckKind::CheckPlain:
      if (type.getCount() > 1)
         return "count";
      return "check";
   case FileCheckKind::CheckNext:
      return "next";
   case FileCheckKind::CheckSame:
      return "same";
   case FileCheckKind::CheckNot:
      return "not";
   case FileCheckKind::CheckDAG:
      return "dag";
   case FileCheckKind::CheckLabel:
      return "label";
   case FileCheckKind::CheckEmpty:
      return "empty";
   case FileCheckKind::CheckEOF:
      return "eof";
   case FileCheckKind::CheckBadNot:
      return "bad-not";
   case FileCheckKind::CheckBadCount:
      return "bad-count";
   case FileCheckKind::CheckNone:
      polar_unreachable("invalid FileCheckType");
   }
   polar_unreachable("unknown FileCheckType");
}

void build_input_annotations(const std::vector<FileCheckDiag> &diags,
                             std::vector<InputAnnotation> &annotations,
                             unsigned &labelWidth)
{
   // How many diagnostics has the current check seen so far?
   unsigned checkDiagCount = 0;
   // What's the widest label?
   labelWidth = 0;
   for (auto diagItr = diags.begin(), diagEnd = diags.end(); diagItr != diagEnd;
        ++diagItr) {
      InputAnnotation A;

      // Build label, which uniquely identifies this check result.
      A.checkLine = diagItr->checkLine;
      RawStringOutStream label(A.label);
      label << get_check_type_abbreviation(diagItr->checkType) << ":"
            << diagItr->checkLine;
      A.checkDiagIndex = UINT_MAX;
      auto DiagNext = std::next(diagItr);
      if (DiagNext != diagEnd && diagItr->checkType == DiagNext->checkType &&
          diagItr->checkLine == DiagNext->checkLine)
         A.checkDiagIndex = checkDiagCount++;
      else if (checkDiagCount) {
         A.checkDiagIndex = checkDiagCount;
         checkDiagCount = 0;
      }
      if (A.checkDiagIndex != UINT_MAX)
         label << "'" << A.checkDiagIndex;
      else
         A.checkDiagIndex = 0;
      label.flush();
      labelWidth = std::max((std::string::size_type)labelWidth, A.label.size());

      MarkerStyle marker = get_marker(diagItr->matchType);
      A.marker = marker;
      A.foundAndExpectedMatch =
            diagItr->matchType == FileCheckDiag::MatchFoundAndExpected;

      // Compute the mark location, and break annotation into multiple
      // annotations if it spans multiple lines.
      A.inputLine = diagItr->inputStartLine;
      A.inputStartCol = diagItr->inputStartCol;
      if (diagItr->inputStartLine == diagItr->inputEndLine) {
         // Sometimes ranges are empty in order to indicate a specific point, but
         // that would mean nothing would be marked, so adjust the range to
         // include the following character.
         A.inputEndCol =
               std::max(diagItr->inputStartCol + 1, diagItr->inputEndCol);
         annotations.push_back(A);
      } else {
         assert(diagItr->inputStartLine < diagItr->inputEndLine &&
                "expected input range not to be inverted");
         A.inputEndCol = UINT_MAX;
         A.marker.note = "";
         annotations.push_back(A);
         for (unsigned L = diagItr->inputStartLine + 1, E = diagItr->inputEndLine;
              L <= E; ++L) {
            // If a range ends before the first column on a line, then it has no
            // characters on that line, so there's nothing to render.
            if (diagItr->inputEndCol == 1 && L == E) {
               annotations.back().marker.note = marker.note;
               break;
            }
            InputAnnotation B;
            B.checkLine = A.checkLine;
            B.checkDiagIndex = A.checkDiagIndex;
            B.label = A.label;
            B.inputLine = L;
            B.marker = marker;
            B.marker.lead = '~';
            B.inputStartCol = 1;
            if (L != E) {
               B.inputEndCol = UINT_MAX;
               B.marker.note = "";
            } else
               B.inputEndCol = diagItr->inputEndCol;
            B.foundAndExpectedMatch = A.foundAndExpectedMatch;
            annotations.push_back(B);
         }
      }
   }
}

void dump_annotated_input(RawOutStream &outStream, const FileCheckRequest &req,
                          StringRef inputFileText,
                          std::vector<InputAnnotation> &annotations,
                          unsigned labelWidth)
{
   outStream << "Full input was:\n<<<<<<\n";

   // Sort annotations.
   //
   // First, sort in the order of input lines to make it easier to find relevant
   // annotations while iterating input lines in the implementation below.
   // FileCheck diagnostics are not always reported and recorded in the order of
   // input lines due to, for example, CHECK-DAG and CHECK-NOT.
   //
   // Second, for annotations for the same input line, sort in the order of the
   // FileCheck directive's line in the check file (where there's at most one
   // directive per line) and then by the index of the match result for that
   // directive.  The rationale of this choice is that, for any input line, this
   // sort establishes a total order of annotations that, with respect to match
   // results, is consistent across multiple lines, thus making match results
   // easier to track from one line to the next when they span multiple lines.
   std::sort(annotations.begin(), annotations.end(),
             [](const InputAnnotation &A, const InputAnnotation &B) {
      if (A.inputLine != B.inputLine)
         return A.inputLine < B.inputLine;
      if (A.checkLine != B.checkLine)
         return A.checkLine < B.checkLine;
      // FIXME: Sometimes CHECK-LABEL reports its match twice with
      // other diagnostics in between, and then diag index incrementing
      // fails to work properly, and then this assert fails.  We should
      // suppress one of those diagnostics or do a better job of
      // computing this index.  For now, we just produce a redundant
      // CHECK-LABEL annotation.
      // assert(A.checkDiagIndex != B.checkDiagIndex &&
      //        "expected diagnostic indices to be unique within a "
      //        " check line");
      return A.checkDiagIndex < B.checkDiagIndex;
   });

   // Compute the width of the label column.
   const unsigned char *InputFilePtr = inputFileText.getBytesBegin(),
         *InputFileEnd = inputFileText.getBytesEnd();
   unsigned LineCount = inputFileText.count('\n');
   if (InputFileEnd[-1] != '\n')
      ++LineCount;
   unsigned LineNoWidth = log10(LineCount) + 1;
   // +3 below adds spaces (1) to the left of the (right-aligned) line numbers
   // on input lines and (2) to the right of the (left-aligned) labels on
   // annotation lines so that input lines and annotation lines are more
   // visually distinct.  For example, the spaces on the annotation lines ensure
   // that input line numbers and check directive line numbers never align
   // horizontally.  Those line numbers might not even be for the same file.
   // One space would be enough to achieve that, but more makes it even easier
   // to see.
   labelWidth = std::max(labelWidth, LineNoWidth) + 3;

   // Print annotated input lines.
   auto AnnotationItr = annotations.begin(), AnnotationEnd = annotations.end();
   for (unsigned Line = 1;
        InputFilePtr != InputFileEnd || AnnotationItr != AnnotationEnd;
        ++Line) {
      const unsigned char *InputFileLine = InputFilePtr;

      // Print right-aligned line number.
      WithColor(outStream, RawOutStream::Colors::BLACK, true)
            << polar::utils::format_decimal(Line, labelWidth) << ": ";

      // For the case where -v and colors are enabled, find the annotations for
      // good matches for expected patterns in order to highlight everything
      // else in the line.  There are no such annotations if -v is disabled.
      std::vector<InputAnnotation> FoundAndExpectedMatches;
      if (req.verbose && WithColor(outStream).colorsEnabled()) {
         for (auto I = AnnotationItr; I != AnnotationEnd && I->inputLine == Line;
              ++I) {
            if (I->foundAndExpectedMatch)
               FoundAndExpectedMatches.push_back(*I);
         }
      }

      // Print numbered line with highlighting where there are no matches for
      // expected patterns.
      bool Newline = false;
      {
         WithColor COS(outStream);
         bool InMatch = false;
         if (req.verbose)
            COS.changeColor(RawOutStream::Colors::CYAN, true, true);
         for (unsigned Col = 1; InputFilePtr != InputFileEnd && !Newline; ++Col) {
            bool WasInMatch = InMatch;
            InMatch = false;
            for (auto M : FoundAndExpectedMatches) {
               if (M.inputStartCol <= Col && Col < M.inputEndCol) {
                  InMatch = true;
                  break;
               }
            }
            if (!WasInMatch && InMatch)
               COS.resetColor();
            else if (WasInMatch && !InMatch)
               COS.changeColor(RawOutStream::Colors::CYAN, true, true);
            if (*InputFilePtr == '\n')
               Newline = true;
            else
               COS << *InputFilePtr;
            ++InputFilePtr;
         }
      }
      outStream << '\n';
      unsigned InputLineWidth = InputFilePtr - InputFileLine - Newline;

      // Print any annotations.
      while (AnnotationItr != AnnotationEnd &&
             AnnotationItr->inputLine == Line) {
         WithColor COS(outStream, AnnotationItr->marker.color, true);
         // The two spaces below are where the ": " appears on input lines.
         COS << polar::utils::left_justify(AnnotationItr->label, labelWidth) << "  ";
         unsigned Col;
         for (Col = 1; Col < AnnotationItr->inputStartCol; ++Col)
            COS << ' ';
         COS << AnnotationItr->marker.lead;
         // If inputEndCol=UINT_MAX, stop at InputLineWidth.
         for (++Col; Col < AnnotationItr->inputEndCol && Col <= InputLineWidth;
              ++Col)
            COS << '~';
         const std::string &note = AnnotationItr->marker.note;
         if (!note.empty()) {
            // Put the note at the end of the input line.  If we were to instead
            // put the note right after the marker, subsequent annotations for the
            // same input line might appear to mark this note instead of the input
            // line.
            for (; Col <= InputLineWidth; ++Col)
               COS << ' ';
            COS << ' ' << note;
         }
         COS << '\n';
         ++AnnotationItr;
      }
   }

   outStream << ">>>>>>\n";
}


} // filechecker
} // polar
