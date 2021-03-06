﻿/*
 * File: math-euclidean.hpp
 * The name of this file might be slightly odd, but it defines commonly used geometric
 * primitives such as axis-aligned bounding boxes in 2D and 3D, spheres, planes, segments,
 * lines, and frustums. Although the functionality is not comprehensive between all
 * primitive types, generally most of the common intersection types are provided. 
 */

#pragma once

#ifndef math_euclidean_hpp
#define math_euclidean_hpp

#include "math-common.hpp"

namespace avl
{
    /////////////////////////////////
    // Axis-Aligned Bounding Areas //
    /////////////////////////////////

    struct Bounds2D
    {
        float2 _min = { 0, 0 };
        float2 _max = { 0, 0 };

        Bounds2D() {}
        Bounds2D(float2 min, float2 max) : _min(min), _max(max) {}
        Bounds2D(float x0, float y0, float x1, float y1) { _min.x = x0; _min.y = y0; _max.x = x1; _max.y = y1; }

        float2 min() const { return _min; }
        float2 max() const { return _max; }

        float2 size() const { return max() - min(); }
        float2 center() const { return{ (_min.x + _max.y) / 2, (_min.y + _max.y) / 2 }; }
        float area() const { return (_max.x - _min.x) * (_max.y - _min.y); }

        float width() const { return _max.x - _min.x; }
        float height() const { return _max.y - _min.y; }

        bool contains(const float px, const float py) const { return px >= _min.x && py >= _min.y && px < _max.x && py < _max.y; }
        bool contains(const float2 & point) const { return contains(point.x, point.y); }

        bool intersects(const Bounds2D & other) const
        {
            if ((_min.x <= other._min.x) && (_max.x >= other._max.x) &&
                (_min.y <= other._min.y) && (_max.y >= other._max.y)) return true;
            return false;
        }
    };

    inline std::ostream & operator << (std::ostream & o, const Bounds2D & b)
    {
        return o << "{" << b.min() << " to " << b.max() << "}";
    }

    struct Bounds3D
    {
        float3 _min = { 0, 0, 0 };
        float3 _max = { 0, 0, 0 };

        Bounds3D() {}
        Bounds3D(float3 min, float3 max) : _min(min), _max(max) {}
        Bounds3D(float x0, float y0, float z0, float x1, float y1, float z1) { _min.x = x0; _min.y = y0; _min.z = z0; _max.x = x1; _max.y = y1; _max.z = z1; }

        float3 min() const { return _min; }
        float3 max() const { return _max; }

        float3 size() const { return _max - _min; }
        float3 center() const { return (_min + _max) * 0.5f; }
        float volume() const { return (_max.x - _min.x) * (_max.y - _min.y) * (_max.z - _min.z); }

        float width() const { return _max.x - _min.x; }
        float height() const { return _max.y - _min.y; }
        float depth() const { return _max.z - _min.z; }

        bool contains(float3 point) const
        {
            if (point.x < _min.x || point.x > _max.x) return false;
            if (point.y < _min.y || point.y > _max.y) return false;
            if (point.z < _min.z || point.z > _max.z) return false;
            return true;
        }

        bool intersects(const Bounds3D & other) const
        {
            if ((_min.x <= other._min.x) && (_max.x >= other._max.x) &&
                (_min.y <= other._min.y) && (_max.y >= other._max.y) &&
                (_min.z <= other._min.z) && (_max.z >= other._max.z)) return true;
            return false;
        }

        // Given a plane through the origin with a normal, returns the corner closest to the plane.
        float3 get_negative(const float3 & normal) const
        {
            float3 result = min();
            const float3 s = size();
            if (normal.x < 0) result.x += s.x;
            if (normal.y < 0) result.y += s.y;
            if (normal.z < 0) result.z += s.z;
            return result;
        }

        // Given a plane through the origin with a normal, returns the corner farthest from the plane.
        float3 get_positive(const float3 & normal) const
        {
            float3 result = min();
            const float3 s = size();
            if (normal.x > 0) result.x += s.x;
            if (normal.y > 0) result.y += s.y;
            if (normal.z > 0) result.z += s.z;
            return result;
        }

