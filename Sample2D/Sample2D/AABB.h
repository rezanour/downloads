#pragma once

// Axis-aligned bounding box
struct AABB
{
    Vector2 Min;
    Vector2 Max;

    AABB()
    {
    }

    AABB(const Vector2& min, const Vector2& max) : Min(min), Max(max)
    {
    }

    AABB Translate(const Vector2& translation) const
    {
        return AABB(Min + translation, Max + translation);
    }
};
