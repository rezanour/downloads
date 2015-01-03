#include "Precomp.h"
#include "Manifold.h"
#include "Body.h"
#include "Shape.h"

// The contact normal always points from A to B
static int CollideBoxes(
    Shape* a, const Vector2& positionA, float rotationA,
    Shape* b, const Vector2& positionB, float rotationB,
    Contact* contacts);

static int CollideCircles(
    Shape* a, const Vector2& positionA, float rotationA,
    Shape* b, const Vector2& positionB, float rotationB,
    Contact* contacts);

static int CollideCircleBox(
    Shape* a, const Vector2& positionA, float rotationA,
    Shape* b, const Vector2& positionB, float rotationB,
    Contact* contacts);

static int CollideBoxCircle(
    Shape* a, const Vector2& positionA, float rotationA,
    Shape* b, const Vector2& positionB, float rotationB,
    Contact* contacts);

int Collide(Body* a, Body* b, Contact* contacts)
{
    Shape* sA = a->shape;
    Shape* sB = b->shape;

    if (sA->type == ShapeType::Box &&
        sB->type == ShapeType::Box)
    {
        return CollideBoxes(
            sA, a->position, a->rotation,
            sB, b->position, b->rotation,
            contacts);
    }
    else if (sA->type == ShapeType::Circle &&
        sB->type == ShapeType::Circle)
    {
        return CollideCircles(
            sA, a->position, a->rotation,
            sB, b->position, b->rotation,
            contacts);
    }
    else if (sA->type == ShapeType::Circle &&
        sB->type == ShapeType::Box)
    {
        return CollideCircleBox(
            sA, a->position, a->rotation,
            sB, b->position, b->rotation,
            contacts);
    }
    else if (sA->type == ShapeType::Box &&
        sB->type == ShapeType::Circle)
    {
        return CollideBoxCircle(
            sA, a->position, a->rotation,
            sB, b->position, b->rotation,
            contacts);
    }

    return 0;
}

#if 0

// Used for testing axes in the separating axis test (SAT).
// Returns true if the projections of the shapes (with the transforms provided)
// overlap along the axis. The overlap is returned in the overlap out parameter.
// If the minimum overlap is on the min side of B with respect to the axis, the
// sign of overlap will be negative.
static inline bool TestAxis(
    const Vector2& axis,
    Shape* a, const Vector2& positionA, const Matrix2& rotationA, const Matrix2& invRotationA,
    Shape* b, const Vector2& positionB, const Matrix2& rotationB, const Matrix2& invRotationB,
    float* overlap,
    int* uniqueId)
{
    int idMinA = 0, idMaxA = 0, idMinB = 0, idMaxB = 0;
    float minA = Dot(positionA + rotationA * a->GetSupportV(invRotationA * -axis, 0.0f, &idMinA), axis);
    float maxA = Dot(positionA + rotationA * a->GetSupportV(invRotationA * axis, 0.0f, &idMaxA), axis);
    float minB = Dot(positionB + rotationB * b->GetSupportV(invRotationB * -axis, 0.0f, &idMinB), axis);
    float maxB = Dot(positionB + rotationB * b->GetSupportV(invRotationB * axis, 0.0f, &idMaxB), axis);

    float dist1 = maxB - minA;
    float dist2 = maxA - minB;
    if (dist1 < 0 || dist2 < 0)
    {
        return false;
    }

    if (dist1 <= dist2)
    {
        *overlap = dist1;
        *uniqueId = idMinA << 8 | idMaxB;
    }
    else
    {
        *overlap = -dist2;
        *uniqueId = idMaxA << 8 | idMinB;
    }

    return true;
}

