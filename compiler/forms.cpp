#include "forms.h"
#include "env.h"
#include "value.h"
#include "util/ustr.h"

namespace basil {
    StateMachine::~StateMachine() {}

    bool Param::matches(const Value& value) {
        switch (kind) {
            case PK_VARIABLE:
            case PK_VARIADIC:
            case PK_TERM:
            case PK_TERM_VARIADIC:
            case PK_QUOTED:
            case PK_QUOTED_VARIADIC:
            case PK_SELF:
                return true;
            case PK_KEYWORD:
                return value.type == T_SYMBOL && value.data.sym == name;
            default:
                return false;
        }
    }

    const Param P_SELF{ S_NONE, PK_SELF };

    Param p_var(Symbol name) {
        return Param{ name, PK_VARIABLE };
    }

    Param p_var(const char* name) {
        return p_var(symbol_from(name));
    }

    Param p_quoted(Symbol name) {
        return Param{ name, PK_QUOTED };
    }

    Param p_quoted(const char* name) {
        return p_quoted(symbol_from(name));
    }

    Param p_term(Symbol name) {
        return Param{ name, PK_TERM };
    }

    Param p_term(const char* name) {
        return p_term(symbol_from(name));
    }

    Param p_term_variadic(Symbol name) {
        return Param{ name, PK_TERM_VARIADIC };
    }

    Param p_term_variadic(const char* name) {
        return p_term_variadic(symbol_from(name));
    }

    Param p_variadic(Symbol name) {
        return Param{ name, PK_VARIADIC };
    }

    Param p_variadic(const char* name) {
        return p_variadic(symbol_from(name));
    }

    Param p_quoted_variadic(Symbol name) {
        return Param{ name, PK_QUOTED_VARIADIC };
    }

    Param p_quoted_variadic(const char* name) {
        return p_quoted_variadic(symbol_from(name));
    }

    Param p_keyword(Symbol name) {
        return Param{ name, PK_KEYWORD };
    }

    Param p_keyword(const char* name) {
        return p_keyword(symbol_from(name));
    }

    bool is_variadic(ParamKind pk) {
        switch (pk) {
            case PK_VARIADIC:
            case PK_QUOTED_VARIADIC:
            case PK_TERM_VARIADIC:
                return true;
            default:
                return false;
        }
    }

    bool is_evaluated(ParamKind pk) {
        return pk == PK_VARIABLE || pk == PK_VARIADIC;
    }
    
    Callable::Callable(const vector<Param>& parameters_in, const optional<FormCallback>& callback_in):
        parameters(ref<vector<Param>>(parameters_in)), callback(callback_in), index(0), stopped(false) {}

    bool Callable::has_prefix_case() {
        return parameters->size() > 0 && (*parameters)[0].kind == PK_SELF;
    }

    bool Callable::has_infix_case() {
        return parameters->size() > 1 && (*parameters)[0].kind != PK_SELF && (*parameters)[1].kind == PK_SELF;
    }

    void Callable::reset() {
        index = 0;
        advances = 0;
        stopped = false;
        wrong_value = none<Value>();
    }  

    bool Callable::precheck_keyword(const Value& keyword) {
        if (is_finished()) return false;
        if ((*parameters)[index].kind == PK_KEYWORD) return (*parameters)[index].matches(keyword);
        if (is_variadic((*parameters)[index].kind)
            && index < parameters->size() - 1
            && (*parameters)[index + 1].kind == PK_KEYWORD
            && (*parameters)[index + 1].matches(keyword)) {
            index ++;
            return true; // we can escape variadics if the next param is a keyword match
        }
        return false;
    }

    bool Callable::precheck_term(const Value& term) {
        return !is_finished() && ((*parameters)[index].kind == PK_TERM 
            || (*parameters)[index].kind == PK_TERM_VARIADIC);
    }

    void Callable::advance(const Value& value) {
        if (is_finished()) {
            if (index == parameters->size() 
                && !is_variadic((*parameters)[index - 1].kind))
                index = parameters->size() + 1; // advance past end so we don't continue to match
            return; // don't advance if we've already stopped
        }
        if ((*parameters)[index].matches(value)) {
            if (!is_variadic((*parameters)[index].kind)) 
                index ++; // don't advance if we're in a variadic
            advances ++; // count this advance
        }
        else {
            stopped = true;
            wrong_value = some<Value>(value);
        }
    }

    bool Callable::is_finished() const {
        return stopped || index >= parameters->size();
    }

    optional<const Callable&> Callable::match() const {
        if (index == parameters->size() - 1 
            && is_variadic(parameters->back().kind))
            return some<const Callable&>(*this);
        return index == parameters->size() ? some<const Callable&>(*this) : none<const Callable&>();
    }

