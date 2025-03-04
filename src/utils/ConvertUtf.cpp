// This source file is part of the polarphp.org open source project
//
// Copyright (c) 2017 - 2019 polarphp software foundation
// Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://polarphp.org/LICENSE.txt for license information
// See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
//
// Created by polarboy on 2018/06/05.
/*
 * Copyright 2001-2004 Unicode, Inc.
 *
 * Disclaimer
 *
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 *
 * Limitations on Rights to Redistribute This Code
 *
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/* ---------------------------------------------------------------------

    Conversions between UTF32, UTF-16, and UTF-8. Source code file.
    Author: Mark E. Davis, 1994.
    Rev History: Rick McGowan, fixes & updates May 2001.
    Sept 2001: fixed const & error conditions per
        mods suggested by S. Parent & A. Lillich.
    June 2002: Tim Dodd added detection and handling of incomplete
        source sequences, enhanced error detection, added casts
        to eliminate compiler warnings.
    July 2003: slight mods to back out aggressive FFFE detection.
    Jan 2004: updated switches in from-UTF8 conversions.
    Oct 2004: updated to use UNI_MAX_LEGAL_UTF32 in UTF-32 conversions.

    See the header file "ConvertUTF.h" for complete documentation.

------------------------------------------------------------------------ */

#include "polarphp/utils/ConvertUtf.h"
#include "polarphp/global/CompilerDetection.h"

#ifdef CVTUTF_DEBUG
#include <stdio.h>
#endif
#include <assert.h>

/*
 * This code extensively uses fall-through switches.
 * Keep the compiler from warning about that.
 */
#if defined(__clang__) && defined(__has_warning)
# if __has_warning("-Wimplicit-fallthrough")
#  define ConvertUTF_DISABLE_WARNINGS() \
   _Pragma("clang diagnostic push")  \
   _Pragma("clang diagnostic ignored \"-Wimplicit-fallthrough\"")
#  define ConvertUTF_RESTORE_WARNINGS \
   _Pragma("clang diagnostic pop")
# endif
#elif defined(__GNUC__) && __GNUC__ > 6
# define ConvertUTF_DISABLE_WARNINGS \
   _Pragma("GCC diagnostic push")    \
   _Pragma("GCC diagnostic ignored \"-Wimplicit-fallthrough\"")
# define ConvertUTF_RESTORE_WARNINGS \
   _Pragma("GCC diagnostic pop")
#endif
#ifndef ConvertUTF_DISABLE_WARNINGS
# define ConvertUTF_DISABLE_WARNINGS
#endif
#ifndef ConvertUTF_RESTORE_WARNINGS
# define ConvertUTF_RESTORE_WARNINGS
#endif

//ConvertUTF_DISABLE_WARNINGS