int CollideBoxes(
    Shape* a, const Vector2& positionA, float rotationA,
    Shape* b, const Vector2& positionB, float rotationB,
    Contact* contacts)
{
    Matrix2 rotA(rotationA);
    Matrix2 rotB(rotationB);
    Matrix2 invRotA = rotA.Transpose();
    Matrix2 invRotB = rotB.Transpose();

    // Using the SAT, the axes to test are the edge normals
    // of each box. For that, we need to know the local axes
    // of the box (which are just the columns of it's rotation matrix)
    // and then we do both positive & negative of each
    // (they're boxes, so opposite edges have equal but negated normals)
    Vector2 axisToTest[] =
    {
        rotA.col1.GetNormalized(),
        rotA.col2.GetNormalized(),
        rotB.col1.GetNormalized(),
        rotB.col2.GetNormalized(),
    };

    int minFeatureId = 0;
    float minSeparation = FLT_MAX;
    Vector2 minVector;
    for (int i = 0; i < _countof(axisToTest); ++i)
    {
        float separation = 0.0f;
        int uniqueId = 0;
        if (!TestAxis(axisToTest[i], a, positionA, rotA, invRotA, b, positionB, rotB, invRotB, &separation, &uniqueId))
        {
            // no overlap if we find any separating axis
            return 0;
        }

        if (fabsf(separation) < fabsf(minSeparation))
        {
            minFeatureId = uniqueId;
            minSeparation = -(fabsf(separation));
            minVector = (separation < 0) ? axisToTest[i] : -axisToTest[i];
        }
    }

    contacts[0].separation = minSeparation;
    contacts[0].normal = minVector;

    // NOTE: picking the position is fairly tricky for box vs box. We don't want to pick a point that's
    // outside of either of the two boxes. We can either do complicated clipping of feature vs feature,
    // or we can do the little hack below which works in the majority of cases.

    // Get a support point from each object involved in the collision
    Vector2 p1 = positionA + rotA * a->GetSupportV(invRotA * contacts[0].normal, 0, nullptr);
    Vector2 p2 = positionB + rotB * b->GetSupportV(invRotB * -contacts[0].normal, 0, nullptr);

    // Check if p1's point is inside (or on surface) of B, then use it.
    // Otherwise, use p2
    Vector2 p1InBSpace = invRotB * (p1 - positionB);
    Vector2 hB = ((BoxShape*)b)->width * 0.5f;
    if (p1InBSpace.x >= -hB.x && p1InBSpace.x <= hB.x &&
        p1InBSpace.y >= -hB.y && p1InBSpace.y <= hB.y)
    {
        contacts[0].position = p1;
    }
    else
    {
        contacts[0].position = p2;
    }
    //contacts[0].position = positionB + minVector * -(fabsf(Dot(minVector, ((BoxShape*)b)->width * 0.5f)));

    contacts[0].feature.value = minFeatureId;
    return 1;
}

#else

///
/// NOTE: This box vs box feature-clipping code is borrowed from Erin Catto's excellent Box2D_Lite.
///       Unlike the SAT method above, this provides up to 2 contact points for box vs box, which
///       dramatically improves stability in stacking and other resting contact scenarios.
///
///       Ideally, we'll remove all of this collision detection code and replace with a single
///       GJK implementation so we can handle all convex shapes in the same manner. With GJK+EPA,
///       we will always get 1 contact point, so to improve stability we'll update the manifold
///       code to track and maintain up to 2 contact points automatically, discarding invalidated
///       ones. I didn't get to it this holiday break, but will do it before I write up all of this
///       in my blog.
///


// Box vertex and edge numbering:
//
//        ^ y
//        |
//        e1
//   v2 ------ v1
//    |        |
// e2 |        | e4  --> x
//    |        |
//   v3 ------ v4
//        e3

enum Axis
{
    FACE_A_X,
    FACE_A_Y,
    FACE_B_X,
    FACE_B_Y
};

enum EdgeNumbers
{
    NO_EDGE = 0,
    EDGE1,
    EDGE2,
    EDGE3,
    EDGE4
};

struct ClipVertex
{
    ClipVertex() { fp.value = 0; }
    Vector2 v;
    FeaturePair fp;
};

