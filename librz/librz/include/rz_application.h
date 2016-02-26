// Simple classic Windows application layer abstraction.
// NOTE: not thread safe
#pragma once

namespace rz
{
    class application
    {
    public:
        application(const HINSTANCE instance, const wchar_t* class_name);
        ~application();

        // main window handle. Only valid after successful initialize call
        HWND main_window() const
        {
            return _main_window;
        }

        // set caller-provided function to be called during idle.
        // returning true from idle handler continues execution.
        // returning false from it quits the application
        void set_idle_handler(const std::function<bool()>& idle_handler)
        {
            _idle_handler = idle_handler;
        }

        // set caller-provided handler for a specific window message
        void set_message_handler(uint32_t message, const std::function<void(WPARAM, LPARAM)>& message_handler)
        {
            _message_handlers[message] = message_handler;
        }

        // Initializes the application's main window, and prepares for message processing
        bool initialize(int client_width, int client_height, const wchar_t* window_title = L"untitled", DWORD window_style = WS_OVERLAPPEDWINDOW);

        // Runs (blocking) the application message pump. Returns Quit exit code
        int run(int show_command);

    private:
        application(const application&) = delete;
        application& operator= (const application&) = delete;

        static LRESULT CALLBACK s_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        void window_proc(UINT msg, WPARAM wParam, LPARAM lParam);

    private:
        const HINSTANCE _instance;
        std::wstring _class_name;
        HWND _main_window;
        std::function<bool()> _idle_handler;
        std::map<uint32_t, std::function<void(WPARAM, LPARAM)>> _message_handlers;
    };
} // namespace rz

#include "rz_application.inl"
