#include "Units.h"

#include "Util.h"
#include "File.h"
#include "Field.h"
#include "Rect.h"
#include "Surface.h"
#include "Tiles.h"
#include "Graphics.h"

#include <stdlib.h>

static void FindPath(const Units units, Unit* const unit, const Map map, const Point cart_goal, const Point cart_grid_offset_goal)
{
    if(!State_IsDead(unit->state))
    {
        const Field field = Field_New(map, units);
        Unit_FreePath(unit);
        unit->path = Field_SearchBreadthFirst(field, unit->cart, cart_goal);
        unit->cart_grid_offset_goal = cart_grid_offset_goal;
        unit->command_group = units.command_group_next;
        Field_Free(field);
    }
}

static Units GenerateTestZone(Units units, const Map map, const Grid grid)
{
#if 1
    for(int32_t x = 0; x < 10; x++)
    for(int32_t y = 0; y < map.rows; y++)
    {
        const Point cart = { x, y };
        units = Units_Append(units, Unit_Make(cart, grid, FILE_TEUTONIC_KNIGHT_IDLE, COLOR_BLU));
    }
    for(int32_t x = map.cols - 10; x < map.cols; x++)
    for(int32_t y = 0; y < map.rows; y++)
    {
        const Point cart = { x, y };
        units = Units_Append(units, Unit_Make(cart, grid, FILE_TEUTONIC_KNIGHT_IDLE, COLOR_RED));
    }
    for(int32_t i = 0; i < units.count; i++)
    {
        static Point zero;
        Unit* const unit = &units.unit[i];
        const Point point = {
            map.cols / 2,
            unit->cart.y < (map.rows / 2) ? (map.rows / 2 - map.rows / 6): (map.rows / 2 + map.rows / 6),
        };
        FindPath(units, unit, map, point, zero);
    }
#else
    const Point carts[] = {
        { 0 + map.cols / 2, 0 + map.rows / 2 },
        { 2 + map.cols / 2, 2 + map.rows / 2 },
    };
    units = Units_Append(units, Unit_Make(carts[0], grid, FILE_MALE_VILLAGER_STANDING, COLOR_BLU));
    units = Units_Append(units, Unit_Make(carts[1], grid, FILE_MALE_VILLAGER_STANDING, COLOR_RED));
#endif
    return units;
}

Units Units_New(const int32_t max, const Map map, const Grid grid)
{
    const int32_t area = grid.rows * grid.cols;
    Unit* const unit = UTIL_ALLOC(Unit, max);
    Stack* const stack = UTIL_ALLOC(Stack, area);
    UTIL_CHECK(unit);
    UTIL_CHECK(stack);
    for(int32_t i = 0; i < area; i++)
        stack[i] = Stack_Build(8);
    static Units zero;
    Units units = zero;
    units.unit = unit;
    units.max = max;
    units.stack = stack;
    units.rows = grid.rows;
    units.cols = grid.cols;
    units = GenerateTestZone(units, map, grid);
    units.cpu_count = 2 * SDL_GetCPUCount();
    return units;
}

Units Units_Append(Units units, Unit unit)
{
    if(units.count == units.max)
    {
        units.max *= 2;
        Unit* const temp = UTIL_REALLOC(units.unit, Unit, units.max);
        UTIL_CHECK(temp);
        units.unit = temp;
    }
    unit.id = units.id_next++;
    units.unit[units.count++] = unit;
    return units;
}

void Units_Free(const Units units)
{
    const int32_t area = units.rows * units.cols;
    for(int32_t i = 0; i < area; i++)
        Stack_Free(units.stack[i]);
    free(units.stack);
    free(units.unit);
}

static bool OutOfBounds(const Units units, const Point point)
{
    return point.x < 0
        || point.y < 0
        || point.x >= units.cols
        || point.y >= units.rows;
}

static Stack* GetStack(const Units units, const Point p)
{
    return &units.stack[p.x + p.y * units.cols]; // Implies cartesian.
}

Stack Units_GetStackCart(const Units units, const Point p)
{
    static Stack zero;
    return OutOfBounds(units, p) ? zero : *GetStack(units, p); // Return a shallow copy of the stack to prevent write access.
}

static Units UnSelectAll(Units units)
{
    units.select_count = 0;
    for(int32_t i = 0; i < units.count; i++)
        units.unit[i].selected = false;
    return units;
}

