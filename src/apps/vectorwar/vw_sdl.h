#ifndef _VW_SDL_H_
#define _VW_SDL_H_

/* System SDL2 packages install <SDL2/SDL.h>; SDL2 built from source (the
 * Android APK) exposes <SDL.h>. */
#if defined(__has_include)
#  if __has_include(<SDL2/SDL.h>)
#    include <SDL2/SDL.h>
#  else
#    include <SDL.h>
#  endif
#else
#  include <SDL2/SDL.h>
#endif

#endif
