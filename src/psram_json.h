#pragma once
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

// ArduinoJson allocator that keeps documents in PSRAM, so big responses
// (favourite lists, Spotify pages) don't eat the small internal heap that
// WiFi/lwIP need.
struct PsramAllocator : ArduinoJson::Allocator {
  void *allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
  void deallocate(void *p) override { heap_caps_free(p); }
  void *reallocate(void *p, size_t n) override {
    return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  static ArduinoJson::Allocator *instance() {
    static PsramAllocator a;
    return &a;
  }
};
