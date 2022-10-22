/*
    @file pico_unit.h
    @brief Separating Axis Test (SAT) written in C99.

    ----------------------------------------------------------------------------
    Licensing information at end of header
    ----------------------------------------------------------------------------
*/

#ifndef PICO_SAT_H
#define PICO_SAT_H

#include <float.h>

#include "pico_math.h"

#ifndef PICO_SAT_MAX_POLY_VERTS
#define PICO_SAT_MAX_POLY_VERTS 8
#endif

typedef struct
{
    pm_v2  pos;
    pm_float radius;
} sat_circle_t;

typedef struct
{
    int   vertex_count;
    pm_v2 vertices[PICO_SAT_MAX_POLY_VERTS];
    pm_v2 normals[PICO_SAT_MAX_POLY_VERTS];
    pm_v2 edges[PICO_SAT_MAX_POLY_VERTS];
} sat_poly_t;

typedef struct
{
    pm_v2    normal;
    pm_float overlap;
} sat_manifold_t;

//later
//pm_b2 sat_polygon_to_aabb(const sat_polygon_t* poly);
//pm_b2 sat_circle_to_aabb(const sat_circle_t* circle);

sat_circle_t sat_make_circle(pm_v2 pos, pm_float radius);
sat_poly_t sat_make_polygon(int vertex_count, pm_v2 vertices[]);
sat_poly_t sat_aabb_to_poly(const pm_b2* aabb);

bool sat_test_poly_poly(const sat_poly_t* p1,
                        const sat_poly_t* p2,
                        sat_manifold_t* manifold);

bool sat_test_circle_poly(const sat_circle_t* c,
                          const sat_poly_t* p,
                          sat_manifold_t* manifold);

bool sat_test_poly_circle(const sat_poly_t* p,
                          const sat_circle_t* c,
                          sat_manifold_t* manifold);

bool sat_test_circle_circle(const sat_circle_t* c1,
                            const sat_circle_t* c2,
                            sat_manifold_t* manifold);

#endif // PICO_SAT_H

#ifdef PICO_SAT_IMPLEMENTATION

void sat_axis_range(const sat_poly_t* poly, pm_v2 normal, pm_float range[2])
{
    pm_float dot = pm_v2_dot(poly->vertices[0], normal);
    pm_float min = dot;
    pm_float max = dot;

    for (int i = 1; i < poly->vertex_count; i++)
    {
        dot = pm_v2_dot(poly->vertices[i], normal);

        if (dot < min)
            min = dot;

        if (dot > max)
            max = dot;
    }

    range[0] = min;
    range[1] = max;
}

pm_float sat_axis_overlap(const sat_poly_t* p1,
                          const sat_poly_t* p2,
                          pm_v2 axis)

{
    pm_float range1[2];
    pm_float range2[2];

    sat_axis_range(p1, axis, range1);
    sat_axis_range(p2, axis, range2);

    if (range1[1] < range2[0] || range2[1] < range1[0])
        return 0.0f;

    pm_float overlap1 = range1[1] - range2[0];
    pm_float overlap2 = range2[1] - range1[0];

    return (overlap2 > overlap1) ? overlap1 : -overlap2;
}

pm_v2 sat_ortho_projection(pm_v2 p, pm_v2 v1, pm_v2 v2, pm_float* t)
{
    pm_v2 e = pm_v2_sub(v2, v1);

    *t = pm_v2_dot(pm_v2_sub(p, v1), e) / pm_v2_dot(e, e);

    return pm_v2_add(v1, pm_v2_scale(e, *t));
}

sat_circle_t sat_make_circle(pm_v2 pos, pm_float radius)
{
    sat_circle_t circle;
    circle.pos = pos;
    circle.radius = radius;
    return circle;
}

sat_poly_t sat_make_poly(int vertex_count, pm_v2 vertices[])
{
    //assert(vertex_count <= PICO_SAT_MAX_POLY_VERTS);

    sat_poly_t poly;

    poly.vertex_count = vertex_count;

    for (int i = 0; i < vertex_count; i++)
    {
        poly.vertices[i] = vertices[i];
    }

    for (int i = 0; i < vertex_count; i++)
    {
        int next = (i + 1) == vertex_count ? 0 : i + 1;

        pm_v2 v1 = vertices[i];
        pm_v2 v2 = vertices[next];
        poly.edges[i] = pm_v2_sub(v2, v1);
        poly.normals[i] = pm_v2_perp(poly.edges[i]);
        poly.normals[i] = pm_v2_normalize(poly.normals[i]);
    }

    return poly;
}

