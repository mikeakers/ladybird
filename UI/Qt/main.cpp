/*
 * Copyright (c) 2022, Andreas Kling <andreas@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/Process.h>
#include <LibCore/System.h>
#include <LibMain/Main.h>
#include <LibWebView/Application.h>
#include <LibWebView/BrowserProcess.h>
#include <LibWebView/URL.h>
#include <LibWebView/Utilities.h>
#include <UI/Qt/Application.h>
#include <UI/Qt/BrowserWindow.h>
#include <UI/Qt/Settings.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#    include <QStyleHints>
#endif

#if defined(AK_OS_MACOS)
#    include <LibWebView/MachPortServer.h>
#endif

namespace Ladybird {

// FIXME: Find a place to put this declaration (and other helper functions).
bool is_using_dark_system_theme(QWidget&);
bool is_using_dark_system_theme(QWidget& widget)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Use the explicitly set or system default color scheme whenever available
    auto color_scheme = QGuiApplication::styleHints()->colorScheme();
    if (color_scheme != Qt::ColorScheme::Unknown)
        return color_scheme == Qt::ColorScheme::Dark;
#endif

    // Calculate luma based on Rec. 709 coefficients
    // https://en.wikipedia.org/wiki/Rec._709#Luma_coefficients
    auto color = widget.palette().color(widget.backgroundRole());
    auto luma = 0.2126f * color.redF() + 0.7152f * color.greenF() + 0.0722f * color.blueF();
    return luma <= 0.5f;
}

}

static ErrorOr<void> handle_attached_debugger()
{
#ifdef AK_OS_LINUX
    // Let's ignore SIGINT if we're being debugged because GDB
    // incorrectly forwards the signal to us even when it's set to
    // "nopass". See https://sourceware.org/bugzilla/show_bug.cgi?id=9425
    // for details.
    if (TRY(Core::Process::is_being_debugged())) {
        dbgln("Debugger is attached, ignoring SIGINT");
        TRY(Core::System::signal(SIGINT, SIG_IGN));
    }
#endif
    return {};
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    AK::set_rich_debug_enabled(true);

    auto app = Ladybird::Application::create(arguments);

    TRY(handle_attached_debugger());

    WebView::platform_init();
    WebView::copy_default_config_files(Ladybird::Settings::the()->directory());

#if defined(AK_OS_MACOS)
    auto mach_port_server = make<WebView::MachPortServer>();
    WebView::set_mach_server_name(mach_port_server->server_port_name());

    mach_port_server->on_receive_child_mach_port = [&app](auto pid, auto port) {
        app->set_process_mach_port(pid, move(port));
    };
    mach_port_server->on_receive_backing_stores = [](WebView::MachPortServer::BackingStoresMessage message) {
        if (auto view = WebView::WebContentClient::view_for_pid_and_page_id(message.pid, message.page_id); view.has_value())
            view->did_allocate_iosurface_backing_stores(message.front_backing_store_id, move(message.front_backing_store_port), message.back_backing_store_id, move(message.back_backing_store_port));
    };
#endif

    WebView::BrowserProcess browser_process;
    TRY(app->launch_services());

    if (auto const& browser_options = Ladybird::Application::browser_options(); !browser_options.headless_mode.has_value()) {
        if (browser_options.force_new_process == WebView::ForceNewProcess::No) {
            auto disposition = TRY(browser_process.connect(browser_options.raw_urls, browser_options.new_window));

            if (disposition == WebView::BrowserProcess::ProcessDisposition::ExitProcess) {
                outln("Opening in existing process");
                return 0;
            }
        }

        app->on_open_file = [&](auto const& file_url) {
            auto& window = app->active_window();
            window.view().load(file_url);
        };

        browser_process.on_new_tab = [&](auto const& urls) {
            auto& window = app->active_window();
            for (size_t i = 0; i < urls.size(); ++i) {
                window.new_tab_from_url(urls[i], (i == 0) ? Web::HTML::ActivateTab::Yes : Web::HTML::ActivateTab::No);
            }
            window.show();
            window.activateWindow();
            window.raise();
        };

        browser_process.on_new_window = [&](auto const& urls) {
            app->new_window(urls);
        };

        auto& window = app->new_window(browser_options.urls);
        window.setWindowTitle("Ladybird");

        if (Ladybird::Settings::the()->is_maximized()) {
            window.showMaximized();
        } else {
            auto last_position = Ladybird::Settings::the()->last_position();
            if (last_position.has_value())
                window.move(last_position.value());
            window.resize(Ladybird::Settings::the()->last_size());
        }

        window.show();
    }

    return app->execute();
}