template <typename T>
void Swap(T& a, T& b)
{
    T temp = a;
    a = b;
    b = temp;
}

inline float Sign(float x)
{
    return x < 0.0f ? -1.0f : 1.0f;
}

void Flip(FeaturePair& fp)
{
    Swap(fp.e.inEdge1, fp.e.inEdge2);
    Swap(fp.e.outEdge1, fp.e.outEdge2);
}

int ClipSegmentToLine(ClipVertex vOut[2], ClipVertex vIn[2],
    const Vector2& normal, float offset, char clipEdge)
{
    // Start with no output points
    int numOut = 0;

    // Calculate the distance of end points to the line
    float distance0 = Dot(normal, vIn[0].v) - offset;
    float distance1 = Dot(normal, vIn[1].v) - offset;

    // If the points are behind the plane
    if (distance0 <= 0.0f) vOut[numOut++] = vIn[0];
    if (distance1 <= 0.0f) vOut[numOut++] = vIn[1];

    // If the points are on different sides of the plane
    if (distance0 * distance1 < 0.0f)
    {
        // Find intersection point of edge and plane
        float interp = distance0 / (distance0 - distance1);
        vOut[numOut].v = vIn[0].v + interp * (vIn[1].v - vIn[0].v);
        if (distance0 > 0.0f)
        {
            vOut[numOut].fp = vIn[0].fp;
            vOut[numOut].fp.e.inEdge1 = clipEdge;
            vOut[numOut].fp.e.inEdge2 = NO_EDGE;
        }
        else
        {
            vOut[numOut].fp = vIn[1].fp;
            vOut[numOut].fp.e.outEdge1 = clipEdge;
            vOut[numOut].fp.e.outEdge2 = NO_EDGE;
        }
        ++numOut;
    }

    return numOut;
}

static void ComputeIncidentEdge(ClipVertex c[2], const Vector2& h, const Vector2& pos,
    const Matrix2& Rot, const Vector2& normal)
{
    // The normal is from the reference box. Convert it
    // to the incident boxe's frame and flip sign.
    Matrix2 RotT = Rot.Transpose();
    Vector2 n = -(RotT * normal);
    Vector2 nAbs = Abs(n);

    if (nAbs.x > nAbs.y)
    {
        if (Sign(n.x) > 0.0f)
        {
            c[0].v = Vector2(h.x, -h.y);
            c[0].fp.e.inEdge2 = EDGE3;
            c[0].fp.e.outEdge2 = EDGE4;

            c[1].v = Vector2(h.x, h.y);
            c[1].fp.e.inEdge2 = EDGE4;
            c[1].fp.e.outEdge2 = EDGE1;
        }
        else
        {
            c[0].v = Vector2(-h.x, h.y);
            c[0].fp.e.inEdge2 = EDGE1;
            c[0].fp.e.outEdge2 = EDGE2;

            c[1].v = Vector2(-h.x, -h.y);
            c[1].fp.e.inEdge2 = EDGE2;
            c[1].fp.e.outEdge2 = EDGE3;
        }
    }
    else
    {
        if (Sign(n.y) > 0.0f)
        {
            c[0].v = Vector2(h.x, h.y);
            c[0].fp.e.inEdge2 = EDGE4;
            c[0].fp.e.outEdge2 = EDGE1;

            c[1].v = Vector2(-h.x, h.y);
            c[1].fp.e.inEdge2 = EDGE1;
            c[1].fp.e.outEdge2 = EDGE2;
        }
        else
        {
            c[0].v = Vector2(-h.x, -h.y);
            c[0].fp.e.inEdge2 = EDGE2;
            c[0].fp.e.outEdge2 = EDGE3;

            c[1].v = Vector2(h.x, -h.y);
            c[1].fp.e.inEdge2 = EDGE3;
            c[1].fp.e.outEdge2 = EDGE4;
        }
    }

    c[0].v = pos + Rot * c[0].v;
    c[1].v = pos + Rot * c[1].v;
}

