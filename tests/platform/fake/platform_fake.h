#ifndef _GGPO_TEST_PLATFORM_FAKE_H_
#define _GGPO_TEST_PLATFORM_FAKE_H_

#include "types.h"

namespace ggpo_test {

void ResetFakePlatform();
void FailNextSocketOpen();
// The next UdpSocket::Send returns this result (IO_WOULD_BLOCK or IO_ERROR).
void FailNextSocketSend(ggpo_platform::IoResult result);
// Number of GGPO ASSERT failures reported to the fake adapter.
int AssertFailureCount();
int NetworkStartupCount();
int NetworkShutdownCount();

}

#endif
