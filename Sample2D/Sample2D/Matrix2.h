#pragma once

// Simple 2x2 matrix
struct Matrix2
{
    Matrix2()
    {
    }

    Matrix2(float angle)
    {
        float c = cosf(angle);
        float s = sinf(angle);
        col1.x = c; col2.x = -s;
        col1.y = s; col2.y = c;
    }

    Matrix2(const Vector2& col1, const Vector2& col2) : col1(col1), col2(col2) {}

    Matrix2 Transpose() const
    {
        return Matrix2(Vector2(col1.x, col2.x), Vector2(col1.y, col2.y));
    }

    Matrix2 Invert() const
    {
        float a = col1.x, b = col2.x, c = col1.y, d = col2.y;
        Matrix2 B;
        float det = a * d - b * c;
        _ASSERT(det != 0.0f);
        det = 1.0f / det;
        B.col1.x = det * d;	B.col2.x = -det * b;
        B.col1.y = -det * c;	B.col2.y = det * a;
        return B;
    }

    Vector2 col1, col2;
};

inline Vector2 operator * (const Matrix2& A, const Vector2& v)
{
    return Vector2(A.col1.x * v.x + A.col2.x * v.y, A.col1.y * v.x + A.col2.y * v.y);
}

inline Matrix2 operator + (const Matrix2& A, const Matrix2& B)
{
    return Matrix2(A.col1 + B.col1, A.col2 + B.col2);
}

inline Matrix2 operator * (const Matrix2& A, const Matrix2& B)
{
    return Matrix2(A * B.col1, A * B.col2);
}

inline Matrix2 Abs(const Matrix2& A)
{
    return Matrix2(Abs(A.col1), Abs(A.col2));
}
