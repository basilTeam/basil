#include "native.h"
#include "stdlib.h"
#include "util/io.h"
#include "values.h"

namespace basil {
    using namespace jasmine;

    void add_native_function(Object& object, const string& name, void* function) {
        using namespace x64;
        writeto(object);
        Symbol sym = global((const char*)name.raw());
        label(sym);
        mov(r64(RAX), imm(i64(function)));
        call(r64(RAX));
        ret();
    }

    void* _cons(i64 value, void* next) {
        void* result = malloc(sizeof(i64) + sizeof(void*));
        *(i64*)result = value;
        *((void**)result + 1) = next;
        return result;
    }

    i64 _listlen(void* list) {
        u32 size = 0;
        while (list) {
            list = *((void**)list + 1);
            size++;
        }
        return size;
    }

    void _display_int(i64 value) {
        println(value);
    }

    void _display_symbol(u64 value) {
        println(symbol_for(value));
    }

    void _display_bool(bool value) {
        println(value);
    }

    void _display_string(const char* value) {
        println(value);
    }

    template <typename T>
    void _display_list(void* value) {
        print("(");
        bool first = true;
        while (value) {
            T i = (T) * (u64*)value;
            print(first ? "" : " ", i);
            value = *((void**)value + 1);
            first = false;
        }
        println(")");
    }

    void _display_int_list(void* value) {
        print("(");
        bool first = true;
        while (value) {
            i64 i = *(i64*)value;
            print(first ? "" : " ", i);
            value = *((void**)value + 1);
            first = false;
        }
        println(")");
    }

    void _display_bool_list(void* value) {
        print("(");
        bool first = true;
        while (value) {
            u64 i = *(u64*)value;
            print(first ? "" : " ", i != 0);
            value = *((void**)value + 1);
            first = false;
        }
        println(")");
    }

    void _display_symbol_list(void* value) {
        print("(");
        bool first = true;
        while (value) {
            u64 i = *(u64*)value;
            print(first ? "" : " ", symbol_for(i));
            value = *((void**)value + 1);
            first = false;
        }
        println(")");
    }

    void _display_native_string_list(void* value) {
        print("(");
        bool first = true;
        while (value) {
            const char* i = *(const char**)value;
            print(first ? "\"" : " \"", i, '"');
            value = *((void**)value + 1);
            first = false;
        }
        println(")");
    }

    void display_native_list(const Type* t, void* list) {
        if (t->kind() != KIND_LIST) return;
        const Type* elt = ((const ListType*)t)->element();
        if (elt == INT) _display_list<i64>(list);
        else if (elt == SYMBOL)
            _display_symbol_list(list);
        else if (elt == BOOL)
            _display_list<bool>(list);
        else if (elt == VOID)
            _display_list<i64>(list);
        else if (elt == STRING)
            _display_native_string_list(list);
    }

    i64 _strcmp(const char* a, const char* b) {
        while (*a && *b && *a == *b) a++, b++;
        return *(const unsigned char*)a - *(const unsigned char*)b;
    }

    i64 _strlen(const char* s) {
        const char* copy = s;
        while (*s++)
            ;
        return s - copy - 1;
    }

    const u8* _read_line() {
        string s;
        while (_stdin.peek() != '\n') { s += _stdin.read(); }
        u8* buf = new u8[s.size() + 1];
        for (u32 i = 0; i < s.size(); i++) buf[i] = s[i];
        buf[s.size()] = '\0';
        return buf;
    }

    i64 _read_int() {
        i64 i;
        read(_stdin, i);
        return i;
    }

    const u8* _read_word() {
        string s;
        read(_stdin, s);
        u8* buf = new u8[s.size() + 1];
        for (u32 i = 0; i < s.size(); i++) buf[i] = s[i];
        buf[s.size()] = '\0';
        return buf;
    }

    u8 _char_at(const char* s, i64 idx) {
        return s[idx];
    }

    const u8* _strcat(const char* a, const char* b) {
        u8* buf = new u8[_strlen(a) + _strlen(b) + 1];
        u32 i = 0;
        for (; *a; a++) buf[i++] = *a;
        for (; *b; b++) buf[i++] = *b;
        buf[i] = '\0';
        return buf;
    }

    const u8* _substr(const char* s, i64 start, i64 end) {
        if (end < start) return new u8[1]{'\0'};
        u8* buf = new u8[end - start + 1];
        i64 i = start;
        for (; i < end; i++) buf[i - start] = s[i];
        buf[i + 1] = '\0';
        return buf;
    }

    void add_native_functions(Object& object) {
        add_native_function(object, "_cons", (void*)_cons);

        add_native_function(object, "_strcmp", (void*)_strcmp);
        add_native_function(object, "_strlen", (void*)_strlen);
        add_native_function(object, "_strcat", (void*)_strcat);
        add_native_function(object, "_substr", (void*)_substr);
        add_native_function(object, "_read_line", (void*)_read_line);
        add_native_function(object, "_read_int", (void*)_read_int);
        add_native_function(object, "_read_word", (void*)_read_word);
        add_native_function(object, "_char_at", (void*)_char_at);
        add_native_function(object, "_listlen", (void*)_listlen);

        add_native_function(object, "_display_int", (void*)_display_int);
        add_native_function(object, "_display_symbol", (void*)_display_symbol);
        add_native_function(object, "_display_bool", (void*)_display_bool);
        add_native_function(object, "_display_string", (void*)_display_string);
        add_native_function(object, "_display_int_list", (void*)_display_int_list);
        add_native_function(object, "_display_symbol_list", (void*)_display_symbol_list);
        add_native_function(object, "_display_bool_list", (void*)_display_bool_list);
        add_native_function(object, "_display_string_list", (void*)_display_native_string_list);
    }
} // namespace basil