#ifndef PID_MANAGER_H
#define PID_MANAGER_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <sys/inotify.h>
#include "common.h"

class PidManager {
public:
  PidManager(const std::string &baseDir) : PIDS_CONFIG_DIR(baseDir), PIDS_CONFIG_PATH(baseDir + PIDS_CONFIG_NAME) {}
  int Initialize();
  int Refresh();
  int GetContainerPid(int hostPid);
  std::string PidsDir() const {
    return PIDS_CONFIG_DIR;
  }
  std::string_view PidsPath() const {
    return PIDS_CONFIG_PATH;
  }

  constexpr static int INVALID_PID = -1;

TESTABLE_PRIVATE:
  void ProcessEvent(inotify_event *event);
  void PidsConfigWatcherThread();

  const std::string PIDS_CONFIG_NAME = "pids.config";
  const int MAX_FILE_NAME_LEN = 255;
  const int MAX_INOTIFY_EVENT_CNT = 10;
  const int BUFFER_SIZE = MAX_INOTIFY_EVENT_CNT * (sizeof(inotify_event) + MAX_FILE_NAME_LEN + 1);
  const std::string PIDS_CONFIG_DIR;
  const std::string PIDS_CONFIG_PATH;

  std::mutex pidsMapMutex_;
  std::unordered_map<int, int> pidsMap_;
};

#endif
