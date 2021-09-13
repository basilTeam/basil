#ifndef BASIL_OBJ_H
#define BASIL_OBJ_H

#include "util/option.h"
#include "util/rc.h"
#include "util/bytebuf.h"
#include "type.h"

namespace basil {
    struct Form;
    struct Type;

    // Associated information for a defined symbol. A list of defined symbols
    // begins every section, and each has one of these info structs associated
    // with it. The form and type are included when the symbol is the name of a
    // variable or function - leave them empty otherwise. All symbols require
    // a defined offset (within the section). 
    struct DefInfo {
        u32 section;
        optional<rc<Form>> form;
        optional<Type> type;
    };

    // The different types of sections permitted within a Basil object.
    enum SectionType {
        ST_NONE = 0,    // The absence of a section.
        ST_SOURCE = 1,  // Raw Basil source code, in text form.
        ST_PARSED = 2,  // Parsed Basil source code.
        ST_EVALED = 3,  // Evaluated Basil source code.
        ST_AST = 4,     // Typed Basil AST.
        ST_JASMINE = 5, // Jasmine bytecode.
        ST_NATIVE = 6,  // Native machine code for a particular architecture.
        ST_LIBRARY = 7, // A wrapper around a shared library.
        ST_DATA = 8,    // Raw data.
        ST_LICENSE = 9  // A license associated with this code/data.
    };
    
    // A section within a Basil object. Contains a list of symbols defined within
    // the section, a section type, and a large block of serialized data. Different
    // section types structure this data differently.
    struct Section {
        SectionType type;
        map<Symbol, DefInfo> defs;

        // Fills in all internal data structures besides 'type' and 'defs' with
        // data from the provided buffer.
        virtual void deserialize(bytebuf& buf) = 0;

        // Writes all internal data structures besides 'type' and 'defs' to the
        // provided buffer.
        virtual void serialize(bytebuf& buf) = 0;
    };

    // Compact semver-style version tuple.
    struct Version {
        u8 major;
        u16 minor;
        u8 patch;
    };

    // A single Basil object, containing some number of sections.
    struct Object {
        Version version;
        vector<rc<Section>> sections;

        // Loads this object in full from the provided stream.
        void read(stream& io);

        // Writes this object to the provided stream.
        void write(stream& io);
    };

    struct Source;
    struct Env;
    struct Value;

    rc<Section> source_section(rc<Source> source);
    rc<Section> parsed_section(const Value& ast);
    rc<Section> evaled_section(rc<Env> env, const Value& result);
}

#endif