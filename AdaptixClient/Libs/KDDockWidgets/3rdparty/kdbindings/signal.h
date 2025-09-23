/*
  This file is part of KDBindings.

  SPDX-FileCopyrightText: 2021 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Sean Harmer <sean.harmer@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <assert.h>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <forward_list>

#ifdef emit
static_assert(false, "KDBindings is not compatible with Qt's 'emit' keyword.\n"
                     "To use KDBindings with Qt, please define QT_NO_EMIT to disable Qt's 'emit' keyword.\n"
                     "If you're using CMake you can set KDBindings_QT_NO_EMIT to ON to add this.");

// Undefine emit to suppress more compiler errors after the static_assert failure.
// Otherwise the compiler would drown our custom error message with more errors.
#undef emit
#endif

#include <kdbindings/connection_evaluator.h>
#include <kdbindings/genindex_array.h>
#include <kdbindings/utils.h>

#include <kdbindings/KDBindingsConfig.h>

/**
 * @brief The main namespace of the KDBindings library.
 *
 * All public parts of KDBindings are members of this namespace.
 */
namespace KDBindings {
/**
 * @brief A Signal provides a mechanism for communication between objects.
 *
 * KDBindings::Signal recreates the <a href="https://doc.qt.io/qt-5/signalsandslots.html">Qt's Signals & Slots mechanism</a> in pure C++17.
 * A Signal can be used to notify any number of slots that a certain event has occurred.
 *
 * The slot can be almost any callable object, including member functions and lambdas.
 *
 * This connection happens in a type-safe manner, as a slot can only be connected to
 * a Signal when the arguments of the slot match the values the Signal emits.
 *
 * The Args type parameter pack describe which value types the Signal will emit.
 *
 * Deferred Connection:
 *
 * KDBindings::Signal supports deferred connections, enabling the decoupling of signal
 * emission from the execution of connected slots. With deferred connections, you can
 * connect slots to the Signal that are not immediately executed when the signal is emitted.
 * Instead, you can evaluate these deferred connections at a later time, allowing for
 * asynchronous or delayed execution of connected slots.
 *
 * Examples:
 * - @ref 01-simple-connection/main.cpp
 * - @ref 02-signal-member/main.cpp
 * - @ref 03-member-arguments/main.cpp
 * - @ref 07-advanced-connections/main.cpp
 */
template<typename... Args>
class Signal
{
    static_assert(
            std::conjunction<std::negation<std::is_rvalue_reference<Args>>...>::value,
            "R-value references are not allowed as Signal parameters!");

    // The Signal::Impl class exists, so Signals can be implemented in a PIMPL-like way.
    // This allows us to easily move Signals without losing their ConnectionHandles, as well as
    // making an unconnected Signal only sizeof(shared_ptr).
    class Impl : public Private::SignalImplBase
    {
    public:
        Impl() noexcept { }

        ~Impl() noexcept { }

        // Signal::Impls are not copyable
        Impl(Impl const &other) = delete;
        Impl &operator=(Impl const &other) = delete;

        // Signal::Impls are not moveable, this would break the ConnectionHandles
        Impl(Impl &&other) = delete;
        Impl &operator=(Impl &&other) = delete;

        // Connects a std::function to the signal. The returned
        // value can be used to disconnect the function again.
        Private::GenerationalIndex connect(std::function<void(Args...)> const &slot)
        {
            Connection newConnection;
            newConnection.slot = slot;
            return m_connections.insert(std::move(newConnection));
        }

        // Establish a deferred connection between signal and slot, where ConnectionEvaluator object
        // is used to queue all the connection to evaluate later. The returned
        // value can be used to disconnect the slot later.
        Private::GenerationalIndex connectDeferred(const std::shared_ptr<ConnectionEvaluator> &evaluator, std::function<void(Args...)> const &slot)
        {
            auto weakEvaluator = std::weak_ptr<ConnectionEvaluator>(evaluator);

            auto deferredSlot = [weakEvaluator = std::move(weakEvaluator), slot](ConnectionHandle &handle, Args... args) {
                if (auto evaluatorPtr = weakEvaluator.lock()) {
                    auto lambda = [slot, args...]() {
                        slot(args...);
                    };
                    evaluatorPtr->enqueueSlotInvocation(handle, lambda);
                } else {
                    throw std::runtime_error("ConnectionEvaluator is no longer alive");
                }
            };

            Connection newConnection;
            newConnection.m_connectionEvaluator = evaluator;
            newConnection.slotReflective = deferredSlot;

            return m_connections.insert(std::move(newConnection));
        }

