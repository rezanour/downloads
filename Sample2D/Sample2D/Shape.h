#pragma once

enum class ShapeType
{
    Box = 0,
    Circle
};

struct Shape
{
    const ShapeType type;

    // Compute moment of inertia for the shape, given mass.
    // NOTE: http://en.wikipedia.org/wiki/List_of_moments_of_inertia contains
    // a list of formulas for common shapes
    virtual float ComputeI(float mass) const = 0;

    // Find vertex along support direction
    virtual Vector2 GetSupportV(const Vector2& dir, float margin, int* vertexId) const = 0;

protected:
    // Force this to be only base class by making default ctor protected
    Shape(ShapeType type);

private:
    // Prevent copy
    Shape(const Shape&);
    Shape& operator= (const Shape&);
};

struct BoxShape : public Shape
{
    BoxShape();
    BoxShape(float w, float h);

    // Total distance across along x & y
    Vector2 width;

    float ComputeI(float mass) const override;
    Vector2 GetSupportV(const Vector2& dir, float margin, int* vertexId) const override;
};

struct CircleShape : public Shape
{
    CircleShape();
    CircleShape(float radius);

    float radius;

    float ComputeI(float mass) const override;
    Vector2 GetSupportV(const Vector2& dir, float margin, int* vertexId) const override;
};
