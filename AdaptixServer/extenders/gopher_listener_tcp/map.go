package main

import (
	"sync"
)

type Map[K comparable, V any] struct {
	mutex sync.RWMutex
	m     map[K]V
}

func NewMap[K comparable, V any]() Map[K, V] {
	return Map[K, V]{
		m: make(map[K]V),
	}
}

func (s *Map[K, V]) Contains(key K) bool {
	s.mutex.RLock()
	defer s.mutex.RUnlock()
	_, exists := s.m[key]
	return exists
}

func (s *Map[K, V]) Put(key K, value V) {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	s.m[key] = value
}

func (s *Map[K, V]) Get(key K) (V, bool) {
	s.mutex.RLock()
	defer s.mutex.RUnlock()
	value, exists := s.m[key]
	return value, exists
}

func (s *Map[K, V]) Delete(key K) {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	delete(s.m, key)
}

func (s *Map[K, V]) GetDelete(key K) (V, bool) {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	if value, exists := s.m[key]; exists {
		delete(s.m, key)
		return value, true
	}
	var zero V
	return zero, false
}

func (s *Map[K, V]) Len() int {
	s.mutex.RLock()
	defer s.mutex.RUnlock()
	return len(s.m)
}

func (s *Map[K, V]) ForEach(f func(key K, value V) bool) {
	s.mutex.RLock()
	copyMap := make(map[K]V, len(s.m))
	for k, v := range s.m {
		copyMap[k] = v
	}
	s.mutex.RUnlock()

	for key, value := range copyMap {
		if !f(key, value) {
			break
		}
	}
}

func (s *Map[K, V]) DirectLock() {
	s.mutex.RLock()
}

func (s *Map[K, V]) DirectUnlock() {
	s.mutex.RUnlock()
}

func (s *Map[K, V]) DirectMap() map[K]V {
	return s.m
}

func (s *Map[K, V]) CutMap() map[K]V {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	oldMap := s.m
	s.m = make(map[K]V)

	return oldMap
}
