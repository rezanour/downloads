#define NOMINMAX
#include <Windows.h>

// librz main header
#include <librz.h>



static bool OnIdle();

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command)
{
    rz::application app(instance, L"TestApp");
    if (!app.initialize(640, 480, L"My Test Window"))
    {
        return -1;
    }

    app.set_idle_handler(OnIdle);

    app.run(show_command);

    return 0;
}

bool OnIdle()
{
    if (GetAsyncKeyState(VK_ESCAPE))
    {
        // exit
        return false;
    }

    return true;
}
