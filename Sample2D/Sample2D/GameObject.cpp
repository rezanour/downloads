#include "Precomp.h"
#include "GameObject.h"
#include "Texture.h"
#include "Body.h"
#include "Shape.h"

GameObject::GameObject()
    : _body(new Body())
{
}

GameObject::~GameObject()
{
    if (_body->shape)
    {
        delete _body->shape;
    }
}
