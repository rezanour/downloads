#include "Precomp.h"
#include "Shape.h"

Shape::Shape(ShapeType type)
    : type(type)
{
}

BoxShape::BoxShape()
    : Shape(ShapeType::Box)
    , width(1, 1)
{
}

BoxShape::BoxShape(float w, float h)
    : Shape(ShapeType::Box)
    , width(w, h)
{
}

float BoxShape::ComputeI(float mass) const
{
    // I for box is computed as m * (w^2 + h^2) / 12
    return mass * (width.x * width.x + width.y * width.y) / 12.0f;
}

Vector2 BoxShape::GetSupportV(const Vector2& dir, float margin, int* vertexId) const
{
    // Assumes dir is normalized
    if (vertexId) *vertexId = 0;
    Vector2 result;
    if (dir.x >= 0)
    {
        if (vertexId) *vertexId = *vertexId + 1;
        result.x = width.x * 0.5f + margin;
    }
    else
    {
        result.x = -width.x * 0.5f - margin;
    }

    if (dir.y >= 0)
    {
        if (vertexId) *vertexId = *vertexId + 2;
        result.y = width.y * 0.5f + margin;
    }
    else
    {
        result.y = -width.y * 0.5f - margin;
    }
    return result;
}

CircleShape::CircleShape()
    : Shape(ShapeType::Circle)
    , radius(0.5f)
{
}

CircleShape::CircleShape(float radius)
    : Shape(ShapeType::Circle)
    , radius(radius)
{
}

float CircleShape::ComputeI(float mass) const
{
    return mass * (radius * radius) / 4.0f;
}

Vector2 CircleShape::GetSupportV(const Vector2& dir, float margin, int* vertexId) const
{
    // Assumes dir is normalized
    if (vertexId) *vertexId = 0;
    return dir * (radius + margin);
}