static Units Select(Units units, const Overview overview, const Input input, const Registrar graphics)
{
    const Quad quad = Overview_GetRenderBox(overview, -200); // XXX, Border needs to be equal to largest building size.
    const Points points = Quad_GetRenderPoints(quad);
    const Tiles tiles = Tiles_PrepGraphics(graphics, overview, units, points); // XXX. A little excessive, as this is done in the renderer, but its gets the job done.
    if(input.lu)
    {
        units = UnSelectAll(units);
        if(Overview_IsSelectionBoxBigEnough(overview))
            units.select_count = Tiles_SelectWithBox(tiles, overview.selection_box);
        else
        {
            const Tile tile = Tiles_SelectOne(tiles, input.point);
            if(tile.reference
            && input.key[SDL_SCANCODE_LCTRL])
                units.select_count = Tiles_SelectSimilar(tiles, tile);
            else
                units.select_count = 1;
        }
    }
    Points_Free(points);
    Tiles_Free(tiles);
    return units;
}

static void FindPathForSelected(const Units units, const Map map, const Point cart_goal, const Point cart_grid_offset_goal)
{
    for(int32_t i = 0; i < units.count; i++)
    {
        Unit* const unit = &units.unit[i];
        if(unit->selected
        && unit->max_speed > 0)
            FindPath(units, unit, map, cart_goal, cart_grid_offset_goal);
    }
}

static Units Command(Units units, const Overview overview, const Input input, const Map map)
{
    if(input.ru)
    {
        units.command_group_next++;
        const Point cart_goal = Overview_IsoToCart(overview, input.point, false);
        const Point cart = Overview_IsoToCart(overview, input.point, true);
        const Point cart_grid_offset_goal = Grid_GetOffsetFromGridPoint(overview.grid, cart);
        if(Units_CanWalk(units, map, cart_goal))
            FindPathForSelected(units, map, cart_goal, cart_grid_offset_goal);
    }
    return units;
}

static void ResetStacks(const Units units)
{
    for(int32_t y = 0; y < units.rows; y++)
    for(int32_t x = 0; x < units.cols; x++)
    {
        const Point point = { x, y };
        GetStack(units, point)->count = 0;
    }
}

static void StackStacks(const Units units)
{
    for(int32_t i = 0; i < units.count; i++)
    {
        Unit* const unit = &units.unit[i];
        Stack* const stack = GetStack(units, unit->cart);
        Stack_Append(stack, unit);
    }
}

static Point CoheseBoids(const Units units, Unit* const unit)
{
    static Point zero;
    if(!State_IsDead(unit->state))
    {
        const Stack stack = Units_GetStackCart(units, unit->cart);
        const Point delta = Point_Sub(stack.center_of_mass, unit->cell);
        const Point cohesion = Point_Div(delta, 128); // XXX. What is a good divisor?
        return stack.count > 0 ? cohesion : zero;
    }
    return zero;
}

static Point Separate(Unit* const unit, Unit* const other)
{
    static Point zero;
    if(!State_IsDead(other->state) && unit->id != other->id)
    {
        const Point diff = Point_Sub(other->cell, unit->cell);
        if(Point_IsZero(diff))
        {
            const Point nudge = {
                1000 * ((Util_Rand() % 1000) - 500),
                1000 * ((Util_Rand() % 1000) - 500),
            };
            return nudge;
        }
        if(Point_Mag(diff) < 20000) // XXX. Use unit width.
            return diff;
    }
    return zero;
}

static Point SeparateBoids(const Units units, Unit* const unit)
{
    static Point zero;
    Point out = zero;
    if(!State_IsDead(unit->state))
    {
        for(int32_t x = -1; x <= 1; x++)
        for(int32_t y = -1; y <= 1; y++)
        {
            const Point cart_offset = { x, y };
            const Point cart = Point_Add(unit->cart, cart_offset);
            const Stack stack = Units_GetStackCart(units, cart);
            for(int32_t i = 0; i < stack.count; i++)
            {
                Unit* const other = stack.reference[i];
                const Point force = Separate(unit, other);
                out = Point_Sub(out, force);
            }
        }
    }
    return Point_Div(out, 32);
}