namespace polar {
namespace utils {

static const int sg_halfShift  = 10; /* used for shifting by 10 bits */

static const Utf32 sg_halfBase = 0x0010000UL;
static const Utf32 sg_halfMask = 0x3FFUL;

#define UNI_SUR_HIGH_START  static_cast<Utf32>(0xD800)
#define UNI_SUR_HIGH_END    static_cast<Utf32>(0xDBFF)
#define UNI_SUR_LOW_START   static_cast<Utf32>(0xDC00)
#define UNI_SUR_LOW_END     static_cast<Utf32>(0xDFFF)

/* --------------------------------------------------------------------- */

/*
 * Index into the table below with the first byte of a UTF-8 sequence to
 * get the number of trailing bytes that are supposed to follow it.
 * Note that *legal* UTF-8 values can't have 4 or 5-bytes. The table is
 * left as-is for anyone who may want to do such conversion, which was
 * allowed in earlier algorithms.
 */
static const char sg_trailingBytesForUTF8[256] = {
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
   2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

/*
 * Magic values subtracted from a buffer value during UTF8 conversion.
 * This table contains as many values as there might be trailing bytes
 * in a UTF-8 sequence.
 */
static const Utf32 sg_offsetsFromUTF8[6] = { 0x00000000UL, 0x00003080UL, 0x000E2080UL,
                                             0x03C82080UL, 0xFA082080UL, 0x82082080UL };

/*
 * Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
 * into the first byte, depending on how many bytes follow.  There are
 * as many entries in this table as there are UTF-8 sequence types.
 * (I.e., one byte sequence, two byte... etc.). Remember that sequencs
 * for *legal* UTF-8 will be 4 or fewer bytes total.
 */
static const Utf8 sg_firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

/* --------------------------------------------------------------------- */

/* The interface converts a whole buffer to avoid function-call overhead.
 * Constants have been gathered. Loops & conditionals have been removed as
 * much as possible for efficiency, in favor of drop-through switches.
 * (See "Note A" at the bottom of the file for equivalent code.)
 * If your compiler supports it, the "isLegalUTF8" call can be turned
 * into an inline function.
 */


/* --------------------------------------------------------------------- */

ConversionResult convert_utf32_to_utf16 (
      const Utf32 **sourceStart, const Utf32 *sourceEnd,
      Utf16 **targetStart, Utf16 *targetEnd, ConversionFlags flags)
{
   ConversionResult result = ConversionResult::ConversionOK;
   const Utf32 *source = *sourceStart;
   Utf16 *target = *targetStart;
   while (source < sourceEnd) {
      Utf32 ch;
      if (target >= targetEnd) {
         result = ConversionResult::TargetExhausted;
         break;
      }
      ch = *source++;
      if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
         /* UTF-16 surrogate values are illegal in UTF-32; 0xffff or 0xfffe are both reserved values */
         if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
            if (flags == ConversionFlags::StrictConversion) {
               --source; /* return to the illegal value itself */
               result = ConversionResult::SourceIllegal;
               break;
            } else {
               *target++ = UNI_REPLACEMENT_CHAR;
            }
         } else {
            *target++ = static_cast<Utf16>(ch); /* normal case */
         }
      } else if (ch > UNI_MAX_LEGAL_UTF32) {
         if (flags == ConversionFlags::StrictConversion) {
            result = ConversionResult::SourceIllegal;
         } else {
            *target++ = UNI_REPLACEMENT_CHAR;
         }
      } else {
         /* target is a character in range 0xFFFF - 0x10FFFF. */
         if (target + 1 >= targetEnd) {
            --source; /* Back up source pointer! */
            result = ConversionResult::TargetExhausted;
            break;
         }
         ch -= sg_halfBase;
         *target++ = static_cast<Utf16>((ch >> sg_halfShift) + UNI_SUR_HIGH_START);
         *target++ = static_cast<Utf16>((ch & sg_halfMask) + UNI_SUR_LOW_START);
      }
   }
   *sourceStart = source;
   *targetStart = target;
   return result;
}

/* --------------------------------------------------------------------- */

ConversionResult convert_utf16_to_utf32 (
      const Utf16 **sourceStart, const Utf16 *sourceEnd,
      Utf32 **targetStart, Utf32 *targetEnd, ConversionFlags flags)
{
   ConversionResult result = ConversionResult::ConversionOK;
   const Utf16 *source = *sourceStart;
   Utf32 *target = *targetStart;
   Utf32 ch, ch2;
   while (source < sourceEnd) {
      const Utf16 *oldSource = source; /*  In case we have to back up because of target overflow. */
      ch = *source++;
      /* If we have a surrogate pair, convert to UTF32 first. */
      if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
         /* If the 16 bits following the high surrogate are in the source buffer... */
         if (source < sourceEnd) {
            ch2 = *source;
            /* If it's a low surrogate, convert to UTF32. */
            if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
               ch = ((ch - UNI_SUR_HIGH_START) << sg_halfShift)
                     + (ch2 - UNI_SUR_LOW_START) + sg_halfBase;
               ++source;
            } else if (flags == ConversionFlags::StrictConversion) { /* it's an unpaired high surrogate */
               --source; /* return to the illegal value itself */
               result = ConversionResult::SourceIllegal;
               break;
            }
         } else { /* We don't have the 16 bits following the high surrogate. */
            --source; /* return to the high surrogate */
            result = ConversionResult::SourceExhausted;
            break;
         }
      } else if (flags == ConversionFlags::StrictConversion) {
         /* UTF-16 surrogate values are illegal in UTF-32 */
         if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
            --source; /* return to the illegal value itself */
            result = ConversionResult::SourceIllegal;
            break;
         }
      }
      if (target >= targetEnd) {
         source = oldSource; /* Back up source pointer! */
         result = ConversionResult::TargetExhausted; break;
      }
      *target++ = ch;
   }
   *sourceStart = source;
   *targetStart = target;
