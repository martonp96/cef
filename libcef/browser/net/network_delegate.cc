// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net/network_delegate.h"

#include <string>
#include <utility>

#include "include/cef_urlrequest.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/cookie_manager_impl.h"
#include "libcef/browser/net/net_util.h"
#include "libcef/browser/net/source_stream.h"
#include "libcef/browser/net/url_request_user_data.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/response_impl.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "chrome/common/net/safe_search_util.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"

namespace {

class CefBeforeResourceLoadCallbackImpl : public CefRequestCallback {
 public:
  typedef net::CompletionOnceCallback CallbackType;

  CefBeforeResourceLoadCallbackImpl(CefRefPtr<CefRequestImpl> cef_request,
                                    GURL* new_url,
                                    net::URLRequest* url_request,
                                    bool force_google_safesearch,
                                    CallbackType callback)
      : cef_request_(cef_request),
        new_url_(new_url),
        url_request_(url_request),
        force_google_safesearch_(force_google_safesearch),
        callback_(std::move(callback)) {
    DCHECK(new_url);
    DCHECK(url_request_);

    // Add an association between the URLRequest and this object.
    url_request_->SetUserData(UserDataKey(),
                              base::WrapUnique(new Disconnector(this)));
  }

  ~CefBeforeResourceLoadCallbackImpl() {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_IOT()) {
        RunNow(cef_request_, new_url_, url_request_, std::move(callback_),
               force_google_safesearch_, false);
      } else {
        CEF_POST_TASK(CEF_IOT,
                      base::Bind(&CefBeforeResourceLoadCallbackImpl::RunNow,
                                 cef_request_, new_url_, url_request_,
                                 base::Passed(std::move(callback_)),
                                 force_google_safesearch_, false));
      }
    }
  }

  void Continue(bool allow) override {
    // Always continue asynchronously.
    CEF_POST_TASK(CEF_IOT,
                  base::Bind(&CefBeforeResourceLoadCallbackImpl::ContinueNow,
                             this, allow));
  }

  void Cancel() override { Continue(false); }

  void ContinueNow(bool allow) {
    CEF_REQUIRE_IOT();
    if (!callback_.is_null()) {
      RunNow(cef_request_, new_url_, url_request_, std::move(callback_),
             force_google_safesearch_, allow);
      Disconnect();
    }
  }

 private:
  void Disconnect() {
    CEF_REQUIRE_IOT();
    cef_request_ = nullptr;
    new_url_ = nullptr;
    url_request_ = nullptr;
    callback_.Reset();
  }

  // Used to disconnect the callback when the associated URLRequest is
  // destroyed.
  class Disconnector : public base::SupportsUserData::Data {
   public:
    explicit Disconnector(CefBeforeResourceLoadCallbackImpl* callback)
        : callback_(callback) {}
    ~Disconnector() override {
      if (callback_)
        callback_->Disconnect();
    }

    void Disconnect() { callback_ = NULL; }

   private:
    CefBeforeResourceLoadCallbackImpl* callback_;
  };

  static void RunNow(CefRefPtr<CefRequestImpl> cef_request,
                     GURL* new_url,
                     net::URLRequest* request,
                     CallbackType callback,
                     bool force_google_safesearch,
                     bool allow) {
    CEF_REQUIRE_IOT();

    if (allow) {
      // Update the URLRequest with only the values that have been changed by
      // the client.
      cef_request->Get(request, true);

      if (!!(cef_request->GetChanges() & CefRequestImpl::kChangedUrl)) {
        // If the URL was changed then redirect the request.
        GURL url = GURL(cef_request->GetURL().ToString());
        DCHECK_NE(url, request->url());
        new_url->Swap(&url);
      }
    }

    // Remove the association between the URLRequest and this object.
    Disconnector* disconnector =
        static_cast<Disconnector*>(request->GetUserData(UserDataKey()));
    DCHECK(disconnector);
    disconnector->Disconnect();
    request->RemoveUserData(UserDataKey());

    // Only execute the callback if the request has not been canceled.
    if (request->status().status() != net::URLRequestStatus::CANCELED) {
      if (force_google_safesearch && allow && new_url->is_empty())
        safe_search_util::ForceGoogleSafeSearch(request->url(), new_url);

      std::move(callback).Run(allow ? net::OK : net::ERR_ABORTED);
    }
  }

  static inline void* UserDataKey() { return &kLocatorKey; }

  CefRefPtr<CefRequestImpl> cef_request_;
  const GURL old_url_;
  GURL* new_url_;
  net::URLRequest* url_request_;
  bool force_google_safesearch_;
  CallbackType callback_;

  // The user data key.
  static int kLocatorKey;

  IMPLEMENT_REFCOUNTING(CefBeforeResourceLoadCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefBeforeResourceLoadCallbackImpl);
};

int CefBeforeResourceLoadCallbackImpl::kLocatorKey = 0;

