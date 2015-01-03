#pragma once


#include "Manifold.h"

struct Body;

class PhysicsWorld
{
public:
    PhysicsWorld(const Vector2& gravity, int iterations);

    void Add(Body* body);

    void Update(float dt);

private:
    void UpdateManifolds();

    std::vector<Body*> bodies;
    std::map<PairKey, Manifold> manifolds;
    Vector2 gravity;
    int iterations;
};