        Private::GenerationalIndex connectReflective(std::function<void(ConnectionHandle &handle, Args...)> const &slot)
        {
            Connection newConnection;
            newConnection.slotReflective = slot;

            return m_connections.insert(std::move(newConnection));
        }

        // Disconnects a previously connected function
        //
        // WARNING: While this function is marked with noexcept, it *may* terminate the program
        // if it is not possible to allocate memory or if mutex locking isn't possible.
        void disconnect(const ConnectionHandle &handle) noexcept override
        {
            // If the connection evaluator is still valid, remove any queued up slot invocations
            // associated with the given handle to prevent them from being evaluated in the future.
            auto idOpt = handle.m_id; // Retrieve the connection associated with this id

            // Proceed only if the id is valid
            if (idOpt.has_value()) {
                auto id = idOpt.value();

                // Retrieve the connection associated with this id
                auto connection = m_connections.get(id);
                if (connection && m_isEmitting) {
                    // We are currently still emitting the signal, so we need to defer the actual
                    // disconnect until the emit is done.
                    connection->toBeDisconnected = true;
                    m_disconnectedDuringEmit = true;
                    return;
                }

                if (connection && connection->m_connectionEvaluator.lock()) {
                    if (auto evaluatorPtr = connection->m_connectionEvaluator.lock()) {
                        evaluatorPtr->dequeueSlotInvocation(handle);
                    }
                }

                // Note: This function may throw if we're out of memory.
                // As `disconnect` is marked as `noexcept`, this will terminate the program.
                m_connections.erase(id);
            }
        }

        // Disconnects all previously connected functions
        //
        // WARNING: While this function is marked with noexcept, it *may* terminate the program
        // if it is not possible to allocate memory or if mutex locking isn't possible.
        void disconnectAll() noexcept
        {
            const auto numEntries = m_connections.entriesSize();

            const auto sharedThis = shared_from_this();
            for (auto i = decltype(numEntries){ 0 }; i < numEntries; ++i) {
                const auto indexOpt = m_connections.indexAtEntry(i);
                if (sharedThis && indexOpt) {
                    disconnect(ConnectionHandle(sharedThis, *indexOpt));
                }
            }
        }

        bool blockConnection(const Private::GenerationalIndex &id, bool blocked) override
        {
            Connection *connection = m_connections.get(id);
            if (connection) {
                const bool wasBlocked = connection->blocked;
                connection->blocked = blocked;
                return wasBlocked;
            } else {
                throw std::out_of_range("Provided ConnectionHandle does not match any connection\nLikely the connection was deleted before!");
            }
        }

        bool isConnectionActive(const Private::GenerationalIndex &id) const noexcept override
        {
            return m_connections.get(id);
        }

        bool isConnectionBlocked(const Private::GenerationalIndex &id) const override
        {
            auto connection = m_connections.get(id);
            if (connection) {
                return connection->blocked;
            } else {
                throw std::out_of_range("Provided ConnectionHandle does not match any connection\nLikely the connection was deleted before!");
            }
        }

