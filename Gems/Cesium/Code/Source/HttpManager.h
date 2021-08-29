#pragma once

#include "GenericIOManager.h"
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/HttpHeaders.h>
#include <aws/core/http/HttpResponse.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/std/string/string.h>

namespace Aws
{
    namespace Http
    {
        class HttpClient;
    }
}

namespace Cesium
{
    struct HttpRequestParameter final
    {
        HttpRequestParameter(AZStd::string&& url, Aws::Http::HttpMethod method)
            : m_url{ std::move(url) }
            , m_method{ method }
        {
        }

        HttpRequestParameter(AZStd::string&& url, Aws::Http::HttpMethod method, CesiumAsync::HttpHeaders&& headers)
            : m_url{ std::move(url) }
            , m_method{ method }
            , m_headers{ std::move(headers) }
        {
        }

        HttpRequestParameter(AZStd::string&& url, Aws::Http::HttpMethod method, CesiumAsync::HttpHeaders&& headers, AZStd::string&& body)
            : m_url{ std::move(url) }
            , m_method{ method }
            , m_headers{ std::move(headers) }
            , m_body{ std::move(body) }
        {
        }

        AZStd::string m_url;

        Aws::Http::HttpMethod m_method;

        CesiumAsync::HttpHeaders m_headers;

        AZStd::string m_body;
    };

    struct HttpResult final
    {
        std::shared_ptr<Aws::Http::HttpRequest> m_request;
        std::shared_ptr<Aws::Http::HttpResponse> m_response;
    };

    class HttpManager final : public GenericIOManager
    {
        struct RequestHandler;
        struct GenericIORequestHandler;

    public:
        HttpManager();

        ~HttpManager() noexcept;

        CesiumAsync::Future<HttpResult> AddRequest(
            const CesiumAsync::AsyncSystem& asyncSystem, HttpRequestParameter&& httpRequestParameter);

        AZStd::string GetParentPath(const AZStd::string& path) override;

        IOContent GetFileContent(const IORequestParameter& request) override;

        IOContent GetFileContent(IORequestParameter&& request) override;

        CesiumAsync::Future<IOContent> GetFileContentAsync(
            const CesiumAsync::AsyncSystem& asyncSystem, const IORequestParameter& request) override;

        CesiumAsync::Future<IOContent> GetFileContentAsync(
            const CesiumAsync::AsyncSystem& asyncSystem, IORequestParameter&& request) override;

        static IOContent GetResponseBodyContent(Aws::Http::HttpResponse& response);

    private:
        AZStd::unique_ptr<AZ::JobManager> m_ioJobManager;
        AZStd::unique_ptr<AZ::JobContext> m_ioJobContext;
        std::shared_ptr<Aws::Http::HttpClient> m_awsHttpClient;
    };
}
