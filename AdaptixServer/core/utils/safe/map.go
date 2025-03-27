package safe

import (
	"sync"
)

type Map struct {
	mutex sync.RWMutex
	m     map[string]interface{}
}

func NewMap() Map {
	return Map{
		m: make(map[string]interface{}),
	}
}

func (s *Map) Contains(key string) bool {
	s.mutex.RLock()
	defer s.mutex.RUnlock()
	_, exists := s.m[key]
	return exists
}

func (s *Map) Put(key string, value interface{}) {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	s.m[key] = value
}

func (s *Map) Get(key string) (interface{}, bool) {
	s.mutex.RLock()
	defer s.mutex.RUnlock()
	value, exists := s.m[key]
	return value, exists
}

func (s *Map) Delete(key string) {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	delete(s.m, key)
}

func (s *Map) GetDelete(key string) (interface{}, bool) {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	if value, exists := s.m[key]; exists {
		delete(s.m, key)
		return value, true
	}
	return nil, false
}

func (s *Map) Len() int {
	s.mutex.RLock()
	defer s.mutex.RUnlock()
	return len(s.m)
}

func (s *Map) ForEach(f func(key string, value interface{}) bool) {
	s.mutex.RLock()
	copyMap := make(map[string]interface{}, len(s.m))
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

func (s *Map) DirectLock() {
	s.mutex.RLock()
}

func (s *Map) DirectUnlock() {
	s.mutex.RUnlock()
}

func (s *Map) DirectMap() map[string]interface{} {
	return s.m
}

func (s *Map) CutMap() map[string]interface{} {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	oldMap := s.m
	s.m = make(map[string]interface{})

	return oldMap
}
