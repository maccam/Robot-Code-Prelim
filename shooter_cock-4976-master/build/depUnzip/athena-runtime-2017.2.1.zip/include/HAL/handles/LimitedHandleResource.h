/*----------------------------------------------------------------------------*/
/* Copyright (c) FIRST 2016-2017. All Rights Reserved.                        */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#pragma once

#include <stdint.h>

#include <array>
#include <memory>

#include "HAL/Types.h"
#include "HAL/cpp/make_unique.h"
#include "HAL/cpp/priority_mutex.h"
#include "HandlesInternal.h"

namespace hal {

/**
 * The LimitedHandleResource class is a way to track handles. This version
 * allows a limited number of handles that are allocated sequentially.
 *
 * @tparam THandle The Handle Type (Must be typedefed from HAL_Handle)
 * @tparam TStruct The struct type held by this resource
 * @tparam size The number of resources allowed to be allocated
 * @tparam enumValue The type value stored in the handle
 *
 */
template <typename THandle, typename TStruct, int16_t size,
          HAL_HandleEnum enumValue>
class LimitedHandleResource {
  friend class LimitedHandleResourceTest;

 public:
  LimitedHandleResource() = default;
  LimitedHandleResource(const LimitedHandleResource&) = delete;
  LimitedHandleResource& operator=(const LimitedHandleResource&) = delete;

  THandle Allocate();
  std::shared_ptr<TStruct> Get(THandle handle);
  void Free(THandle handle);

 private:
  std::array<std::shared_ptr<TStruct>, size> m_structures;
  std::array<priority_mutex, size> m_handleMutexes;
  priority_mutex m_allocateMutex;
};

template <typename THandle, typename TStruct, int16_t size,
          HAL_HandleEnum enumValue>
THandle LimitedHandleResource<THandle, TStruct, size, enumValue>::Allocate() {
  // globally lock to loop through indices
  std::lock_guard<priority_mutex> sync(m_allocateMutex);
  int16_t i;
  for (i = 0; i < size; i++) {
    if (m_structures[i] == nullptr) {
      // if a false index is found, grab its specific mutex
      // and allocate it.
      std::lock_guard<priority_mutex> sync(m_handleMutexes[i]);
      m_structures[i] = std::make_shared<TStruct>();
      return static_cast<THandle>(createHandle(i, enumValue));
    }
  }
  return HAL_kInvalidHandle;
}

template <typename THandle, typename TStruct, int16_t size,
          HAL_HandleEnum enumValue>
std::shared_ptr<TStruct>
LimitedHandleResource<THandle, TStruct, size, enumValue>::Get(THandle handle) {
  // get handle index, and fail early if index out of range or wrong handle
  int16_t index = getHandleTypedIndex(handle, enumValue);
  if (index < 0 || index >= size) {
    return nullptr;
  }
  std::lock_guard<priority_mutex> sync(m_handleMutexes[index]);
  // return structure. Null will propogate correctly, so no need to manually
  // check.
  return m_structures[index];
}

template <typename THandle, typename TStruct, int16_t size,
          HAL_HandleEnum enumValue>
void LimitedHandleResource<THandle, TStruct, size, enumValue>::Free(
    THandle handle) {
  // get handle index, and fail early if index out of range or wrong handle
  int16_t index = getHandleTypedIndex(handle, enumValue);
  if (index < 0 || index >= size) return;
  // lock and deallocated handle
  std::lock_guard<priority_mutex> sync(m_allocateMutex);
  std::lock_guard<priority_mutex> lock(m_handleMutexes[index]);
  m_structures[index].reset();
}
}  // namespace hal