#ifdef CVTUTF_DEBUG
   if (result == ConversionResult::SourceIllegal) {
      fprintf(stderr, "ConvertUTF16toUTF32 illegal seq 0x%04x,%04x\n", ch, ch2);
      fflush(stderr);
   }
#endif
   return result;
}

ConversionResult convert_utf16_to_utf8 (
      const Utf16 **sourceStart, const Utf16 *sourceEnd,
      Utf8 **targetStart, Utf8 *targetEnd, ConversionFlags flags)
{
   ConversionResult result = ConversionResult::ConversionOK;
   const Utf16 *source = *sourceStart;
   Utf8 *target = *targetStart;
   while (source < sourceEnd) {
      Utf32 ch;
      unsigned short bytesToWrite = 0;
      const Utf32 byteMask = 0xBF;
      const Utf32 byteMark = 0x80;
      const Utf16 *oldSource = source; /* In case we have to back up because of target overflow. */
      ch = *source++;
      /* If we have a surrogate pair, convert to UTF32 first. */
      if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
         /* If the 16 bits following the high surrogate are in the source buffer... */
         if (source < sourceEnd) {
            Utf32 ch2 = *source;
            /* If it's a low surrogate, convert to UTF32. */
            if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
               ch = ((ch - UNI_SUR_HIGH_START) << sg_halfShift)
                     + (ch2 - UNI_SUR_LOW_START) + sg_halfBase;
               ++source;
            } else if (flags == ConversionFlags::StrictConversion) { /* it's an unpaired high surrogate */
               --source; /* return to the illegal value itself */
               result = ConversionResult::SourceIllegal;
               break;
            }
         } else { /* We don't have the 16 bits following the high surrogate. */
            --source; /* return to the high surrogate */
            result = ConversionResult::SourceExhausted;
            break;
         }
      } else if (flags == ConversionFlags::StrictConversion) {
         /* UTF-16 surrogate values are illegal in UTF-32 */
         if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
            --source; /* return to the illegal value itself */
            result = ConversionResult::SourceIllegal;
            break;
         }
      }
      /* Figure out how many bytes the result will require */
      if (ch < static_cast<Utf32>(0x80)) {
         bytesToWrite = 1;
      } else if (ch < static_cast<Utf32>(0x800)) {
         bytesToWrite = 2;
      } else if (ch < static_cast<Utf32>(0x10000)) {
         bytesToWrite = 3;
      } else if (ch < static_cast<Utf32>(0x110000)) {
         bytesToWrite = 4;
      } else {
         bytesToWrite = 3;
         ch = UNI_REPLACEMENT_CHAR;
      }

      target += bytesToWrite;
      if (target > targetEnd) {
         source = oldSource; /* Back up source pointer! */
         target -= bytesToWrite;
         result = ConversionResult::TargetExhausted;
         break;
      }
      switch (bytesToWrite) { /* note: everything falls through. */
      case 4:
      {
         *--target = static_cast<Utf8>((ch | byteMark) & byteMask);
         ch >>= 6;
         [[fallthrough]];
      }
      case 3:
      {
         *--target = static_cast<Utf8>((ch | byteMark) & byteMask);
         ch >>= 6;
         [[fallthrough]];
      }
      case 2:
      {
          *--target = static_cast<Utf8>((ch | byteMark) & byteMask);
         ch >>= 6;
         [[fallthrough]];
      }
      case 1:
      {
         *--target = static_cast<Utf8>(ch | sg_firstByteMark[bytesToWrite]);
      }
      }
      target += bytesToWrite;
   }
   *sourceStart = source;
   *targetStart = target;
   return result;
}

