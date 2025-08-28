package safe

import (
	"errors"
	"sync"
)

var ErrEmptyQueue = errors.New("queue is empty")

type Queue struct {
	lock     sync.RWMutex
	buffer   []interface{}
	head     int
	tail     int
	size     int
	capacity int
}

func NewSafeQueue(capacity int) *Queue {
	if capacity <= 0 {
		capacity = 0x100
	}
	return &Queue{
		buffer:   make([]interface{}, capacity),
		capacity: capacity,
	}
}

func (q *Queue) Len() int {
	q.lock.RLock()
	defer q.lock.RUnlock()
	return q.size
}

func (q *Queue) IsEmpty() bool {
	q.lock.RLock()
	defer q.lock.RUnlock()
	return q.size == 0
}

func (q *Queue) Push(value interface{}) {
	q.lock.Lock()
	defer q.lock.Unlock()

	if q.size == q.capacity {
		q.resize()
	}
	q.buffer[q.tail] = value
	q.tail = (q.tail + 1) % q.capacity
	q.size++
}

func (q *Queue) PushFront(value interface{}) {
	q.lock.Lock()
	defer q.lock.Unlock()

	if q.size == q.capacity {
		q.resize()
	}

	q.head = (q.head - 1 + q.capacity) % q.capacity
	q.buffer[q.head] = value
	q.size++
}

func (q *Queue) Pop() (interface{}, error) {
	q.lock.Lock()
	defer q.lock.Unlock()

	if q.size == 0 {
		return nil, ErrEmptyQueue
	}
	val := q.buffer[q.head]
	q.buffer[q.head] = nil
	q.head = (q.head + 1) % q.capacity
	q.size--
	return val, nil
}

func (q *Queue) Peek() (interface{}, error) {
	q.lock.RLock()
	defer q.lock.RUnlock()

	if q.size == 0 {
		return nil, ErrEmptyQueue
	}
	return q.buffer[q.head], nil
}

func (q *Queue) Clear() {
	q.lock.Lock()
	defer q.lock.Unlock()

	q.buffer = make([]interface{}, q.capacity)
	q.head = 0
	q.tail = 0
	q.size = 0
}

func (q *Queue) resize() {
	newCap := q.capacity * 2
	newBuf := make([]interface{}, newCap)

	for i := 0; i < q.size; i++ {
		newBuf[i] = q.buffer[(q.head+i)%q.capacity]
	}
	q.buffer = newBuf
	q.head = 0
	q.tail = q.size
	q.capacity = newCap
}

func (q *Queue) RemoveIf(predicate func(v interface{}) bool) (bool, interface{}) {
	q.lock.Lock()
	defer q.lock.Unlock()

	var found bool
	var removed interface{}
	newBuf := make([]interface{}, 0, q.capacity)

	for i := 0; i < q.size; i++ {
		idx := (q.head + i) % q.capacity
		item := q.buffer[idx]
		if !found && predicate(item) {
			found = true
			removed = item
			continue
		}
		newBuf = append(newBuf, item)
	}

	if !found {
		return false, nil
	}

	q.buffer = make([]interface{}, q.capacity)
	copy(q.buffer, newBuf)
	q.head = 0
	q.tail = len(newBuf)
	q.size = len(newBuf)

	return true, removed
}

func (q *Queue) FindIf(predicate func(v interface{}) bool) (bool, interface{}) {
	q.lock.RLock()
	defer q.lock.RUnlock()

	for i := 0; i < q.size; i++ {
		idx := (q.head + i) % q.capacity
		item := q.buffer[idx]
		if predicate(item) {
			return true, item
		}
	}
	return false, nil
}
