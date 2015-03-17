/******************************************************************************\

          This file is part of the C! library.  A.K.A the cbang library.

              Copyright (c) 2003-2015, Cauldron Development LLC
                 Copyright (c) 2003-2015, Stanford University
                             All rights reserved.

        The C! library is free software: you can redistribute it and/or
        modify it under the terms of the GNU Lesser General Public License
        as published by the Free Software Foundation, either version 2.1 of
        the License, or (at your option) any later version.

        The C! library is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
        Lesser General Public License for more details.

        You should have received a copy of the GNU Lesser General Public
        License along with the C! library.  If not, see
        <http://www.gnu.org/licenses/>.

        In addition, BSD licensing may be granted on a case by case basis
        by written permission from at least one of the copyright holders.
        You may request written permission by emailing the authors.

                For information regarding this software email:
                               Joseph Coffland
                        joseph@cauldrondevelopment.com

\******************************************************************************/

#include "Buffer.h"

#include <cbang/Exception.h>
#include <cbang/String.h>

#include <event2/buffer.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#endif

using namespace std;
using namespace cb::Event;


Buffer::Buffer(evbuffer *evb, bool deallocate) :
  evb(evb), deallocate(deallocate) {
}


Buffer::Buffer(const char *data, unsigned length) :
  evb(evbuffer_new()), deallocate(true) {
  if (!evb) THROW("Failed to create event buffer");
  add(data, length);
}


Buffer::Buffer(const char *s) : evb(evbuffer_new()), deallocate(true) {
  if (!evb) THROW("Failed to create event buffer");
  add(s);
}


Buffer::Buffer(const string &s) : evb(evbuffer_new()), deallocate(true) {
  if (!evb) THROW("Failed to create event buffer");
  add(s);
}


Buffer::Buffer() : evb(evbuffer_new()), deallocate(true) {
  if (!evb) THROW("Failed to create event buffer");
}


Buffer::~Buffer() {
  if (evb && deallocate) evbuffer_free(evb);
}


unsigned Buffer::getLength() const {
  return evbuffer_get_length(evb);
}


const char *Buffer::toCString() const {
  return (const char *)evbuffer_pullup(evb, -1);
}


string Buffer::toString() const {
  return string(toCString(), getLength());
}


string Buffer::hexdump() const {
  return String::hexdump(toCString(), getLength());
}


void Buffer::clear() {
  if (evbuffer_drain(evb, evbuffer_get_length(evb)))
    THROW("Buffer drain failed");
}


void Buffer::add(const char *data, unsigned length) {
  if (evbuffer_add(evb, data, length)) THROW("Buffer add failed");
}


void Buffer::add(const Buffer &buf) {
  if (evbuffer_add_buffer(evb, buf.getBuffer())) THROW("Add buffer failed");
}


void Buffer::addRef(const Buffer &buf) {
  if (evbuffer_add_buffer_reference(evb, buf.getBuffer()))
    THROW("Add buffer reference failed");
}


void Buffer::add(const char *s) {
  if (evbuffer_add(evb, s, strlen(s))) THROW("Buffer add failed");
}


void Buffer::add(const string &s) {
#ifndef WIN32
  add(s.data(), s.length());
#else
  add(s.c_str(), s.length());
#endif
}


void Buffer::addFile(const string &path) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd == -1) THROWS("Failed to open file " << path);

  struct stat buf;
  if (fstat(fd, &buf)) THROWS("Failed to get file size " << path);

  if (evbuffer_add_file(evb, fd, 0, buf.st_size))
    THROWS("Failed to add file to buffer: " << path);
}


int Buffer::remove(char *data, unsigned length) {
  return evbuffer_remove(evb, data, length);
}
