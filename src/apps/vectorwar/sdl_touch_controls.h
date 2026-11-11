#ifndef _SDL_TOUCH_CONTROLS_H_
#define _SDL_TOUCH_CONTROLS_H_

/* On-screen controls for touch devices (the Android APK). Positions are
 * normalized window coordinates; the renderer stretches the 640x480 arena to
 * the window, so the same coordinates drive input and drawing. Each control
 * produces the same input bits as the keyboard. */

#include "vectorwar_inputs.h"
#include "vw_sdl.h"

namespace vw_touch {

const float kPadX = 0.14f, kPadY = 0.72f, kPadRadius = 0.13f, kDeadZone = 0.03f;
const float kBombX = 0.68f, kFireX = 0.84f, kButtonY = 0.62f, kButtonW = 0.14f, kButtonH = 0.30f;

inline int InputAt(float x, float y)
{
   int input = 0;
   const float dx = x - kPadX, dy = y - kPadY;
   if (dx * dx + dy * dy <= (kPadRadius * 1.6f) * (kPadRadius * 1.6f)) {
      if (dy < -kDeadZone) input |= INPUT_THRUST;
      if (dy > kDeadZone) input |= INPUT_BREAK;
      if (dx < -kDeadZone) input |= INPUT_ROTATE_LEFT;
      if (dx > kDeadZone) input |= INPUT_ROTATE_RIGHT;
   }
   if (y >= kButtonY && y <= kButtonY + kButtonH) {
      if (x >= kFireX && x <= kFireX + kButtonW) input |= INPUT_FIRE;
      if (x >= kBombX && x <= kBombX + kButtonW) input |= INPUT_BOMB;
   }
   return input;
}

/* Combined input of every finger currently on any touch device. */
inline int ReadInput()
{
   int input = 0;
   for (int device = 0; device < SDL_GetNumTouchDevices(); ++device) {
      const SDL_TouchID id = SDL_GetTouchDevice(device);
      for (int index = 0; index < SDL_GetNumTouchFingers(id); ++index) {
         const SDL_Finger *finger = SDL_GetTouchFinger(id, index);
         if (finger) input |= InputAt(finger->x, finger->y);
      }
   }
   return input;
}

/* Outline the controls in arena (logical) coordinates. */
inline void Draw(SDL_Renderer *renderer, int width, int height)
{
   SDL_SetRenderDrawColor(renderer, 90, 90, 90, SDL_ALPHA_OPAQUE);
   const int cx = (int)(kPadX * width), cy = (int)(kPadY * height);
   const int r = (int)(kPadRadius * height);
   SDL_Rect pad = { cx - r, cy - r, 2 * r, 2 * r };
   SDL_RenderDrawRect(renderer, &pad);
   SDL_RenderDrawLine(renderer, cx - r, cy, cx + r, cy);
   SDL_RenderDrawLine(renderer, cx, cy - r, cx, cy + r);
   SDL_Rect bomb = { (int)(kBombX * width), (int)(kButtonY * height),
                     (int)(kButtonW * width), (int)(kButtonH * height) };
   SDL_Rect fire = { (int)(kFireX * width), (int)(kButtonY * height),
                     (int)(kButtonW * width), (int)(kButtonH * height) };
   SDL_RenderDrawRect(renderer, &bomb);
   SDL_SetRenderDrawColor(renderer, 160, 60, 60, SDL_ALPHA_OPAQUE);
   SDL_RenderDrawRect(renderer, &fire);
}

}  // namespace vw_touch

#endif
