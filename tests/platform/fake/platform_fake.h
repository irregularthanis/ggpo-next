#ifndef _GGPO_TEST_PLATFORM_FAKE_H_
#define _GGPO_TEST_PLATFORM_FAKE_H_

#include "types.h"

namespace ggpo_test {

void ResetFakePlatform();
void FailNextSocketOpen();
int NetworkStartupCount();
int NetworkShutdownCount();

}

#endif
