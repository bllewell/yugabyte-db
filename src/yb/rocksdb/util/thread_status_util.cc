// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//
// The following only applies to changes made to this file as part of YugaByte development.
//
// Portions Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.  See the License for the specific language governing permissions and limitations
// under the License.
//

#include "yb/rocksdb/util/thread_status_util.h"

#include "yb/rocksdb/env.h"
#include "yb/rocksdb/util/thread_status_updater.h"

namespace rocksdb {


#if ROCKSDB_USING_THREAD_STATUS
__thread ThreadStatusUpdater*
    ThreadStatusUtil::thread_updater_local_cache_ = nullptr;
__thread bool ThreadStatusUtil::thread_updater_initialized_ = false;

void ThreadStatusUtil::RegisterThread(
    const Env* env, ThreadStatus::ThreadType thread_type) {
  if (!MaybeInitThreadLocalUpdater(env)) {
    return;
  }
  assert(thread_updater_local_cache_);
  thread_updater_local_cache_->RegisterThread(
      thread_type, env->GetThreadID());
}

void ThreadStatusUtil::UnregisterThread() {
  thread_updater_initialized_ = false;
  if (thread_updater_local_cache_ != nullptr) {
    thread_updater_local_cache_->UnregisterThread();
    thread_updater_local_cache_ = nullptr;
  }
}

void ThreadStatusUtil::SetColumnFamily(const ColumnFamilyData* cfd,
                                       const Env* env,
                                       bool enable_thread_tracking) {
  if (!MaybeInitThreadLocalUpdater(env)) {
    return;
  }
  assert(thread_updater_local_cache_);
  if (cfd != nullptr && enable_thread_tracking) {
    thread_updater_local_cache_->SetColumnFamilyInfoKey(cfd);
  } else {
    // When cfd == nullptr or enable_thread_tracking == false, we set
    // ColumnFamilyInfoKey to nullptr, which makes SetThreadOperation
    // and SetThreadState become no-op.
    thread_updater_local_cache_->SetColumnFamilyInfoKey(nullptr);
  }
}

void ThreadStatusUtil::SetThreadOperation(ThreadStatus::OperationType op) {
  if (thread_updater_local_cache_ == nullptr) {
    // thread_updater_local_cache_ must be set in SetColumnFamily
    // or other ThreadStatusUtil functions.
    return;
  }

  if (op != ThreadStatus::OP_UNKNOWN) {
    uint64_t current_time = Env::Default()->NowMicros();
    thread_updater_local_cache_->SetOperationStartTime(current_time);
  } else {
    // TDOO(yhchiang): we could report the time when we set operation to
    // OP_UNKNOWN once the whole instrumentation has been done.
    thread_updater_local_cache_->SetOperationStartTime(0);
  }
  thread_updater_local_cache_->SetThreadOperation(op);
}

ThreadStatus::OperationStage ThreadStatusUtil::SetThreadOperationStage(
    ThreadStatus::OperationStage stage) {
  if (thread_updater_local_cache_ == nullptr) {
    // thread_updater_local_cache_ must be set in SetColumnFamily
    // or other ThreadStatusUtil functions.
    return ThreadStatus::STAGE_UNKNOWN;
  }

  return thread_updater_local_cache_->SetThreadOperationStage(stage);
}

void ThreadStatusUtil::SetThreadOperationProperty(
    int code, uint64_t value) {
  if (thread_updater_local_cache_ == nullptr) {
    // thread_updater_local_cache_ must be set in SetColumnFamily
    // or other ThreadStatusUtil functions.
    return;
  }

  thread_updater_local_cache_->SetThreadOperationProperty(
      code, value);
}

void ThreadStatusUtil::IncreaseThreadOperationProperty(
    int code, uint64_t delta) {
  if (thread_updater_local_cache_ == nullptr) {
    // thread_updater_local_cache_ must be set in SetColumnFamily
    // or other ThreadStatusUtil functions.
    return;
  }

  thread_updater_local_cache_->IncreaseThreadOperationProperty(
      code, delta);
}

void ThreadStatusUtil::SetThreadState(ThreadStatus::StateType state) {
  if (thread_updater_local_cache_ == nullptr) {
    // thread_updater_local_cache_ must be set in SetColumnFamily
    // or other ThreadStatusUtil functions.
    return;
  }

  thread_updater_local_cache_->SetThreadState(state);
}

void ThreadStatusUtil::ResetThreadStatus() {
  if (thread_updater_local_cache_ == nullptr) {
    return;
  }
  thread_updater_local_cache_->ResetThreadStatus();
}

void ThreadStatusUtil::NewColumnFamilyInfo(const DB* db,
                                           const ColumnFamilyData* cfd,
                                           const std::string& cf_name,
                                           const Env* env) {
  if (!MaybeInitThreadLocalUpdater(env)) {
    return;
  }
  assert(thread_updater_local_cache_);
  if (thread_updater_local_cache_) {
    thread_updater_local_cache_->NewColumnFamilyInfo(db, db->GetName(), cfd,
                                                     cf_name);
  }
}

void ThreadStatusUtil::EraseColumnFamilyInfo(
    const ColumnFamilyData* cfd) {
  if (thread_updater_local_cache_ == nullptr) {
    return;
  }
  thread_updater_local_cache_->EraseColumnFamilyInfo(cfd);
}

void ThreadStatusUtil::EraseDatabaseInfo(const DB* db) {
  if (thread_updater_local_cache_ == nullptr) {
    return;
  }
  thread_updater_local_cache_->EraseDatabaseInfo(db);
}

bool ThreadStatusUtil::MaybeInitThreadLocalUpdater(const Env* env) {
  if (!thread_updater_initialized_ && env != nullptr) {
    thread_updater_initialized_ = true;
    thread_updater_local_cache_ = env->GetThreadStatusUpdater();
  }
  return (thread_updater_local_cache_ != nullptr);
}

AutoThreadOperationStageUpdater::AutoThreadOperationStageUpdater(
    ThreadStatus::OperationStage stage) {
  prev_stage_ = ThreadStatusUtil::SetThreadOperationStage(stage);
}

AutoThreadOperationStageUpdater::~AutoThreadOperationStageUpdater() {
  ThreadStatusUtil::SetThreadOperationStage(prev_stage_);
}

#else

ThreadStatusUpdater* ThreadStatusUtil::thread_updater_local_cache_ = nullptr;
bool ThreadStatusUtil::thread_updater_initialized_ = false;

bool ThreadStatusUtil::MaybeInitThreadLocalUpdater(const Env* env) {
  return false;
}

void ThreadStatusUtil::SetColumnFamily(const ColumnFamilyData* cfd,
                                       const Env* env,
                                       bool enable_thread_tracking) {}

void ThreadStatusUtil::SetThreadOperation(ThreadStatus::OperationType op) {
}

void ThreadStatusUtil::SetThreadOperationProperty(
    int code, uint64_t value) {
}

void ThreadStatusUtil::IncreaseThreadOperationProperty(
    int code, uint64_t delta) {
}

void ThreadStatusUtil::SetThreadState(ThreadStatus::StateType state) {
}

void ThreadStatusUtil::NewColumnFamilyInfo(const DB* db,
                                           const ColumnFamilyData* cfd,
                                           const std::string& cf_name,
                                           const Env* env) {}

void ThreadStatusUtil::EraseColumnFamilyInfo(
    const ColumnFamilyData* cfd) {
}

void ThreadStatusUtil::EraseDatabaseInfo(const DB* db) {
}

void ThreadStatusUtil::ResetThreadStatus() {
}

AutoThreadOperationStageUpdater::AutoThreadOperationStageUpdater(
    ThreadStatus::OperationStage stage) {
}

AutoThreadOperationStageUpdater::~AutoThreadOperationStageUpdater() {
}

#endif  // ROCKSDB_USING_THREAD_STATUS

}  // namespace rocksdb