static void ChaseBoids(const Units units, Unit* const unit, const Map map)
{
    if(!State_IsDead(unit->state))
    {
        Unit* closest = NULL;
        int64_t max = INT64_MAX;
        for(int32_t x = -1; x <= 1; x++)
        for(int32_t y = -1; y <= 1; y++)
        {
            const Point cart_offset = { x, y };
            const Point cart = Point_Add(unit->cart, cart_offset);
            const Stack stack = Units_GetStackCart(units, cart);
            for(int32_t i = 0; i < stack.count; i++)
            {
                Unit* const other = stack.reference[i];
                if(other->color != unit->color
                && !State_IsDead(other->state))
                {
                    const Point diff = Point_Sub(other->cell, unit->cell);
                    const int64_t mag = Point_Mag(diff);
                    if(mag < max)
                    {
                        max = mag;
                        closest = other;
                    }
                }
            }
        }
        if(closest != NULL)
            FindPath(units, unit, map, closest->cart, closest->cart_grid_offset);
    }
}

static Point AlignBoids(const Units units, Unit* const unit)
{
    static Point zero;
    Point out = zero;
    if(!State_IsDead(unit->state))
    {
        const Stack stack = Units_GetStackCart(units, unit->cart);
        if(stack.count > 1)
        {
            for(int32_t i = 0; i < stack.count; i++)
            {
                Unit* const other = stack.reference[i];
                if(!State_IsDead(other->state))
                    if(unit->id != other->id)
                        out = Point_Add(out, other->velocity);
            }
            out = Point_Div(out, stack.count - 1);
            return Point_Div(Point_Sub(out, unit->velocity), 8);
        }
    }
    return zero;
}

static Point WallPushBoids(const Units units, Unit* const unit, const Map map, const Grid grid)
{
    static Point zero;
    Point out = zero;
    if(!State_IsDead(unit->state))
    {
        const Point n = {  0, -1 }; // XXX. Rewrite this all with loops.
        const Point e = { +1,  0 };
        const Point s = {  0, +1 };
        const Point w = { -1,  0 };
        const Point n_point = Point_Add(unit->cart, n);
        const Point e_point = Point_Add(unit->cart, e);
        const Point s_point = Point_Add(unit->cart, s);
        const Point w_point = Point_Add(unit->cart, w);
        const bool n_walk = Units_CanWalk(units, map, n_point); // XXX. This is very heavy duty. Is there another way to nicely slide off walks eg. dot product?
        const bool e_walk = Units_CanWalk(units, map, e_point);
        const bool s_walk = Units_CanWalk(units, map, s_point);
        const bool w_walk = Units_CanWalk(units, map, w_point);
        const Point offset = Grid_GetCornerOffset(grid, unit->cart_grid_offset);
        const int32_t repulsion = 10 * unit->accel; // XXX. How strong should this be?
        const Point n_force = Point_Mul(n, repulsion);
        const Point e_force = Point_Mul(e, repulsion);
        const Point s_force = Point_Mul(s, repulsion);
        const Point w_force = Point_Mul(w, repulsion); // XXX. Boids are still jumping out of walls - need some sort of reset to put them back in their last good spot.
        const int32_t border = 10;
        if(!n_walk && offset.y < border) out = Point_Add(out, s_force);
        if(!w_walk && offset.x < border) out = Point_Add(out, e_force);
        if(!s_walk && offset.y > grid.tile_cart_height - border) out = Point_Add(out, n_force);
        if(!e_walk && offset.x > grid.tile_cart_width  - border) out = Point_Add(out, w_force);
    }
    return out;
}

// Boids, when swept up in a current of other boids, will
// try to go back to a path point if they were swept past the path point.
// This function ensures all boids on a tile share the same path index
// so the group acts like it guided by a single leader.

static void UnifyBoids(const Units units, Unit* const unit)
{
    if(!State_IsDead(unit->state))
    {
        const Stack stack = Units_GetStackCart(units, unit->cart);
        const int32_t max = Stack_GetMaxPathIndex(stack, unit->color, unit->command_group);
        for(int32_t i = 0; i < stack.count; i++)
        {
            Unit* const other = stack.reference[i];
            if(!State_IsDead(other->state))
                if(other->path.count > max && Unit_InPlatoon(unit, other))
                    other->path_index = max;
        }
    }
}

// Boids, when reaching their final destination, will struggle in
// a "mosh pit" like style to reach the final point in the grid tile.
// A simple solution is to stop all boids from reaching their final point
// within a grid tile by stopping all boids within the grid tile.

