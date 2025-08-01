/*
 WString.cpp - String library for Wiring & Arduino
 ...mostly rewritten by Paul Stoffregen...
 Copyright (c) 2009-10 Hernando Barragan.  All rights reserved.
 Copyright 2011, Paul Stoffregen, paul@pjrc.com
 Modified by Ivan Grokhotkov, 2014 - esp8266 support
 Modified by Michael C. Miller, 2015 - esp8266 progmem support

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Arduino.h"
#include "WString.h"
#include "stdlib_noniso.h"
#include "esp32-hal-log.h"

/*********************************************/
/*  Constructors                             */
/*********************************************/

String::String(const char *cstr) {
  init();
  if (cstr) {
    copy(cstr, strlen(cstr));
  }
}

String::String(const char *cstr, unsigned int length) {
  init();
  if (cstr) {
    copy(cstr, length);
  }
}

String::String(const String &value) {
  init();
  *this = value;
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
String::String(String &&rval) {
  init();
  move(rval);
}

String::String(StringSumHelper &&rval) {
  init();
  move(rval);
}
#endif

String::String(char c) {
  init();
  char buf[] = {c, '\0'};
  *this = buf;
}

String::String(unsigned char value, unsigned char base) {
  init();
  char buf[1 + 8 * sizeof(unsigned char)];
  utoa(value, buf, base);
  *this = buf;
}

String::String(int value, unsigned char base) {
  init();
  char buf[2 + 8 * sizeof(int)];
  itoa(value, buf, base);
  *this = buf;
}

String::String(unsigned int value, unsigned char base) {
  init();
  char buf[1 + 8 * sizeof(unsigned int)];
  utoa(value, buf, base);
  *this = buf;
}

String::String(long value, unsigned char base) {
  init();
  char buf[2 + 8 * sizeof(long)];
  ltoa(value, buf, base);
  *this = buf;
}

String::String(unsigned long value, unsigned char base) {
  init();
  char buf[1 + 8 * sizeof(unsigned long)];
  ultoa(value, buf, base);
  *this = buf;
}

String::String(float value, unsigned int decimalPlaces) {
  init();
  char *buf = (char *)malloc(decimalPlaces + 42);
  if (buf) {
    *this = dtostrf(value, (decimalPlaces + 2), decimalPlaces, buf);
    free(buf);
  } else {
    *this = "nan";
    log_e("No enough memory for the operation.");
  }
}

String::String(double value, unsigned int decimalPlaces) {
  init();
  char *buf = (char *)malloc(decimalPlaces + 312);
  if (buf) {
    *this = dtostrf(value, (decimalPlaces + 2), decimalPlaces, buf);
    free(buf);
  } else {
    *this = "nan";
    log_e("No enough memory for the operation.");
  }
}

String::String(long long value, unsigned char base) {
  init();
  char buf[2 + 8 * sizeof(long long)];
  lltoa(value, buf, base);
  *this = buf;
}

String::String(unsigned long long value, unsigned char base) {
  init();
  char buf[1 + 8 * sizeof(unsigned long long)];
  ulltoa(value, buf, base);
  *this = buf;
}

String::~String() {
  invalidate();
}

/*********************************************/
/*  Memory Management                        */
/*********************************************/

inline void String::init(void) {
  setSSO(false);
  setBuffer(nullptr);
  setCapacity(0);
  setLen(0);
}

void String::invalidate(void) {
  if (!isSSO() && wbuffer()) {
    free(wbuffer());
  }
  init();
}

bool String::reserve(unsigned int size) {
  if (buffer() && capacity() >= size) {
    return true;
  }
  if (changeBuffer(size)) {
    if (len() == 0) {
      wbuffer()[0] = 0;
    }
    return true;
  }
  return false;
}

bool String::changeBuffer(unsigned int maxStrLen) {
  // Can we use SSO here to avoid allocation?
  if (maxStrLen < sizeof(sso.buff) - 1) {
    if (isSSO() || !buffer()) {
      // Already using SSO, nothing to do
      size_t oldLen = len();
      setSSO(true);
      setLen(oldLen);
    } else {  // if bufptr && !isSSO()
      // Using bufptr, need to shrink into sso.buff
      char temp[sizeof(sso.buff)];
      memcpy(temp, buffer(), maxStrLen);
      free(wbuffer());
      size_t oldLen = len();
      setSSO(true);
      memcpy(wbuffer(), temp, maxStrLen);
      setLen(oldLen);
    }
    return true;
  }
  // Fallthrough to normal allocator
  size_t newSize = (maxStrLen + 16) & (~0xf);
  // Make sure we can fit newsize in the buffer
  if (newSize > CAPACITY_MAX) {
    return false;
  }
  size_t oldLen = len();
  char *newbuffer = (char *)realloc(isSSO() ? nullptr : wbuffer(), newSize);
  if (newbuffer) {
    size_t oldSize = capacity() + 1;  // include NULL.
    if (isSSO()) {
      // Copy the SSO buffer into allocated space
      memmove(newbuffer, sso.buff, sizeof(sso.buff));
    }
    if (newSize > oldSize) {
      memset(newbuffer + oldSize, 0, newSize - oldSize);
    }
    setSSO(false);
    setCapacity(newSize - 1);
    setBuffer(newbuffer);
    setLen(oldLen);  // Needed in case of SSO where len() never existed
    return true;
  }
  return false;
}

/*********************************************/
/*  Copy and Move                            */
/*********************************************/

String &String::copy(const char *cstr, unsigned int length) {
  if (cstr == nullptr || !reserve(length)) {
    invalidate();
    return *this;
  }
  memmove(wbuffer(), cstr, length);
  setLen(length);
  return *this;
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
void String::move(String &rhs) {
  if (buffer()) {
    if (capacity() >= rhs.len()) {
      // Use case: When 'reserve()' was called and the first
      // assignment/append is the return value of a function.
      if (rhs.len() && rhs.buffer()) {
        memmove(wbuffer(), rhs.buffer(), rhs.length());
      }
      setLen(rhs.len());
      rhs.invalidate();
      return;
    }
    if (!isSSO()) {
      free(wbuffer());
      setBuffer(nullptr);
    }
  }
  if (rhs.isSSO()) {
    setSSO(true);
    memmove(sso.buff, rhs.sso.buff, sizeof(sso.buff));
  } else {
    setSSO(false);
    setBuffer(rhs.wbuffer());
  }
  setCapacity(rhs.capacity());
  setLen(rhs.len());
  rhs.init();
}
#endif

String &String::operator=(const String &rhs) {
  if (this == &rhs) {
    return *this;
  }
  return copy(rhs.buffer(), rhs.len());
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
String &String::operator=(String &&rval) {
  if (this != &rval) {
    move(rval);
  }
  return *this;
}

String &String::operator=(StringSumHelper &&rval) {
  if (this != &rval) {
    move(rval);
  }
  return *this;
}
#endif

String &String::operator=(const char *cstr) {
  const uint32_t length = cstr ? strlen(cstr) : 0u;
  return copy(cstr, length);
}

/*********************************************/
/*  concat                                   */
/*********************************************/

bool String::concat(const String &s) {
  // Special case if we're concatting ourself (s += s;) since we may end up
  // realloc'ing the buffer and moving s.buffer in the method called
  if (&s == this) {
    if (s.len() == 0) {
      return true;
    }
    if (!s.buffer()) {
      return false;
    }
    unsigned int newlen = 2 * len();
    if (!reserve(newlen)) {
      return false;
    }
    memmove(wbuffer() + len(), buffer(), len());
    setLen(newlen);
    return true;
  }
  return concat(s.buffer(), s.len());
}

bool String::concat(const char *cstr, unsigned int length) {
  unsigned int newlen = len() + length;
  if (!cstr) {
    return false;
  }
  if (length == 0) {
    return true;
  }
  if (!reserve(newlen)) {
    return false;
  }
  if (cstr >= wbuffer() && cstr < wbuffer() + len()) {
    // compatible with SSO in ram #6155 (case "x += x.c_str()")
    memmove(wbuffer() + len(), cstr, length);
  } else {
    // compatible with source in flash #6367
    memcpy_P(wbuffer() + len(), cstr, length);
  }
  setLen(newlen);
  return true;
}

bool String::concat(const char *cstr) {
  if (!cstr) {
    return false;
  }
  return concat(cstr, strlen(cstr));
}

bool String::concat(char c) {
  char buf[] = {c, '\0'};
  return concat(buf, 1);
}

bool String::concat(unsigned char num) {
  char buf[1 + 3 * sizeof(unsigned char)];
  utoa(num, buf, 10);
  return concat(buf, strlen(buf));
}

bool String::concat(int num) {
  char buf[2 + 3 * sizeof(int)];
  itoa(num, buf, 10);
  return concat(buf, strlen(buf));
}

bool String::concat(unsigned int num) {
  char buf[1 + 3 * sizeof(unsigned int)];
  utoa(num, buf, 10);
  return concat(buf, strlen(buf));
}

bool String::concat(long num) {
  char buf[2 + 3 * sizeof(long)];
  ltoa(num, buf, 10);
  return concat(buf, strlen(buf));
}

bool String::concat(unsigned long num) {
  char buf[1 + 3 * sizeof(unsigned long)];
  ultoa(num, buf, 10);
  return concat(buf, strlen(buf));
}

bool String::concat(long long num) {
  char buf[2 + 3 * sizeof(long long)];
  lltoa(num, buf, 10);
  return concat(buf, strlen(buf));
}

bool String::concat(unsigned long long num) {
  char buf[1 + 3 * sizeof(unsigned long long)];
  ulltoa(num, buf, 10);
  return concat(buf, strlen(buf));
}

bool String::concat(float num) {
  char buf[20];
  char *string = dtostrf(num, 4, 2, buf);
  return concat(string, strlen(string));
}

bool String::concat(double num) {
  char buf[20];
  char *string = dtostrf(num, 4, 2, buf);
  return concat(string, strlen(string));
}

/*********************************************/
/*  Concatenate                              */
/*********************************************/

StringSumHelper &operator+(const StringSumHelper &lhs, const String &rhs) {
  StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
  if (!a.concat(rhs.buffer(), rhs.len())) {
    a.invalidate();
  }
  return a;
}

StringSumHelper &operator+(const StringSumHelper &lhs, const char *cstr) {
  StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
  if (!cstr || !a.concat(cstr, strlen(cstr))) {
    a.invalidate();
  }
  return a;
}

StringSumHelper &operator+(const StringSumHelper &lhs, char c) {
  StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
  if (!a.concat(c)) {
    a.invalidate();
  }
  return a;
}

StringSumHelper &operator+(const StringSumHelper &lhs, unsigned char num) {
  StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
  if (!a.concat(num)) {
    a.invalidate();
  }
  return a;
}

StringSumHelper &operator+(const StringSumHelper &lhs, int num) {
  StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
  if (!a.concat(num)) {
    a.invalidate();
  }
  return a;
}

StringSumHelper &operator+(const StringSumHelper &lhs, unsigned int num) {
  StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
  if (!a.concat(num)) {
    a.invalidate();
  }
  return a;
}

StringSumHelper &operator+(const StringSumHelper &lhs, long num) {
  StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
  if (!a.concat(num)) {
    a.invalidate();
  }
  return a;
}

StringSumHelper &operator+(const StringSumHelper &lhs, unsigned long num) {
  StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
  if (!a.concat(num)) {
    a.invalidate();
  }
  return a;
}

StringSumHelper &operator+(const StringSumHelper &lhs, float num) {
  StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
  if (!a.concat(num)) {
    a.invalidate();
  }
  return a;
}

StringSumHelper &operator+(const StringSumHelper &lhs, double num) {
  StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
  if (!a.concat(num)) {
    a.invalidate();
  }
  return a;
}

StringSumHelper &operator+(const StringSumHelper &lhs, long long num) {
  StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
  if (!a.concat(num)) {
    a.invalidate();
  }
  return a;
}

StringSumHelper &operator+(const StringSumHelper &lhs, unsigned long long num) {
  StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
  if (!a.concat(num)) {
    a.invalidate();
  }
  return a;
}

/*********************************************/
/*  Comparison                               */
/*********************************************/

int String::compareTo(const String &s) const {
  if (!buffer() || !s.buffer()) {
    if (s.buffer() && s.len() > 0) {
      return 0 - *(unsigned char *)s.buffer();
    }
    if (buffer() && len() > 0) {
      return *(unsigned char *)buffer();
    }
    return 0;
  }
  return strcmp(buffer(), s.buffer());
}

bool String::equals(const String &s2) const {
  return (len() == s2.len() && compareTo(s2) == 0);
}

bool String::equals(const char *cstr) const {
  if (len() == 0) {
    return (cstr == NULL || *cstr == 0);
  }
  if (cstr == NULL) {
    return buffer()[0] == 0;
  }
  return strcmp(buffer(), cstr) == 0;
}

bool String::operator<(const String &rhs) const {
  return compareTo(rhs) < 0;
}

bool String::operator>(const String &rhs) const {
  return compareTo(rhs) > 0;
}

bool String::operator<=(const String &rhs) const {
  return compareTo(rhs) <= 0;
}

bool String::operator>=(const String &rhs) const {
  return compareTo(rhs) >= 0;
}

bool String::equalsIgnoreCase(const String &s2) const {
  if (this == &s2) {
    return true;
  }
  if (len() != s2.len()) {
    return false;
  }
  if (len() == 0) {
    return true;
  }
  const char *p1 = buffer();
  const char *p2 = s2.buffer();
  while (*p1) {
    if (tolower(*p1++) != tolower(*p2++)) {
      return false;
    }
  }
  return true;
}

unsigned char String::equalsConstantTime(const String &s2) const {
  // To avoid possible time-based attacks present function
  // compares given strings in a constant time.
  if (len() != s2.len()) {
    return 0;
  }
  //at this point lengths are the same
  if (len() == 0) {
    return 1;
  }
  //at this point lengths are the same and non-zero
  const char *p1 = buffer();
  const char *p2 = s2.buffer();
  unsigned int equalchars = 0;
  unsigned int diffchars = 0;
  while (*p1) {
    if (*p1 == *p2) {
      ++equalchars;
    } else {
      ++diffchars;
    }
    ++p1;
    ++p2;
  }
  //the following should force a constant time eval of the condition without a compiler "logical shortcut"
  unsigned char equalcond = (equalchars == len());
  unsigned char diffcond = (diffchars == 0);
  return (equalcond & diffcond);  //bitwise AND
}

bool String::startsWith(const String &s2) const {
  if (len() < s2.len()) {
    return false;
  }
  return startsWith(s2, 0);
}

bool String::startsWith(const String &s2, unsigned int offset) const {
  if (offset > (unsigned)(len() - s2.len()) || !buffer() || !s2.buffer()) {
    return false;
  }
  return strncmp(&buffer()[offset], s2.buffer(), s2.len()) == 0;
}

bool String::endsWith(const String &s2) const {
  if (len() < s2.len() || !buffer() || !s2.buffer()) {
    return false;
  }
  return strcmp(&buffer()[len() - s2.len()], s2.buffer()) == 0;
}

/*********************************************/
/*  Character Access                         */
/*********************************************/

char String::charAt(unsigned int loc) const {
  return operator[](loc);
}

void String::setCharAt(unsigned int loc, char c) {
  if (loc < len()) {
    wbuffer()[loc] = c;
  }
}

char &String::operator[](unsigned int index) {
  static char dummy_writable_char;
  if (index >= len() || !buffer()) {
    dummy_writable_char = 0;
    return dummy_writable_char;
  }
  return wbuffer()[index];
}

char String::operator[](unsigned int index) const {
  if (index >= len() || !buffer()) {
    return 0;
  }
  return buffer()[index];
}

void String::getBytes(unsigned char *buf, unsigned int bufsize, unsigned int index) const {
  if (!bufsize || !buf) {
    return;
  }
  if (index >= len()) {
    buf[0] = 0;
    return;
  }
  unsigned int n = bufsize - 1;
  if (n > len() - index) {
    n = len() - index;
  }
  strncpy((char *)buf, buffer() + index, n);
  buf[n] = 0;
}

/*********************************************/
/*  Search                                   */
/*********************************************/

int String::indexOf(char c) const {
  return indexOf(c, 0);
}

int String::indexOf(char ch, unsigned int fromIndex) const {
  if (fromIndex >= len()) {
    return -1;
  }
  const char *temp = strchr(buffer() + fromIndex, ch);
  if (temp == NULL) {
    return -1;
  }
  return temp - buffer();
}

int String::indexOf(const String &s2) const {
  return indexOf(s2, 0);
}

int String::indexOf(const String &s2, unsigned int fromIndex) const {
  if (fromIndex >= len()) {
    return -1;
  }
  const char *found = strstr(buffer() + fromIndex, s2.buffer());
  if (found == NULL) {
    return -1;
  }
  return found - buffer();
}

int String::lastIndexOf(char theChar) const {
  return lastIndexOf(theChar, len() - 1);
}

int String::lastIndexOf(char ch, unsigned int fromIndex) const {
  if (fromIndex >= len()) {
    return -1;
  }
  char tempchar = buffer()[fromIndex + 1];
  wbuffer()[fromIndex + 1] = '\0';
  char *temp = strrchr(wbuffer(), ch);
  wbuffer()[fromIndex + 1] = tempchar;
  if (temp == NULL) {
    return -1;
  }
  const int rv = temp - buffer();
  if (rv >= len()) {
    return -1;
  }
  return rv;
}

int String::lastIndexOf(const String &s2) const {
  return lastIndexOf(s2, len() - s2.len());
}

int String::lastIndexOf(const String &s2, unsigned int fromIndex) const {
  if (s2.len() == 0 || len() == 0 || s2.len() > len()) {
    return -1;
  }
  if (fromIndex >= len()) {
    fromIndex = len() - 1;
  }
  int found = -1;
  for (char *p = wbuffer(); p <= wbuffer() + fromIndex; p++) {
    p = strstr(p, s2.buffer());
    if (!p) {
      break;
    }
    if ((unsigned int)(p - wbuffer()) <= fromIndex) {
      found = p - buffer();
    }
  }
  return found;
}

String String::substring(unsigned int left, unsigned int right) const {
  if (left > right) {
    unsigned int temp = right;
    right = left;
    left = temp;
  }
  String out;
  if (left >= len()) {
    return out;
  }
  if (right > len()) {
    right = len();
  }
  out.copy(buffer() + left, right - left);
  return out;
}

/*********************************************/
/*  Modification                             */
/*********************************************/

void String::replace(char find, char replace) {
  if (!buffer()) {
    return;
  }
  for (char *p = wbuffer(); *p; p++) {
    if (*p == find) {
      *p = replace;
    }
  }
}

void String::replace(const String &find, const String &replace) {
  if (len() == 0 || find.len() == 0) {
    return;
  }
  int diff = replace.len() - find.len();
  char *readFrom = wbuffer();
  char *foundAt;
  if (diff == 0) {
    while ((foundAt = strstr(readFrom, find.buffer())) != NULL) {
      memmove(foundAt, replace.buffer(), replace.len());
      readFrom = foundAt + replace.len();
    }
  } else if (diff < 0) {
    char *writeTo = wbuffer();
    unsigned int l = len();
    while ((foundAt = strstr(readFrom, find.buffer())) != NULL) {
      unsigned int n = foundAt - readFrom;
      memmove(writeTo, readFrom, n);
      writeTo += n;
      memmove(writeTo, replace.buffer(), replace.len());
      writeTo += replace.len();
      readFrom = foundAt + find.len();
      l += diff;
    }
    memmove(writeTo, readFrom, strlen(readFrom) + 1);
    setLen(l);
  } else {
    unsigned int size = len();  // compute size needed for result
    while ((foundAt = strstr(readFrom, find.buffer())) != NULL) {
      readFrom = foundAt + find.len();
      size += diff;
    }
    if (size == len()) {
      return;
    }
    if (size > capacity() && !changeBuffer(size)) {
      log_w("String.Replace() Insufficient space to replace string");
      return;
    }
    int index = len() - 1;
    while (index >= 0 && (index = lastIndexOf(find, index)) >= 0) {
      readFrom = wbuffer() + index + find.len();
      memmove(readFrom + diff, readFrom, len() - (readFrom - buffer()));
      int newLen = len() + diff;
      memmove(wbuffer() + index, replace.buffer(), replace.len());
      setLen(newLen);
      wbuffer()[newLen] = 0;
      index--;
    }
  }
}

void String::remove(unsigned int index) {
  // Pass the biggest integer as the count. The remove method
  // below will take care of truncating it at the end of the
  // string.
  remove(index, (unsigned int)-1);
}

void String::remove(unsigned int index, unsigned int count) {
  if (index >= len()) {
    return;
  }
  if (count <= 0) {
    return;
  }
  if (count > len() - index) {
    count = len() - index;
  }
  char *writeTo = wbuffer() + index;
  unsigned int newlen = len() - count;
  memmove(writeTo, wbuffer() + index + count, newlen - index);
  setLen(newlen);
  wbuffer()[newlen] = 0;
}

void String::toLowerCase(void) {
  if (!buffer()) {
    return;
  }
  for (char *p = wbuffer(); *p; p++) {
    *p = tolower(*p);
  }
}

void String::toUpperCase(void) {
  if (!buffer()) {
    return;
  }
  for (char *p = wbuffer(); *p; p++) {
    *p = toupper(*p);
  }
}

void String::trim(void) {
  if (!buffer() || len() == 0) {
    return;
  }
  char *begin = wbuffer();
  while (isspace(*begin)) {
    begin++;
  }
  char *end = wbuffer() + len() - 1;
  while (isspace(*end) && end >= begin) {
    end--;
  }
  unsigned int newlen = end + 1 - begin;
  if (begin > buffer()) {
    memmove(wbuffer(), begin, newlen);
  }
  setLen(newlen);
  wbuffer()[newlen] = 0;
}

/*********************************************/
/*  Parsing / Conversion                     */
/*********************************************/

long String::toInt(void) const {
  if (buffer()) {
    return atol(buffer());
  }
  return 0;
}

float String::toFloat(void) const {
  if (buffer()) {
    return atof(buffer());
  }
  return 0;
}

double String::toDouble(void) const {
  if (buffer()) {
    return atof(buffer());
  }
  return 0.0;
}

// global empty string to allow returning const String& with nothing

const String emptyString;