class CefAuthCallbackImpl : public CefAuthCallback {
 public:
  typedef net::NetworkDelegate::AuthCallback CallbackType;

  CefAuthCallbackImpl(CallbackType callback, net::AuthCredentials* credentials)
      : callback_(std::move(callback)), credentials_(credentials) {}
  ~CefAuthCallbackImpl() override {
    if (!callback_.is_null()) {
      // The auth callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_IOT()) {
        CancelNow(std::move(callback_));
      } else {
        CEF_POST_TASK(CEF_IOT, base::Bind(&CefAuthCallbackImpl::CancelNow,
                                          base::Passed(std::move(callback_))));
      }
    }
  }

  void Continue(const CefString& username, const CefString& password) override {
    if (CEF_CURRENTLY_ON_IOT()) {
      if (!callback_.is_null()) {
        credentials_->Set(username, password);
        std::move(callback_).Run(
            net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);
      }
    } else {
      CEF_POST_TASK(CEF_IOT, base::Bind(&CefAuthCallbackImpl::Continue, this,
                                        username, password));
    }
  }

  void Cancel() override {
    if (CEF_CURRENTLY_ON_IOT()) {
      if (!callback_.is_null()) {
        CancelNow(std::move(callback_));
      }
    } else {
      CEF_POST_TASK(CEF_IOT, base::Bind(&CefAuthCallbackImpl::Cancel, this));
    }
  }

  CallbackType Disconnect() WARN_UNUSED_RESULT { return std::move(callback_); }

 private:
  static void CancelNow(CallbackType callback) {
    CEF_REQUIRE_IOT();
    std::move(callback).Run(
        net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION);
  }

  CallbackType callback_;
  net::AuthCredentials* credentials_;

  IMPLEMENT_REFCOUNTING(CefAuthCallbackImpl);
};

// Match the logic from ChromeNetworkDelegate and
// RenderFrameMessageFilter::OnSetCookie.
bool AreExperimentalCookieFeaturesEnabled() {
  static bool initialized = false;
  static bool enabled = false;
  if (!initialized) {
    enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    initialized = true;
  }
  return enabled;
}

}  // namespace

CefNetworkDelegate::CefNetworkDelegate() : force_google_safesearch_(nullptr) {}

CefNetworkDelegate::~CefNetworkDelegate() {}

std::unique_ptr<net::SourceStream> CefNetworkDelegate::CreateSourceStream(
    net::URLRequest* request,
    std::unique_ptr<net::SourceStream> upstream) {
  if (net_util::IsInternalRequest(request))
    return upstream;

  CefRefPtr<CefResponseFilter> cef_filter;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForRequest(request);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame = browser->GetFrameForRequest(request);

        CefRefPtr<CefRequestImpl> cefRequest = new CefRequestImpl();
        cefRequest->Set(request);
        cefRequest->SetReadOnly(true);

        CefRefPtr<CefResponseImpl> cefResponse = new CefResponseImpl();
        cefResponse->Set(request);
        cefResponse->SetReadOnly(true);

        cef_filter = handler->GetResourceResponseFilter(
            browser.get(), frame, cefRequest.get(), cefResponse.get());
      }
    }
  }

  if (cef_filter && cef_filter->InitFilter())
    return std::make_unique<CefSourceStream>(cef_filter, std::move(upstream));

  return upstream;
}

int CefNetworkDelegate::OnBeforeURLRequest(net::URLRequest* request,
                                           net::CompletionOnceCallback callback,
                                           GURL* new_url) {
  if (net_util::IsInternalRequest(request))
    return net::OK;

  const bool force_google_safesearch =
      (force_google_safesearch_ && force_google_safesearch_->GetValue());

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForRequest(request);
  if (browser.get()) {
    const CefBrowserSettings& browser_settings = browser->settings();
    if (browser_settings.accept_language_list.length > 0) {
      const std::string& accept_language =
          net::HttpUtil::GenerateAcceptLanguageHeader(
              CefString(&browser_settings.accept_language_list));
      request->SetExtraRequestHeaderByName(
          net::HttpRequestHeaders::kAcceptLanguage, accept_language, false);
    }
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame = browser->GetFrameForRequest(request);

        // Populate the request data.
        CefRefPtr<CefRequestImpl> requestPtr(new CefRequestImpl());
        requestPtr->Set(request);
        requestPtr->SetTrackChanges(true);

        CefRefPtr<CefBeforeResourceLoadCallbackImpl> callbackImpl(
            new CefBeforeResourceLoadCallbackImpl(requestPtr, new_url, request,
                                                  force_google_safesearch,
                                                  std::move(callback)));

        // Give the client an opportunity to evaluate the request.
        cef_return_value_t retval = handler->OnBeforeResourceLoad(
            browser.get(), frame, requestPtr.get(), callbackImpl.get());
        if (retval == RV_CANCEL) {
          // Cancel the request.
          callbackImpl->Continue(false);
        } else if (retval == RV_CONTINUE) {
          // Continue the request.
          callbackImpl->Continue(true);
        }

        // Continue or cancel the request asynchronously.
        return net::ERR_IO_PENDING;
      }
    }
  }

  if (force_google_safesearch && new_url->is_empty())
    safe_search_util::ForceGoogleSafeSearch(request->url(), new_url);

  // Continue the request immediately.
  return net::OK;
}

