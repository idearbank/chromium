// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/input_event_tracker.h"

#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ExpectationSet;
using ::testing::InSequence;

namespace remoting {
namespace protocol {

static const MouseEvent::MouseButton BUTTON_LEFT = MouseEvent::BUTTON_LEFT;
static const MouseEvent::MouseButton BUTTON_RIGHT = MouseEvent::BUTTON_RIGHT;

MATCHER_P2(EqualsVkeyEvent, keycode, pressed, "") {
  return arg.keycode() == keycode && arg.pressed() == pressed;
}

MATCHER_P2(EqualsUsbEvent, usb_keycode, pressed, "") {
  return arg.usb_keycode() == static_cast<uint32>(usb_keycode) &&
         arg.pressed() == pressed;
}

MATCHER_P4(EqualsMouseEvent, x, y, button, down, "") {
  return arg.x() == x && arg.y() == y && arg.button() == button &&
         arg.button_down() == down;
}

static KeyEvent NewVkeyEvent(int keycode, bool pressed) {
  KeyEvent event;
  event.set_keycode(keycode);
  event.set_pressed(pressed);
  return event;
}

static void PressAndReleaseVkey(InputStub* input_stub, int keycode) {
  input_stub->InjectKeyEvent(NewVkeyEvent(keycode, true));
  input_stub->InjectKeyEvent(NewVkeyEvent(keycode, false));
}

static KeyEvent NewUsbEvent(uint32 usb_keycode, bool pressed) {
  KeyEvent event;
  event.set_usb_keycode(usb_keycode);
  event.set_pressed(pressed);
  return event;
}

static void PressAndReleaseUsb(InputStub* input_stub,
                               uint32 usb_keycode) {
  input_stub->InjectKeyEvent(NewUsbEvent(usb_keycode, true));
  input_stub->InjectKeyEvent(NewUsbEvent(usb_keycode, false));
}

static KeyEvent NewVkeyUsbEvent(int keycode, int usb_keycode,
                                          bool pressed) {
  KeyEvent event;
  event.set_keycode(keycode);
  event.set_usb_keycode(usb_keycode);
  event.set_pressed(pressed);
  return event;
}

static MouseEvent NewMouseEvent(int x, int y,
    MouseEvent::MouseButton button, bool down) {
  MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  event.set_button(button);
  event.set_button_down(down);
  return event;
}

// Verify that keys that were pressed and released aren't re-released.
TEST(InputEventTrackerTest, NothingToRelease) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);

  {
    InSequence s;

    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, false)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, false)));

    EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_LEFT, true)));
    EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_LEFT, false)));
  }

  PressAndReleaseUsb(&input_tracker, 1);
  PressAndReleaseUsb(&input_tracker, 2);

  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_LEFT, true));
  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_LEFT, false));

  input_tracker.ReleaseAll();
}

// Verify that keys that were left pressed get released.
TEST(InputEventTrackerTest, ReleaseAllKeys) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  ExpectationSet injects;

  {
    InSequence s;

    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, false)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, false)));

    injects += EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_RIGHT, true)));
    injects += EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_LEFT, true)));
    injects += EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(1, 1, BUTTON_LEFT, false)));
  }

  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, false)))
      .After(injects);
  EXPECT_CALL(mock_stub,
              InjectMouseEvent(EqualsMouseEvent(1, 1, BUTTON_RIGHT, false)))
      .After(injects);

  input_tracker.InjectKeyEvent(NewUsbEvent(3, true));
  PressAndReleaseUsb(&input_tracker, 1);
  PressAndReleaseUsb(&input_tracker, 2);

  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_RIGHT, true));
  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_LEFT, true));
  input_tracker.InjectMouseEvent(NewMouseEvent(1, 1, BUTTON_LEFT, false));

  EXPECT_FALSE(input_tracker.IsKeyPressed(1));
  EXPECT_FALSE(input_tracker.IsKeyPressed(2));
  EXPECT_TRUE(input_tracker.IsKeyPressed(3));
  EXPECT_EQ(1, input_tracker.PressedKeyCount());

  input_tracker.ReleaseAll();
}

// Verify that we track both VK- and USB-based key events correctly.
TEST(InputEventTrackerTest, TrackVkeyAndUsb) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  ExpectationSet injects;

  {
    InSequence s;

    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsVkeyEvent(1, true)));
    injects += EXPECT_CALL(mock_stub,
        InjectKeyEvent(EqualsVkeyEvent(1, false)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsVkeyEvent(4, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(6, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(7, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(5, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(5, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, false)));
  }

  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, false)))
      .After(injects);
  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsVkeyEvent(4, false)))
      .After(injects);
  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(6, false)))
      .After(injects);
  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(7, false)))
      .After(injects);
  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(5, false)))
      .After(injects);

  input_tracker.InjectKeyEvent(NewUsbEvent(3, true));
  PressAndReleaseVkey(&input_tracker, 1);
  input_tracker.InjectKeyEvent(NewVkeyEvent(4, true));
  input_tracker.InjectKeyEvent(NewVkeyUsbEvent(5, 6, true));
  input_tracker.InjectKeyEvent(NewVkeyUsbEvent(5, 7, true));
  input_tracker.InjectKeyEvent(NewVkeyUsbEvent(6, 5, true));
  input_tracker.InjectKeyEvent(NewVkeyUsbEvent(7, 5, true));
  PressAndReleaseUsb(&input_tracker, 2);

  EXPECT_FALSE(input_tracker.IsKeyPressed(1));
  EXPECT_FALSE(input_tracker.IsKeyPressed(2));
  EXPECT_TRUE(input_tracker.IsKeyPressed(3));
  EXPECT_FALSE(input_tracker.IsKeyPressed(4)); // 4 was a VKEY.
  EXPECT_TRUE(input_tracker.IsKeyPressed(5));
  EXPECT_TRUE(input_tracker.IsKeyPressed(6));
  EXPECT_TRUE(input_tracker.IsKeyPressed(7));
  EXPECT_EQ(5, input_tracker.PressedKeyCount());

  input_tracker.ReleaseAll();
}

// Verify that invalid events get passed through but not tracked.
TEST(InputEventTrackerTest, InvalidEventsNotTracked) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  ExpectationSet injects;

  {
    InSequence s;

    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, false)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(_)).Times(3);
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsVkeyEvent(4, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, false)));
  }

  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, false)))
      .After(injects);
  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsVkeyEvent(4, false)))
      .After(injects);

  input_tracker.InjectKeyEvent(NewUsbEvent(3, true));
  PressAndReleaseUsb(&input_tracker, 1);

  KeyEvent invalid_event1;
  invalid_event1.set_pressed(true);
  input_tracker.InjectKeyEvent(invalid_event1);

  KeyEvent invalid_event2;
  invalid_event2.set_keycode(5);
  input_tracker.InjectKeyEvent(invalid_event2);

  KeyEvent invalid_event3;
  invalid_event3.set_usb_keycode(6);
  input_tracker.InjectKeyEvent(invalid_event3);

  input_tracker.InjectKeyEvent(NewVkeyEvent(4, true));
  PressAndReleaseUsb(&input_tracker, 2);

  EXPECT_FALSE(input_tracker.IsKeyPressed(1));
  EXPECT_FALSE(input_tracker.IsKeyPressed(2));
  EXPECT_TRUE(input_tracker.IsKeyPressed(3));
  EXPECT_FALSE(input_tracker.IsKeyPressed(4)); // Injected as VKEY.
  EXPECT_EQ(2, input_tracker.PressedKeyCount());

  input_tracker.ReleaseAll();
}

}  // namespace protocol
}  // namespace remoting