// The normal points from A to B
int CollideBoxes(
    Shape* a, const Vector2& positionA, float rotationA,
    Shape* b, const Vector2& positionB, float rotationB,
    Contact* contacts)
{
    // Setup
    BoxShape* boxA = (BoxShape*)a;
    BoxShape* boxB = (BoxShape*)b;
    Vector2 hA = 0.5f * boxA->width;
    Vector2 hB = 0.5f * boxB->width;

    Vector2 posA = positionA;
    Vector2 posB = positionB;

    Matrix2 RotA(rotationA), RotB(rotationB);

    Matrix2 RotAT = RotA.Transpose();
    Matrix2 RotBT = RotB.Transpose();

    Vector2 a1 = RotA.col1, a2 = RotA.col2;
    Vector2 b1 = RotB.col1, b2 = RotB.col2;

    Vector2 dp = posB - posA;
    Vector2 dA = RotAT * dp;
    Vector2 dB = RotBT * dp;

    Matrix2 C = RotAT * RotB;
    Matrix2 absC = Abs(C);
    Matrix2 absCT = absC.Transpose();

    // Box A faces
    Vector2 faceA = Abs(dA) - hA - absC * hB;
    if (faceA.x > 0.0f || faceA.y > 0.0f)
        return 0;

    // Box B faces
    Vector2 faceB = Abs(dB) - absCT * hA - hB;
    if (faceB.x > 0.0f || faceB.y > 0.0f)
        return 0;

    // Find best axis
    Axis axis;
    float separation;
    Vector2 normal;

    // Box A faces
    axis = FACE_A_X;
    separation = faceA.x;
    normal = dA.x > 0.0f ? RotA.col1 : -RotA.col1;

    const float relativeTol = 0.95f;
    const float absoluteTol = 0.01f;

    if (faceA.y > relativeTol * separation + absoluteTol * hA.y)
    {
        axis = FACE_A_Y;
        separation = faceA.y;
        normal = dA.y > 0.0f ? RotA.col2 : -RotA.col2;
    }

    // Box B faces
    if (faceB.x > relativeTol * separation + absoluteTol * hB.x)
    {
        axis = FACE_B_X;
        separation = faceB.x;
        normal = dB.x > 0.0f ? RotB.col1 : -RotB.col1;
    }

    if (faceB.y > relativeTol * separation + absoluteTol * hB.y)
    {
        axis = FACE_B_Y;
        separation = faceB.y;
        normal = dB.y > 0.0f ? RotB.col2 : -RotB.col2;
    }

    // Setup clipping plane data based on the separating axis
    Vector2 frontNormal, sideNormal;
    ClipVertex incidentEdge[2];
    float front, negSide, posSide;
    char negEdge, posEdge;

    // Compute the clipping lines and the line segment to be clipped.
    switch (axis)
    {
    case FACE_A_X:
    {
        frontNormal = normal;
        front = Dot(posA, frontNormal) + hA.x;
        sideNormal = RotA.col2;
        float side = Dot(posA, sideNormal);
        negSide = -side + hA.y;
        posSide = side + hA.y;
        negEdge = EDGE3;
        posEdge = EDGE1;
        ComputeIncidentEdge(incidentEdge, hB, posB, RotB, frontNormal);
    }
    break;

    case FACE_A_Y:
    {
        frontNormal = normal;
        front = Dot(posA, frontNormal) + hA.y;
        sideNormal = RotA.col1;
        float side = Dot(posA, sideNormal);
        negSide = -side + hA.x;
        posSide = side + hA.x;
        negEdge = EDGE2;
        posEdge = EDGE4;
        ComputeIncidentEdge(incidentEdge, hB, posB, RotB, frontNormal);
    }
    break;

    case FACE_B_X:
    {
        frontNormal = -normal;
        front = Dot(posB, frontNormal) + hB.x;
        sideNormal = RotB.col2;
        float side = Dot(posB, sideNormal);
        negSide = -side + hB.y;
        posSide = side + hB.y;
        negEdge = EDGE3;
        posEdge = EDGE1;
        ComputeIncidentEdge(incidentEdge, hA, posA, RotA, frontNormal);
    }
    break;

    case FACE_B_Y:
    {
        frontNormal = -normal;
        front = Dot(posB, frontNormal) + hB.y;
        sideNormal = RotB.col1;
        float side = Dot(posB, sideNormal);
        negSide = -side + hB.x;
        posSide = side + hB.x;
        negEdge = EDGE2;
        posEdge = EDGE4;
        ComputeIncidentEdge(incidentEdge, hA, posA, RotA, frontNormal);
    }
    break;

    default:
        _ASSERT(false);
        return 0;
    }

    // clip other face with 5 box planes (1 face plane, 4 edge planes)

    ClipVertex clipPoints1[2];
    ClipVertex clipPoints2[2];
    int np;

    // Clip to box side 1
    np = ClipSegmentToLine(clipPoints1, incidentEdge, -sideNormal, negSide, negEdge);

    if (np < 2)
        return 0;

    // Clip to negative box side 1
    np = ClipSegmentToLine(clipPoints2, clipPoints1, sideNormal, posSide, posEdge);

    if (np < 2)
        return 0;

    // Now clipPoints2 contains the clipping points.
    // Due to roundoff, it is possible that clipping removes all points.

    int numContacts = 0;
    for (int i = 0; i < 2; ++i)
    {
        float separation = Dot(frontNormal, clipPoints2[i].v) - front;

        if (separation <= 0)
        {
            contacts[numContacts].separation = separation;
            contacts[numContacts].normal = normal;
            // slide contact point onto reference face (easy to cull)
            contacts[numContacts].position = clipPoints2[i].v - separation * frontNormal;
            contacts[numContacts].feature = clipPoints2[i].fp;
            if (axis == FACE_B_X || axis == FACE_B_Y)
                Flip(contacts[numContacts].feature);
            ++numContacts;
        }
    }

    return numContacts;
}

