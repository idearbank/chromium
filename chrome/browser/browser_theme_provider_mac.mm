// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_theme_provider.h"

#import <Cocoa/Cocoa.h>

#include "app/gfx/color_utils.h"
#include "base/logging.h"
#include "chrome/browser/browser_theme_pack.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"

NSString* const kBrowserThemeDidChangeNotification =
    @"BrowserThemeDidChangeNotification";

namespace {

void HSLToHSB(const color_utils::HSL& hsl, CGFloat* h, CGFloat* s, CGFloat* b) {
  SkColor color = color_utils::HSLToSkColor(hsl, 255);  // alpha doesn't matter
  SkScalar hsv[3];
  SkColorToHSV(color, hsv);

  *h = SkScalarToDouble(hsv[0]) / 360.0;
  *s = SkScalarToDouble(hsv[1]);
  *b = SkScalarToDouble(hsv[2]);
}

}

NSImage* BrowserThemeProvider::GetNSImageNamed(int id,
                                               bool allow_default) const {
  DCHECK(CalledOnValidThread());

  if (!allow_default && !HasCustomImage(id))
    return nil;

  // Check to see if we already have the image in the cache.
  NSImageMap::const_iterator nsimage_iter = nsimage_cache_.find(id);
  if (nsimage_iter != nsimage_cache_.end())
    return nsimage_iter->second;

  // Why don't we load the file directly into the image instead of the whole
  // SkBitmap > native conversion?
  // - For consistency with other platforms.
  // - To get the generated tinted images.
  SkBitmap* bitmap = GetBitmapNamed(id);
  NSImage* nsimage = gfx::SkBitmapToNSImage(*bitmap);

  // We loaded successfully.  Cache the image.
  if (nsimage) {
    nsimage_cache_[id] = [nsimage retain];
    return nsimage;
  }

  // We failed to retrieve the bitmap, show a debugging red square.
  LOG(WARNING) << "Unable to load NSImage with id " << id;
  NOTREACHED();  // Want to assert in debug mode.

  static NSImage* empty_image = NULL;
  if (!empty_image) {
    // The placeholder image is bright red so people notice the problem.  This
    // image will be leaked, but this code should never be hit.
    NSRect image_rect = NSMakeRect(0, 0, 32, 32);
    empty_image = [[NSImage alloc] initWithSize:image_rect.size];
    [empty_image lockFocus];
    [[NSColor redColor] set];
    NSRectFill(image_rect);
    [empty_image unlockFocus];
  }

  return empty_image;
}

NSColor* BrowserThemeProvider::GetNSImageColorNamed(int id,
                                                    bool allow_default) const {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the color in the cache.
  NSColorMap::const_iterator nscolor_iter = nscolor_cache_.find(id);
  if (nscolor_iter != nscolor_cache_.end()) {
    bool cached_is_default = nscolor_iter->second.second;
    if (!cached_is_default || allow_default)
      return nscolor_iter->second.first;
  }

  NSImage* image = GetNSImageNamed(id, allow_default);
  if (!image)
    return nil;
  NSColor* image_color = [NSColor colorWithPatternImage:image];

  // We loaded successfully.  Cache the color.
  if (image_color) {
    nscolor_cache_[id] = std::make_pair([image_color retain],
                                        !HasCustomImage(id));
  }

  return image_color;
}

NSColor* BrowserThemeProvider::GetNSColor(int id,
                                          bool allow_default) const {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the color in the cache.
  NSColorMap::const_iterator nscolor_iter = nscolor_cache_.find(id);
  if (nscolor_iter != nscolor_cache_.end()) {
    bool cached_is_default = nscolor_iter->second.second;
    if (!cached_is_default || allow_default)
      return nscolor_iter->second.first;
  }

  bool is_default = false;
  SkColor sk_color;
  if (theme_pack_.get() && theme_pack_->GetColor(id, &sk_color)) {
    is_default = false;
  } else {
    is_default = true;
    sk_color = GetDefaultColor(id);
  }

  if (is_default && !allow_default)
    return nil;

  NSColor* color = [NSColor
      colorWithCalibratedRed:SkColorGetR(sk_color)/255.0
                       green:SkColorGetG(sk_color)/255.0
                        blue:SkColorGetB(sk_color)/255.0
                       alpha:SkColorGetA(sk_color)/255.0];

  // We loaded successfully.  Cache the color.
  if (color)
    nscolor_cache_[id] = std::make_pair([color retain], is_default);

  return color;
}

NSColor* BrowserThemeProvider::GetNSColorTint(int id,
                                              bool allow_default) const {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the color in the cache.
  NSColorMap::const_iterator nscolor_iter = nscolor_cache_.find(id);
  if (nscolor_iter != nscolor_cache_.end()) {
    bool cached_is_default = nscolor_iter->second.second;
    if (!cached_is_default || allow_default)
      return nscolor_iter->second.first;
  }

  bool is_default = false;
  color_utils::HSL tint;
  if (theme_pack_.get() && theme_pack_->GetTint(id, &tint)) {
    is_default = false;
  } else {
    is_default = true;
    tint = GetDefaultTint(id);
  }

  if (is_default && !allow_default)
    return nil;

  NSColor* tint_color = nil;
  if (tint.h == -1 && tint.s == -1 && tint.l == -1) {
    tint_color = [NSColor blackColor];
  } else {
    CGFloat hue, saturation, brightness;
    HSLToHSB(tint, &hue, &saturation, &brightness);

    tint_color = [NSColor colorWithCalibratedHue:hue
                                      saturation:saturation
                                      brightness:brightness
                                           alpha:1.0];
  }

  // We loaded successfully.  Cache the color.
  if (tint_color)
    nscolor_cache_[id] = std::make_pair([tint_color retain], is_default);

  return tint_color;
}

