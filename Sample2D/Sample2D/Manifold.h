#pragma once

struct Body;

/// NOTE: Initially, this code only tracked contacts, not full manifolds.
/// However, due to instability of box/box stacking I ended up borrowing Box2D_Lite's
/// feature-clipping collision check, which returns up to 2 points at once, and so I just
/// threw something together that's somewhere in between Box2D_Lite's Arbiter (which is
/// just the same as what I call a manifold) and a normal iterative manifold which is built up
/// frame to frame (similar to what Bullet used last I checked, which was a while ago and may have changed).
/// With some further modification, this will allow us to build up contacts later even when we add something
/// like GJK+EPA instead of using the one-off collision routines like we have now.


// Used by the feature-clipping function borrowed from Box2D_Lite
// if you enable that define. For other paths, .value is used as a
// loose unique ID for the contact, so we know if it's new or the same
// as an existing one
union FeaturePair
{
    struct Edges
    {
        char inEdge1;
        char outEdge1;
        char inEdge2;
        char outEdge2;
    } e;
    int value;
};

struct Contact
{
    Contact();

    Vector2 position;
    Vector2 normal;
    Vector2 r1, r2;     // Vector from center of each object to the contact point
    float separation;
    float Pn;           // accum. normal impulse
    float Pt;           // accum. tangent impulse
    float Pnb;          // accum. normal impulse for position bias
    float massNormal, massTangent;  // mass along normal & tangent
    float bias;
    FeaturePair feature;
};

// Used to uniquely identify a pair of objects. We implement a < operator
// for this type, so that it can be used efficiently as the key of std::map
struct PairKey
{
    PairKey(Body* a, Body* b);

    Body* body1;
    Body* body2;
};

inline bool operator < (const PairKey& a, const PairKey& b)
{
    if (a.body1 < b.body1)
    {
        return true;
    }

    if (a.body1 == b.body1 && a.body2 < b.body2)
    {
        return true;
    }

    return false;
}

// A contact manifold tracks contacts between a single pair of objects
struct Manifold
{
    static const int MaxPoints = 2;

    Manifold(Body* a, Body* b);

    // Add/update the manifold with a list of (possibly new) contacts
    void Update(Contact* contacts, int numContacts);

    // Prep the manifold for the frame, prior to beginning iteration.
    // Allows 1 time calculations to happen once
    void PreStep(float invDt);
    void ApplyImpulse();

    Contact contacts[MaxPoints];
    int numContacts;

    Body* body1;
    Body* body2;

    // Combined friction
    float friction;
};

// Detects collisions between body1 & body2, placing up to 2 contacts
// into the contacts array out parameter, and returning the numContacts as return value.
int Collide(Body* body1, Body* body2, Contact* contacts);
