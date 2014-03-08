/******************************************************************************\

          This file is part of the C! library.  A.K.A the cbang library.

              Copyright (c) 2003-2014, Cauldron Development LLC
                 Copyright (c) 2003-2014, Stanford University
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

#ifndef CBANG_SCANNER_H
#define CBANG_SCANNER_H

#include <cbang/FileLocation.h>
#include <cbang/io/InputSource.h>

namespace cb {
  class Scanner {
    int x;
    InputSource source;
    FileLocation location;

  public:
    Scanner(const InputSource &source);

    FileLocation &getLocation() {return location;}
    const FileLocation &getLocation() const {return location;}

    bool hasMore();
    int peek();
    void advance();
    void match(char c);
    bool consume(char c);
    std::string seek(const char *s, bool inverse = false, bool skip = false);
    void skipWhiteSpace(const char *s = " \t\r\n");

  protected:
    int next()
    {return source.getStream().good() ? source.getStream().get() : -1;}
  };
}

#endif // CBANG_SCANNER_H

