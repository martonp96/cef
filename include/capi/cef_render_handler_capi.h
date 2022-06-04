// Copyright (c) 2022 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ---------------------------------------------------------------------------
//
// This file was generated by the CEF translator tool and should not edited
// by hand. See the translator.README.txt file in the tools directory for
// more information.
//
// $hash=ff7e4de5301fb3f098c4267d04344ee69a9eb809$
//

#ifndef CEF_INCLUDE_CAPI_CEF_RENDER_HANDLER_CAPI_H_
#define CEF_INCLUDE_CAPI_CEF_RENDER_HANDLER_CAPI_H_
#pragma once

#include "include/capi/cef_accessibility_handler_capi.h"
#include "include/capi/cef_base_capi.h"
#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_drag_data_capi.h"

#ifdef __cplusplus
extern "C" {
#endif

///
// Implement this structure to handle events when window rendering is disabled.
// The functions of this structure will be called on the UI thread.
///
typedef struct _cef_render_handler_t {
  ///
  // Base structure.
  ///
  cef_base_ref_counted_t base;

  ///
  // Return the handler for accessibility notifications. If no handler is
  // provided the default implementation will be used.
  ///
  struct _cef_accessibility_handler_t*(CEF_CALLBACK* get_accessibility_handler)(
      struct _cef_render_handler_t* self);

  ///
  // Called to retrieve the root window rectangle in screen DIP coordinates.
  // Return true (1) if the rectangle was provided. If this function returns
  // false (0) the rectangle from GetViewRect will be used.
  ///
  int(CEF_CALLBACK* get_root_screen_rect)(struct _cef_render_handler_t* self,
                                          struct _cef_browser_t* browser,
                                          cef_rect_t* rect);

  ///
  // Called to retrieve the view rectangle in screen DIP coordinates. This
  // function must always provide a non-NULL rectangle.
  ///
  void(CEF_CALLBACK* get_view_rect)(struct _cef_render_handler_t* self,
                                    struct _cef_browser_t* browser,
                                    cef_rect_t* rect);

  ///
  // Called to retrieve the translation from view DIP coordinates to screen
  // coordinates. Windows/Linux should provide screen device (pixel) coordinates
  // and MacOS should provide screen DIP coordinates. Return true (1) if the
  // requested coordinates were provided.
  ///
  int(CEF_CALLBACK* get_screen_point)(struct _cef_render_handler_t* self,
                                      struct _cef_browser_t* browser,
                                      int viewX,
                                      int viewY,
                                      int* screenX,
                                      int* screenY);

  ///
  // Called to allow the client to fill in the CefScreenInfo object with
  // appropriate values. Return true (1) if the |screen_info| structure has been
  // modified.
  //
  // If the screen info rectangle is left NULL the rectangle from GetViewRect
  // will be used. If the rectangle is still NULL or invalid popups may not be
  // drawn correctly.
  ///
  int(CEF_CALLBACK* get_screen_info)(struct _cef_render_handler_t* self,
                                     struct _cef_browser_t* browser,
                                     struct _cef_screen_info_t* screen_info);

  ///
  // Called when the browser wants to show or hide the popup widget. The popup
  // should be shown if |show| is true (1) and hidden if |show| is false (0).
  ///
  void(CEF_CALLBACK* on_popup_show)(struct _cef_render_handler_t* self,
                                    struct _cef_browser_t* browser,
                                    int show);

  ///
  // Called when the browser wants to move or resize the popup widget. |rect|
  // contains the new location and size in view coordinates.
  ///
  void(CEF_CALLBACK* on_popup_size)(struct _cef_render_handler_t* self,
                                    struct _cef_browser_t* browser,
                                    const cef_rect_t* rect);

  ///
  // Called when an element should be painted. Pixel values passed to this
  // function are scaled relative to view coordinates based on the value of
  // CefScreenInfo.device_scale_factor returned from GetScreenInfo. |type|
  // indicates whether the element is the view or the popup widget. |buffer|
  // contains the pixel data for the whole image. |dirtyRects| contains the set
  // of rectangles in pixel coordinates that need to be repainted. |buffer| will
  // be |width|*|height|*4 bytes in size and represents a BGRA image with an
  // upper-left origin. This function is only called when
  // cef_window_tInfo::shared_texture_enabled is set to false (0).
  ///
  void(CEF_CALLBACK* on_paint)(struct _cef_render_handler_t* self,
                               struct _cef_browser_t* browser,
                               cef_paint_element_type_t type,
                               size_t dirtyRectsCount,
                               cef_rect_t const* dirtyRects,
                               const void* buffer,
                               int width,
                               int height);

  ///
  // Called when an element has been rendered to the shared texture handle.
  // |type| indicates whether the element is the view or the popup widget.
  // |dirtyRects| contains the set of rectangles in pixel coordinates that need
  // to be repainted. |shared_handle| is an OS specific type defined below. This
  // function is only called when cef_window_tInfo::shared_texture_enabled is
  // set to true (1) (1), and is currently only supported on Windows and Mac.
  //
  // Internally, there is a small queue of textures being sent round-robbin via
  // this call - clients may hold on to a texture reference until the next time
  // OnAcceleratedPaint is called, at which time the old texture must be
  // released. If a client needs the reference longer, it must be copied. It may
  // be assumed that every call to OnAcceleratedPaint is a different texture
  // handle than the immediately previous call.
  //
  // On Windows: |shared_handle| is the handle for a D3D11 Texture2D that can be
  // accessed via ID3D11Device1 using the OpenSharedResource1 function. The
  // texture was created with the flags: D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
  // D3D11_RESOURCE_MISC_SHARED_KEYED_MUTEX.  Clients must acquire the DXGI
  // keyed mutex via IDXGIKeyedMutex::AcquireSync with a value of 1, and release
  // it when finished with a value of 0.
  //
  // On Mac: |shared_handle| is an IOSurface wrapped in a mach_port_t. Clients
  // can access this by using:
  //  IOSurfaceLookupFromMachPort((mach_port_t)shared_handle)
  // and then using CGLTexImageIOSurface2D() to bind it to an OpenGL texture.
  // Clients will need to use GL_TEXTURE_RECTANGLE and not GL_TEXTURE_2D.
  ///
  void(CEF_CALLBACK* on_accelerated_paint)(struct _cef_render_handler_t* self,
                                           struct _cef_browser_t* browser,
                                           cef_paint_element_type_t type,
                                           size_t dirtyRectsCount,
                                           cef_rect_t const* dirtyRects,
                                           void* shared_handle);

  ///
  // Called when the user starts dragging content in the web view. Contextual
  // information about the dragged content is supplied by |drag_data|. (|x|,
  // |y|) is the drag start location in screen coordinates. OS APIs that run a
  // system message loop may be used within the StartDragging call.
  //
  // Return false (0) to abort the drag operation. Don't call any of
  // cef_browser_host_t::DragSource*Ended* functions after returning false (0).
  //
  // Return true (1) to handle the drag operation. Call
  // cef_browser_host_t::DragSourceEndedAt and DragSourceSystemDragEnded either
  // synchronously or asynchronously to inform the web view that the drag
  // operation has ended.
  ///
  int(CEF_CALLBACK* start_dragging)(struct _cef_render_handler_t* self,
                                    struct _cef_browser_t* browser,
                                    struct _cef_drag_data_t* drag_data,
                                    cef_drag_operations_mask_t allowed_ops,
                                    int x,
                                    int y);

  ///
  // Called when the web view wants to update the mouse cursor during a drag &
  // drop operation. |operation| describes the allowed operation (none, move,
  // copy, link).
  ///
  void(CEF_CALLBACK* update_drag_cursor)(struct _cef_render_handler_t* self,
                                         struct _cef_browser_t* browser,
                                         cef_drag_operations_mask_t operation);

  ///
  // Called when the scroll offset has changed.
  ///
  void(CEF_CALLBACK* on_scroll_offset_changed)(
      struct _cef_render_handler_t* self,
      struct _cef_browser_t* browser,
      double x,
      double y);

  ///
  // Called when the IME composition range has changed. |selected_range| is the
  // range of characters that have been selected. |character_bounds| is the
  // bounds of each character in view coordinates.
  ///
  void(CEF_CALLBACK* on_ime_composition_range_changed)(
      struct _cef_render_handler_t* self,
      struct _cef_browser_t* browser,
      const cef_range_t* selected_range,
      size_t character_boundsCount,
      cef_rect_t const* character_bounds);

  ///
  // Called when text selection has changed for the specified |browser|.
  // |selected_text| is the currently selected text and |selected_range| is the
  // character range.
  ///
  void(CEF_CALLBACK* on_text_selection_changed)(
      struct _cef_render_handler_t* self,
      struct _cef_browser_t* browser,
      const cef_string_t* selected_text,
      const cef_range_t* selected_range);

  ///
  // Called when an on-screen keyboard should be shown or hidden for the
  // specified |browser|. |input_mode| specifies what kind of keyboard should be
  // opened. If |input_mode| is CEF_TEXT_INPUT_MODE_NONE, any existing keyboard
  // for this browser should be hidden.
  ///
  void(CEF_CALLBACK* on_virtual_keyboard_requested)(
      struct _cef_render_handler_t* self,
      struct _cef_browser_t* browser,
      cef_text_input_mode_t input_mode);
} cef_render_handler_t;

#ifdef __cplusplus
}
#endif

#endif  // CEF_INCLUDE_CAPI_CEF_RENDER_HANDLER_CAPI_H_
