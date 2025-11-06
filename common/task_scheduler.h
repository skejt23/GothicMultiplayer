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

#include <functional>

namespace gmp {

/**
 * @brief Interface for scheduling tasks to execute on the main/render thread.
 *
 * This is used to safely execute callbacks from background threads (e.g., network threads)
 * on the main thread where game engine operations are safe.
 *
 * Implementations should ensure thread-safety and execute tasks in FIFO order.
 */
class TaskScheduler {
public:
  using Task = std::function<void()>;

  virtual ~TaskScheduler() = default;

  /**
   * @brief Schedule a task to be executed on the main thread.
   *
   * This method is thread-safe and can be called from any thread.
   * The task will be queued and executed during the next main thread tick.
   *
   * @param task The task to execute. Should be lightweight and non-blocking.
   */
  virtual void ScheduleOnMainThread(Task task) = 0;
};

}  // namespace gmp
