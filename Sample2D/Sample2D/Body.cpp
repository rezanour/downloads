#include "Precomp.h"
#include "Body.h"
#include "Shape.h"

Body::Body()
    : mass(FLT_MAX)
    , invMass(0.0f)
    , rotation(0.0f)
    , angularVelocity(0.0f)
    , torque(0.0f)
    , I(FLT_MAX)
    , invI(0.0f)
    , friction(0.2f)
    , shape(nullptr)
{
}

void Body::Init(Shape* shape, float mass)
{
    this->shape = shape;
    this->mass = mass;

    if (mass < FLT_MAX)
    {
        invMass = 1.0f / mass;
        I = shape->ComputeI(mass);
        invI = 1.0f / I;
    }
    else
    {
        // approx infinite mass (immovable)
        invMass = 0.0f;
        I = FLT_MAX;
        invI = 0.0f;
    }
}

void Body::SetFixedRotation(bool value)
{
    if (value)
    {
        I = FLT_MAX;
        invI = 0.0f;
    }
    else
    {
        I = shape->ComputeI(mass);
        invI = 1.0f / I;
    }
}

void Body::AddForce(const Vector2& force)
{
    this->force += force;
}

void Body::AddImpulse(const Vector2& impulse)
{
    velocity += invMass * impulse;
}