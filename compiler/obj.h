/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_OBJ_H
#define BASIL_OBJ_H

#include "util/option.h"
#include "util/rc.h"
#include "util/bytebuf.h"
#include "type.h"
#include "ssa.h"
#include "jasmine/obj.h"

namespace basil {
    struct Form;
    struct Type;

    // Associated information for a defined symbol. A list of defined symbols
    // begins every section, and each has one of these info structs associated
    // with it. The form and type are included when the symbol is the name of a
    // variable or function - leave them empty otherwise. All symbols require
    // a defined offset (within the section). 
    struct DefInfo {
        u32 offset;
        optional<rc<Form>> form;
        optional<Type> type;
    };

    // The different types of sections permitted within a Basil object.
    enum SectionType {
        ST_NONE = 0,    // The absence of a section.
        ST_SOURCE = 1,  // Raw Basil source code, in text form.
        ST_PARSED = 2,  // Parsed Basil source code.
        ST_EVAL = 3,    // Evaluated Basil module.
        ST_AST = 4,     // Typed Basil AST.
        ST_IR = 5,      // SSA-based Basil IR.
        ST_JASMINE = 6, // Jasmine bytecode.
        ST_NATIVE = 7,  // Native machine code for a particular architecture.
        ST_LIBRARY = 8, // A wrapper around a shared library.
        ST_DATA = 9,    // Raw data.
        ST_LICENSE = 10 // A license associated with this code/data.
    };
    
    // A section within a Basil object. Contains a list of symbols defined within
    // the section, a section type, and a large block of serialized data. Different
    // section types structure this data differently.
    struct Section {
        SectionType type;
        ustring name;
        map<Symbol, DefInfo> defs;

        // Constructs a section from a SectionType and def table.
        Section(SectionType type, const ustring& name, const map<Symbol, DefInfo>& defs);

        virtual ~Section();

        // Writes the section type and definition table to the provided byte
        // buffer.
        void serialize_header(bytebuf& buf);

        // Fills in all internal data structures besides 'type' and 'defs' with
        // data from the provided buffer.
        virtual void deserialize(bytebuf& buf) = 0;

        // Writes all internal data structures besides 'type' and 'defs' to the
        // provided buffer.
        virtual void serialize(bytebuf& buf) = 0;
    };

    // Compact semver-style version tuple.
    struct Version {
        u16 major, minor, patch;
    };

    // A single Basil object, containing some number of sections.
    struct Object {
        Version version;
        optional<u32> main_section;
        vector<rc<Section>> sections;

        // Creates an empty object with the current default version.
        Object();

        // Loads this object in full from the provided stream.
        void read(stream& io);

        // Writes this object to the provided stream.
        void write(stream& io);

        // Prints information about this object file to the provided stream.
        void show(stream& io);
    };

    class Source;
    struct Env;
    struct Value;
    struct AST;

    rc<Source> source_from_section(rc<Section> section);
    Value parsed_from_section(rc<Section> section);
    rc<Env> module_from_section(rc<Section> section);
    Value module_main(rc<Section> section);
    const map<Symbol, rc<AST>>& ast_from_section(rc<Section> section);
    rc<AST> ast_main(rc<Section> section);
    rc<Env> ast_env(rc<Section> section);
    const map<Symbol, rc<IRFunction>>& ir_from_section(rc<Section> section);
    rc<IRFunction> ir_main(rc<Section> section);
    const jasmine::Object& jasmine_from_section(rc<Section> section);
    const jasmine::Object& native_from_section(rc<Section> section);

    rc<Section> source_section(const ustring& name, rc<Source> source);
    rc<Section> parsed_section(const ustring& name, const Value& term);
    rc<Section> module_section(const ustring& name, const Value& main, rc<Env> module);
    rc<Section> ast_section(const ustring& name, rc<AST> main, const map<Symbol, rc<AST>>& functions, rc<Env> env);
    rc<Section> ir_section(const ustring& name, rc<IRFunction> main, const map<Symbol, rc<IRFunction>>& functions);
    rc<Section> jasmine_section(const ustring& name, rc<jasmine::Object> object);
    rc<Section> native_section(const ustring& name, rc<jasmine::Object> object);
}

#endif