#endif




int CollideCircles(
    Shape* a, const Vector2& positionA, float rotationA,
    Shape* b, const Vector2& positionB, float rotationB,
    Contact* contacts)
{
    UNREFERENCED_PARAMETER(rotationA);
    UNREFERENCED_PARAMETER(rotationB);

    CircleShape* cirA = (CircleShape*)a;
    CircleShape* cirB = (CircleShape*)b;

    float rA = cirA->radius;
    float rB = cirB->radius;
    float r = rA + rB;

    Vector2 posA = positionA;
    Vector2 posB = positionB;

    float distSq = (posA - posB).GetLengthSq();

    if (distSq > r*r)
    {
        return 0;
    }

    float separation = sqrtf(distSq) - r;
    contacts[0].separation = separation;
    contacts[0].normal = (posB - posA).GetNormalized();
    contacts[0].position = posB + contacts[0].normal * separation;
    contacts[0].feature.value = 0;

    return 1;
}

int CollideCircleBox(
    Shape* a, const Vector2& positionA, float rotationA,
    Shape* b, const Vector2& positionB, float rotationB,
    Contact* contacts)
{
    UNREFERENCED_PARAMETER(rotationA);

    CircleShape* cirA = (CircleShape*)a;
    BoxShape* boxB = (BoxShape*)b;

    float r = cirA->radius;
    Vector2 hB = 0.5f * boxB->width;

    Vector2 posA = positionA;
    Vector2 posB = positionB;

    // transform box to origin, then check for circle vs aabb
    Vector2 relPosA = posA - posB;
    Matrix2 rotB = Matrix2(rotationB).Transpose();
    relPosA = rotB * relPosA;

    // If the center of the sphere is outside of the aabb
    if (relPosA.x < -hB.x || relPosA.x > hB.x ||
        relPosA.y < -hB.y || relPosA.y > hB.y)
    {
        Vector2 closestPoint = Vector2(min(max(relPosA.x, -hB.x), hB.x), min(max(relPosA.y, -hB.y), hB.y));
        Vector2 toClosest = closestPoint - relPosA;
        float d = toClosest.GetLength();
        if (d > r)
        {
            return 0;
        }

        contacts[0].separation = d - r;
        contacts[0].normal = (rotB.Transpose() * toClosest).GetNormalized();
        contacts[0].position = posA + contacts[0].normal * r;
        contacts[0].feature.value = 0;
        return 1;
    }
    else
    {
        // Otherwise, find side we're closest to & use that
        float dists[] = 
        {
            relPosA.x - (-hB.x),
            hB.x - relPosA.x,
            relPosA.y - (-hB.y),
            hB.y - relPosA.y
        };

        int iMin = 0;
        for (int i = 1; i < _countof(dists); ++i)
        {
            if (dists[i] < dists[iMin])
            {
                iMin = i;
            }
        }

        contacts[0].separation = -(dists[iMin] + r);
        Vector2 norm;
        switch (iMin)
        {
        case 0: norm = Vector2(-1, 0); break;
        case 1: norm = Vector2(1, 0); break;
        case 2: norm = Vector2(0, -1); break;
        case 3: norm = Vector2(0, 1); break;
        }

        // negate norm to point towards B by convention
        contacts[0].normal = (rotB.Transpose() * -norm).GetNormalized();
        contacts[0].position = posA + contacts[0].normal * r;
        contacts[0].feature.value = 0;
        return 1;
    }
}

