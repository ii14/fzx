// Licensed under LGPLv3 - see LICENSE file for details.

#include "fzx/thread.hpp"

#include "fzx/macros.hpp"

namespace fzx {

void Thread::pin(int cpu)
{
  UNUSED(cpu);
#if 0
  if (cpu < 0)
    return;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);

  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
}

} // namespace fzx
