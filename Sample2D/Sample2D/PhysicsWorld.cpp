#include "Precomp.h"
#include "PhysicsWorld.h"
#include "Body.h"
#include "Manifold.h"

PhysicsWorld::PhysicsWorld(const Vector2& gravity, int iterations)
    : gravity(gravity)
    , iterations(iterations)
{
}

void PhysicsWorld::Add(Body* body)
{
    bodies.push_back(body);
}

void PhysicsWorld::Update(float dt)
{
    float invDt = dt > 0.0f ? 1.0f / dt : 0.0f;

    // Determine overlapping bodies and update contact points
    UpdateManifolds();

    // Integrate forces to obtain updated velocities
    for (auto& body : bodies)
    {
        if (body->invMass == 0.0f)
            continue;

        body->velocity += dt * (gravity + body->invMass * body->force);
        body->angularVelocity += dt * body->invI * body->torque;
    }

    // Do all one time init for the manifolds
    for (auto& manifold : manifolds)
    {
        manifold.second.PreStep(invDt);
    }

    // Sequential Impulse (SI) loop. See Erin Catto's GDC slides for SI info
    for (int i = 0; i < iterations; ++i)
    {
        for (auto& manifold : manifolds)
        {
            manifold.second.ApplyImpulse();
        }
    }

    // Integrate new velocities to obtain final state vector (position, rotation).
    // Also clear out any forces in preparation for the next frame
    for (auto& body : bodies)
    {
        body->position += dt * body->velocity;
        body->rotation += dt * body->angularVelocity;

        body->force = Vector2(0, 0);
        body->torque = 0.0f;
    }
}

void PhysicsWorld::UpdateManifolds()
{
    // TODO: implement some sort of broadphase if we want to support more than
    // a trivial number of bodies. For now, just brute force them all!

    // For every body...
    for (int i = 0; i < (int)bodies.size(); ++i)
    {
        Body* b1 = bodies[i];

        // Check all bodies after them in the list (since we already were checked
        // by bodies in front of us in an earlier iteration)
        for (int j = i + 1; j < (int)bodies.size(); ++j)
        {
            Body* b2 = bodies[j];

            // If both are completely immovable, nothing to do
            if (b1->invMass == 0.0f && b2->invMass == 0.0f)
                continue;

            // Define a manifold between them. This checks for contacts
            Manifold manifold(b1, b2);

            // Unique key identifying the pair
            PairKey key(b1, b2);

            if (manifold.numContacts > 0)
            {
                // If there rare contacts, try and find the manifold in our existing list
                auto iter = manifolds.find(key);
                if (iter == manifolds.end())
                {
                    // No? then add it
                    manifolds.insert(std::make_pair(key, manifold));
                }
                else
                {
                    // Yes? Update the existing info
                    iter->second.Update(manifold.contacts, manifold.numContacts);
                }
            }
            else
            {
                // No contacts? Remove the manifold from our list completely
                manifolds.erase(key);
            }
        }
    }
}