/* --------------------------------------------------------------------- */

ConversionResult convert_utf32_to_utf8 (
      const Utf32 **sourceStart, const Utf32 *sourceEnd,
      Utf8 **targetStart, Utf8 *targetEnd, ConversionFlags flags)
{
   ConversionResult result = ConversionResult::ConversionOK;
   const Utf32 *source = *sourceStart;
   Utf8 *target = *targetStart;
   while (source < sourceEnd) {
      Utf32 ch;
      unsigned short bytesToWrite = 0;
      const Utf32 byteMask = 0xBF;
      const Utf32 byteMark = 0x80;
      ch = *source++;
      if (flags == ConversionFlags::StrictConversion) {
         /* UTF-16 surrogate values are illegal in UTF-32 */
         if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
            --source; /* return to the illegal value itself */
            result = ConversionResult::SourceIllegal;
            break;
         }
      }
      /*
         * Figure out how many bytes the result will require. Turn any
         * illegally large UTF32 things (> Plane 17) into replacement chars.
         */
      if (ch < static_cast<Utf32>(0x80)) {
         bytesToWrite = 1;
      } else if (ch < static_cast<Utf32>(0x800)) {
         bytesToWrite = 2;
      } else if (ch < static_cast<Utf32>(0x10000)) {
         bytesToWrite = 3;
      } else if (ch <= UNI_MAX_LEGAL_UTF32) {
         bytesToWrite = 4;
      } else {
         bytesToWrite = 3;
         ch = UNI_REPLACEMENT_CHAR;
         result = ConversionResult::SourceIllegal;
      }

      target += bytesToWrite;
      if (target > targetEnd) {
         --source; /* Back up source pointer! */
         target -= bytesToWrite;
         result = ConversionResult::TargetExhausted; break;
      }
      switch (bytesToWrite) { /* note: everything falls through. */
      case 4: *--target = static_cast<Utf8>((ch | byteMark) & byteMask); ch >>= 6;POLAR_FALLTHROUGH;
      case 3: *--target = static_cast<Utf8>((ch | byteMark) & byteMask); ch >>= 6;POLAR_FALLTHROUGH;
      case 2: *--target = static_cast<Utf8>((ch | byteMark) & byteMask); ch >>= 6;POLAR_FALLTHROUGH;
      case 1: *--target = static_cast<Utf8>(ch | sg_firstByteMark[bytesToWrite]);
      }
      target += bytesToWrite;
   }
   *sourceStart = source;
   *targetStart = target;
   return result;
}

/* --------------------------------------------------------------------- */

/*
 * Utility routine to tell whether a sequence of bytes is legal UTF-8.
 * This must be called with the length pre-determined by the first byte.
 * If not calling this from ConvertUTF8to*, then the length can be set by:
 *  length = trailingBytesForUTF8[*source]+1;
 * and the sequence is illegal right away if there aren't that many bytes
 * available.
 * If presented with a length > 4, this returns false.  The Unicode
 * definition of UTF-8 goes up to 4-byte sequences.
 */

static Boolean is_legal_utf8(const Utf8 *source, int length)
{
   Utf8 a;
   const Utf8 *srcptr = source + length;
   switch (length) {
   default: return false;
      /* Everything else falls through when "true"... */
   case 4: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;POLAR_FALLTHROUGH;
   case 3: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;POLAR_FALLTHROUGH;
   case 2: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
      switch (*source) {
      /* no fall-through in this inner switch */
      case 0xE0: if (a < 0xA0) return false; break;
      case 0xED: if (a > 0x9F) return false; break;
      case 0xF0: if (a < 0x90) return false; break;
      case 0xF4: if (a > 0x8F) return false; break;
      default:   if (a < 0x80) return false;
      }
   POLAR_FALLTHROUGH;
   case 1: if (*source >= 0x80 && *source < 0xC2) return false;
   }
   if (*source > 0xF4) {
      return false;
   }
   return true;
}

/* --------------------------------------------------------------------- */

