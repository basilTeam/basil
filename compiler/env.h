#ifndef BASIL_ENV_H
#define BASIL_ENV_H

#include "util/hash.h"
#include "util/vec.h"
#include "util/rc.h"
#include "util/option.h"
#include "type.h"

namespace basil {
    struct Value;

    // Represents an environment within which evaluation may be performed.
    // Environments are essentially just mappings from symbols to values -
    // all form and AST information is stored in the normal value representation
    // so no additional tracking is necessary here. In addition to being mappings,
    // though, each environment also serves as a node in a tree. Each environment
    // tracks its parent environment, as well as a list of child environments forked
    // from it.
    //
    // This bidirectional reference pattern means that environments are not
    // collected at the end of a given function scope! They are tied into the
    // overall tree of the compilation session and remain there unless
    // explicitly untied later.
    struct Env {
        rc<Env> parent;
        vector<rc<Env>> children;
        map<Symbol, Value> values;

        // Constructs an empty environment with no parent.
        Env();

        // Binds a name to a value within this environment. Will replace prior mappings
        // if called for a name that already exists within the environment.
        void def(Symbol name, const Value& value);

        // Looks up a name within the environment. Returns a none optional if the name
        // is not present in this environment or any parent environment. Otherwise, returns
        // the value in this environment or the nearest parent that contained the name.
        optional<const Value&> find(Symbol name) const;
        optional<Value&> find(Symbol name);
        
        // Constructs an environment with a parent. Use extend() instead of calling this
        // directly.
        Env(rc<Env> parent);
    };
    // Extends the provided parent environment with an empty child environment.
    rc<Env> extend(rc<Env> parent);
}

#endif