    rc<StateMachine> Callable::clone() {
        return ref<Callable>(*this);
    }
    
    optional<const Param&> Callable::current_param() const {
        return index < parameters->size() ? some<const Param&>((*parameters)[index]) : none<const Param&>();
    }

    bool Callable::operator==(const Callable& other) const {
        if (parameters->size() != other.parameters->size()) return false; // should have same parameter count
        for (u32 i = 0; i < parameters->size(); i ++) {
            Param ours = (*parameters)[i], theirs = (*other.parameters)[i];
            if (ours.kind != theirs.kind) return false; // all parameters should have the same kinds
            if (ours.kind == PK_KEYWORD && ours.name != theirs.name) return false; // ...and same names
        }
        return true; // everything must have matched
    }

    bool Callable::operator!=(const Callable& other) const {
        return !(*this == other);
    }

    u64 Callable::hash() {
        if (!lazy_hash) {
            u64 h = 12877513369093186357ul;
            for (const auto& p : *parameters) {
                h *= 16698397012925964971ul;
                h ^= raw_hash(p.kind);
                if (p.kind == PK_KEYWORD) {
                    h *= 5169422403109494793ul;
                    h ^= raw_hash(p.name);
                }
            }
            lazy_hash = some<u64>(h);
        }
        return *lazy_hash;
    }

    Overloaded::Overloaded(const vector<rc<Callable>>& overloads_in):
        overloads(overloads_in), mangled(ref<set<Symbol>>()) {}

    bool Overloaded::has_prefix_case() {
        if (!has_prefix) {
            has_prefix = some<bool>(false);
            for (auto& overload : overloads) *has_prefix = *has_prefix || overload->has_prefix_case();
        }
        return *has_prefix;
    }

    bool Overloaded::has_infix_case() {
        if (!has_infix) {
            has_infix = some<bool>(false);
            for (auto& overload : overloads) *has_infix = *has_infix || overload->has_infix_case();
        }
        return *has_infix;
    }

    void Overloaded::reset() {
        for (auto& overload : overloads) overload->reset();
    }

    bool Overloaded::precheck_keyword(const Value& keyword) {
        bool matched = false;
        for (auto& overload : overloads) 
            matched = matched || overload->precheck_keyword(keyword); // we match if any child matches
        if (matched) { // if there were any matches...
            for (auto& overload : overloads) 
                if (!overload->precheck_keyword(keyword)) // ...stop any machines that didn't match
                    overload->stopped = true;
        }
        return matched;
    }

    bool Overloaded::precheck_term(const Value& term) {
        bool matched = false;
        for (auto& overload : overloads) 
            matched = matched || overload->precheck_term(term); // we match if any child matches
        if (matched) { // if there were any matches...
            for (auto& overload : overloads) 
                if (!overload->precheck_term(term)) // ...stop any machines that didn't match
                    overload->stopped = true;
        }
        return matched;
    }

    void Overloaded::advance(const Value& value) {
        for (auto& overload : overloads) overload->advance(value);
    }

    bool Overloaded::is_finished() const {
        bool finished = true;
        for (auto& overload : overloads) 
            finished = finished && overload->is_finished(); // we only finish if all children are finished
        return finished;
    }

    optional<const Callable&> Overloaded::match() const {
        for (auto& overload : overloads) 
            if (const auto& m = overload->match()) 
                return m; // we don't allow ambiguous overloads, so we can assume the first is the only match
        return none<const Callable&>();
    }

    rc<StateMachine> Overloaded::clone() {
        vector<rc<Callable>> subforms;
        // subforms.clear();
        for (auto& overload : overloads) subforms.push(overload->clone());
        return ref<Overloaded>(subforms);
    }

    bool Overloaded::operator==(const Overloaded& other) const {
        if (other.overloads.size() != overloads.size()) return false; // should have same number of cases
        for (u32 i = 0; i < overloads.size(); i ++) {
            if (*overloads[i] != *other.overloads[i]) return false; // all cases should match
        }
        return true;
    }

    bool Overloaded::operator!=(const Overloaded& other) const {
        return !(*this == other);
    }

    u64 Overloaded::hash() {
        if (!lazy_hash) {
            u64 h = 9970700534761675987ul;
            for (auto c : overloads) {
                h *= 15605238538515081067ul;
                h ^= c->hash();
            }
            lazy_hash = some<u64>(h);
        }
        return *lazy_hash;
    }
    
    Compound::Compound(const map<Value, rc<Form>>& members_in):
        members(members_in) {}