/*
 * Exported function to return whether a UTF-8 sequence is legal or not.
 * This is not used here; it's just exported.
 */
Boolean is_legal_utf8_sequence(const Utf8 *source, const Utf8 *sourceEnd)
{
   int length = sg_trailingBytesForUTF8[*source] + 1;
   if (length > sourceEnd - source) {
      return false;
   }
   return is_legal_utf8(source, length);
}

/* --------------------------------------------------------------------- */

static unsigned
find_maximal_subpart_of_illformed_utf8_sequence(const Utf8 *source,
                                                const Utf8 *sourceEnd)
{
   Utf8 b1, b2, b3;

   assert(!is_legal_utf8_sequence(source, sourceEnd));

   /*
   * Unicode 6.3.0, D93b:
   *
   *   Maximal subpart of an ill-formed subsequence: The longest code unit
   *   subsequence starting at an unconvertible offset that is either:
   *   a. the initial subsequence of a well-formed code unit sequence, or
   *   b. a subsequence of length one.
   */
   if (source == sourceEnd) {
      return 0;
   }
   /*
   * Perform case analysis.  See Unicode 6.3.0, Table 3-7. Well-Formed UTF-8
   * Byte Sequences.
   */

   b1 = *source;
   ++source;
   if (b1 >= 0xC2 && b1 <= 0xDF) {
      /*
     * First byte is valid, but we know that this code unit sequence is
     * invalid, so the maximal subpart has to end after the first byte.
     */
      return 1;
   }

   if (source == sourceEnd) {
      return 1;
   }
   b2 = *source;
   ++source;
   if (b1 == 0xE0) {
      return (b2 >= 0xA0 && b2 <= 0xBF) ? 2 : 1;
   }
   if (b1 >= 0xE1 && b1 <= 0xEC) {
      return (b2 >= 0x80 && b2 <= 0xBF) ? 2 : 1;
   }
   if (b1 == 0xED) {
      return (b2 >= 0x80 && b2 <= 0x9F) ? 2 : 1;
   }
   if (b1 >= 0xEE && b1 <= 0xEF) {
      return (b2 >= 0x80 && b2 <= 0xBF) ? 2 : 1;
   }
   if (b1 == 0xF0) {
      if (b2 >= 0x90 && b2 <= 0xBF) {
         if (source == sourceEnd) {
            return 2;
         }
         b3 = *source;
         return (b3 >= 0x80 && b3 <= 0xBF) ? 3 : 2;
      }
      return 1;
   }
   if (b1 >= 0xF1 && b1 <= 0xF3) {
      if (b2 >= 0x80 && b2 <= 0xBF) {
         if (source == sourceEnd) {
            return 2;
         }
         b3 = *source;
         return (b3 >= 0x80 && b3 <= 0xBF) ? 3 : 2;
      }
      return 1;
   }
   if (b1 == 0xF4) {
      if (b2 >= 0x80 && b2 <= 0x8F) {
         if (source == sourceEnd) {
            return 2;
         }
         b3 = *source;
         return (b3 >= 0x80 && b3 <= 0xBF) ? 3 : 2;
      }
      return 1;
   }

   assert((b1 >= 0x80 && b1 <= 0xC1) || b1 >= 0xF5);
   /*
   * There are no valid sequences that start with these bytes.  Maximal subpart
   * is defined to have length 1 in these cases.
   */
   return 1;
}

/* --------------------------------------------------------------------- */

/*
 * Exported function to return the total number of bytes in a codepoint
 * represented in UTF-8, given the value of the first byte.
 */
unsigned get_num_bytes_for_utf8(Utf8 first)
{
   return static_cast<unsigned>(sg_trailingBytesForUTF8[first] + 1);
}

/* --------------------------------------------------------------------- */

/*
 * Exported function to return whether a UTF-8 string is legal or not.
 * This is not used here; it's just exported.
 */
Boolean is_legal_utf8_string(const Utf8 **source, const Utf8 *sourceEnd)
{
   while (*source != sourceEnd) {
      int length = sg_trailingBytesForUTF8[**source] + 1;
      if (length > sourceEnd - *source || !is_legal_utf8(*source, length)) {
         return false;
      }
      *source += length;
   }
   return true;
}

