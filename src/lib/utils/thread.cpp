/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2018 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "lib/utils/thread.h"
#include "lib/utils/clock.h"

namespace ebusd {

void* Thread::runThread(void* arg) {
  reinterpret_cast<Thread*>(arg)->enter();
  return nullptr;
}

Thread::~Thread() {
  if (m_started) {
    pthread_cancel(m_threadid);
    pthread_detach(m_threadid);
  }
}

bool Thread::start(const char* name) {
  int result = pthread_create(&m_threadid, nullptr, runThread, this);
  if (result == 0) {
#ifdef HAVE_PTHREAD_SETNAME_NP
#ifndef __MACH__
    pthread_setname_np(m_threadid, name);
#endif
#endif
    m_started = true;
    return true;
  }
  return false;
}

bool Thread::join() {
  int result = -1;
  if (m_started) {
    m_stopped = true;
    result = pthread_join(m_threadid, nullptr);
    if (result == 0) {
      m_started = false;
    }
  }
  return result == 0;
}

void Thread::enter() {
  m_running = true;
  run();
  m_running = false;
}


WaitThread::WaitThread()
  : Thread() {
  pthread_mutex_init(&m_mutex, nullptr);
  pthread_cond_init(&m_cond, nullptr);
}

WaitThread::~WaitThread() {
  pthread_mutex_destroy(&m_mutex);
  pthread_cond_destroy(&m_cond);
}

void WaitThread::stop() {
  pthread_mutex_lock(&m_mutex);
  pthread_cond_signal(&m_cond);
  pthread_mutex_unlock(&m_mutex);
  Thread::stop();
}

bool WaitThread::join() {
  pthread_mutex_lock(&m_mutex);
  pthread_cond_signal(&m_cond);
  pthread_mutex_unlock(&m_mutex);
  return Thread::join();
}

bool WaitThread::Wait(int seconds) {
  struct timespec t;
  clockGettime(&t);
  t.tv_sec += seconds;
  pthread_mutex_lock(&m_mutex);
  pthread_cond_timedwait(&m_cond, &m_mutex, &t);
  pthread_mutex_unlock(&m_mutex);
  return isRunning();
}

}  // namespace ebusd
