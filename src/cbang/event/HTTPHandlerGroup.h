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

#ifndef CB_EVENT_HTTPHANDLER_GROUP_H
#define CB_EVENT_HTTPHANDLER_GROUP_H

#include "HTTPHandler.h"
#include "JSONHandler.h"

#include <cbang/SmartPointer.h>

#include <vector>
#include <string>


namespace cb {
  class Resource;

  namespace Event {
    class HTTPHandlerGroup : public HTTPHandler {
      typedef std::vector<SmartPointer<HTTPHandler> > handlers_t;
      handlers_t handlers;

    public:
      void addHandler(const SmartPointer<HTTPHandler> &handler);
      void addHandler(unsigned methods, const std::string &pattern,
                      const SmartPointer<HTTPHandler> &handler);
      void addHandler(const std::string &pattern, const Resource &res);
      void addHandler(const Resource &res) {addHandler("", res);}

      template <class T>
      void addMemberHandler
      (T *obj, typename HTTPHandlerMemberFunctor<T>::member_t member) {
        addHandler(new HTTPHandlerMemberFunctor<T>(obj, member));
      }

      template <class T>
      void addMemberHandler
      (unsigned methods, const std::string &pattern,
       T *obj, typename HTTPHandlerMemberFunctor<T>::member_t member) {
        addHandler(methods, pattern,
                   new HTTPHandlerMemberFunctor<T>(obj, member));
      }

      SmartPointer<HTTPHandlerGroup>
      addHandlerGroup(unsigned methods, const std::string &pattern);

      template <class T>
      void addMemberHandler
      (T *obj, typename JSONHandlerMemberFunctor<T>::member_t member) {
        addHandler(new JSONHandlerMemberFunctor<T>(obj, member));
      }

      template <class T>
      void addMemberHandler
      (unsigned methods, const std::string &pattern,
       T *obj, typename JSONHandlerMemberFunctor<T>::member_t member) {
        addHandler(methods, pattern,
                   new JSONHandlerMemberFunctor<T>(obj, member));
      }

      // From HTTPHandler
      bool operator()(Request &req);
    };
  }
}

#endif // CB_EVENT_HTTPHANDLER_GROUP_H

