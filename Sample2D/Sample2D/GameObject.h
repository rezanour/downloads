#pragma once

class Texture;
struct Body;

class GameObject
{
public:
    GameObject();
    ~GameObject();

    Body* GetBody() { return _body.get(); }

    const std::shared_ptr<Texture>& GetTexture() const { return _texture; }
    void SetTexture(const std::shared_ptr<Texture>& value) { _texture = value; }

private:
    // Prevent copy
    GameObject(const GameObject&);
    GameObject& operator= (const GameObject&);

protected:
    std::unique_ptr<Body> _body;

    // Visual representation of the object
    std::shared_ptr<Texture> _texture;
};