        void emit(Args... p)
        {
            if (m_isEmitting) {
                throw std::runtime_error("Signal is already emitting, nested emits are not supported!");
            }
            m_isEmitting = true;

            const auto numEntries = m_connections.entriesSize();

            // This loop can *not* tolerate new connections being added to the signal inside a slot
            // Doing so will be undefined behavior
            for (auto i = decltype(numEntries){ 0 }; i < numEntries; ++i) {
                const auto index = m_connections.indexAtEntry(i);

                if (index) {
                    const auto con = m_connections.get(*index);

                    if (!con->blocked) {
                        if (con->slotReflective) {
                            if (auto sharedThis = shared_from_this(); sharedThis) {
                                ConnectionHandle handle(sharedThis, *index);
                                con->slotReflective(handle, p...);
                            }
                        } else if (con->slot) {
                            con->slot(p...);
                        }
                    }
                }
            }
            m_isEmitting = false;

            if (m_disconnectedDuringEmit) {
                m_disconnectedDuringEmit = false;

                // Because m_connections is using a GenerationIndexArray, this loop can tolerate
                // deletions inside the loop. So iterating over the array and deleting entries from it
                // should not lead to undefined behavior.
                for (auto i = decltype(numEntries){ 0 }; i < numEntries; ++i) {
                    const auto index = m_connections.indexAtEntry(i);

                    if (index.has_value()) {
                        const auto con = m_connections.get(index.value());
                        if (con->toBeDisconnected) {
                            disconnect(ConnectionHandle(shared_from_this(), index));
                        }
                    }
                }
            }
        }

    private:
        friend class Signal;
        struct Connection {
            std::function<void(Args...)> slot;
            std::function<void(ConnectionHandle &, Args...)> slotReflective;
            std::weak_ptr<ConnectionEvaluator> m_connectionEvaluator;
            bool blocked{ false };
            // When we disconnect while the signal is still emitting, we need to defer the actual disconnection
            // until the emit is done. This flag is set to true when the connection should be disconnected.
            bool toBeDisconnected{ false };
        };

        mutable Private::GenerationalIndexArray<Connection> m_connections;

        // If a reflective slot disconnects itself, we need to make sure to not deconstruct the std::function
        // while it is still running.
        // Therefore, defer all slot disconnections until the emit is done.
        //
        // Previously, we stored the ConnectionHandles that were to be disconnected in a list.
        // However, that would mean that disconnecting cannot be noexcept, as it may need to allocate memory in that list.
        // We can fix this by storing this information within each connection itself (the toBeDisconnected flag).
        // Because of the memory layout of the `struct Connection`, this shouldn't even use any more memory.
        //
        // The only downside is that we need to iterate over all connections to find the ones that need to be disconnected.
        // This is helped by using the m_disconnedDuringEmit flag to avoid unnecessary iterations.
        bool m_isEmitting = false;
        bool m_disconnectedDuringEmit = false;
    };

public:
    /** Signals are default constructible */
    Signal() = default;

    /**
     * Signals cannot be copied.
     **/
    Signal(const Signal &) = delete;
    Signal &operator=(Signal const &other) = delete;

    /** Signals can be moved */
    Signal(Signal &&other) noexcept = default;
    Signal &operator=(Signal &&other) noexcept = default;

    /**
     * A signal disconnects all slots when it is destructed
     *
     * Therefore, all active ConnectionHandles that belonged to this Signal
     * will no longer be active (i.e. ConnectionHandle::isActive will return false).
     *
     * @warning While this function isn't marked as throwing, it *may* throw and terminate the program
     * if it is not possible to allocate memory or if mutex locking isn't possible.
     */
    ~Signal() noexcept
    {
        disconnectAll();
    }

    /**
     * Connects a std::function to the signal.
     *
     * When emit() is called on the Signal, the functions will be called with
     * the arguments provided to emit().
     *
     * @return An instance of ConnectionHandle, that can be used to disconnect
     * or temporarily block the connection.
     *
     * @warning Connecting functions to a signal that throw an exception when called is currently undefined behavior.
     * All connected functions should handle their own exceptions.
     * For backwards-compatibility, the slot function is not required to be noexcept.
     */
    KDBINDINGS_WARN_UNUSED ConnectionHandle connect(std::function<void(Args...)> const &slot)
    {
        ensureImpl();

        return ConnectionHandle{ m_impl, m_impl->connect(slot) };
    }

    /**
     * Establishes a connection between a signal and a slot, allowing the slot to access and manage its own connection handle.
     * This method is particularly useful for creating connections that can autonomously manage themselves, such as disconnecting
     * after being triggered a certain number of times or under specific conditions. It wraps the given slot function
     * to include a reference to the ConnectionHandle as the first parameter, enabling the slot to interact with
     * its own connection state directly.
     *
     * @param slot A std::function that takes a ConnectionHandle reference followed by the signal's parameter types.
     * @return A ConnectionHandle to the newly established connection, allowing for advanced connection management.
     *
     * @warning Connecting functions to a signal that throw an exception when called is currently undefined behavior.
     * All connected functions should handle their own exceptions.
     * For backwards-compatibility, the slot function is not required to be noexcept.
     */
    KDBINDINGS_WARN_UNUSED ConnectionHandle connectReflective(std::function<void(ConnectionHandle &, Args...)> const &slot)
    {
        ensureImpl();

        return ConnectionHandle{ m_impl, m_impl->connectReflective(slot) };
    }