static void ConditionallyStopBoids(const Units units, Unit* const unit)
{
    if(!State_IsDead(unit->state))
    {
        const Stack stack = Units_GetStackCart(units, unit->cart);
        for(int32_t i = 0; i < stack.count; i++)
        {
            Unit* const other = stack.reference[i];
            if(!State_IsDead(other->state))
                if(unit->path.count == 0 && Unit_InPlatoon(unit, other))
                {
                    Unit_UpdateFileByState(unit, STATE_IDLE, false);
                    Unit_FreePath(other);
                }
        }
    }
}

static void CalculateBoidStressors(const Units units, Unit* const unit, const Map map, const Grid grid)
{
    if(!State_IsDead(unit->state))
    {
        const Point point[] = {
            CoheseBoids(units, unit),
            SeparateBoids(units, unit),
            AlignBoids(units, unit),
            WallPushBoids(units, unit, map, grid),
        };
        static Point zero;
        Point stressors = zero;
        for(int32_t j = 0; j < UTIL_LEN(point); j++)
            stressors = Point_Add(stressors, point[j]);
        unit->stressors = (Point_Mag(stressors) < 100) ? zero : stressors;
        if(Point_Mag(unit->stressors) < 500) // XXX. Whats a good threshold?
            unit->stressors = zero;
    }
}

static void Repath(const Units units, const Map map, Unit* const unit)
{
    if(!State_IsDead(unit->state) && unit->path_index_time > 100) // XXX, What is a good timeout time?
    {
        const Stack stack = Units_GetStackCart(units, unit->cart);
        if(unit->path.count > 0) // Unit must have a goal already.
        {
            const Point cart_goal = unit->path.point[unit->path.count - 1];
            for(int32_t j = 0; j < stack.count; j++)
            {
                Unit* const reference = stack.reference[j];
                if(reference->color == unit->color)
                    FindPath(units, reference, map, cart_goal, unit->cart_grid_offset_goal);
            }
        }
    }
}

// If any boid is stuck, chances are good a few surrounding boids
// are stuck too. This repath function will reroute all stuck boids on one tlie
// if one boid is stuck on that tile.
//
// DO NOT multithread.

static void RepathStuckBoids(const Units units, const Map map) // XXX. Causing segfaults.
{
    for(int32_t i = 0; i < units.count; i++)
    {
        Unit* const unit = &units.unit[i];
        Repath(units, map, unit);
    }
}

// DO NOT multithread.

static void Melee(Unit* const unit, Unit* const other)
{
    if(!State_IsDead(unit->state)
    && !State_IsDead(other->state))
    {
        const Point diff = Point_Sub(other->cell, unit->cell);
        if(Point_Mag(diff) < 30000) // XXX. Should be per unit.
        {
            unit->dir = Direction_CartToIso(Direction_GetCart(diff));
            Unit_UpdateFileByState(unit, STATE_ATTACK, false);
            other->health -= unit->attack;
            if(other->health <= 0)
                Unit_UpdateFileByState(other, STATE_FALL, true);
        }
    }
}

// DO NOT multithread.

static void FightBoids(const Units units, Unit* const unit)
{
    if(!State_IsDead(unit->state))
    {
        for(int32_t x = -1; x <= 1; x++)
        for(int32_t y = -1; y <= 1; y++)
        {
            const Point cart_offset = { x, y };
            const Point cart = Point_Add(unit->cart, cart_offset);
            const Stack stack = Units_GetStackCart(units, cart);
            for(int32_t i = 0; i < stack.count; i++)
            {
                Unit* const other = stack.reference[i];
                if(unit->color != other->color)
                    Melee(unit, other);
            }
        }
    }
}

// DO NOT multithread.

static void RunHardBoidRules(const Units units, const Map map)
{
    for(int32_t i = 0; i < units.count; i++) ConditionallyStopBoids(units, &units.unit[i]);
    for(int32_t i = 0; i < units.count; i++) UnifyBoids(units, &units.unit[i]);
    for(int32_t i = 0; i < units.count; i++) ChaseBoids(units, &units.unit[i], map);
    for(int32_t i = 0; i < units.count; i++) FightBoids(units, &units.unit[i]);
}

typedef struct
{
    Units units;
    Map map;
    Grid grid;
    int32_t a;
    int32_t b;
}
Needle;

static int32_t RunStressorNeedle(void* data)
{
    Needle* const needle = (Needle*) data;
    for(int32_t i = needle->a; i < needle->b; i++)
    {
        Unit* const unit = &needle->units.unit[i];
        CalculateBoidStressors(needle->units, unit, needle->map, needle->grid);
    }
    return 0;
}

