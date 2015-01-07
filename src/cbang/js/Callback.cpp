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

#include "Callback.h"

#include <cbang/Exception.h>
#include <cbang/SStream.h>

using namespace std;
using namespace cb::js;


Callback::Callback(const Signature &sig) :
  sig(sig), data(v8::External::New(this)),
  function(v8::FunctionTemplate::New(&Callback::callback, data)) {
}


v8::Handle<v8::Value> Callback::callback(const v8::Arguments &args) {
  Callback *cb =
    static_cast<Callback *>(v8::External::Cast(*args.Data())->Value());

  try {
    return (*cb)(Arguments(args, cb->sig)).getV8Value();

  } catch (const Exception &e) {
    return v8::ThrowException(v8::String::New(SSTR(e).c_str()));

  } catch (const std::exception &e) {
    return v8::ThrowException(v8::String::New(e.what()));

  } catch (...) {
    return v8::ThrowException(v8::String::New("Unknown exception"));
  }
}