    /**
     * Establishes a single-shot connection between a signal and a slot and when the signal is emitted, the connection will be
     * disconnected and the slot will be called. Note that the slot will be disconnected before it is called. If the slot
     * triggers another signal emission of the same signal, the slot will not be called again.
     *
     * @param slot A std::function that takes the signal's parameter types.
     * @return An instance of ConnectionHandle, that can be used to disconnect.
     *
     * @warning Connecting functions to a signal that throw an exception when called is currently undefined behavior.
     * All connected functions should handle their own exceptions.
     * For backwards-compatibility, the slot function is not required to be noexcept.
     */
    KDBINDINGS_WARN_UNUSED ConnectionHandle connectSingleShot(std::function<void(Args...)> const &slot)
    {
        return connectReflective([slot](ConnectionHandle &handle, Args... args) {
            handle.disconnect();
            slot(args...);
        });
    }

    /**
     * @brief Establishes a deferred connection between the provided evaluator and slot.
     *
     * @warning Deferred connections are experimental and may be removed or changed in the future.
     *
     * This function allows connecting an evaluator and a slot such that the slot's execution
     * is deferred until the conditions evaluated by the `evaluator` are met.
     *
     * First argument to the function is reference to a shared pointer to the ConnectionEvaluator responsible for determining
     * when the slot should be executed.
     *
     * @return An instance of ConnectionHandle, that can be used to disconnect
     * or temporarily block the connection.
     *
     * @note
     * The Signal class itself is not thread-safe. While the ConnectionEvaluator is inherently
     * thread-safe, ensure that any concurrent access to this Signal is protected externally to maintain thread safety.
     *
     * @warning Connecting functions to a signal that throw an exception when called is currently undefined behavior.
     * All connected functions should handle their own exceptions.
     * For backwards-compatibility, the slot function is not required to be noexcept.
     */
    KDBINDINGS_WARN_UNUSED ConnectionHandle connectDeferred(const std::shared_ptr<ConnectionEvaluator> &evaluator, std::function<void(Args...)> const &slot)
    {
        ensureImpl();

        ConnectionHandle handle(m_impl, {});
        handle.setId(m_impl->connectDeferred(evaluator, slot));
        return handle;
    }

    /**
     * A template overload of Signal::connect that makes it easier to connect arbitrary functions to this
     * Signal.
     * It connects a function to this Signal, binds any provided arguments to that function and discards
     * any values emitted by this Signal that aren't needed by the resulting function.
     *
     * This is especially useful for connecting member functions to signals.
     *
     * Examples:
     * @code
     * Signal<int> signal;
     * std::vector<int> numbers{ 1, 2, 3 };
     * bool emitted = false;
     *
     * // disambiguation necessary, as push_back is overloaded.
     * void (std::vector<int>::*push_back)(const int &) = &std::vector<int>::push_back;
     * signal.connect(push_back, &numbers);
     *
     * // this slot doesn't require the int argument, so it will be discarded.
     * signal.connect([&emitted]() { emitted = true; });
     *
     * signal.emit(4); // Will add 4 to the vector and set emitted to true
     * @endcode
     *
     * For more examples see the @ref 07-advanced-connections/main.cpp example.
     *
     * @return An instance of a Signal::ConnectionHandle that refers to this connection.
     *          Warning: When connecting a member function you must use the returned ConnectionHandle
     *          to disconnect when the object containing the slot goes out of scope!
     *
     * @warning Connecting functions to a signal that throw an exception when called is currently undefined behavior.
     * All connected functions should handle their own exceptions.
     * For backwards-compatibility, the slot function is not required to be noexcept.
     **/
    // The enable_if_t makes sure that this connect function specialization is only
    // available if we provide a function that cannot be otherwise converted to a
    // std::function<void(Args...)>, as it otherwise tries to take precedence
    // over the normal connect function.
    template<typename Func, typename... FuncArgs, typename = std::enable_if_t<std::disjunction_v<std::negation<std::is_convertible<Func, std::function<void(Args...)>>>, std::integral_constant<bool, sizeof...(FuncArgs) /*Also enable this function if we want to bind at least one argument*/>>>>
    KDBINDINGS_WARN_UNUSED ConnectionHandle connect(Func &&slot, FuncArgs &&...args)
    {
        std::function<void(Args...)> bound = Private::bind_first(std::forward<Func>(slot), std::forward<FuncArgs>(args)...);
        return connect(bound);
    }

