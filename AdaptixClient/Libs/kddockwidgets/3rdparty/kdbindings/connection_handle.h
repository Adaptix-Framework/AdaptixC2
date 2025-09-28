/*
  This file is part of KDBindings.

  SPDX-FileCopyrightText: 2021 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Sean Harmer <sean.harmer@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <kdbindings/genindex_array.h>
#include <kdbindings/utils.h>
#include <memory>

namespace KDBindings {

template<typename... Args>
class Signal;

class ConnectionHandle;

namespace Private {
//
// This class defines a virtual interface, that the Signal this ConnectionHandle refers
// to must implement.
// It allows ConnectionHandle to refer to this non-template class, which then dispatches
// to the template implementation using virtual function calls.
// It allows ConnectionHandle to be a non-template class.
class SignalImplBase : public std::enable_shared_from_this<SignalImplBase>
{
public:
    SignalImplBase() = default;

    virtual ~SignalImplBase() = default;

    virtual void disconnect(const ConnectionHandle &handle) noexcept = 0;
    virtual bool blockConnection(const GenerationalIndex &id, bool blocked) = 0;
    virtual bool isConnectionActive(const GenerationalIndex &id) const noexcept = 0;
    virtual bool isConnectionBlocked(const GenerationalIndex &id) const = 0;
};

} // namespace Private

/**
 * @brief A ConnectionHandle represents the connection of a Signal
 * to a slot (i.e. a function that is called when the Signal is emitted).
 *
 * It is returned from a Signal when a connection is created and used to
 * manage the connection by disconnecting, (un)blocking it and checking its state.
 **/
class ConnectionHandle
{
public:
    /**
     * A ConnectionHandle can be default constructed.
     * In this case the ConnectionHandle will not reference any active connection (i.e. isActive() will return false),
     * and not belong to any Signal.
     **/
    ConnectionHandle() = default;

    /**
     * A ConnectionHandle can be copied.
     **/
    ConnectionHandle(const ConnectionHandle &) noexcept = default;
    ConnectionHandle &operator=(const ConnectionHandle &) noexcept = default;

    /**
     * A ConnectionHandle can be moved.
     **/
    ConnectionHandle(ConnectionHandle &&) noexcept = default;
    ConnectionHandle &operator=(ConnectionHandle &&) noexcept = default;

    /**
     * Disconnect the slot.
     *
     * When this function is called, the function that was passed to Signal::connect
     * to create this ConnectionHandle will no longer be called when the Signal is emitted.
     *
     * If the ConnectionHandle is not active or the connection has already been disconnected,
     * nothing happens.
     *
     * After this call, the ConnectionHandle will be inactive (i.e. isActive() returns false)
     * and will no longer belong to any Signal (i.e. belongsTo returns false).
     *
     * @warning While this function is marked with noexcept, it *may* terminate the program
     * if it is not possible to allocate memory or if mutex locking isn't possible.
     **/
    void disconnect() noexcept
    {
        if (auto shared_impl = checkedLock()) {
            shared_impl->disconnect(*this);
        }

        // ConnectionHandle is no longer active;
        m_signalImpl.reset();
    }

    /**
     * Check whether the connection of this ConnectionHandle is active.
     *
     * @return true if the ConnectionHandle refers to an active Signal
     * and the connection was not disconnected previously, false otherwise.
     **/
    bool isActive() const
    {
        return static_cast<bool>(checkedLock());
    }

    /**
     * Sets the block state of the connection.
     * If a connection is blocked, emitting the Signal will no longer call this
     * connections slot, until the connection is unblocked.
     *
     * Behaves the same as calling Signal::blockConnection with this
     * ConnectionHandle as argument.
     *
     * To temporarily block a connection, consider using an instance of ConnectionBlocker,
     * which offers a RAII-style implementation that makes sure the connection is always
     * returned to its original state.
     *
     * @param blocked The new blocked state of the connection.
     * @return whether the connection was previously blocked.
     * @throw std::out_of_range Throws if the connection is not active (i.e. isActive() returns false).
     **/
    bool block(bool blocked)
    {
        if (auto shared_impl = checkedLock()) {
            return shared_impl->blockConnection(*m_id, blocked);
        }
        throw std::out_of_range("Cannot block a non-active connection!");
    }

    /**
     * Checks whether the connection is currently blocked.
     *
     * To change the blocked state of a connection, call ConnectionHandle::block.
     *
     * @return whether the connection is currently blocked.
     **/
    bool isBlocked() const
    {
        if (auto shared_impl = checkedLock()) {
            return shared_impl->isConnectionBlocked(*m_id);
        }
        throw std::out_of_range("Cannot check whether a non-active connection is blocked!");
    }

    /**
     * Check whether this ConnectionHandle belongs to the given Signal.
     *
     * @return true if this ConnectionHandle refers to a connection within the given Signal
     **/
    template<typename... Args>
    bool belongsTo(const Signal<Args...> &signal) const
    {
        auto shared_impl = m_signalImpl.lock();
        return shared_impl && shared_impl == std::static_pointer_cast<Private::SignalImplBase>(signal.m_impl);
    }