void CefNetworkDelegate::OnCompleted(net::URLRequest* request,
                                     bool started,
                                     int net_error) {
  if (net_util::IsInternalRequest(request))
    return;

  if (!started)
    return;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForRequest(request);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame = browser->GetFrameForRequest(request);

        CefRefPtr<CefRequestImpl> cefRequest = new CefRequestImpl();
        cefRequest->Set(request);
        cefRequest->SetReadOnly(true);

        CefRefPtr<CefResponseImpl> cefResponse = new CefResponseImpl();
        cefResponse->Set(request);
        cefResponse->SetReadOnly(true);

        cef_urlrequest_status_t status = UR_UNKNOWN;
        switch (request->status().status()) {
          case net::URLRequestStatus::SUCCESS:
            status = UR_SUCCESS;
            break;
          case net::URLRequestStatus::CANCELED:
            status = UR_CANCELED;
            break;
          case net::URLRequestStatus::FAILED:
            status = UR_FAILED;
            break;
          default:
            NOTREACHED();
            break;
        }

        const int64 received_content_length =
            request->received_response_content_length();
        handler->OnResourceLoadComplete(browser.get(), frame, cefRequest.get(),
                                        cefResponse.get(), status,
                                        received_content_length);
      }
    }
  }
}

net::NetworkDelegate::AuthRequiredResponse CefNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    AuthCallback callback,
    net::AuthCredentials* credentials) {
  if (net_util::IsInternalRequest(request))
    return AUTH_REQUIRED_RESPONSE_NO_ACTION;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForRequest(request);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame = browser->GetFrameForRequest(request);

        CefRefPtr<CefAuthCallbackImpl> callbackPtr(
            new CefAuthCallbackImpl(std::move(callback), credentials));
        if (handler->GetAuthCredentials(
                browser.get(), frame, auth_info.is_proxy,
                auth_info.challenger.host(), auth_info.challenger.port(),
                auth_info.realm, auth_info.scheme, callbackPtr.get())) {
          return AUTH_REQUIRED_RESPONSE_IO_PENDING;
        } else {
          callback = callbackPtr->Disconnect();
        }
      }
    }
  }

  CefURLRequestUserData* user_data =
      (CefURLRequestUserData*)request->GetUserData(
          CefURLRequestUserData::kUserDataKey);
  if (user_data) {
    CefRefPtr<CefURLRequestClient> client = user_data->GetClient();
    if (client.get()) {
      CefRefPtr<CefAuthCallbackImpl> callbackPtr(
          new CefAuthCallbackImpl(std::move(callback), credentials));
      if (client->GetAuthCredentials(
              auth_info.is_proxy, auth_info.challenger.host(),
              auth_info.challenger.port(), auth_info.realm, auth_info.scheme,
              callbackPtr.get())) {
        return AUTH_REQUIRED_RESPONSE_IO_PENDING;
      } else {
        callback = callbackPtr->Disconnect();
      }
    }
  }

  return AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool CefNetworkDelegate::OnCanGetCookies(const net::URLRequest& request,
                                         const net::CookieList& cookie_list,
                                         bool allowed_from_caller) {
  if (!allowed_from_caller)
    return false;
  if (net_util::IsInternalRequest(&request))
    return true;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForRequest(&request);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame = browser->GetFrameForRequest(&request);

        CefRefPtr<CefRequestImpl> cefRequest = new CefRequestImpl();
        cefRequest->Set(&request);
        cefRequest->SetReadOnly(true);

        return handler->CanGetCookies(browser.get(), frame, cefRequest.get());
      }
    }
  }

  return true;
}

bool CefNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                        const net::CanonicalCookie& cookie,
                                        net::CookieOptions* options,
                                        bool allowed_from_caller) {
  if (!allowed_from_caller)
    return false;
  if (net_util::IsInternalRequest(&request))
    return true;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForRequest(&request);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame = browser->GetFrameForRequest(&request);

        CefRefPtr<CefRequestImpl> cefRequest = new CefRequestImpl();
        cefRequest->Set(&request);
        cefRequest->SetReadOnly(true);

        CefCookie cefCookie;
        if (!CefCookieManagerImpl::GetCefCookie(cookie, cefCookie))
          return true;

        return handler->CanSetCookie(browser.get(), frame, cefRequest.get(),
                                     cefCookie);
      }
    }
  }

  return true;
}

bool CefNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  return true;
}

bool CefNetworkDelegate::OnAreExperimentalCookieFeaturesEnabled() const {
  return ::AreExperimentalCookieFeaturesEnabled();
}
