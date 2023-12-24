#include "fzx/tui/term_app.hpp"
#include "fzx/macros.hpp"
#include "fzx/tui/tty.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <thread>
#include <iostream>

extern "C" {
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
}

static std::atomic<bool> gQuitSignal { false };
static std::atomic<bool> gResizeSignal { false };

static void signalHandler(int sig)
{
  switch (sig) {
  case SIGINT:
  case SIGTERM:
  case SIGQUIT:
  case SIGHUP:
    gQuitSignal.store(true, std::memory_order_relaxed);
    return;
  case SIGWINCH:
    gResizeSignal.store(true, std::memory_order_relaxed);
    return;
  default:
    return;
  }
}

int main(int argc, char** argv)
{
  UNUSED(argc);
  UNUSED(argv);

  fzx::TermApp app;
  if (!app.mInput.open())
    return 1;
  if (!app.mTTY.open())
    return 1;
  if (auto err = app.mEventFd.open(); !err.empty())
    return 1;

  // TODO: command line option
  app.mFzx.setThreads(std::thread::hardware_concurrency());
  app.mFzx.setCallback([](void* app) {
    static_cast<fzx::TermApp*>(app)->mEventFd.notify();
  }, &app);

  // TODO: handle SIGTERM, SIGQUIT, SIGHUP
  struct sigaction sa {};
  sa.sa_handler = signalHandler;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGWINCH, &sa, nullptr);

  app.mFzx.start();

  auto processSignals = [&]() -> bool {
    if (gQuitSignal.load(std::memory_order_relaxed)) {
      gQuitSignal.store(false, std::memory_order_relaxed);
      app.quit(false);
      return true;
    }

    if (gResizeSignal.load(std::memory_order_relaxed)) {
      gResizeSignal.store(false, std::memory_order_relaxed);
      app.processResize();
    }

    return false;
  };

  while (app.running()) {
    fd_set fds {};
    FD_ZERO(&fds);
    int nfds = 0;
    auto addFd = [&](int fd) {
      if (fd == -1)
        return;
      FD_SET(fd, &fds);
      if (nfds < fd)
        nfds = fd;
    };

    addFd(app.mTTY.fd());
    addFd(app.mInput.fd());
    addFd(app.mEventFd.fd());

    if (processSignals())
      break;

    const int res = pselect(nfds + 1, &fds, nullptr, nullptr, nullptr, nullptr);

    if (res == -1) {
      if (errno != EINTR) {
        perror("select");
        exit(1);
      }
      processSignals();
    } else if (res > 0) {
      auto checkFd = [&fds](int fd) -> bool {
        if (fd == -1)
          return false;
        return FD_ISSET(fd, &fds);
      };
      if (checkFd(app.mTTY.fd()))
        app.processTTY();
      if (checkFd(app.mInput.fd()))
        app.processInput();
      if (checkFd(app.mEventFd.fd())) {
        app.mEventFd.consume();
        app.processWakeup();
      }
    }
  }
  app.mTTY.close();
  if (app.mStatus == fzx::Status::ExitSuccess) {
    if (app.mSelection.empty()) {
      std::cout << app.currentItem() << std::endl;
    } else {
      app.printSelection();
    }
  }
}
