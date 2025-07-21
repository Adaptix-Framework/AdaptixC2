#pragma once
#include "ApiLoader.h"

template<typename T>
class Vector {
    T* v_data;
    size_t v_capacity;
    size_t v_size;
    HANDLE v_heapHandle;

    BOOL resize(size_t new_capacity) {
        T* new_data = static_cast<T*>(ApiWin->HeapAlloc(v_heapHandle, 0, new_capacity * sizeof(T)));
        if (!new_data) return false;

        if (v_data) {
            for (size_t i = 0; i < v_size; ++i) {
                new_data[i] = v_data[i];
            }
            ApiWin->HeapFree(v_heapHandle, 0, v_data);
        }

        v_data = new_data;
        v_capacity = new_capacity;
        return true;
    }

    size_t find(const T& value) const {
        for (size_t i = 0; i < v_size; ++i) {
            if (v_data[i] == value) {
                return i;
            }
        }
        return v_size;
    }

public:

    Vector() : v_data(nullptr), v_capacity(0), v_size(0) {
        v_heapHandle = ApiWin->HeapCreate(0, 0, 0);
    }

    void destroy() {
        if (v_data) {
            ApiWin->HeapFree(v_heapHandle, 0, v_data);
        }
        ApiWin->HeapDestroy(v_heapHandle);
    }

    BOOL push_back(const T& value) {
        if (v_size >= v_capacity) {
            resize(v_capacity == 0 ? 1 : v_capacity * 2);
        }
        if (v_data) {
            v_data[v_size++] = value;
            return true;
        }
        return false;
    }

    BOOL remove(size_t index) {
        if (index >= v_size) return false;

        for (size_t i = index; i < v_size - 1; ++i) {
            v_data[i] = v_data[i + 1];
        }
        --v_size;
        return true;
    }

    void pop_back() {
        if (v_size > 0)
            --v_size;
    }

    T& operator[](size_t index) { return v_data[index]; }

    const T& operator[](size_t index) const { return v_data[index]; }

    size_t size() const { return v_size; }

    size_t capacity() const { return v_capacity; }

    T* begin() { return v_data; }

    T* end() { return v_data + v_size; }
};

template<typename K, typename V>
class Map {

    struct Pair {
        K key;
        V value;
    };

    Pair* m_data;
    size_t m_capacity;
    size_t m_size;
    HANDLE m_heapHandle;

    BOOL resize(size_t new_capacity) {
        Pair* new_data = static_cast<Pair*>(ApiWin->HeapAlloc(m_heapHandle, 0, new_capacity * sizeof(Pair)));
        if (!new_data) return false;

        for (size_t i = 0; i < m_size; ++i) {
            new_data[i] = m_data[i];
        }
        ApiWin->HeapFree(m_heapHandle, 0, m_data);
        m_data = new_data;
        m_capacity = new_capacity;
        return true;
    }

    int find_index(const K& key) const {
        for (size_t i = 0; i < m_size; ++i) {
            if (m_data[i].key == key) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

public:

    Map() : m_data(nullptr), m_capacity(0), m_size(0) {
        m_heapHandle = ApiWin->HeapCreate(0, 0, 0);
    }

    void destroy() {
        if (m_data) {
            ApiWin->HeapFree(m_heapHandle, 0, m_data);
        }
        ApiWin->HeapDestroy(m_heapHandle);
    }

    class Iterator {
    private:
        Pair* ptr;
    public:
        Iterator(Pair* p) : ptr(p) {}

        Iterator& operator++() { ++ptr; return *this; }
        BOOL operator!=(const Iterator& other) const { return ptr != other.ptr; }
        Pair& operator*() { return *ptr; }
    };

    Iterator begin() { return Iterator(m_data); }

    Iterator end() { return Iterator(m_data + m_size); }

    BOOL insert(const K& key, const V& value) {
        int index = find_index(key);
        if (index != -1) {
            m_data[index].value = value;
            return true;
        }

        if (m_size >= m_capacity)
            resize(m_capacity == 0 ? 1 : m_capacity * 2);

        m_data[m_size++] = { key, value };
        return true;
    }

    BOOL get(const K& key, V& value) const {
        int index = find_index(key);
        if (index != -1) {
            value = m_data[index].value;
            return true;
        }
        return false;
    }

    BOOL contains(const K& key) const {
        int index = find_index(key);
        if (index == -1) {
            return false;
        }
        return true;
    }

    V& operator[](const K& key) {
        int index = find_index(key);
        if (index == -1) {
            insert(key, V{});
            index = find_index(key);
        }
        return m_data[index].value;
    }

    BOOL remove(const K& key) {
        int index = find_index(key);
        if (index == -1) return false;

        for (size_t i = index; i < m_size - 1; ++i) {
            m_data[i] = m_data[i + 1];
        }
        --m_size;
        return true;
    }

    size_t size() const { return m_size; }
};