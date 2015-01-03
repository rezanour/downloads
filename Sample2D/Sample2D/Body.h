#pragma once

struct Shape;

struct Body
{
    Body();

    // Configure body parameters
    void Init(Shape* shape, float mass);
    void SetFixedRotation(bool value);

    void AddForce(const Vector2& force);
    void AddImpulse(const Vector2& impulse);

    // Linear properties
    Vector2 position;
    Vector2 velocity;
    Vector2 force;
    float mass, invMass;

    // Angular properties
    float rotation;
    float angularVelocity;
    float torque;
    float I, invI;      // Moment of inertia, similar to mass for angular motion

    float friction;
    Shape* shape;
};