int CollideBoxCircle(
    Shape* a, const Vector2& positionA, float rotationA,
    Shape* b, const Vector2& positionB, float rotationB,
    Contact* contacts)
{
    UNREFERENCED_PARAMETER(rotationB);

    BoxShape* boxA = (BoxShape*)a;
    CircleShape* cirB = (CircleShape*)b;

    float r = cirB->radius;
    Vector2 hA = 0.5f * boxA->width;

    Vector2 posA = positionA;
    Vector2 posB = positionB;

    // transform box to origin, then circle vs aabb
    Vector2 relPosB = posB - posA;
    Matrix2 rotA = Matrix2(rotationA).Transpose();
    relPosB = rotA * relPosB;

    // If the center of the sphere is outside of the aabb
    if (relPosB.x < -hA.x || relPosB.x > hA.x ||
        relPosB.y < -hA.y || relPosB.y > hA.y)
    {
        Vector2 closestPoint = Vector2(min(max(relPosB.x, -hA.x), hA.x), min(max(relPosB.y, -hA.y), hA.y));
        Vector2 toClosest = relPosB - closestPoint;
        float d = toClosest.GetLength();
        if (d > r)
        {
            return 0;
        }

        contacts[0].separation = d - r;
        contacts[0].normal = (rotA.Transpose() * toClosest).GetNormalized();
        contacts[0].position = posB - contacts[0].normal * r;
        contacts[0].feature.value = 0;
        return 1;
    }
    else
    {
        // Otherwise, find side we're closest to & use that
        float dists[] =
        {
            relPosB.x - (-hA.x),
            hA.x - relPosB.x,
            relPosB.y - (-hA.y),
            hA.y - relPosB.y
        };

        int iMin = 0;
        for (int i = 1; i < _countof(dists); ++i)
        {
            if (dists[i] < dists[iMin])
            {
                iMin = i;
            }
        }

        contacts[0].separation = -(dists[iMin] + r);
        Vector2 norm;
        switch (iMin)
        {
        case 0: norm = Vector2(-1, 0); break;
        case 1: norm = Vector2(1, 0); break;
        case 2: norm = Vector2(0, -1); break;
        case 3: norm = Vector2(0, 1); break;
        }
        contacts[0].normal = (rotA.Transpose() * norm).GetNormalized();
        contacts[0].position = posA + contacts[0].normal * r;
        contacts[0].feature.value = 0;
        return 1;
    }
}
