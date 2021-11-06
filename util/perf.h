/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_PERF_H
#define BASIL_PERF_H

#include "util/defs.h"
#include "util/ustr.h"
#include "util/rc.h"

struct PerfMarker {
    ustring name;
    PerfMarker(const ustring& name_in);
    ~PerfMarker();
};

void perf_begin(const ustring& subsection);
void perf_end(const ustring& subsection);

#endif