sat_poly_t sat_aabb_to_poly(const pm_b2* aabb)
{
    pm_v2 pos = pm_b2_pos(aabb);
    pm_v2 size = pm_b2_size(aabb);

    pm_v2 vertices[] =
    {
        { pos.x, pos.y                   },
        { pos.x,          pos.y + size.y },
        { pos.x + size.x, pos.y + size.y },
        { pos.x + size.x, pos.y          }
    };

    return sat_make_poly(4, vertices);
}
void sat_update_manifold(sat_manifold_t* manifold, pm_v2 normal, pm_float overlap)
{
    pm_float abs_overlap = pm_abs(overlap);

    if (abs_overlap < manifold->overlap)
    {
        manifold->overlap = abs_overlap;

        if (overlap < 0.0f)
            manifold->normal = pm_v2_neg(normal);
        else if (overlap > 0.0f)
            manifold->normal = normal;
    }
}

int sat_support(const sat_poly_t* p, pm_v2 n)
{
    int max_index = 0;
    pm_float max_dot = pm_v2_dot(p->vertices[0], n);

    for (int i = 1; i < p->vertex_count; i++)
    {
        pm_float dot = pm_v2_dot(p->vertices[i], n);

        if (dot > max_dot)
        {
            max_index = i;
            max_dot = dot;
        }
    }

    return max_index;
}

bool sat_is_axis_separating(const sat_poly_t* p1, const sat_poly_t* p2)
{
    for (int i = 0; i < p1->vertex_count; i++)
    {
        pm_v2 n = p1->normals[i];
        pm_float c = pm_v2_dot(p1->vertices[i], n);
        int index = sat_support(p2, pm_v2_neg(n));
        pm_float d = pm_v2_dot(p2->vertices[index], n) - c;

        if (d >= 0)
            return true;
    }

    return false;
}

bool sat_test_poly_poly(const sat_poly_t* p1,
                        const sat_poly_t* p2,
                        sat_manifold_t* manifold)
{
    if (manifold)
    {
        manifold->overlap = FLT_MAX;
        manifold->normal  = pm_v2_zero();
    }

    if (sat_is_axis_separating(p1, p2)) return false;
    if (sat_is_axis_separating(p2, p1)) return false;

    return true;
}

typedef enum
{
    VORONOI_LEFT,
    VORONOI_RIGHT,
    VORONOI_MIDDLE
} sat_voronoi_region_t;

sat_voronoi_region_t sat_voronoi_region(pm_v2 point, pm_v2 line)
{
    pm_float len2 = pm_v2_len2(line);
    pm_float dot  = pm_v2_dot(point, line);

    if (dot < 0.0f)
        return VORONOI_LEFT;
    else if (dot > len2)
        return VORONOI_RIGHT;
    else
        return VORONOI_MIDDLE;
}

bool sat_test_poly_circle(const sat_poly_t* p,
                          const sat_circle_t* c,
                          sat_manifold_t* manifold)
{
    if (manifold)
    {
        manifold->overlap = FLT_MAX;
        manifold->normal  = pm_v2_zero();
    }

    pm_float radius2 = c->radius * c->radius;

    int count = p->vertex_count;

    for (int i = 0; i < count; i++)
    {
        int next = (i + 1) == count ? 0 : i + 1;
        int prev = (i - 1) <= 0 ? count - 1 : i - 1;

        pm_v2 normal = pm_v2_zero();
        pm_float overlap = FLT_MAX;

        pm_v2 edge = p->edges[i];
        pm_v2 point = pm_v2_sub(c->pos, p->vertices[i]);

        sat_voronoi_region_t region = sat_voronoi_region(point, edge);

        if (region == VORONOI_LEFT)
        {
            pm_v2 point2 = pm_v2_sub(c->pos, p->vertices[prev]);
            edge = p->edges[prev];

            region = sat_voronoi_region(point2, edge);

            if (region == VORONOI_RIGHT)
            {
                pm_float diff2 = pm_v2_len2(point);

                if (diff2 > radius2)
                    return false;

                if (manifold)
                {
                    pm_float diff = pm_sqrt(diff2);
                    overlap = c->radius - diff;
                    normal = pm_v2_normalize(point);
                }
            }
        }
        else if (region == VORONOI_RIGHT)
        {
            pm_v2 point2 = pm_v2_sub(c->pos, p->vertices[next]);
            edge = p->edges[next];

            region = sat_voronoi_region(point2, edge);

            if (region == VORONOI_LEFT)
            {
                pm_float diff2 = pm_v2_len2(point);

                if (diff2 > radius2)
                    return false;

                if (manifold)
                {
                    pm_float diff = pm_sqrt(diff2);
                    overlap = c->radius - diff;
                    normal = pm_v2_normalize(point);
                }
            }
        }
        else // VORONOI_MIDDLE
        {
            normal = p->normals[i];

            pm_float diff = pm_v2_dot(normal, point);
            pm_float abs_diff = pm_abs(diff);

            if (diff > 0.0f && abs_diff > c->radius)
                return false;

            overlap = c->radius - diff;
        }

        if (manifold)
            sat_update_manifold(manifold, normal, overlap);
    }

    return true;
}

#endif // PICO_SAT_IMPLEMENTATION