NSGradient* BrowserThemeProvider::GetNSGradient(int id) const {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the gradient in the cache.
  NSGradientMap::const_iterator nsgradient_iter = nsgradient_cache_.find(id);
  if (nsgradient_iter != nsgradient_cache_.end())
    return nsgradient_iter->second;

  NSGradient* gradient = nil;

  // Note that we are not leaking when we assign a retained object to
  // |gradient|; in all cases we cache it before we return.
  switch (id) {
    case GRADIENT_FRAME_INCOGNITO:
    case GRADIENT_FRAME_INCOGNITO_INACTIVE: {
      // TODO(avi): can we simplify this?
      BOOL active = id == GRADIENT_FRAME_INCOGNITO;
      NSColor* base_color = [NSColor colorWithCalibratedRed:83/255.0
                                                      green:108.0/255.0
                                                       blue:140/255.0
                                                      alpha:1.0];

      CGFloat luminance = [base_color gtm_luminance];

      // Adjust luminance so it never hits black.
      if (luminance < 0.5) {
        CGFloat adjustment = (0.5 - luminance) / 1.5;
        base_color = [base_color gtm_colorByAdjustingLuminance:adjustment];
      }

      NSColor *start_color =
          [base_color gtm_colorAdjustedFor:GTMColorationBaseMidtone
                                     faded:!active];
      NSColor *end_color =
          [base_color gtm_colorAdjustedFor:GTMColorationBaseShadow
                                     faded:!active];

      if (!active) {
        start_color = [start_color gtm_colorByAdjustingLuminance:0.1
                                                      saturation:0.5];
        end_color = [end_color gtm_colorByAdjustingLuminance:0.1
                                                  saturation:0.5];
      }

      gradient = [[NSGradient alloc] initWithStartingColor:start_color
                                               endingColor:end_color];
      break;
    }

    case GRADIENT_TOOLBAR:
    case GRADIENT_TOOLBAR_INACTIVE: {
      NSColor* base_color = [NSColor colorWithCalibratedWhite:0.5 alpha:1.0];
      BOOL faded = (id == GRADIENT_TOOLBAR_INACTIVE ) ||
                   (id == GRADIENT_TOOLBAR_BUTTON_INACTIVE);
      NSColor* start_color =
          [base_color gtm_colorAdjustedFor:GTMColorationLightHighlight
                                     faded:faded];
      NSColor* mid_color =
          [base_color gtm_colorAdjustedFor:GTMColorationLightMidtone
                                     faded:faded];
      NSColor* end_color =
          [base_color gtm_colorAdjustedFor:GTMColorationLightShadow
                                     faded:faded];
      NSColor* glow_color =
          [base_color gtm_colorAdjustedFor:GTMColorationLightPenumbra
                                     faded:faded];

      gradient =
          [[NSGradient alloc] initWithColorsAndLocations:start_color, 0.0,
                                                         mid_color, 0.25,
                                                         end_color, 0.5,
                                                         glow_color, 0.75,
                                                         nil];
      break;
    }

    case GRADIENT_TOOLBAR_BUTTON:
    case GRADIENT_TOOLBAR_BUTTON_INACTIVE: {
      NSColor* start_color = [NSColor colorWithCalibratedWhite:1.0 alpha:0.0];
      NSColor* end_color = [NSColor colorWithCalibratedWhite:1.0 alpha:0.3];
      gradient = [[NSGradient alloc] initWithStartingColor:start_color
                                               endingColor:end_color];
      break;
    }
    case GRADIENT_TOOLBAR_BUTTON_PRESSED:
    case GRADIENT_TOOLBAR_BUTTON_PRESSED_INACTIVE: {
      NSColor* base_color = [NSColor colorWithCalibratedWhite:0.5 alpha:1.0];
      BOOL faded = id == GRADIENT_TOOLBAR_BUTTON_PRESSED_INACTIVE;
      NSColor* start_color =
          [base_color gtm_colorAdjustedFor:GTMColorationBaseShadow
                                     faded:faded];
      NSColor* end_color =
          [base_color gtm_colorAdjustedFor:GTMColorationBaseMidtone
                                     faded:faded];

      gradient = [[NSGradient alloc] initWithStartingColor:start_color
                                               endingColor:end_color];
      break;
    }
    default:
      LOG(WARNING) << "Gradient request with unknown id " << id;
      NOTREACHED();  // Want to assert in debug mode.
      break;
  }

  // We loaded successfully.  Cache the gradient.
  if (gradient)
    nsgradient_cache_[id] = gradient;  // created retained

  return gradient;
}

// Let all the browser views know that themes have changed in a platform way.
void BrowserThemeProvider::NotifyPlatformThemeChanged() {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter postNotificationName:kBrowserThemeDidChangeNotification
                               object:[NSValue valueWithPointer:this]];
}

void BrowserThemeProvider::FreePlatformCaches() {
  DCHECK(CalledOnValidThread());

  // Free images.
  for (NSImageMap::iterator i = nsimage_cache_.begin();
       i != nsimage_cache_.end(); i++) {
    [i->second release];
  }
  nsimage_cache_.clear();

  // Free colors.
  for (NSColorMap::iterator i = nscolor_cache_.begin();
       i != nscolor_cache_.end(); i++) {
    [i->second.first release];
  }
  nscolor_cache_.clear();

  // Free gradients.
  for (NSGradientMap::iterator i = nsgradient_cache_.begin();
       i != nsgradient_cache_.end(); i++) {
    [i->second release];
  }
  nsgradient_cache_.clear();
}
