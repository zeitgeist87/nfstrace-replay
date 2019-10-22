/*
 * nfstrace-replay - Small command line tool to replay file system traces
 * Copyright (C) 2014  Andreas Rohner
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

class Settings {
 private:
  static uint64_t parseTime(const char *input) {
    int year;
    int month;
    int day;

    if (sscanf(input, "%d-%d-%d", &year, &month, &day) == 3) {
      struct tm timeinfo;
      memset(&timeinfo, 0, sizeof(struct tm));

      char *tz = getenv("TZ");
      setenv("TZ", "", 1);
      tzset();

      timeinfo.tm_year = year - 1900;
      timeinfo.tm_mon = month - 1;
      timeinfo.tm_mday = day;

      if (tz)
        setenv("TZ", tz, 1);
      else
        unsetenv("TZ");
      tzset();

      return mktime(&timeinfo);
    }
    return -1;
  }

 public:
  bool writeZero = false;
  bool displayTime = true;
  bool debugOutput = false;
  int syncMinutes = 10;
  bool noSync = false;
  bool dataSync = false;
  bool inodeTest = false;
  bool enableGC = true;
  std::string reportPath;
  int64_t startTime = -1;
  int64_t endTime = -1;
  int startAfterDays = -1;
  int endAfterDays = -1;
  int syncFd = -1;

  void setStartTime(const char *time) {
    startTime = parseTime(time);

    if (startTime < 0) {
      int tmp = atoi(time);
      if (tmp > 0) startAfterDays = tmp;
    }
  }

  void setEndTime(const char *time) {
    endTime = parseTime(time);

    if (endTime < 0) {
      int tmp = atoi(time);
      if (tmp > 0) endAfterDays = tmp;
    }
  }
};

#endif /* SETTINGS_H_ */
