#pragma once

class SpriteBatch;
class SpriteBatch2;
class GameObject;
class PhysicsWorld;

class Game
{
public:
    Game(const std::shared_ptr<SpriteBatch2>& spriteBatch);
    ~Game();

    void Update(float elapsedSeconds);
    void Draw();

private:
    // Prevent copy
    Game(const Game&);
    Game& operator= (const Game&);

private:
    std::shared_ptr<SpriteBatch2> _spriteBatch;
    std::unique_ptr<PhysicsWorld> _physicsWorld;
    std::vector<std::shared_ptr<GameObject>> _gameObjects;
    std::shared_ptr<GameObject> _player;
};
