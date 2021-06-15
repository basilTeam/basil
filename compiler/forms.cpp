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
            case PK_QUOTED:
            case PK_SELF:
                return true;
            case PK_KEYWORD:
                return value.type == T_SYMBOL && value.data.sym == name;
            default:
                return false;
        }
    }

    const Param P_VAR{ S_NONE, PK_VARIABLE },
                P_QUOTED{ S_NONE, PK_QUOTED },
                P_TERM{ S_NONE, PK_TERM },
                P_SELF{ S_NONE, PK_SELF };

    Param p_keyword(Symbol name) {
        return Param{ name, PK_KEYWORD };
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
        stopped = false;
    }  

    bool Callable::precheck_keyword(const Value& keyword) {
        if (is_finished()) return false;
        if ((*parameters)[index].kind == PK_KEYWORD) return (*parameters)[index].matches(keyword);
        if ((*parameters)[index].kind == PK_VARIADIC 
            && index < parameters->size() - 1
            && (*parameters)[index + 1].kind == PK_KEYWORD
            && (*parameters)[index + 1].matches(keyword)) {
            index ++;
            return true; // we can escape variadics if the next param is a keyword match
        }
        return false;
    }

    bool Callable::precheck_term(const Value& term) {
        return !is_finished() && (*parameters)[index].kind == PK_TERM;
    }

    void Callable::advance(const Value& value) {
        if (is_finished()) {
            if (index == parameters->size() && (*parameters)[index - 1].kind != PK_VARIADIC)
                index = parameters->size() + 1; // advance past end so we don't continue to match
            return; // don't advance if we've already stopped
        }
        if ((*parameters)[index].matches(value)) {
            if ((*parameters)[index].kind != PK_VARIADIC) index ++; // don't advance if we're in a variadic
        }
        else stopped = true;
    }

    bool Callable::is_finished() const {
        return stopped || index >= parameters->size();
    }

    optional<const Callable&> Callable::match() const {
        return index == parameters->size() ? some<const Callable&>(*this) : none<const Callable&>();
    }

    rc<StateMachine> Callable::clone() {
        return ref<Callable>(*this);
    }
    
    optional<const Param&> Callable::current_param() const {
        return index < parameters->size() ? some<const Param&>((*parameters)[index]) : none<const Param&>();
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
        return ref<Overloaded>(*this);
    }

    Form::Form(): kind(FK_TERM), precedence(0), assoc(ASSOC_LEFT) {}

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

    optional<rc<Form>> f_overloaded(i64 precedence, Associativity assoc, const vector<rc<Form>>& overloads) {
        for (const auto& form : overloads) if (form->kind == FK_TERM)
            panic("Attempted to construct overloaded form with at least one non-invokable form!");
        rc<Form> form = ref<Form>(FK_OVERLOADED, precedence, assoc);
        static vector<rc<Callable>> callables;
        callables.clear();
        for (rc<Form> form : overloads) {
            if (form->kind == FK_CALLABLE) callables.push((rc<Callable>)form->invokable);
            else if (form->kind == FK_OVERLOADED) {
                for (rc<Callable> callable : ((rc<Overloaded>)form->invokable)->overloads)
                    callables.push(callable);
            }
        }
        rc<Overloaded> invokable = ref<Overloaded>(callables);
        for (const auto& callable : callables) {
            Symbol m = mangle(callable);
            if (invokable->mangled->contains(m)) return none<rc<Form>>(); // ambiguous
            else invokable->mangled->insert(m);
        }
        form->invokable = invokable;
        return some<rc<Form>>(form);
    }

    optional<rc<Form>> f_add_overload(rc<Form> overloaded, rc<Form> addend) {
        if (overloaded->kind != FK_OVERLOADED) panic("Attempted to add overload to non-overloaded form!");
        if (addend->kind == FK_TERM) panic("Attempted to append non-invokable form to overloaded form!");
        rc<Overloaded> existing = (rc<Overloaded>)overloaded->invokable;
        if (addend->kind == FK_CALLABLE) {
            Symbol m = mangle((rc<Callable>)addend->invokable);
            if (existing->mangled->contains(m)) return none<rc<Form>>(); // ambiguous
            else {
                existing->overloads.push((rc<Callable>)addend->invokable);
                existing->mangled->insert(m);
            }
        }
        else if (addend->kind == FK_OVERLOADED) {
            for (rc<Callable> callable : ((rc<Overloaded>)addend->invokable)->overloads) {
                Symbol m = mangle(callable);
                if (existing->mangled->contains(m)) return none<rc<Form>>(); // ambiguous
                else {
                    existing->overloads.push(callable);
                    existing->mangled->insert(m);
                }
            }
        }
        return some<rc<Form>>(overloaded);
    }

    const char* FK_NAMES[NUM_FORM_KINDS] = {
        "term",
        "callable",
        "overloaded"
    };

    const char* PK_NAMES[NUM_PARAM_KINDS] = {
        "variable",
        "variadic",
        "keyword"
    };
}

void write(stream& io, basil::ParamKind pk) {
    write(io, basil::PK_NAMES[pk]);
}

void write(stream& io, basil::FormKind fk) {
    write(io, basil::FK_NAMES[fk]);
}