#include "pch.h"

#include "FBroBaseType.h"
#include "FBroHsEvent.h"
#include "FBroInit.h"

#include <array>
#include <filesystem>
#include <string>

namespace {

std::string ToSystemAnsi(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_ACP, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_ACP, 0, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::filesystem::path ExeDir() {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD len = GetModuleFileNameW(nullptr, buffer.data(),
        static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring(buffer.data(), len)).parent_path();
}

class DemoInitEvent final : public FBroHsInitEvent {
public:
    void OnContextInitialized() override {
        CreateBrowser();
    }

private:
    void CreateBrowser() {
        CefWindowInfo window_info;
        window_info.SetAsPopup(nullptr, L"Native FBro Demo");

        CefBrowserSettings browser_settings;
        browser_settings.background_color = CefColorSetARGB(255, 255, 255, 255);

        const CefString url("https://www.baidu.com");
        CefBrowserHost::CreateBrowser(
            window_info,
            new DemoClient(),
            url,
            browser_settings,
            nullptr,
            nullptr);
    }

    class DemoClient final : public CefClient,
                             public CefLifeSpanHandler,
                             public CefLoadHandler {
    public:
        CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
            return this;
        }

        CefRefPtr<CefLoadHandler> GetLoadHandler() override {
            return this;
        }

        void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
            browser_ = browser;
        }

        void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
            browser_ = nullptr;
            FBroQuitMessageLoop();
        }

        void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int http_status_code) override {
            if (frame && frame->IsMain()) {
                OutputDebugStringW(L"Native FBro Demo: page load finished.\n");
            }
        }

    private:
        CefRefPtr<CefBrowser> browser_;

        IMPLEMENT_REFCOUNTING(DemoClient);
    };

    IMPLEMENT_REFCOUNTING(DemoInitEvent);
};

} // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    FBroHsSetProcessDPI(0);

    const auto app_dir = ExeDir();
    const auto subprocess = app_dir / L"FBroSubprocess.exe";
    const auto cache_dir = app_dir / L"CacheData";
    const auto log_file = app_dir / L"debug.log";
    const auto locales_dir = app_dir / L"locales";

    const auto app_dir_ansi = ToSystemAnsi(app_dir.wstring());
    const auto subprocess_ansi = ToSystemAnsi(subprocess.wstring());
    const auto cache_ansi = ToSystemAnsi(cache_dir.wstring());
    const auto log_ansi = ToSystemAnsi(log_file.wstring());
    const auto locales_ansi = ToSystemAnsi(locales_dir.wstring());

    FBroInitSettings settings{};
    settings.no_sandbox = TRUE;
    settings.browser_subprocess_path = const_cast<char*>(subprocess_ansi.c_str());
    settings.multi_threaded_message_loop = FALSE;
    settings.external_message_pump = FALSE;
    settings.windowless_rendering_enabled = FALSE;
    settings.command_line_args_disabled = FALSE;
    settings.cache_path = const_cast<char*>(cache_ansi.c_str());
    settings.persist_session_cookies = TRUE;
    settings.locale = const_cast<char*>("zh-CN");
    settings.log_file = const_cast<char*>(log_ansi.c_str());
    settings.log_severity = LOGSEVERITY_DEFAULT;
    settings.resources_dir_path = const_cast<char*>(app_dir_ansi.c_str());
    settings.locales_dir_path = const_cast<char*>(locales_ansi.c_str());
    settings.remote_debugging_port = 9222;
    settings.enable_auto_multiple = TRUE;

    CefRefPtr<DemoInitEvent> init_event = new DemoInitEvent();
    if (!FBroHsInitPro(&settings, init_event, 1024)) {
        MessageBoxW(nullptr, L"FBro initialization failed.", L"Native FBro Demo", MB_ICONERROR);
        return 1;
    }

    FBroRunMessageLoop();
    FBroShutdown(TRUE);
    return 0;
}
