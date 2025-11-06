/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <mutex>
#include <queue>

#include "task_scheduler.h"

namespace gmp {

/**
 * @brief Gothic-specific implementation of task scheduler.
 *
 * This implementation processes queued tasks during the render hook,
 * ensuring all tasks execute on the main/render thread.
 */
class GothicTaskScheduler : public TaskScheduler {
public:
  GothicTaskScheduler() = default;
  ~GothicTaskScheduler() override = default;

  // TaskScheduler implementation
  void ScheduleOnMainThread(Task task) override;

  /**
   * @brief Process all queued tasks.
   *
   * This should be called from the main/render thread, typically
   * from a render hook. Executes all queued tasks in FIFO order.
   */
  void ProcessTasks();

private:
  std::queue<Task> task_queue_;
  std::mutex queue_mutex_;
};

}  // namespace gmp