    Form::Form(): kind(FK_TERM), precedence(INT64_MIN), assoc(ASSOC_LEFT) {}

    Form::Form(FormKind kind_in, i64 precedence_in, Associativity assoc_in): 
        kind(kind_in), precedence(precedence_in), assoc(assoc_in) {}

    bool Form::is_invokable() const {
        return invokable;
    }

    rc<StateMachine> Form::start() {
        if (!is_invokable()) panic("Attempted to start state machine from non-invokable form!");
        auto machine = invokable->clone();
        machine->reset();
        return machine;
    }

    bool Form::has_prefix_case() {
        return is_invokable() && invokable->has_prefix_case();
    }

    bool Form::has_infix_case() {
        return is_invokable() && invokable->has_infix_case();
    }

    bool Form::operator==(const Form& other) const {
        if (kind != other.kind) return false; // kinds should be the same

        switch (kind) {
            case FK_OVERLOADED:
                return *(rc<Overloaded>)invokable == *(rc<Overloaded>)other.invokable;
            case FK_CALLABLE:
                return *(rc<Callable>)invokable == *(rc<Callable>)other.invokable;
            case FK_COMPOUND:
                for (const auto& [k, v] : compound->members) {
                    auto it = other.compound->members.find(k);
                    if (it == other.compound->members.end() || it->second != v) return false; 
                }
                return true;
            case FK_TERM:
                return true; // if the kind matched, we're good
            default:
                panic("Unsupported form kind!");
                return false;
        }
    }

    bool Form::operator!=(const Form& other) const {
        return !(*this == other);
    }

    u64 Form::hash() const {
        u64 kind_hash = raw_hash(kind);
        switch (kind) {
            case FK_CALLABLE:
                return kind_hash * 14361106427190892639ul ^ ((rc<Callable>)invokable)->hash();
            case FK_OVERLOADED:
                return kind_hash * 14114865678206345347ul ^ ((rc<Overloaded>)invokable)->hash();
            case FK_COMPOUND:
                for (const auto& [k, v] : compound->members) {
                    kind_hash *= 12024490689113390177ul;
                    kind_hash ^= k.hash();
                    kind_hash *= 12541430991573364627ul;
                    kind_hash ^= v->hash();
                }
                return kind_hash;
            default:
                return kind_hash;
        }
    }
    
    bool operator==(rc<Form> a, rc<Form> b) {
        return *a == *b;
    }
    
    bool operator!=(rc<Form> a, rc<Form> b) {
        return *a != *b;
    }

    const rc<Form> F_TERM = ref<Form>(FK_TERM, 0, ASSOC_LEFT);

    rc<Form> f_callable(i64 precedence, Associativity assoc, const vector<Param>& parameters) {
        if (parameters.size() == 0) panic("Attempted to construct callable form with no parameters!");
        if (parameters[0].kind != PK_SELF && (parameters.size() == 1 || parameters[1].kind != PK_SELF))
            panic("Attempted to construct callable form without a valid self parameter!");
        rc<Form> form = ref<Form>(FK_CALLABLE, precedence, assoc);
        form->invokable = ref<Callable>(parameters, none<FormCallback>());
        return form;
    }

    rc<Form> f_callable(i64 precedence, Associativity assoc, FormCallback callback, const vector<Param>& parameters) {
        if (parameters.size() == 0) panic("Attempted to construct callable form with no parameters!");
        if (parameters[0].kind != PK_SELF && (parameters.size() == 1 || parameters[1].kind != PK_SELF))
            panic("Attempted to construct callable form without a valid self parameter!");
        if (!callback) panic("Provided null callback when constructing callable form!");
        rc<Form> form = ref<Form>(FK_CALLABLE, precedence, assoc);
        form->invokable = ref<Callable>(parameters, some<FormCallback>(callback));
        return form;
    }

    Symbol mangle(const rc<Callable>& callable) {
        ustring acc;
        for (Param p : *callable->parameters) {
            if (p.kind == PK_KEYWORD) acc += string_from(p.name);
            else if (p.kind == PK_SELF) acc += "(self)";
            else acc += '#'; // '#' is invalid in identifiers, so we use it as a placeholder
            acc += '\\'; // separator

            // TODO: handle empty variadics
        }
        // we return a symbol to avoid some of the overhead in keeping a set of unicode strings
        return symbol_from(acc);
    }

