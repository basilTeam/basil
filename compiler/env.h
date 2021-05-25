#ifndef BASIL_ENV_H
#define BASIL_ENV_H

#include "util/hash.h"
#include "util/vec.h"
#include "util/rc.h"
#include "util/option.h"
#include "type.h"

namespace basil {
    struct Value;

    struct Env {
        rc<Env> parent;
        map<Symbol, Value> values;

        Env();
        Env(rc<Env> parent);
        void def(Symbol name, const Value& value);
        optional<const Value&> find(Symbol name) const;
        optional<Value&> find(Symbol name);
    };

    rc<Env> extend(rc<Env> parent);
}

#endif