        void surround(const float3 & p)
        {
            _min = linalg::min(_min, p);
            _max = linalg::max(_max, p);
        }

        void surround(const Bounds3D & other)
        {
            _min = linalg::min(_min, other._min);
            _max = linalg::max(_max, other._max);
        }

        uint32_t maximum_extent() const
        {
            auto d = _max - _min;
            if (d.x > d.y && d.x > d.z) return 0;
            else if (d.y > d.z) return 1;
            else return 2;
        }

        Bounds3D add(const Bounds3D & other) const
        {
            Bounds3D result;
            result._min = linalg::min(_min, other._min);
            result._max = linalg::max(_max, other._max);
            return result;
        }
    };

    inline std::ostream & operator << (std::ostream & o, const Bounds3D & b)
    {
        return o << "{" << b.min() << " to " << b.max() << "}";
    }
   
    ////////////////
    //   Sphere   //
    ////////////////

    static const float SPHERE_EPSILON = 0.0001f;
    
    struct Sphere
    {
        float3 center;
        float radius;
        
        Sphere() {}
        Sphere(const float3 & center, float radius) : center(center), radius(radius) {}
    };

    // Makes use of the "bouncing bubble" solution to the minimal enclosing ball problem. Runs in O(n).
    // http://stackoverflow.com/questions/17331203/bouncing-bubble-algorithm-for-smallest-enclosing-sphere
    inline Sphere compute_enclosing_sphere(const std::vector<float3> & vertices, float minRadius = SPHERE_EPSILON)
    {
        if (vertices.size() > 3) return Sphere();
        if (minRadius < SPHERE_EPSILON) minRadius = SPHERE_EPSILON;

        Sphere s;

        for (int t = 0; t < 2; t++)
        {
            for (const auto & v : vertices)
            {
                const float distSqr = length2(v - s.center);
                const float radSq = s.radius * s.radius;
                if (distSqr > radSq)
                {
                    const float p = std::sqrt(distSqr) / s.radius;
                    const float p_inv = 1.f / p;
                    const float p_inv_sqr = p_inv * p_inv;
                    s.radius = 0.5f * (p + p_inv) * s.radius;
                    s.center = ((1.f + p_inv_sqr)*s.center + (1.f - p_inv_sqr) * v) / 2.f;
                }
            }
        }

        for (const auto & v : vertices)
        {
            const float distSqr = length2(v - s.center);
            const float radSqr = s.radius * s.radius;
            if (distSqr > radSqr)
            {
                const float dist = std::sqrt(distSqr);
                s.radius = (s.radius + dist) / 2.0f;
                s.center += (v - s.center) * (dist - s.radius) / dist;
            }
        }

        return s;
    }

    ///////////////
    //   Plane   //
    ///////////////
    
    static const float PLANE_EPSILON = 0.0001f;
    
    struct Plane
    {
        float4 equation = { 0, 0, 0, 0 }; // ax * by * cz + d form (xyz normal, w distance)
        Plane() {}
        Plane(const float4 & equation) : equation(equation) {}
        Plane(const float3 & normal, const float & distance) { equation = float4(normal.x, normal.y, normal.z, distance); }
        Plane(const float3 & normal, const float3 & point) { equation = float4(normal.x, normal.y, normal.z, -dot(normal, point)); }
        float3 get_normal() const { return equation.xyz(); }
        bool is_negative_half_space(const float3 & point) const { return (dot(get_normal(), point) < equation.w); }; // +eq.w?
        bool is_positive_half_space(const float3 & point) const { return (dot(get_normal(), point) > equation.w); };
        void normalize() { float n = 1.0f / length(get_normal()); equation *= n; };
        float get_distance() const { return equation.w; }
        float distance_to(const float3 & point) const { return dot(get_normal(), point) + equation.w; };
        bool contains(const float3 & point) const { return std::abs(distance_to(point)) < PLANE_EPSILON; };
        float3 reflect_coord(const float3 & c) const { return get_normal() * distance_to(c) * -2.f + c; }
        float3 reflect_vector(const float3 & v) const { return get_normal() * dot(get_normal(), v) * 2.f - v; }
    };

