/*
 * Copyright (c) 2022, Dex♪ <dexes.ttp@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "WebSocketClientAdapter.h"
#include <LibRequests/Request.h>
#include <LibRequests/RequestClient.h>
#include <LibWebView/RequestServerAdapter.h>

namespace WebView {

ErrorOr<NonnullRefPtr<RequestServerRequestAdapter>> RequestServerRequestAdapter::try_create(NonnullRefPtr<Requests::Request> request)
{
    return adopt_nonnull_ref_or_enomem(new (nothrow) RequestServerRequestAdapter(move(request)));
}

RequestServerRequestAdapter::RequestServerRequestAdapter(NonnullRefPtr<Requests::Request> request)
    : m_request(request)
{
    request->on_certificate_requested = [weak_this = make_weak_ptr()]() {
        if (auto strong_this = weak_this.strong_ref()) {
            if (strong_this->on_certificate_requested) {
                auto certificate_and_key = strong_this->on_certificate_requested();
                return Requests::Request::CertificateAndKey {
                    .certificate = move(certificate_and_key.certificate),
                    .key = move(certificate_and_key.key),
                };
            }
        }

        return Requests::Request::CertificateAndKey {};
    };
}

RequestServerRequestAdapter::~RequestServerRequestAdapter() = default;

void RequestServerRequestAdapter::set_buffered_request_finished_callback(Requests::Request::BufferedRequestFinished on_buffered_request_finished)
{
    m_request->set_buffered_request_finished_callback(move(on_buffered_request_finished));
}

void RequestServerRequestAdapter::set_unbuffered_request_callbacks(Requests::Request::HeadersReceived on_headers_received, Requests::Request::DataReceived on_data_received, Requests::Request::RequestFinished on_finished)
{
    m_request->set_unbuffered_request_callbacks(move(on_headers_received), move(on_data_received), move(on_finished));
}

bool RequestServerRequestAdapter::stop()
{
    return m_request->stop();
}

ErrorOr<NonnullRefPtr<RequestServerAdapter>> RequestServerAdapter::try_create(NonnullRefPtr<Requests::RequestClient> protocol_client)
{
    return try_make_ref_counted<RequestServerAdapter>(move(protocol_client));
}

ErrorOr<NonnullRefPtr<RequestServerAdapter>> RequestServerAdapter::try_create()
{
    auto protocol_client = TRY(Requests::RequestClient::try_create());
    return try_make_ref_counted<RequestServerAdapter>(move(protocol_client));
}

RequestServerAdapter::RequestServerAdapter(NonnullRefPtr<Requests::RequestClient> protocol_client)
    : m_protocol_client(protocol_client)
{
}

RequestServerAdapter::~RequestServerAdapter() = default;

RefPtr<Web::ResourceLoaderConnectorRequest> RequestServerAdapter::start_request(ByteString const& method, URL::URL const& url, HTTP::HeaderMap const& headers, ReadonlyBytes body, Core::ProxyData const& proxy)
{
    auto protocol_request = m_protocol_client->start_request(method, url, headers, body, proxy);
    if (!protocol_request)
        return {};
    return RequestServerRequestAdapter::try_create(protocol_request.release_nonnull()).release_value_but_fixme_should_propagate_errors();
}

RefPtr<Web::WebSockets::WebSocketClientSocket> RequestServerAdapter::websocket_connect(URL::URL const& url, AK::ByteString const& origin, Vector<AK::ByteString> const& protocols)
{
    auto underlying_websocket = m_protocol_client->websocket_connect(url, origin, protocols);
    if (!underlying_websocket)
        return {};
    return WebSocketClientSocketAdapter::create(underlying_websocket.release_nonnull());
}

void RequestServerAdapter::prefetch_dns(URL::URL const& url)
{
    m_protocol_client->ensure_connection(url, RequestServer::CacheLevel::ResolveOnly);
}

void RequestServerAdapter::preconnect(URL::URL const& url)
{
    m_protocol_client->ensure_connection(url, RequestServer::CacheLevel::CreateConnection);
}

}