    rc<Form> f_overloaded(i64 precedence, Associativity assoc, const vector<rc<Form>>& overloads) {
        for (const auto& form : overloads) if (form->kind == FK_TERM)
            panic("Attempted to construct overloaded form with at least one non-invokable form!");
        rc<Form> form = ref<Form>(FK_OVERLOADED, precedence, assoc);
        vector<rc<Callable>> callables;
        for (rc<Form> form : overloads) {
            if (form->kind == FK_CALLABLE) {
                bool found_match = false;
                for (rc<Callable> callable : callables) {
                    if (*callable == *(rc<Callable>)form->invokable) {
                        found_match = true;
                        break;
                    }
                }
                if (!found_match) callables.push((rc<Callable>)form->invokable);
            }
            else if (form->kind == FK_OVERLOADED) {
                for (rc<Callable> callable : ((rc<Overloaded>)form->invokable)->overloads) {
                    bool found_match = false;
                    for (rc<Callable> existing_callable : callables) {
                        if (*callable == *existing_callable) {
                            found_match = true;
                            break;
                        }
                    }
                    if (!found_match) callables.push(callable);
                }
            }
        }
        rc<Overloaded> invokable = ref<Overloaded>(callables);
        form->invokable = invokable;
        return form;
    }
    
    rc<Form> f_overloaded(i64 precedence, Associativity assoc, const vector<rc<Callable>>& overloads) {
        rc<Form> form = ref<Form>(FK_OVERLOADED, precedence, assoc);
        vector<rc<Callable>> callables;
        for (rc<Callable> callable : overloads) {
            bool found_match = false;
            for (rc<Callable> callable : callables) {
                if (*callable == *(rc<Callable>)form->invokable) {
                    found_match = true;
                    break;
                }
            }
            if (!found_match) callables.push((rc<Callable>)form->invokable);
        }
        rc<Overloaded> invokable = ref<Overloaded>(callables);
        form->invokable = invokable;
        return form;
    }

    rc<Form> f_compound(const map<Value, rc<Form>>& members) {
        rc<Form> form = ref<Form>(FK_COMPOUND, INT64_MIN, ASSOC_LEFT); // precedence and associativity are irrelevant here
        form->compound = ref<Compound>(members);
        return form;
    }

    const char* FK_NAMES[NUM_FORM_KINDS] = {
        "term",
        "callable",
        "overloaded",
        "compound"
    };

    const char* PK_NAMES[NUM_PARAM_KINDS] = {
        "variable",
        "variadic",
        "keyword",
        "term",
        "term-variadic",
        "quoted",
        "quoted-variadic",
        "self"
    };
}

template<>
u64 hash(const rc<basil::Form>& form) {
    return form->hash();
}

void write(stream& io, const basil::Param& param) {
    switch (param.kind) {
        case basil::PK_VARIABLE: write(io, ITALICWHITE, param.name, RESET, "?"); break;
        case basil::PK_QUOTED: write(io, ITALICWHITE, ":", param.name, RESET, "?"); break;
        case basil::PK_TERM: write(io, ITALICWHITE, ";", param.name, RESET, "?"); break;
        case basil::PK_VARIADIC: write(io, ITALICWHITE, param.name, RESET, "...?"); break;
        case basil::PK_QUOTED_VARIADIC: write(io, ITALICWHITE, param.name, RESET, "...?"); break;
        case basil::PK_TERM_VARIADIC: write(io, ITALICWHITE, param.name, RESET, "...?"); break;
        case basil::PK_KEYWORD: write(io, param.name); break;
        case basil::PK_SELF: write(io, "<self>"); break;
        default: break;
    }
}

void write(stream& io, const rc<basil::Form>& form) {
    if (form->kind == basil::FK_TERM) {
        write(io, "term");
    }
    else if (form->kind == basil::FK_CALLABLE) {
        write_seq(io, *((rc<basil::Callable>)form->invokable)->parameters, "(", " ", ")");
    }
    else if (form->kind == basil::FK_OVERLOADED) {
        bool first = true;
        for (const rc<basil::Callable>& callable : ((rc<basil::Overloaded>)form->invokable)->overloads) {
            if (!first) write(io, " & ");
            first = false;
            write_seq(io, *callable->parameters, "(", " ", ")");
        }
    }
    else if (form->kind == basil::FK_COMPOUND) {
        write_pairs(io, form->compound->members, "{", ": ", ", ", "}");
    }
}

void write_with_self(stream& io, const basil::Value& self, const rc<basil::Callable>& callable) {
    bool first = true;
    for (const basil::Param& p : *callable->parameters) {
        if (!first) write(io, " ");
        first = false;
        if (p.kind == basil::PK_SELF) write(io, self);
        else write(io, p);
    }
}

void write(stream& io, basil::ParamKind pk) {
    write(io, basil::PK_NAMES[pk]);
}

void write(stream& io, basil::FormKind fk) {
    write(io, basil::FK_NAMES[fk]);
}