    /**
     * Disconnect a previously connected slot.
     *
     * After the slot was successfully disconnected, the ConnectionHandle will no
     * longer be active. (i.e. ConnectionHandle::isActive will return false).
     *
     * @throw std::out_of_range - If the ConnectionHandle does not belong to this
     * Signal (i.e. ConnectionHandle::belongsTo returns false).
     */
    void disconnect(const ConnectionHandle &handle)
    {
        if (m_impl && handle.belongsTo(*this) && handle.m_id.has_value()) {
            m_impl->disconnect(handle);
            // TODO check if Impl is now empty and reset
        } else {
            throw std::out_of_range("Provided ConnectionHandle does not match any connection\nLikely the connection was deleted before!");
        }
    }

    /**
     * Disconnect all previously connected functions.
     *
     * All currently active ConnectionHandles that belong to this Signal will no
     * longer be active afterwards. (i.e. ConnectionHandle::isActive will return false).
     *
     * @warning While this function is marked with noexcept, it *may* terminate the program
     * if it is not possible to allocate memory or if mutex locking isn't possible.
     */
    void disconnectAll() noexcept
    {
        if (m_impl) {
            m_impl->disconnectAll();
            // Once all connections are disconnected, we can release ownership of the Impl.
            // This does not destroy the Signal itself, just the Impl object.
            // If another slot is connected, another Impl object will be constructed.
            m_impl.reset();
        }
        // If m_impl is nullptr, we don't have any connections to disconnect
    }

    /**
     * Sets the block state of the connection.
     * If a connection is blocked, emitting the Signal will no longer call this
     * connections slot, until the connection is unblocked.
     *
     * ConnectionHandle::block can be used as an alternative.
     *
     * To temporarily block a connection, consider using an instance of ConnectionBlocker,
     * which offers a RAII-style implementation that makes sure the connection is always
     * returned to its original state.
     *
     * @param blocked Whether the connection should be blocked from now on.
     * @param handle The ConnectionHandle to block.
     * @return Whether the connection was previously blocked.
     * @throw std::out_of_range - If the ConnectionHandle does not belong to this
     * Signal (i.e. ConnectionHandle::belongsTo returns false).
     */
    bool blockConnection(const ConnectionHandle &handle, bool blocked)
    {
        if (m_impl && handle.belongsTo(*this) && handle.m_id.has_value()) {
            return m_impl->blockConnection(*handle.m_id, blocked);
        } else {
            throw std::out_of_range("Provided ConnectionHandle does not match any connection\nLikely the connection was deleted before!");
        }
    }

    /**
     * Checks whether the connection is currently blocked.
     *
     * To change the blocked state of a connection, call blockConnection().
     *
     * @return Whether the connection is currently blocked
     * @throw std::out_of_range - If the ConnectionHandle does not belong to this
     * Signal (i.e. ConnectionHandle::belongsTo returns false).
     */
    bool isConnectionBlocked(const ConnectionHandle &handle) const
    {
        assert(handle.belongsTo(*this));
        if (!m_impl) {
            throw std::out_of_range("Provided ConnectionHandle does not match any connection\nLikely the connection was deleted before!");
        }

        if (handle.m_id.has_value()) {
            return m_impl->isConnectionBlocked(*handle.m_id);
        } else {
            return false;
        }
    }