    // Define an operator== function to compare ConnectionHandle objects.
    bool operator==(const ConnectionHandle &other) const
    {
        auto thisSignalImpl = m_signalImpl.lock();
        auto otherSignalImpl = other.m_signalImpl.lock();

        // If both signalImpl pointers are valid, compare them along with the IDs.
        if (thisSignalImpl && otherSignalImpl) {
            return (thisSignalImpl == otherSignalImpl) && (m_id == other.m_id);
        }

        // If neither instance has an ID, and both signalImpl pointers are invalid, consider them equal.
        if (!m_id.has_value() && !other.m_id.has_value() && !thisSignalImpl && !otherSignalImpl) {
            return true;
        }

        // In all other cases, they are not equal.
        return false;
    }

    /**
     * This function exists to intentionally silence the [[nodiscard]] warning generated when connecting.
     *
     * In cases where you know for certain that the lifetime of the signal does not extend beyond the lifetime of the slot, you may not need the ConnectionHandle, so you can use release to discard it.
     *
     * Example:
     * ```cpp
     * bool called = false;
     * {
     *     Signal<int> mySignal;
     *     // The signal will not outlive the reference to the `called` bool, so it's fine to ignore the ConnectionHandle.
     *     mySignal.connect([&called](){ called = true; }).release();
     * }
     * ```
     */
    void release() const { }

private:
    template<typename...>
    friend class Signal;

    std::weak_ptr<Private::SignalImplBase> m_signalImpl;
    std::optional<Private::GenerationalIndex> m_id;

    // private, so it is only available from Signal
    ConnectionHandle(std::weak_ptr<Private::SignalImplBase> signalImpl, std::optional<Private::GenerationalIndex> id)
        : m_signalImpl{ std::move(signalImpl) }, m_id{ std::move(id) }
    {
    }
    void setId(const Private::GenerationalIndex &id)
    {
        m_id = id;
    }

    // Checks that the weak_ptr can be locked and that the connection is
    // still active
    std::shared_ptr<Private::SignalImplBase> checkedLock() const noexcept
    {
        if (m_id.has_value()) {
            auto shared_impl = m_signalImpl.lock();
            if (shared_impl && shared_impl->isConnectionActive(*m_id)) {
                return shared_impl;
            }
        }
        return nullptr;
    }
};

/**
 * @brief A ScopedConnection is a RAII-style way to make sure a Connection is disconnected.
 *
 * When the ScopedConnections scope ends, the connection this ScopedConnection guards will be disconnected.
 *
 * Example:
 * - @ref 08-managing-connections/main.cpp
 */
class ScopedConnection
{
public:
    /**
     * @brief A ScopedConnection can be default constructed
     *
     * A default constructed ScopedConnection has no connection to guard.
     * Therefore it does nothing when it is destructed, unless a ConnectionHandle is assigned to it.
     */
    ScopedConnection() = default;

    /** A ScopedConnection can be move constructed */
    ScopedConnection(ScopedConnection &&) noexcept = default;

    /** A ScopedConnection cannot be copied */
    ScopedConnection(const ScopedConnection &) = delete;
    /** A ScopedConnection cannot be copied */
    ScopedConnection &operator=(const ScopedConnection &) = delete;

    /** A ScopedConnection can be move assigned */
    ScopedConnection &operator=(ScopedConnection &&other) noexcept
    {
        m_connection.disconnect();
        m_connection = std::move(other.m_connection);
        return *this;
    }

    /**
     * A ScopedConnection can be constructed from a ConnectionHandle
     */
    ScopedConnection(ConnectionHandle &&h) noexcept
        : m_connection(std::move(h))
    {
    }

    /**
     * A ScopedConnection can be assigned from a ConnectionHandle
     */
    ScopedConnection &operator=(ConnectionHandle &&h) noexcept
    {
        return *this = ScopedConnection(std::move(h));
    }

    /**
     * @return the handle to the connection this instance is managing
     */
    ConnectionHandle &handle()
    {
        return m_connection;
    }

    /**
     * @overload
     */
    const ConnectionHandle &handle() const
    {
        return m_connection;
    }

    /**
     * Convenience access to the underlying ConnectionHandle using the `->` operator.
     */
    ConnectionHandle *operator->()
    {
        return &m_connection;
    }

    /**
     * @overload
     */
    const ConnectionHandle *operator->() const
    {
        return &m_connection;
    }

    /**
     * When a ConnectionHandle is destructed it disconnects the connection it guards.
     *
     * @warning While this function isn't marked as throwing, it *may* throw and terminate the program
     * if it is not possible to allocate memory or if mutex locking isn't possible.
     */
    ~ScopedConnection() noexcept
    {
        m_connection.disconnect();
    }

private:
    ConnectionHandle m_connection;
};

} // namespace KDBindings
