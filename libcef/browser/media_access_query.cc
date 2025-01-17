// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/media_access_query.h"

#include "include/cef_permission_handler.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/media_stream_registrar.h"
#include "libcef/common/cef_switches.h"

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace media_access_query {

namespace {

class CefMediaAccessQuery {
 public:
  using CallbackType = content::MediaResponseCallback;

  CefMediaAccessQuery(CefBrowserHostBase* const browser,
                      const content::MediaStreamRequest& request,
                      CallbackType&& callback)
      : browser_(browser), request_(request), callback_(std::move(callback)) {}

  CefMediaAccessQuery(CefMediaAccessQuery&& query)
      : browser_(query.browser_),
        request_(query.request_),
        callback_(std::move(query.callback_)) {}
  CefMediaAccessQuery& operator=(CefMediaAccessQuery&& query) {
    browser_ = query.browser_;
    request_ = query.request_;
    callback_ = std::move(query.callback_);
    return *this;
  }

  CefMediaAccessQuery(const CefMediaAccessQuery&) = delete;
  CefMediaAccessQuery& operator=(const CefMediaAccessQuery&) = delete;

  bool is_null() const { return callback_.is_null(); }

  uint32_t requested_permissions() const {
    int requested_permissions = CEF_MEDIA_PERMISSION_NONE;
    if (device_audio_requested()) {
      requested_permissions |= CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE;
    }
    if (device_video_requested()) {
      requested_permissions |= CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE;
    }
    if (desktop_audio_requested()) {
      requested_permissions |= CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE;
    }
    if (desktop_video_requested()) {
      requested_permissions |= CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE;
    }
    return requested_permissions;
  }

  [[nodiscard]] CallbackType DisconnectCallback() {
    return std::move(callback_);
  }

  void ExecuteCallback(uint32_t allowed_permissions) {
    CEF_REQUIRE_UIT();

    blink::mojom::MediaStreamRequestResult result;
    blink::mojom::StreamDevicesPtr stream_devices;

    if (allowed_permissions == CEF_MEDIA_PERMISSION_NONE) {
      result = blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
      stream_devices = blink::mojom::StreamDevices::New();
    } else {
      bool error = false;
      if (allowed_permissions == requested_permissions()) {
        stream_devices = GetRequestedMediaDevices();
      } else {
        stream_devices = GetAllowedMediaDevices(allowed_permissions, error);
      }
      result = error ? blink::mojom::MediaStreamRequestResult::INVALID_STATE
                     : blink::mojom::MediaStreamRequestResult::OK;
    }

    bool has_video = stream_devices->video_device.has_value();
    bool has_audio = stream_devices->audio_device.has_value();

    auto media_stream_ui =
        browser_->GetMediaStreamRegistrar()->MaybeCreateMediaStreamUI(
            has_video, has_audio);

    std::move(callback_).Run(*stream_devices, result,
                             std::move(media_stream_ui));
  }

 private:
  bool device_audio_requested() const {
    return request_.audio_type ==
           blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
  }

  bool device_video_requested() const {
    return request_.video_type ==
           blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE;
  }

  bool desktop_audio_requested() const {
    return (request_.audio_type ==
            blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE) ||
           (request_.audio_type ==
            blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE);
  }

  bool desktop_video_requested() const {
    return (request_.video_type ==
            blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) ||
           (request_.video_type ==
            blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE);
  }

  blink::mojom::StreamDevicesPtr GetRequestedMediaDevices() const {
    CEF_REQUIRE_UIT();

    blink::MediaStreamDevices audio_devices;
    blink::MediaStreamDevices video_devices;

    if (device_audio_requested()) {
      // Pick the desired device or fall back to the first available of the
      // given type.
      CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request_.requested_audio_device_id, true, false, &audio_devices);
    }

    if (device_video_requested()) {
      // Pick the desired device or fall back to the first available of the
      // given type.
      CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request_.requested_video_device_id, false, true, &video_devices);
    }

    if (desktop_audio_requested()) {
      audio_devices.push_back(blink::MediaStreamDevice(
          request_.audio_type, "loopback", "System Audio"));
    }

    if (desktop_video_requested()) {
      content::DesktopMediaID media_id;
      if (request_.requested_video_device_id.empty()) {
        media_id =
            content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                    -1 /* webrtc::kFullDesktopScreenId */);
      } else {
        media_id =
            content::DesktopMediaID::Parse(request_.requested_video_device_id);
      }
      video_devices.push_back(blink::MediaStreamDevice(
          request_.video_type, media_id.ToString(), "Screen"));
    }

    blink::mojom::StreamDevicesPtr stream_devices =
        blink::mojom::StreamDevices::New();
    blink::mojom::StreamDevices& devices =
        *stream_devices;

    // At most one audio device and one video device can be used in a stream.
    if (!audio_devices.empty())
      devices.audio_device = audio_devices.front();
    if (!video_devices.empty())
      devices.video_device = video_devices.front();

    return stream_devices;
  }

  blink::mojom::StreamDevicesPtr GetAllowedMediaDevices(
      uint32_t allowed_permissions,
      bool& error) {
    error = false;

    const auto req_permissions = requested_permissions();

    const bool device_audio_allowed =
        allowed_permissions & CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE;
    const bool device_video_allowed =
        allowed_permissions & CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE;
    const bool desktop_audio_allowed =
        allowed_permissions & CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE;
    const bool desktop_video_allowed =
        allowed_permissions & CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE;

    blink::mojom::StreamDevicesPtr stream_devices;

    // getDisplayMedia must always request video
    if (desktop_video_requested() &&
        (!desktop_video_allowed && desktop_audio_allowed)) {
      LOG(WARNING) << "Response to getDisplayMedia is not allowed to only "
                      "return Audio";
      error = true;
    } else if (!desktop_video_requested() &&
               req_permissions != allowed_permissions) {
      LOG(WARNING)
          << "Response to getUserMedia must match requested permissions ("
          << req_permissions << " vs " << allowed_permissions << ")";
      error = true;
    }

    if (error) {
      stream_devices = blink::mojom::StreamDevices::New();
    } else {
      if (!device_audio_allowed && !desktop_audio_allowed) {
        request_.audio_type = blink::mojom::MediaStreamType::NO_SERVICE;
      }
      if (!device_video_allowed && !desktop_video_allowed) {
        request_.video_type = blink::mojom::MediaStreamType::NO_SERVICE;
      }
      stream_devices = GetRequestedMediaDevices();
    }

    return stream_devices;
  }

  CefRefPtr<CefBrowserHostBase> browser_;
  content::MediaStreamRequest request_;
  CallbackType callback_;
};

