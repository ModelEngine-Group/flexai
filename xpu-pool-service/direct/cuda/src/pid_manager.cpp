/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include <climits>
#include <fstream>
#include <filesystem>
#include <string>
#include <thread>
#include <cerrno>
#include <unistd.h>
#include "register.h"
#include "pid_manager.h"
#include "log.h"

using namespace std;
using namespace xpu;

int PidManager::Initialize()
{
  std::thread t(&PidManager::PidsConfigWatcherThread, this);
#ifndef UNIT_TEST
  t.detach();
#else
  t.join();
#endif
  return RegisterToDevicePlugin();
}

int PidManager::Refresh() {
  const string pidsPath(PidsPath());
  ifstream file(pidsPath);
  if (!file.is_open()) {
    FileOperateErrorHandler(file, pidsPath);
    return RET_FAIL;
  }

  string line;
  int base = 10;
  constexpr int valueWidth = 11;
  constexpr int shift = valueWidth + 1;
  constexpr int length = shift * 2 - 1;
  long firstValue;
  long secondValue;
  lock_guard<mutex> lock(pidsMapMutex_);
  pidsMap_.clear();
  while (getline(file, line)) {
    if (line.size() != length) {
      continue;
    }
    firstValue = strtol(line.c_str(), nullptr, base);
    if (firstValue <= 0 || firstValue > INT_MAX) {
      continue;
    }
    secondValue = strtol(line.c_str() + shift, nullptr, base);
    if (secondValue <= 0 || secondValue > INT_MAX) {
      continue;
    }
    pidsMap_[static_cast<int>(firstValue)] = static_cast<int>(secondValue);
  }

  return RET_SUCC;
}

int PidManager::GetContainerPid(int hostPid) {
  lock_guard<mutex> lock(pidsMapMutex_);
  auto iter = pidsMap_.find(hostPid);
  if (iter == pidsMap_.end()) {
    return INVALID_PID;
  }
  return iter->second;
}

void PidManager::ProcessEvent(inotify_event *event) {
  if (event->mask & IN_CREATE) {
    log_trace("file created : {}", event->name);
  } else if (event->mask & IN_MODIFY) {
    log_trace("file modified : {}", event->name);
  } else {
    return;
  }
  if (string(event->name) == PIDS_CONFIG_NAME) {
    log_trace("load pids config");
    int err = Refresh();
    if (err) {
      log_err("load pids config failed");
    }
  }
}

void PidManager::PidsConfigWatcherThread() {
  char buffer[BUFFER_SIZE];

  int fd = inotify_init();
  if (fd == -1) {
    return;
  }

  int wd = inotify_add_watch(fd, PidsDir().c_str(), IN_MODIFY | IN_CREATE);
  if (wd == -1) {
    close(fd);
    return;
  }

  if (std::filesystem::exists(PidsPath())) {
    Refresh();
  }

  while (true) {
    ssize_t num_read = read(fd, buffer, BUFFER_SIZE);
    if (num_read < static_cast<ssize_t>(sizeof(inotify_event))) {
      break;
    }

    for (char *ptr = buffer; ptr < buffer + num_read;) {
      inotify_event *event = reinterpret_cast<inotify_event*>(ptr);
      ProcessEvent(event);
      ptr += sizeof(inotify_event) + event->len;
    }
#ifdef UNIT_TEST
    break;
#endif
  }

  inotify_rm_watch(fd, wd);
  close(fd);
}
