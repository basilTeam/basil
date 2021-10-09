/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_ENV_H
#define BASIL_ENV_H

#include "util/hash.h"
#include "util/vec.h"
#include "util/rc.h"
#include "util/option.h"
#include "value.h"
#include "type.h"

namespace basil {
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

        // Duplicates this environment, creating an identical environment with the same parent.
        rc<Env> clone() const;
        
        // Constructs an environment with a parent. Use extend() instead of calling this
        // directly.
        Env(rc<Env> parent);
    };

    // Extends the provided parent environment with an empty child environment.
    rc<Env> extend(rc<Env> parent);

    // Looks up a symbol within the environment, returning a reference to the
    // specific environment in which the name is defined. Returns a none optional
    // if neither the provided environment or any of its parents defines the
    // provided symbol.
    optional<rc<Env>> locate(rc<Env> env, Symbol name);
}

void write(stream& io, rc<basil::Env> env);

#endif