// Copyright Mihai Preda.

#pragma once

#include "Buffer.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>

template<typename T> class ConstBuffer;
template<typename T> class Buffer;

struct TimeInfo {
  double total{};
  u32 n{};

  bool operator<(const TimeInfo& rhs) const { return total > rhs.total; }
  void add(double deltaTime, u32 deltaN = 1) { total += deltaTime; n += deltaN; }
  void clear() { total = 0; n = 0; }
};

class Event : public EventHolder {
public:
  double secs() { return float(getEventNanos(this->get())) * 1e-9f; }
  bool isComplete() { return getEventInfo(this->get()) == CL_COMPLETE; }
};

using QueuePtr = std::shared_ptr<class Queue>;

class Queue : public QueueHolder {
  using TimeMap = std::map<std::string, TimeInfo>;
  TimeMap timeMap;
  std::vector<std::pair<Event, TimeMap::iterator>> events;
  bool profile{};
  bool cudaYield{};

public:
  Queue(cl_queue q, bool profile, bool cudaYield) : QueueHolder{q}, profile{profile}, cudaYield{cudaYield} {}  
  static QueuePtr make(const Context& context, bool profile, bool cudaYield) { return make_shared<Queue>(makeQueue(context.deviceId(), context.get(), profile), profile, cudaYield); }
    
  template<typename T> vector<T> read(const ConstBuffer<T>& buf, size_t sizeOrFull = 0) {
    auto size = sizeOrFull ? sizeOrFull : buf.size;
    assert(size <= buf.size);
    vector<T> ret(size);
    ::read(get(), true, buf.get(), size * sizeof(T), ret.data());
    return ret;
  }

  template<typename T> void readAsync(const ConstBuffer<T>& buf, vector<T>& out, size_t sizeOrFull = 0) {
    auto size = sizeOrFull ? sizeOrFull : buf.size();
    assert(size <= buf.size());
    out.resize(size);
    ::read(get(), false, buf.get(), size * sizeof(T), out.data());
  }
    
  template<typename T> void write(Buffer<T>& buf, const vector<T> &vect) {
    assert(vect.size() <= buf.size);
    ::write(get(), true, buf.get(), vect.size() * sizeof(T), vect.data());
  }

  template<typename T> void writeAsync(Buffer<T>& buf, const vector<T> &vect) {
    assert(vect.size() <= buf.size);
    ::write(get(), false, buf.get(), vect.size() * sizeof(T), vect.data());
  }
  
  void run(cl_kernel kernel, size_t groupSize, size_t workSize, const string &name) {
    Event event{::run(get(), kernel, groupSize, workSize, name)};
    auto it = profile ? timeMap.insert({name, TimeInfo{}}).first : timeMap.end();
    if (profile || events.empty()) {
      events.emplace_back(std::move(event), it);
    } else {
      events.front() = std::make_pair(std::move(event), it);
    }
  }

  bool allEventsCompleted() { return events.empty() || events.back().first.isComplete(); }
  
  void finish() {
    if (cudaYield) {
      ::flush(get());
      while (!allEventsCompleted()) { usleep(500); }
    }
    
    ::finish(get());
    
    if (profile) { for (auto& [event, it] : events) { it->second.add(event.secs()); } }
    events.clear();
  }

  using Profile = std::vector<std::pair<TimeInfo, std::string>>;
  Profile getProfile() {
    Profile profile;
    for (auto& [name, info] : timeMap) { profile.emplace_back(info, name); }
    std::sort(profile.begin(), profile.end());
    return profile;
  }

  void clearProfile() {
    events.clear();
    timeMap.clear();
  }
  
  template<typename T> void zero(Buffer<T>& buf, size_t sizeOrFull = 0) {
    auto size = sizeOrFull ? sizeOrFull : buf.size;
    assert(size <= buf.size);
    T zero = 0;
    fillBuf(get(), buf.get(), &zero, sizeof(T), size * sizeof(T));
  }
};
