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

source_group("Portable core" FILES ${GGPO_EXAMPLES_VECTORWAR_INC_NOFILTER} ${GGPO_EXAMPLES_VECTORWAR_SRC_NOFILTER})
source_group("Windows host" FILES ${GGPO_EXAMPLES_VECTORWAR_WIN32_INC} ${GGPO_EXAMPLES_VECTORWAR_WIN32_SRC} ${GGPO_EXAMPLES_VECTORWAR_WIN32RES})

set(GGPO_EXAMPLES_VECTORWAR_SRC
	${GGPO_EXAMPLES_VECTORWAR_INC_NOFILTER}
	${GGPO_EXAMPLES_VECTORWAR_SRC_NOFILTER}
)
