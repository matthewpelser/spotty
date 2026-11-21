#include "buttons.h"

#include <Bounce2.h>

#include "actions.h"
#include "config.h"

namespace buttons {

namespace {

struct Binding {
  uint8_t pin;
  ActionType type;
  uint8_t index;
  bool repeat;  // auto-repeat while held (volume)
};

const Binding BINDINGS[] = {
    {PIN_PRESET[0], ActionType::Preset, 0, false},
    {PIN_PRESET[1], ActionType::Preset, 1, false},
    {PIN_PRESET[2], ActionType::Preset, 2, false},
    {PIN_PRESET[3], ActionType::Preset, 3, false},
    {PIN_PLAYPAUSE, ActionType::PlayPause, 0, false},
    {PIN_STOP, ActionType::Stop, 0, false},
    {PIN_VOL_DOWN, ActionType::VolDown, 0, true},
    {PIN_VOL_UP, ActionType::VolUp, 0, true},
    {PIN_NEXT, ActionType::Next, 0, false},
    {PIN_RANDOM, ActionType::Random, 0, false},
};
constexpr size_t COUNT = sizeof(BINDINGS) / sizeof(BINDINGS[0]);

Bounce2::Button g_buttons[COUNT];
uint32_t g_nextRepeat[COUNT];

}  // namespace

void begin() {
  for (size_t i = 0; i < COUNT; i++) {
    g_buttons[i].attach(BINDINGS[i].pin, INPUT_PULLUP);
    g_buttons[i].interval(DEBOUNCE_MS);
    g_buttons[i].setPressedState(LOW);
  }
}

void update() {
  uint32_t now = millis();
  for (size_t i = 0; i < COUNT; i++) {
    auto &b = g_buttons[i];
    const Binding &bind = BINDINGS[i];
    b.update();
    if (b.pressed()) {
      actions::post(bind.type, bind.index);
      g_nextRepeat[i] = now + VOLUME_REPEAT_MS * 2;  // a little delay before repeating
    } else if (bind.repeat && b.isPressed() && (int32_t)(now - g_nextRepeat[i]) >= 0) {
      actions::post(bind.type, bind.index);
      g_nextRepeat[i] = now + VOLUME_REPEAT_MS;
    }
  }
}

}  // namespace buttons
