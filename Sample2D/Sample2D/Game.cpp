#include "Precomp.h"
#include "Game.h"
#include "GameObject.h"
#include "SpriteBatch.h"
#include "SpriteBatch2.h"
#include "Texture.h"
#include "PhysicsWorld.h"
#include "Shape.h"
#include "Body.h"

Game::Game(const std::shared_ptr<SpriteBatch2>& spriteBatch)
    : _spriteBatch(spriteBatch)
{
    _physicsWorld.reset(new PhysicsWorld(Vector2(0, -50), 10));

    _player = std::make_shared<GameObject>();
    _player->GetBody()->Init(new BoxShape(2, 4), 100);
    _player->GetBody()->position = Vector2(0, 10);
    _player->GetBody()->friction = 0.9f;
    _player->GetBody()->SetFixedRotation(true);
    _player->SetTexture(std::make_shared<Texture>(_spriteBatch->GetDevice(), L"link.png"));
    _gameObjects.push_back(_player);
    _physicsWorld->Add(_player->GetBody());

    std::shared_ptr<Texture> tex = std::make_shared<Texture>(_spriteBatch->GetDevice(), L"brick.png");
    std::shared_ptr<Texture> cirTex = std::make_shared<Texture>(_spriteBatch->GetDevice(), L"circle.png");

    std::shared_ptr<GameObject> block = std::make_shared<GameObject>();
    block->GetBody()->Init(new BoxShape(20, 2), FLT_MAX);
    block->SetTexture(tex);
    _gameObjects.push_back(block);
    _physicsWorld->Add(block->GetBody());

    block = std::make_shared<GameObject>();
    block->GetBody()->Init(new BoxShape(20, 1), FLT_MAX);
    block->GetBody()->position = Vector2(20, -5);
    block->GetBody()->rotation = 0.525f;
    block->SetTexture(tex);
    _gameObjects.push_back(block);
    _physicsWorld->Add(block->GetBody());

    block = std::make_shared<GameObject>();
    block->GetBody()->Init(new BoxShape(20, 20), FLT_MAX);
    block->GetBody()->position = Vector2(0, -20);
    block->GetBody()->rotation = -0.225f;
    block->SetTexture(tex);
    _gameObjects.push_back(block);
    _physicsWorld->Add(block->GetBody());

    block = std::make_shared<GameObject>();
    block->GetBody()->Init(new BoxShape(20, 1), FLT_MAX);
    block->GetBody()->position = Vector2(20, -20);
    block->GetBody()->rotation = 0.325f;
    block->SetTexture(tex);
    _gameObjects.push_back(block);
    _physicsWorld->Add(block->GetBody());

    // Dump a bunch of dynamic objects in
    for (int i = 0; i < 50; ++i)
    {
        block = std::make_shared<GameObject>();
        if (rand() % 2 == 0)
        {
            block->GetBody()->Init(new CircleShape(1.0f), (float)(rand() % 15 + 5));
            block->SetTexture(cirTex);
        }
        else
        {
            block->GetBody()->Init(new BoxShape(2, 2), (float)(rand() % 15 + 5));
            block->SetTexture(tex);
        }
        block->GetBody()->position = Vector2(rand() % 40 - 20, rand() % 100 + 30);
        block->GetBody()->rotation = (rand() % 180) * (180.0f / 3.14156f);
        _gameObjects.push_back(block);
        _physicsWorld->Add(block->GetBody());
    }
}

Game::~Game()
{
}

void Game::Update(float elapsedSeconds)
{
    static const float PlayerMoveForce = 40;
    static const float PlayerJumpImpulse = 2000;
    static const float MaxInputVelocity = 20;
    static bool SpaceHeld = false;

    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
    {
        if (_player->GetBody()->velocity.x > -MaxInputVelocity)
        {
            _player->GetBody()->AddForce(Vector2(-PlayerMoveForce * _player->GetBody()->mass, 0.0f));
        }
    }
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
    {
        if (_player->GetBody()->velocity.x < MaxInputVelocity)
        {
            _player->GetBody()->AddForce(Vector2(PlayerMoveForce * _player->GetBody()->mass, 0.0f));
        }
    }
    // This is really bad jump only for quick testing purposes. It doesn't even check if you're standing on something! :)
    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
    {
        if (!SpaceHeld)
        {
            _player->GetBody()->AddImpulse(Vector2(0.0f, PlayerJumpImpulse));
        }
        SpaceHeld = true;
    }
    else
    {
        SpaceHeld = false;
    }

    _physicsWorld->Update(elapsedSeconds);
}

void Game::Draw()
{
    _spriteBatch->Begin(Vector2(0, -5), Vector2(75, 35));

    for (auto& gameObject : _gameObjects)
    {
        Body* body = gameObject->GetBody();
        if (body->shape->type == ShapeType::Box)
        {
            BoxShape* box = (BoxShape*)body->shape;
            _spriteBatch->Draw(gameObject->GetTexture(), nullptr, body->position, box->width, body->rotation);
        }
        else if (body->shape->type == ShapeType::Circle)
        {
            CircleShape* circle = (CircleShape*)body->shape;
            _spriteBatch->Draw(gameObject->GetTexture(), nullptr, body->position, Vector2(circle->radius * 2, circle->radius * 2), body->rotation);
        }
    }

    _spriteBatch->End();
}