    inline Plane transform_plane(const float4x4 & transform, const Plane & p)
    {
        const float3 normal = transform_vector(transform, p.get_normal());
        const float3 point_on_plane = transform_coord(transform, p.get_distance() * p.get_normal());
        return Plane(normal, point_on_plane);
    }

    inline float3 get_plane_point(const Plane & p)
    {
        return -1.0f * p.get_distance() * p.get_normal();
    }

    inline float3 plane_intersection(const Plane & a, const Plane & b, const Plane & c)
    {
        const float3 p1 = get_plane_point(a);
        const float3 p2 = get_plane_point(b);
        const float3 p3 = get_plane_point(c);

        const float3 n1 = a.get_normal();
        const float3 n2 = b.get_normal();
        const float3 n3 = c.get_normal();

        const float det = dot(n1, cross(n2, n3));

        return (dot(p1, n1) * cross(n2, n3) + dot(p2, n2) * cross(n3, n1) + dot(p3, n3) * cross(n1, n2)) / det;
    }

    ////////////////////////////
    //   Lines and Segments   //
    ////////////////////////////
    
    struct Segment
    {
        float3 a, b;
        Segment(const float3 & first, const float3 & second) : a(a), b(b) {}
        float3 get_direction() const { return safe_normalize(b - a); };
    };

    struct Line
    {
        float3 origin, direction;
        Line(const float3 & origin, const float3 & direction) : origin(origin), direction(direction) {}
    };

    inline float3 closest_point_on_segment(const float3 & point, const Segment & s)
    {
        const float length = distance(s.a, s.b);
        const float3 dir = (s.b - s.a) / length;
        const float d = dot(point - s.a, dir);
        if (d <= 0.f) return s.a;
        if (d >= length) return s.b;
        return s.a + dir * d;
    }

    inline Line plane_intersection(const Plane & p1, const Plane & p2)
    {
        const float ndn = dot(p1.get_normal(), p2.get_normal());
        const float recDeterminant = 1.f / (1.f - (ndn * ndn));
        const float c1 = (-p1.get_distance() + (p2.get_distance() * ndn)) * recDeterminant;
        const float c2 = (-p2.get_distance() + (p1.get_distance() * ndn)) * recDeterminant;
        return Line((c1 * p1.get_normal()) + (c2 * p2.get_normal()), normalize(cross(p1.get_normal(), p2.get_normal())));
    }

    /////////////////////////////////
    // Object-Object intersections //
    /////////////////////////////////
    
    inline float3 intersect_line_plane(const Line & l, const Plane & p)
    {
        const float d = dot(l.direction, p.get_normal());
        const float distance = p.distance_to(l.origin) / d;
        return (l.origin - (distance * l.direction));
    }

    /////////////
    // Frustum //
    /////////////

    enum FrustumPlane { RIGHT, LEFT, BOTTOM, TOP, NEAR, FAR };

    struct Frustum
    {
        // frustum normals point inward
        Plane planes[6];

        Frustum()
        {
            planes[FrustumPlane::RIGHT] = Plane({ -1, 0, 0 }, 1.f);
            planes[FrustumPlane::LEFT] = Plane({ +1, 0, 0 }, 1.f);
            planes[FrustumPlane::BOTTOM] = Plane({ 0, +1, 0 }, 1.f);
            planes[FrustumPlane::TOP] = Plane({ 0, -1, 0 }, 1.f);
            planes[FrustumPlane::NEAR] = Plane({ 0, 0, +1 }, 1.f);
            planes[FrustumPlane::FAR] = Plane({ 0, 0, -1 }, 1.f);
        }

