/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "util/perf.h"
#include "util/ustr.h"
#include "util/vec.h"
#include "time.h"

static bool do_perf = false; // TODO: cmdline frontend to set or unset this

struct PerfTime {
    ustring name;
    double ms;

    vector<rc<PerfTime>> children;

    void format(stream& io, u32 depth) const {
        static const char* bullets[4] = {
            "▫",
            "•",
            "◦",
            "▪"
        };
        for (i64 i = 0; i < i64(depth) - 1; i ++)
            write(io, "    ");
        if (depth)
            write(io, "  ", bullets[depth % 4], " ");

        write(io, name, " took ");
        const char* color = BOLDGREEN;
        if (ms >= 100) color = BOLDYELLOW;
        if (ms >= 1000) color = BOLDRED;
        writeln(io, ITALIC, color, ms, RESET, " ms");
        
        for (auto subtime : children) subtime->format(io, depth + 1);
    }
};

struct PerfEntry {
    ustring name;
    u64 start;

    vector<rc<PerfTime>> children;
};

static vector<PerfEntry> perf_sections;

void perf_begin(const ustring& subsection) {
    if (!do_perf) return;

    perf_sections.push({ subsection, (u64)clock() });
}

void perf_end(const ustring& subsection) {
    if (!do_perf) return;

    static const char* bullets[4] = {
        "▪",
        "▫",
        "•",
        "◦"
    };
    if (!perf_sections.size() || perf_sections.back().name != subsection)
        panic("Couldn't close perf section '", subsection, "'!");

    double ms = double(clock() - perf_sections.back().start) / (CLOCKS_PER_SEC / 1000);

    rc<PerfTime> timer = ref<PerfTime>();
    timer->name = subsection;
    timer->ms = ms;
    timer->children = perf_sections.back().children;

    if (perf_sections.size() > 1) {
        perf_sections.pop();
        perf_sections.back().children.push(timer);
    }
    else {
        perf_sections.pop();
        timer->format(_stdout, 0);
    }
}

PerfMarker::PerfMarker(const ustring& name_in):
    name(name_in) { perf_begin(name); }

PerfMarker::~PerfMarker() {
    perf_end(name);
}