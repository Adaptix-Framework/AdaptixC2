/*
  This file is part of KDBindings.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Shivam Kunwar <shivam.kunwar@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/
#pragma once

#include <algorithm>
#include <functional>
#include <mutex>

#include <kdbindings/connection_handle.h>

namespace KDBindings {

/**
 * @brief Manages and evaluates deferred Signal connections.
 *
 * @warning Deferred connections are experimental and may be removed or changed in the future.
 *
 * The ConnectionEvaluator class is responsible for managing and evaluating connections
 * to Signals. It provides mechanisms to delay and control the evaluation of connections.
 * It therefore allows controlling when and on which thread slots connected to a Signal are executed.
 *
 * @see Signal::connectDeferred()
 */
class ConnectionEvaluator
{

public:
    /** ConnectionEvaluators are default constructible */
    ConnectionEvaluator() = default;

    /** Connectionevaluators are not copyable */
    // As it is designed to manage connections,
    // and copying it could lead to unexpected behavior, including duplication of connections and issues
    // related to connection lifetimes. Therefore, it is intentionally made non-copyable.
    ConnectionEvaluator(const ConnectionEvaluator &) noexcept = delete;

    ConnectionEvaluator &operator=(const ConnectionEvaluator &) noexcept = delete;

    /** ConnectionEvaluators are not moveable */
    // As they are captures by-reference
    // by the Signal, so moving them would lead to a dangling reference.
    ConnectionEvaluator(ConnectionEvaluator &&other) noexcept = delete;

    ConnectionEvaluator &operator=(ConnectionEvaluator &&other) noexcept = delete;

    virtual ~ConnectionEvaluator() = default;

    /**
     * @brief Evaluate the deferred connections.
     *
     * This function is responsible for evaluating and executing deferred connections.
     * This function is thread safe.
     *
     * @warning Evaluating slots that throw an exception is currently undefined behavior.
     */
    void evaluateDeferredConnections()
    {
        std::lock_guard<std::recursive_mutex> lock(m_slotInvocationMutex);

        if (m_isEvaluating) {
            // We're already evaluating, so we don't want to re-enter this function.
            return;
        }
        m_isEvaluating = true;

        // Current best-effort error handling will remove any further invocations that were queued.
        // We could use a queue and use a `while(!empty) { pop_front() }` loop instead to avoid this.
        // However, we would then ideally use a ring-buffer to avoid excessive allocations, which isn't in the STL.
        try {
            for (auto &pair : m_deferredSlotInvocations) {
                pair.second();
            }
        } catch (...) {
            // Best-effort: Reset the ConnectionEvaluator so that it at least doesn't execute the same erroneous slot multiple times.
            m_deferredSlotInvocations.clear();
            m_isEvaluating = false;
            throw;
        }

        m_deferredSlotInvocations.clear();
        m_isEvaluating = false;
    }

protected:
    /**
     * @brief Called when a new slot invocation is added.
     *
     * This function can be overwritten by subclasses to get notified whenever a new invocation is added to this evaluator.
     * The default implementation does nothing and does not have to be called by subclasses when overriding.
     *
     * ⚠️  *Note that this function will be executed on the thread that enqueued the slot invocation (i.e. the thread that called .emit() on the signal),
     * which is usually not the thread that is responsible for evaluating the connections!
     * Therefore it is usually not correct to call evaluateDeferredConnections() within this function!
     * User code is responsible for ensuring that the threads are synchronized correctly.*
     *
     * For example, if you plan to evaluate (execute) the slot invocations in some "main" thread A
     * and a signal is emitted in thread B, than this method will be called on thread B.
     * It is a good place to "wake up" the event loop of thread A so that thread A can call `evaluateDeferredConnections()`.
     */
    virtual void onInvocationAdded() { }

private:
    template<typename...>
    friend class Signal;

    void enqueueSlotInvocation(const ConnectionHandle &handle, const std::function<void()> &slotInvocation)
    {
        {
            std::lock_guard<std::recursive_mutex> lock(m_slotInvocationMutex);
            m_deferredSlotInvocations.push_back({ handle, std::move(slotInvocation) });
        }
        onInvocationAdded();
    }

    // Note: This function is marked with noexcept but may theoretically encounter an exception and terminate the program if locking the mutex fails.
    // If this does happen though, there's likely something very wrong, so std::terminate is actually a reasonable way to handle this.
    //
    // In addition, we do need to use a recursive_mutex, as otherwise a slot from `enqueueSlotInvocation` may theoretically call this function and cause undefined behavior.
    void dequeueSlotInvocation(const ConnectionHandle &handle) noexcept
    {
        std::lock_guard<std::recursive_mutex> lock(m_slotInvocationMutex);

        if (m_isEvaluating) {
            // It's too late, we're already evaluating the deferred connections.
            // We can't remove the invocation now, as it might be currently evaluated.
            // And removing any invocations would be undefined behavior as we would invalidate
            // the loop indices in `evaluateDeferredConnections`.
            return;
        }

        auto handleMatches = [&handle](const auto &invocationPair) {
            return invocationPair.first == handle;
        };

        // Remove all invocations that match the handle
        m_deferredSlotInvocations.erase(
                std::remove_if(m_deferredSlotInvocations.begin(), m_deferredSlotInvocations.end(), handleMatches),
                m_deferredSlotInvocations.end());
    }

    std::vector<std::pair<ConnectionHandle, std::function<void()>>> m_deferredSlotInvocations;
    // We need to use a recursive mutex here, as `evaluateDeferredConnections` executes arbitrary user code.
    // This may end up in a call to dequeueSlotInvocation, which locks the same mutex.
    // We'll also need to add a flag to make sure we don't actually dequeue invocations while we're evaluating them.
    std::recursive_mutex m_slotInvocationMutex;
    bool m_isEvaluating = false;
};
} // namespace KDBindings
