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

#ifndef CB_JSAPI_MEMBER_FUNCTOR_H
#define CB_JSAPI_MEMBER_FUNCTOR_H

#include "Handler.h"


namespace cb {
  namespace JSAPI {
    template <class T>
    class MemberFunctor : public Handler {
      T *object;
      typedef void (T::*member_t)(HTTP::WebContext &, const std::string &,
                                  const JSON::ValuePtr &, JSON::Sync &sync);
      member_t member;

    public:
      MemberFunctor(T *object, member_t member) :
        object(object), member(member) {}

      // From Handler
      void handle(HTTP::WebContext &ctx, const std::string &cmd,
                  const JSON::ValuePtr &msg, JSON::Sync &sync) {
        (*object.*member)(ctx, cmd, msg, sync);
      }
    };
  }
}

#endif // CB_JSAPI_MEMBER_FUNCTOR_H