/* --------------------------------------------------------------------- */

ConversionResult convert_utf8_to_utf16 (
      const Utf8 **sourceStart, const Utf8 *sourceEnd,
      Utf16 **targetStart, Utf16 *targetEnd, ConversionFlags flags)
{
   ConversionResult result = ConversionResult::ConversionOK;
   const Utf8 *source = *sourceStart;
   Utf16 *target = *targetStart;
   while (source < sourceEnd) {
      Utf32 ch = 0;
      unsigned short extraBytesToRead = static_cast<unsigned short>(sg_trailingBytesForUTF8[*source]);
      if (extraBytesToRead >= sourceEnd - source) {
         result = ConversionResult::SourceExhausted; break;
      }
      /* Do this check whether lenient or strict */
      if (!is_legal_utf8(source, extraBytesToRead + 1)) {
         result = ConversionResult::SourceIllegal;
         break;
      }
      /*
         * The cases all fall through. See "Note A" below.
         */
      switch (extraBytesToRead) {
      case 5: ch += *source++; ch <<= 6;POLAR_FALLTHROUGH; /* remember, illegal UTF-8 */
      case 4: ch += *source++; ch <<= 6;POLAR_FALLTHROUGH; /* remember, illegal UTF-8 */
      case 3: ch += *source++; ch <<= 6;POLAR_FALLTHROUGH;
      case 2: ch += *source++; ch <<= 6;POLAR_FALLTHROUGH;
      case 1: ch += *source++; ch <<= 6;POLAR_FALLTHROUGH;
      case 0: ch += *source++;
      }
      ch -= sg_offsetsFromUTF8[extraBytesToRead];

      if (target >= targetEnd) {
         source -= (extraBytesToRead+1); /* Back up source pointer! */
         result = ConversionResult::TargetExhausted;
         break;
      }
      if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
         /* UTF-16 surrogate values are illegal in UTF-32 */
         if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
            if (flags == ConversionFlags::StrictConversion) {
               source -= (extraBytesToRead+1); /* return to the illegal value itself */
               result = ConversionResult::SourceIllegal;
               break;
            } else {
               *target++ = UNI_REPLACEMENT_CHAR;
            }
         } else {
            *target++ = static_cast<Utf16>(ch); /* normal case */
         }
      } else if (ch > UNI_MAX_UTF16) {
         if (flags == ConversionFlags::StrictConversion) {
            result = ConversionResult::SourceIllegal;
            source -= (extraBytesToRead+1); /* return to the start */
            break; /* Bail out; shouldn't continue */
         } else {
            *target++ = UNI_REPLACEMENT_CHAR;
         }
      } else {
         /* target is a character in range 0xFFFF - 0x10FFFF. */
         if (target + 1 >= targetEnd) {
            source -= (extraBytesToRead + 1); /* Back up source pointer! */
            result = ConversionResult::TargetExhausted; break;
         }
         ch -= sg_halfBase;
         *target++ = static_cast<Utf16>((ch >> sg_halfShift) + UNI_SUR_HIGH_START);
         *target++ = static_cast<Utf16>((ch & sg_halfMask) + UNI_SUR_LOW_START);
      }
   }
   *sourceStart = source;
   *targetStart = target;
   return result;
}

/* --------------------------------------------------------------------- */

