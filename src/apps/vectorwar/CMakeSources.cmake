set(GGPO_EXAMPLES_VECTORWAR_INC_NOFILTER
	"gamestate.h"
	"gdi_renderer.h"
	"ggpo_perfmon.h"
	"nongamestate.h"
	"renderer.h"
	"state_contract.h"
	"vectorwar_host.h"
	"vectorwar_inputs.h"
	"vectorwar.h"
)

set(GGPO_EXAMPLES_VECTORWAR_SRC_NOFILTER
	"gamestate.cpp"
	"state_contract.cpp"
	"vectorwar.cpp"
)

set(GGPO_EXAMPLES_VECTORWAR_WIN32_INC
	"gdi_renderer.h"
	"ggpo_perfmon.h"
	"Resource.h"
	"targetver.h"
	"win32_host.h"
)

set(GGPO_EXAMPLES_VECTORWAR_WIN32_SRC
	"gdi_renderer.cpp"
	"ggpo_perfmon.cpp"
	"main.cpp"
	"win32_host.cpp"
)

set(GGPO_EXAMPLES_VECTORWAR_WIN32RES
	"VectorWar.rc"
)

set(GGPO_EXAMPLES_VECTORWAR_SDL_SRC
	"sdl_host.cpp"
	"sdl_host.h"
	"sdl_main.cpp"
	"sdl_renderer.cpp"
	"sdl_renderer.h"
	"sdl_touch_controls.h"
	"vw_sdl.h"
)

source_group("Portable core" FILES ${GGPO_EXAMPLES_VECTORWAR_INC_NOFILTER} ${GGPO_EXAMPLES_VECTORWAR_SRC_NOFILTER})
source_group("Windows host" FILES ${GGPO_EXAMPLES_VECTORWAR_WIN32_INC} ${GGPO_EXAMPLES_VECTORWAR_WIN32_SRC} ${GGPO_EXAMPLES_VECTORWAR_WIN32RES})
source_group("SDL host" FILES ${GGPO_EXAMPLES_VECTORWAR_SDL_SRC})

set(GGPO_EXAMPLES_VECTORWAR_SRC
	${GGPO_EXAMPLES_VECTORWAR_INC_NOFILTER}
	${GGPO_EXAMPLES_VECTORWAR_SRC_NOFILTER}
)

# Matchmaking client codec and hole punching with relay fallback; portable
# and free of GGPO, so its contract test runs on every platform.
set(GGPO_EXAMPLES_VECTORWAR_NET_SRC
	"net/vw_net_protocol.cpp"
	"net/vw_net_protocol.h"
	"net/vw_net_types.cpp"
	"net/vw_net_types.h"
	"net/vw_path_probe.cpp"
	"net/vw_path_probe.h"
	"net/vw_path_selector.cpp"
	"net/vw_path_selector.h"
	"net/vw_siphash.cpp"
	"net/vw_siphash.h"
)
