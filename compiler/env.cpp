#include "env.h"
#include "eval.h"
#include "value.h"

namespace basil {
    Env::Env(): parent(nullptr) {}

    Env::Env(rc<Env> parent_in): parent(parent_in) {}

    void Env::def(Symbol name, const Value& value) {
        values.put(name, value);
    }

    optional<const Value&> Env::find(Symbol name) const {
        auto it = values.find(name);
        if (it == values.end()) {
            if (parent) return parent->find(name);
            return none<const Value&>();
        }
        else return some<const Value&>(it->second);
    }

    optional<Value&> Env::find(Symbol name) {
        auto it = values.find(name);
        if (it == values.end()) {
            if (parent) return parent->find(name);
            return none<Value&>();
        }
        else return some<Value&>(it->second);
    }

    rc<Env> extend(rc<Env> parent) {
        rc<Env> env = ref<Env>(parent);
        parent->children.push(env);
        return env;
    }

    optional<rc<Env>> locate(rc<Env> env, Symbol name) {
        auto it = env->values.find(name);
        if (it != env->values.end()) return some<rc<Env>>(env);
        else if (env->parent) return locate(env->parent, name);
        else return none<rc<Env>>();
    }
}

void write(stream& io, rc<basil::Env> env) {
    write_pairs(io, env->values, "{", ": ", ", ", "}");
}