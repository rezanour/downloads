#pragma once

struct Vector2
{
    float x, y;

    Vector2() : x(0), y(0)
    {
    }

    Vector2(const Vector2& other) : x(other.x), y(other.y)
    {
    }

    Vector2(float x, float y) : x(x), y(y)
    {
    }

    Vector2(int x, int y) : x((float)x), y((float)y)
    {
    }

    Vector2(uint32_t x, uint32_t y) : x((float)x), y((float)y)
    {
    }

    // Basic methods
    float GetLengthSq() const
    {
        return x * x + y * y;
    }

    float GetLength() const
    {
        return sqrtf(GetLengthSq());
    }

    Vector2 GetNormalized() const
    {
        float invLength = 1.0f / GetLength();
        return Vector2(x * invLength, y * invLength);
    }

    void Normalize()
    {
        *this = GetNormalized();
    }

    // Returns true if both components are greater or equal
    bool StrictlyGreaterThanOrEqual(const Vector2& other) const
    {
        return x >= other.x && y >= other.y;
    }

    // General operators
    Vector2& operator += (const Vector2& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vector2& operator -= (const Vector2& other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    Vector2 operator -() const
    {
        return Vector2(-x, -y);
    }
};

// General operators
inline Vector2 operator+ (const Vector2& a, const Vector2& b)
{
    return Vector2(a.x + b.x, a.y + b.y);
}

inline Vector2 operator- (const Vector2& a, const Vector2& b)
{
    return Vector2(a.x - b.x, a.y - b.y);
}

inline Vector2 operator* (const Vector2& v, float s)
{
    return Vector2(v.x * s, v.y * s);
}

inline Vector2 operator* (float s, const Vector2& v)
{
    return Vector2(v.x * s, v.y * s);
}

// Returns a vector containing the max of each pair of components
inline Vector2 ComponentMax(const Vector2& a, const Vector2& b)
{
    return Vector2(max(a.x, b.x), max(a.y, b.y));
}

// Returns a vector containing the min of each pair of components
inline Vector2 ComponentMin(const Vector2& a, const Vector2& b)
{
    return Vector2(min(a.x, b.x), min(a.y, b.y));
}

inline Vector2 Abs(const Vector2& a)
{
    return Vector2(fabsf(a.x), fabsf(a.y));
}

// Returns dot product
inline float Dot(const Vector2& a, const Vector2& b)
{
    return a.x * b.x + a.y * b.y;
}

inline float Cross(const Vector2& a, const Vector2& b)
{
    return a.x * b.y - a.y * b.x;
}

// 2D Cross (perp) product with a given scale factor
inline Vector2 Cross(const Vector2& a, float s)
{
    return Vector2(s * a.y, -s * a.x);
}

// 2D Cross (perp) product with a given scale factor
inline Vector2 Cross(float s, const Vector2& a)
{
    return Vector2(-s * a.y, s * a.x);
}