static void BulkProcess(const Units units, const Map map, const Grid grid, int32_t Run(void* data))
{
    Needle* const needles = UTIL_ALLOC(Needle, units.cpu_count);
    SDL_Thread** const threads = UTIL_ALLOC(SDL_Thread*, units.cpu_count);
    UTIL_CHECK(needles);
    UTIL_CHECK(threads);
    const int32_t width = units.count / units.cpu_count;
    const int32_t remainder = units.count % units.cpu_count;
    for(int32_t i = 0; i < units.cpu_count; i++)
    {
        needles[i].units = units;
        needles[i].map = map;
        needles[i].grid = grid;
        needles[i].a = (i + 0) * width;
        needles[i].b = (i + 1) * width;
    }
    needles[units.cpu_count - 1].b += remainder;
    for(int32_t i = 0; i < units.cpu_count; i++) threads[i] = SDL_CreateThread(Run, "N/A", &needles[i]);
    for(int32_t i = 0; i < units.cpu_count; i++) SDL_WaitThread(threads[i], NULL);
    free(needles);
    free(threads);
}

static int32_t RunFlowNeedle(void* data)
{
    Needle* const needle = (Needle*) data;
    for(int32_t i = needle->a; i < needle->b; i++)
    {
        Unit* const unit = &needle->units.unit[i];
        if(!State_IsDead(unit->state))
        {
            Unit_Flow(unit, needle->grid);
            Unit_Move(unit, needle->grid);
            if(!Units_CanWalk(needle->units, needle->map, unit->cart))
                Unit_UndoMove(unit, needle->grid);
        }
    }
    return 0;
}

// See the boids pseudocode:
//   http://www.kfish.org/boids/pseudocode.html

static void FollowPathBoids(const Units units, const Grid grid, const Map map)
{
    BulkProcess(units, map, grid, RunStressorNeedle);
    BulkProcess(units, map, grid, RunFlowNeedle);
    RepathStuckBoids(units, map);
    RunHardBoidRules(units, map);
}

static void SortStacks(const Units units)
{
    for(int32_t y = 0; y < units.rows; y++)
    for(int32_t x = 0; x < units.cols; x++)
    {
        const Point point = { x, y };
        const Stack stack = Units_GetStackCart(units, point);
        Stack_Sort(stack);
    }
}

static void CalculateCenters(const Units units)
{
    for(int32_t y = 0; y < units.rows; y++)
    for(int32_t x = 0; x < units.cols; x++)
    {
        const Point point = { x, y };
        Stack* const stack = GetStack(units, point);
        Stack_UpdateCenterOfMass(stack);
    }
}

void Delete(const Units units, const Input input)
{
    if(input.key[SDL_SCANCODE_DELETE])
        for(int32_t i = 0; i < units.count; i++)
        {
            Unit* const unit = &units.unit[i];
            if(unit->selected
            && !State_IsDead(unit->state))
                Unit_UpdateFileByState(unit, STATE_FALL, true);
        }
}

static void Tick(const Units units)
{
    for(int32_t i = 0; i < units.count; i++)
    {
        Unit* const unit = &units.unit[i];
        unit->timer++;
    }
}

static void Decay(const Units units)
{
    for(int32_t i = 0; i < units.count; i++)
    {
        Unit* const unit = &units.unit[i];
        if(unit->state == STATE_FALL
        && unit->timer == 15 * ANIMATION_DIVISOR) // XXX: Reasonable to cheat with this hardcoding? Can get from Animation, but Tiles are only constructed with units on screen.
            Unit_UpdateFileByState(unit, STATE_DECAY, true);
    }
}

Units Units_Caretake(Units units, const Registrar graphics, const Overview overview, const Grid grid, const Input input, const Map map)
{
    Delete(units, input);
    FollowPathBoids(units, grid, map);
    ResetStacks(units);
    StackStacks(units);
    SortStacks(units);
    CalculateCenters(units);
    units = Select(units, overview, input, graphics);
    units = Command(units, overview, input, map);
    Tick(units);
    Decay(units);
    // XXX. Need a unit Remove() function to take unit off map when they are fully decayed.
    // Just sort and lower count value.
    return units;
}

bool Units_CanWalk(const Units units, const Map map, const Point point)
{
    const Terrain terrain = Map_GetTerrainFile(map, point);
    const Stack stack = Units_GetStackCart(units, point);
    return stack.reference != NULL // Out of bounds check.
        && Terrain_IsWalkable(terrain)
        && Stack_IsWalkable(stack);
}