    /**
     * Emits the Signal, which causes all connected slots to be called,
     * as long as they are not blocked.
     *
     * The arguments provided to emit will be passed to each slot by copy,
     * therefore consider using (const) references as the Args to the Signal
     * wherever possible.
     *
     * Note: Slots may disconnect themselves during an emit, which will cause the
     * connection to be disconnected after all slots have been called.
     *
     * ⚠️ *Note: Connecting a new slot to a signal while the signal is still
     * in the emit function is undefined behavior.*
     *
     * ⚠️ *Note: This function is **not thread-safe** and **not reentrant**.
     * Specifically, this means it is undefined behavior to emit a signal from
     * a slot of that same signal.*
     */
    void emit(Args... p) const
    {
        if (m_impl)
            m_impl->emit(p...);

        // if m_impl is nullptr, we don't have any slots connected, don't bother emitting
    }

private:
    friend class ConnectionHandle;

    void ensureImpl()
    {
        if (!m_impl) {
            m_impl = std::make_shared<Impl>();
        }
    }

    // shared_ptr is used here instead of unique_ptr, so ConnectionHandle instances can
    // use a weak_ptr to check if the Signal::Impl they reference is still alive.
    //
    // This makes Signals easily copyable in theory, but the semantics of this are unclear.
    // Copying could either simply copy the shared_ptr, which means the copy would share
    // the connections of the original, which is possibly unintuitive, or the Impl would
    // have to be copied as well.
    // This would however leave connections without handles to disconnect them.
    // So copying is forbidden for now.
    //
    // Think of this shared_ptr more like a unique_ptr with additional weak_ptr's
    // in ConnectionHandle that can check whether the Impl object is still alive.
    mutable std::shared_ptr<Impl> m_impl;
};

/**
 * @brief A ConnectionBlocker is a convenient RAII-style mechanism for temporarily blocking a connection.
 *
 * When a ConnectionBlocker is constructed, it will block the connection.
 *
 * When it is destructed, it will return the connection to the blocked state it was in
 * before the ConnectionBlocker was constructed.
 *
 * Example:
 * - @ref 08-managing-connections/main.cpp
 */
class ConnectionBlocker
{
public:
    /**
     * Constructs a new ConnectionBlocker and blocks the connection this ConnectionHandle
     * refers to.
     *
     * @throw std::out_of_range If the connection is not active (i.e. ConnectionHandle::isActive() returns false).
     */
    explicit ConnectionBlocker(const ConnectionHandle &handle)
        : m_handle{ handle }
    {
        m_wasBlocked = m_handle.block(true);
    }

    /**
     * Destructs the ConnectionBlocker and returns the connection into the blocked state it was in
     * before the ConnectionBlocker was constructed.
     */
    ~ConnectionBlocker()
    {
        m_handle.block(m_wasBlocked);
    }

private:
    ConnectionHandle m_handle;
    bool m_wasBlocked{ false };
};

/**
 * @example 01-simple-connection/main.cpp
 *
 * A simple example of how to create a KDBindings::Signal and connect a lambda to it.
 *
 * The output of this example is:
 * ```
 * The answer: 42
 * ```
 */

/**
 * @example 02-signal-member/main.cpp
 *
 * An example of how to connect a member function to a KDBindings::Signal.
 *
 * The output of this example is:
 * ```
 * Hello World!
 * ```
 */

/**
 * @example 03-member-arguments/main.cpp
 *
 * An example of how to connect a member function with arguments to a KDBindings::Signal.
 *
 * The output of this example is:
 * ```
 * Bob received: Have a nice day!
 * Alice received: Thank you!
 * ```
 */

/**
 * @example 07-advanced-connections/main.cpp
 *
 * An example of how to use the KDBindings::Signal::connect() overloaded function for advanced slot connections.
 *
 * The output of this example is:
 * ```
 * Hello World!
 * Emitted value: 5
 * true
 * ```
 */

/**
 * @example 08-managing-connections/main.cpp
 *
 * An example of how to use a ScopedConnection and ConnectionBlocker to manage
 * when a Connection is disconnected or blocked.
 *
 * Expected output:
 * ```
 * Guard is connected: 1
 * Connection is not blocked: 3
 * Connection is not blocked: 5
 * ```
 */

} // namespace KDBindings