        Frustum(const float4x4 & viewProj)
        {
            // See "Fast Extraction of Viewing Frustum Planes from the WorldView-Projection Matrix" by Gil Gribb and Klaus Hartmann
            for (int i = 0; i < 4; ++i) planes[FrustumPlane::RIGHT].equation[i] = viewProj[i][3] - viewProj[i][0];
            for (int i = 0; i < 4; ++i) planes[FrustumPlane::LEFT].equation[i] = viewProj[i][3] + viewProj[i][0];
            for (int i = 0; i < 4; ++i) planes[FrustumPlane::BOTTOM].equation[i] = viewProj[i][3] + viewProj[i][1];
            for (int i = 0; i < 4; ++i) planes[FrustumPlane::TOP].equation[i] = viewProj[i][3] - viewProj[i][1];
            for (int i = 0; i < 4; ++i) planes[FrustumPlane::NEAR].equation[i] = viewProj[i][3] + viewProj[i][2];
            for (int i = 0; i < 4; ++i) planes[FrustumPlane::FAR].equation[i] = viewProj[i][3] - viewProj[i][2];
            for (auto & p : planes) p.normalize();
        }

        // A point is within the frustum if it is in front of all six planes simultaneously.
        // Returns true if point is within the frustum.
        bool contains(const float3 & point) const
        {
            for (int p = 0; p < 6; p++)
            {
                if (planes[p].distance_to(point) <= PLANE_EPSILON) return false;
            }
            return true;
        }

        // Returns true if the sphere is fully contained within the frustum. 
        bool contains(const float3 & center, const float radius) const
        {
            for (int p = 0; p < 6; p++)
            {
                if (planes[p].distance_to(center) < radius) return false;
            }
            return true;
        }

        // Returns true if the box is fully contained within the frustum.
        bool contains(const float3 & center, const float3 & size) const
        {
            const float3 half = size * 0.5f;
            const Bounds3D box(float3(center - half), float3(center + half));
            for (int p = 0; p < 6; p++)
            {
                if (planes[p].distance_to(box.get_positive(planes[p].get_normal())) < 0.f) return false;
                else if (planes[p].distance_to(box.get_negative(planes[p].get_normal())) < 0.f) return false;
            }
            return true;
        }

        // Returns true if a sphere is fully or partially contained within the frustum.
        bool intersects(const float3 & center, const float radius) const
        {
            for (int p = 0; p < 6; p++)
            {
                if (planes[p].distance_to(center) <= -radius) return false;
            }
            return true;
        }

        // Returns true if the box is fully or partially contained within the frustum.
        bool intersects(const float3 & center, const float3 & size) const
        {
            const float3 half = size * 0.5f;
            const Bounds3D box(float3(center - half), float3(center + half));
            for (int p = 0; p < 6; p++)
            {
                if (planes[p].distance_to(box.get_positive(planes[p].get_normal())) < 0.f) return false;
            }
            return true;
        }

    };

    inline std::array<float3, 8> make_frustum_corners(const Frustum & f)
    {
        std::array<float3, 8> corners;

        corners[0] = plane_intersection(f.planes[FrustumPlane::FAR],  f.planes[FrustumPlane::TOP],      f.planes[FrustumPlane::LEFT]);
        corners[1] = plane_intersection(f.planes[FrustumPlane::FAR],  f.planes[FrustumPlane::BOTTOM],   f.planes[FrustumPlane::RIGHT]);
        corners[2] = plane_intersection(f.planes[FrustumPlane::FAR],  f.planes[FrustumPlane::BOTTOM],   f.planes[FrustumPlane::LEFT]);
        corners[3] = plane_intersection(f.planes[FrustumPlane::FAR],  f.planes[FrustumPlane::TOP],      f.planes[FrustumPlane::RIGHT]);
        corners[4] = plane_intersection(f.planes[FrustumPlane::NEAR], f.planes[FrustumPlane::TOP],      f.planes[FrustumPlane::LEFT]);
        corners[5] = plane_intersection(f.planes[FrustumPlane::NEAR], f.planes[FrustumPlane::BOTTOM],   f.planes[FrustumPlane::RIGHT]);
        corners[6] = plane_intersection(f.planes[FrustumPlane::NEAR], f.planes[FrustumPlane::BOTTOM],   f.planes[FrustumPlane::LEFT]);
        corners[7] = plane_intersection(f.planes[FrustumPlane::NEAR], f.planes[FrustumPlane::TOP],      f.planes[FrustumPlane::RIGHT]);

        return corners;
    }

}

#endif // end math_euclidean_hpp