static ConversionResult convert_utf8_to_utf32_impl(
      const Utf8 **sourceStart, const Utf8 *sourceEnd,
      Utf32 **targetStart, Utf32 *targetEnd, ConversionFlags flags,
      Boolean InputIsPartial)
{
   ConversionResult result = ConversionResult::ConversionOK;
   const Utf8 *source = *sourceStart;
   Utf32 *target = *targetStart;
   while (source < sourceEnd) {
      Utf32 ch = 0;
      unsigned short extraBytesToRead = static_cast<unsigned short>(sg_trailingBytesForUTF8[*source]);
      if (extraBytesToRead >= sourceEnd - source) {
         if (flags == ConversionFlags::StrictConversion || InputIsPartial) {
            result = ConversionResult::SourceExhausted;
            break;
         } else {
            result = ConversionResult::SourceIllegal;

            /*
                 * Replace the maximal subpart of ill-formed sequence with
                 * replacement character.
                 */
            source += find_maximal_subpart_of_illformed_utf8_sequence(source,
                                                                      sourceEnd);
            *target++ = UNI_REPLACEMENT_CHAR;
            continue;
         }
      }
      if (target >= targetEnd) {
         result = ConversionResult::TargetExhausted; break;
      }

      /* Do this check whether lenient or strict */
      if (!is_legal_utf8(source, extraBytesToRead+1)) {
         result = ConversionResult::SourceIllegal;
         if (flags == ConversionFlags::StrictConversion) {
            /* Abort conversion. */
            break;
         } else {
            /*
                 * Replace the maximal subpart of ill-formed sequence with
                 * replacement character.
                 */
            source += find_maximal_subpart_of_illformed_utf8_sequence(source,
                                                                      sourceEnd);
            *target++ = UNI_REPLACEMENT_CHAR;
            continue;
         }
      }
      /*
         * The cases all fall through. See "Note A" below.
         */
      switch (extraBytesToRead) {
      case 5: ch += *source++; ch <<= 6;POLAR_FALLTHROUGH;
      case 4: ch += *source++; ch <<= 6;POLAR_FALLTHROUGH;
      case 3: ch += *source++; ch <<= 6;POLAR_FALLTHROUGH;
      case 2: ch += *source++; ch <<= 6;POLAR_FALLTHROUGH;
      case 1: ch += *source++; ch <<= 6;POLAR_FALLTHROUGH;
      case 0: ch += *source++;
      }
      ch -= sg_offsetsFromUTF8[extraBytesToRead];

      if (ch <= UNI_MAX_LEGAL_UTF32) {
         /*
             * UTF-16 surrogate values are illegal in UTF-32, and anything
             * over Plane 17 (> 0x10FFFF) is illegal.
             */
         if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
            if (flags == ConversionFlags::StrictConversion) {
               source -= (extraBytesToRead + 1); /* return to the illegal value itself */
               result = ConversionResult::SourceIllegal;
               break;
            } else {
               *target++ = UNI_REPLACEMENT_CHAR;
            }
         } else {
            *target++ = ch;
         }
      } else { /* i.e., ch > UNI_MAX_LEGAL_UTF32 */
         result = ConversionResult::SourceIllegal;
         *target++ = UNI_REPLACEMENT_CHAR;
      }
   }
   *sourceStart = source;
   *targetStart = target;
   return result;
}

ConversionResult convert_utf8_to_utf32_partial(const Utf8 **sourceStart,
                                               const Utf8 *sourceEnd,
                                               Utf32 **targetStart,
                                               Utf32 *targetEnd,
                                               ConversionFlags flags)
{
   return convert_utf8_to_utf32_impl(sourceStart, sourceEnd, targetStart, targetEnd,
                                     flags, /*InputIsPartial=*/true);
}

ConversionResult convert_utf8_to_utf32(const Utf8 **sourceStart,
                                       const Utf8 *sourceEnd, Utf32 **targetStart,
                                       Utf32 *targetEnd, ConversionFlags flags)
{
   return convert_utf8_to_utf32_impl(sourceStart, sourceEnd, targetStart, targetEnd,
                                     flags, /*InputIsPartial=*/false);
}

} // utils
} // polar

/* ---------------------------------------------------------------------

    Note A.
    The fall-through switches in UTF-8 reading code save a
    temp variable, some decrements & conditionals.  The switches
    are equivalent to the following loop:
        {
            int tmpBytesToRead = extraBytesToRead+1;
            do {
                ch += *source++;
                --tmpBytesToRead;
                if (tmpBytesToRead) ch <<= 6;
            } while (tmpBytesToRead > 0);
        }
    In UTF-8 writing code, the switches on "bytesToWrite" are
    similarly unrolled loops.

   --------------------------------------------------------------------- */

//ConvertUTF_RESTORE_WARNINGS
