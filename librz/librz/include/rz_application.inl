// Implementation of application class

namespace rz
{
    application::application(const HINSTANCE instance, const wchar_t* class_name)
        : _instance(instance)
        , _main_window(nullptr)
        , _class_name(class_name)
    {
    }

    application::~application()
    {
        if (_main_window)
        {
            DestroyWindow(_main_window);
            _main_window = nullptr;
        }
    }

    bool application::initialize(int client_width, int client_height, const wchar_t* window_title, DWORD window_style)
    {
        WNDCLASSEX wcx{};
        wcx.cbSize = sizeof(wcx);
        wcx.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wcx.hInstance = _instance;
        wcx.lpfnWndProc = s_window_proc;
        wcx.lpszClassName = _class_name.c_str();

        if (RegisterClassEx(&wcx) == INVALID_ATOM)
        {
            assert(false);
            return false;
        }

        RECT rc{};
        rc.right = client_width;
        rc.bottom = client_height;
        AdjustWindowRect(&rc, window_style, FALSE);

        _main_window = CreateWindow(_class_name.c_str(), window_title, window_style, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.right - rc.top, nullptr, nullptr, _instance, nullptr);

        if (!_main_window)
        {
            assert(false);
            return false;
        }

        SetWindowLongPtr(_main_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        return true;
    }

    int application::run(int show_command)
    {
        ShowWindow(_main_window, show_command);
        UpdateWindow(_main_window);

        MSG msg{};
        while (msg.message != WM_QUIT)
        {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
            {
                if (_idle_handler)
                {
                    bool should_continue = _idle_handler();
                    if (!should_continue)
                    {
                        break;
                    }
                }
            }
        }

        return (msg.message == WM_QUIT) ? static_cast<int>(msg.wParam) : 0;
    }

    LRESULT CALLBACK application::s_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        application* pThis = reinterpret_cast<application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (pThis)
        {
            pThis->window_proc(msg, wParam, lParam);
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    void application::window_proc(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto it = _message_handlers.find(msg);
        if (it != _message_handlers.end())
        {
            it->second(wParam, lParam);
        }
        else
        {
            // default handling
            switch (msg)
            {
            case WM_CLOSE:
                PostQuitMessage(0);
                break;
            }
        }
    }
} // namespace rz
