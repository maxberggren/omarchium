// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#include "omarchium/browser/theme/hyprland_ipc.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/posix/eintr_wrapper.h"
#include "base/values.h"

namespace omarchium::hyprland {

namespace {

std::optional<base::FilePath> SocketDir() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::optional<std::string> sig = env->GetVar("HYPRLAND_INSTANCE_SIGNATURE");
  if (!sig || sig->empty() || sig->find('/') != std::string::npos) {
    return std::nullopt;
  }
  std::string runtime = env->GetVar("XDG_RUNTIME_DIR").value_or("/tmp");
  return base::FilePath(runtime).Append("hypr").Append(*sig);
}

base::ScopedFD Connect(const char* name, bool nonblocking) {
  std::optional<base::FilePath> dir = SocketDir();
  if (!dir) {
    return {};
  }
  const std::string path = dir->Append(name).value();
  sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;
  if (path.size() >= sizeof(addr.sun_path)) {
    return {};
  }
  memcpy(addr.sun_path, path.c_str(), path.size() + 1);
  base::ScopedFD fd(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (!fd.is_valid() ||
      HANDLE_EINTR(connect(fd.get(), reinterpret_cast<sockaddr*>(&addr),
                           sizeof(addr))) != 0) {
    return {};
  }
  if (nonblocking) {
    fcntl(fd.get(), F_SETFL, fcntl(fd.get(), F_GETFL) | O_NONBLOCK);
  } else {
    timeval timeout = {.tv_sec = 1, .tv_usec = 0};
    setsockopt(fd.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd.get(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  }
  return fd;
}

}  // namespace

std::optional<int> QueryRounding() {
  base::ScopedFD fd = Connect(".socket.sock", /*nonblocking=*/false);
  if (!fd.is_valid()) {
    return std::nullopt;
  }
  static constexpr char kRequest[] = "j/getoption decoration:rounding";
  if (HANDLE_EINTR(write(fd.get(), kRequest, sizeof(kRequest) - 1)) < 0) {
    return std::nullopt;
  }
  std::string reply;
  char buf[512];
  for (;;) {
    ssize_t n = HANDLE_EINTR(read(fd.get(), buf, sizeof(buf)));
    if (n <= 0 || reply.size() > 4096) {
      break;
    }
    reply.append(buf, n);
  }
  // {"option": "decoration:rounding", "int": 6, "set": true}
  std::optional<base::DictValue> json =
      base::JSONReader::ReadDict(reply, base::JSON_PARSE_RFC);
  std::optional<int> rounding = json ? json->FindInt("int") : std::nullopt;
  if (rounding && *rounding >= 0) {
    return rounding;
  }
  return std::nullopt;
}

ConfigReloadListener::ConfigReloadListener(
    base::RepeatingClosure on_config_reloaded)
    : on_config_reloaded_(std::move(on_config_reloaded)),
      fd_(Connect(".socket2.sock", /*nonblocking=*/true)) {
  if (fd_.is_valid()) {
    controller_ = base::FileDescriptorWatcher::WatchReadable(
        fd_.get(), base::BindRepeating(&ConfigReloadListener::OnReadable,
                                       base::Unretained(this)));
  }
}

ConfigReloadListener::~ConfigReloadListener() = default;

void ConfigReloadListener::OnReadable() {
  char buf[4096];
  bool reloaded = false;
  for (;;) {
    ssize_t n = HANDLE_EINTR(read(fd_.get(), buf, sizeof(buf)));
    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
      // Hyprland went away; stop listening.
      controller_.reset();
      fd_.reset();
      break;
    }
    if (n < 0) {
      break;
    }
    pending_.append(buf, n);
    size_t nl;
    while ((nl = pending_.find('\n')) != std::string::npos) {
      // Events look like "configreloaded>>".
      if (pending_.compare(0, 14, "configreloaded") == 0) {
        reloaded = true;
      }
      pending_.erase(0, nl + 1);
    }
  }
  if (reloaded) {
    on_config_reloaded_.Run();
  }
}

}  // namespace omarchium::hyprland
