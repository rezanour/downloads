#include "Precomp.h"
#include "PhysicsWorld.h"
#include "Manifold.h"
#include "Body.h"
#include "Shape.h"

Contact::Contact()
    : separation(0.0f)
    , Pn(0.0f)
    , Pt(0.0f)
    , Pnb(0.0f)
    , massNormal(0.0f)
    , massTangent(0.0f)
    , bias(0.0f)
{
}

PairKey::PairKey(Body* a, Body* b)
{
    if (a < b)
    {
        body1 = a;
        body2 = b;
    }
    else
    {
        body1 = b;
        body2 = a;
    }
}

Manifold::Manifold(Body* a, Body* b)
{
    if (a < b)
    {
        body1 = a;
        body2 = b;
    }
    else
    {
        body1 = b;
        body2 = a;
    }

    // Get initial contact list
    numContacts = Collide(body1, body2, contacts);

    // Not sure if this is actually accurate. I was just using the min of their
    // frictions before, but that wasn't giving very good results. Borrowed
    // this from Box2D_Lite and it seems to work better than what I had.
    friction = sqrtf(body1->friction * body2->friction);
}

void Manifold::Update(Contact* newContacts, int numNewContacts)
{
    Contact mergedContacts[MaxPoints];

    for (int i = 0; i < numNewContacts; ++i)
    {
        Contact* newContact = &newContacts[i];

        int k = -1;
        for (int j = 0; j < numContacts; ++j)
        {
            Contact* oldContact = &contacts[j];
            if (newContact->feature.value == oldContact->feature.value)
            {
                k = j;
                break;
            }
        }

        if (k > -1)
        {
            mergedContacts[i] = *newContact;
            Contact* c = &mergedContacts[i];
            Contact* oldContact = &contacts[k];

            // Warm start with the previous impulse values
            c->Pn = oldContact->Pn;
            c->Pt = oldContact->Pt;
            c->Pnb = oldContact->Pnb;
        }
        else
        {
            // Accept new contact as new
            mergedContacts[i] = newContacts[i];
        }
    }

    // Copy merged list of contacts into our main list
    for (int i = 0; i < numNewContacts; ++i)
    {
        contacts[i] = mergedContacts[i];
    }

    numContacts = numNewContacts;
}

void Manifold::PreStep(float invDt)
{
    static const float Slop = 0.01f;
    static const float BiasFactor = 0.2f;

    for (int i = 0; i < numContacts; ++i)
    {
        Contact* contact = &contacts[i];

        // Vectors from each object's center to the contact point
        Vector2 r1 = contact->position - body1->position;
        Vector2 r2 = contact->position - body2->position;

        // Find how much of each r is along contact normal
        float rn1 = Dot(r1, contact->normal);
        float rn2 = Dot(r2, contact->normal);

        // To compute effective inverseMass along contact normal,
        // start with the linear masses.
        float kNormal = body1->invMass + body2->invMass;

        // Then, to account for rotational moment of inertia, we need
        // to apply the square of the amount of r perpendicular to the normal.
        // See http://chrishecker.com/images/e/e7/Gdmphys3.pdf for a good discussion
        // of how we arrive at this. We can either find the perp & dot it with the
        // normal directly (as Chris does in the pdf linked), or we can use
        // pythagorean theorem and subtract the edge rn^2 from hypotenus r^2 we
        // already have to find the value as below.
        kNormal +=
            body1->invI * (Dot(r1, r1) - rn1 * rn1) +
            body2->invI * (Dot(r2, r2) - rn2 * rn2);

        // The impulse computation actually needs the inverse of these values, so invert here
        contact->massNormal = kNormal > 0.0f ? 1.0f / kNormal : 0.0f;

        // Now, do the exact same thing for tangent
        Vector2 tangent = Cross(contact->normal, 1.0f);
        float rt1 = Dot(r1, tangent);
        float rt2 = Dot(r2, tangent);
        float kTangent = body1->invMass + body2->invMass;
        kTangent +=
            body1->invI * (Dot(r1, r1) - rt1 * rt1) +
            body2->invI * (Dot(r2, r2) - rt2 * rt2);
        contact->massTangent = kTangent > 0.0f ? 1.0f / kTangent : 0.0f;

        // The bias is an additional boost to the impulse to compensate for already penetrating
        // objects to resolve the penetration more quickly
        contact->bias = -BiasFactor * invDt * min(0.0f, contact->separation + Slop);

        // Apply normal + friction impulse
        Vector2 totalImpulse = contact->Pn * contact->normal + contact->Pt * tangent;

        body1->velocity -= body1->invMass * totalImpulse;
        body1->angularVelocity -= body1->invI * Cross(r1, totalImpulse);

        body2->velocity += body2->invMass * totalImpulse;
        body2->angularVelocity += body2->invI * Cross(r2, totalImpulse);
    }
}

void Manifold::ApplyImpulse()
{
    for (int i = 0; i < numContacts; ++i)
    {
        Contact* contact = &contacts[i];

        contact->r1 = contact->position - body1->position;
        contact->r2 = contact->position - body2->position;

        //
        // Do all computations for normal first
        //

        // Relative velocity at contact
        Vector2 dv = body2->velocity + Cross(body2->angularVelocity, contact->r2) -
            body1->velocity - Cross(body1->angularVelocity, contact->r1);

        // Compute normal impulse, using the mass normal we prebuilt
        float vn = Dot(dv, contact->normal);
        float dPn = contact->massNormal * (-vn + contact->bias);

        // Clamp the accum. impulse
        float Pn0 = contact->Pn;
        contact->Pn = max(Pn0 + dPn, 0.0f);
        dPn = contact->Pn - Pn0;

        // Apply normal impulse to each object
        Vector2 Pn = dPn * contact->normal;

        // For linear, we apply directly
        body1->velocity -= body1->invMass * Pn;
        // For angular, we convert to resulting angular component
        // by crossing with the vector from center to the point of contact
        body1->angularVelocity -= body1->invI * Cross(contact->r1, Pn);

        body2->velocity += body2->invMass * Pn;
        body2->angularVelocity += body2->invI * Cross(contact->r2, Pn);

        //
        // And again for tangent
        //

        // Relative velocity at contact
        dv = body2->velocity + Cross(body2->angularVelocity, contact->r2) -
            body1->velocity - Cross(body1->angularVelocity, contact->r1);

        // Compute tangent impulse
        Vector2 tangent = Cross(contact->normal, 1.0f);
        float vt = Dot(dv, tangent);
        float dPt = contact->massTangent * (-vt);

        // Compute friction impulse.
        float maxPt = friction * contact->Pn;

        // Clamp friction
        float oldTangentImpulse = contact->Pt;
        contact->Pt = min(max(oldTangentImpulse + dPt, -maxPt), maxPt);
        dPt = contact->Pt - oldTangentImpulse;

        // Apply contact impulse
        Vector2 Pt = dPt * tangent;

        body1->velocity -= body1->invMass * Pt;
        body1->angularVelocity -= body1->invI * Cross(contact->r1, Pt);

        body2->velocity += body2->invMass * Pt;
        body2->angularVelocity += body2->invI * Cross(contact->r2, Pt);
    }
}
