#pragma once
// CardputerTask / CardputerTaskManager: cross-platform task abstraction
// Hardware (ESP32): real FreeRTOS tasks via xTaskCreatePinnedToCore.
// SDL/web: cooperative — each task's function is called once per
//          CardputerTaskManager::update(), which you call in loop().
//
// Usage:
//   CardputerTaskManager tasks;
//
//   CardputerTask t1, t2;
//
//   void setup() {
//     tasks.setup();
//     t1.start("blink", 2048, []() {
//       // on hardware: loops forever in its own FreeRTOS task
//       // on SDL/web: called once per tasks.update()
//       CardputerTask::delay(500);  // yields on SDL, real delay on hardware
//     }, &tasks);
//   }
//
//   void loop() {
//     tasks.update();  // no-op on hardware
//   }

#include <M5GFX.h>
#include <functional>
#include <vector>

#ifndef SDL_h_
#include "M5Cardputer.h"
#endif

// Forward declaration so CardputerTask::start() can accept a manager pointer.
class CardputerTaskManager;

// ---------------------------------------------------------------------------
// CardputerTask — one task
// ---------------------------------------------------------------------------
class CardputerTask {
public:
  // Start the task.
  //   name      : task name (FreeRTOS scheduler / debug label)
  //   stackSize : stack bytes (hardware only; ignored on SDL/web)
  //   fn        : work function. On hardware it is wrapped in a forever-loop
  //               inside the FreeRTOS task. On SDL it is called once per
  //               CardputerTaskManager::update() and should return quickly.
  //   manager   : optional — registers this task for SDL cooperative polling.
  //               Pass the global CardputerTaskManager instance.
  //   priority  : FreeRTOS priority (hardware only)
  //   core      : CPU core affinity, -1 = no pin (hardware only)
  void start(const char* name, uint32_t stackSize,
             std::function<void()> fn,
             CardputerTaskManager* manager = nullptr,
             int priority = 1, int core = -1);

  // Signal the task to stop. On hardware kills the FreeRTOS task.
  // On SDL prevents further calls from update().
  void stop() {
    _running = false;
#ifndef SDL_h_
    if (_handle) {
      vTaskDelete(reinterpret_cast<TaskHandle_t>(_handle));
      _handle = nullptr;
    }
#endif
  }

  bool isRunning() const { return _running; }

  // Portable delay. Use inside task functions instead of vTaskDelay/SDL_Delay.
  // On SDL this is a no-op (the task runs cooperatively — sleeping would block
  // the whole app). Structure SDL tasks as state machines if you need timing.
  static void delay(uint32_t ms) {
#ifndef SDL_h_
    vTaskDelay(pdMS_TO_TICKS(ms));
#else
    (void)ms;
#endif
  }

  // Called by CardputerTaskManager::update() — not for direct use.
  void _poll() {
#ifdef SDL_h_
    if (_running && _fn) _fn();
#endif
  }

private:
  std::function<void()> _fn;
  bool  _running = false;
  void* _handle  = nullptr;
};

// ---------------------------------------------------------------------------
// CardputerTaskManager — drives cooperative polling on SDL/web.
// On hardware all methods are no-ops (FreeRTOS handles scheduling).
// ---------------------------------------------------------------------------
class CardputerTaskManager {
public:
  void setup() {}

  // Register a task for SDL polling. Called automatically by
  // CardputerTask::start() when a manager pointer is provided.
  void add(CardputerTask* task) {
#ifdef SDL_h_
    _tasks.push_back(task);
#else
    (void)task;
#endif
  }

  // Poll all registered tasks. Call once per loop().
  // No-op on hardware.
  void update() {
#ifdef SDL_h_
    for (auto* t : _tasks) t->_poll();
#endif
  }

private:
#ifdef SDL_h_
  std::vector<CardputerTask*> _tasks;
#endif
};

// ---------------------------------------------------------------------------
// CardputerTask::start() — defined here after CardputerTaskManager is complete
// ---------------------------------------------------------------------------
inline void CardputerTask::start(const char* name, uint32_t stackSize,
                                  std::function<void()> fn,
                                  CardputerTaskManager* manager,
                                  int priority, int core) {
  _fn      = fn;
  _running = true;

  if (manager) manager->add(this);

#ifndef SDL_h_
  auto trampoline = [](void* arg) {
    auto* self = static_cast<CardputerTask*>(arg);
    while (self->_running) {
      self->_fn();
    }
    self->_handle = nullptr;
    vTaskDelete(nullptr);
  };
  if (core < 0) {
    xTaskCreate(trampoline, name, stackSize, this,
                priority, reinterpret_cast<TaskHandle_t*>(&_handle));
  } else {
    xTaskCreatePinnedToCore(trampoline, name, stackSize, this,
                            priority,
                            reinterpret_cast<TaskHandle_t*>(&_handle), core);
  }
#else
  (void)name; (void)stackSize; (void)priority; (void)core;
#endif
}
