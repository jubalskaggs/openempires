#pragma once

#include "Unit.h"
#include "Overview.h"
#include "Field.h"
#include "Map.h"
#include "Points.h"
#include "Registrar.h"
#include "Stack.h"

// Units are arranged in a linear array, but also referenced with
// a 2D tile array for quick point lookups.
// Given multiple units occupy the same tile, a unit stack
// can reference multiple units per tile.
typedef struct
{
    Unit* unit;
    Stack* stack;
    int32_t count;
    int32_t max;
    int32_t rows;
    int32_t cols;
    int32_t command_group_next;
    int32_t select_count;
    int32_t cpu_count;
    int32_t repath_index;
}
Units;

Units Units_New(const int32_t max, const Map map, const Grid grid, const Registrar graphics);

Units Units_Append(Units, const Unit);

void Units_Free(const Units);

Stack Units_GetStackCart(const Units, const Point);

Units Units_Caretake(Units, const Registrar, const Overview, const Grid, const Input, const Map, const Field, const Points);

bool Units_CanWalk(const Units, const Map, const Point);

Field Units_Field(const Units, const Map);