class CefMediaAccessCallbackImpl : public CefMediaAccessCallback {
 public:
  using CallbackType = CefMediaAccessQuery;

  explicit CefMediaAccessCallbackImpl(CallbackType&& callback)
      : callback_(std::move(callback)) {}

  CefMediaAccessCallbackImpl(const CefMediaAccessCallbackImpl&) = delete;
  CefMediaAccessCallbackImpl& operator=(const CefMediaAccessCallbackImpl&) =
      delete;

  ~CefMediaAccessCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        RunNow(std::move(callback_), CEF_MEDIA_PERMISSION_NONE);
      } else {
        CEF_POST_TASK(
            CEF_UIT,
            base::BindOnce(&CefMediaAccessCallbackImpl::RunNow,
                           std::move(callback_), CEF_MEDIA_PERMISSION_NONE));
      }
    }
  }

  void Continue(uint32_t allowed_permissions) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        RunNow(std::move(callback_), allowed_permissions);
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefMediaAccessCallbackImpl::Continue, this,
                                   allowed_permissions));
    }
  }

  void Cancel() override { Continue(CEF_MEDIA_PERMISSION_NONE); }

  [[nodiscard]] CallbackType Disconnect() { return std::move(callback_); }
  bool IsDisconnected() const { return callback_.is_null(); }

 private:
  static void RunNow(CallbackType callback, uint32_t allowed_permissions) {
    callback.ExecuteCallback(allowed_permissions);
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefMediaAccessCallbackImpl);
};

bool CheckCommandLinePermission() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableMediaStream);
}

}  // namespace

bool CheckMediaAccessPermission(CefBrowserHostBase* browser,
                                content::RenderFrameHost* render_frame_host,
                                const GURL& security_origin,
                                blink::mojom::MediaStreamType type) {
  // Always allowed here. RequestMediaAccessPermission will be called.
  return true;
}

content::MediaResponseCallback RequestMediaAccessPermission(
    CefBrowserHostBase* browser,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    bool default_disallow) {
  CEF_REQUIRE_UIT();

  CefMediaAccessQuery query(browser, request, std::move(callback));

  if (CheckCommandLinePermission()) {
    // Allow all requested permissions.
    query.ExecuteCallback(query.requested_permissions());
    return base::NullCallback();
  }

  bool handled = false;

  if (auto client = browser->GetClient()) {
    if (auto handler = client->GetPermissionHandler()) {
      const auto requested_permissions = query.requested_permissions();
      CefRefPtr<CefMediaAccessCallbackImpl> callbackImpl(
          new CefMediaAccessCallbackImpl(std::move(query)));

      auto frame =
          browser->GetFrameForGlobalId(content::GlobalRenderFrameHostId(
              request.render_process_id, request.render_frame_id));
      if (!frame)
        frame = browser->GetMainFrame();
      handled = handler->OnRequestMediaAccessPermission(
          browser, frame, request.security_origin.spec(), requested_permissions,
          callbackImpl.get());
      if (!handled) {
        LOG_IF(ERROR, callbackImpl->IsDisconnected())
            << "Should return true from OnRequestMediaAccessPermission when "
               "executing the callback";
        query = callbackImpl->Disconnect();
      }
    }
  }

  if (!query.is_null()) {
    if (default_disallow && !handled) {
      // Disallow access by default.
      query.ExecuteCallback(CEF_MEDIA_PERMISSION_NONE);
    } else {
      // Proceed with default handling.
      return query.DisconnectCallback();
    }
  }

  return base::NullCallback();
}

}  // namespace media_access_query
