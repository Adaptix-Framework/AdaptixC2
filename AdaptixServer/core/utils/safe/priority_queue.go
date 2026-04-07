package safe

import (
	"errors"
	"sort"
	"sync"
)

var ErrEmptyPQueue = errors.New("priority queue is empty")

type PriorityQueue struct {
	mu     sync.Mutex
	queues map[uint]*queue // priority → FIFO queue
	levels []uint          // sorted descending (highest first)
	total  int
}

func NewPriorityQueue(capacity int) *PriorityQueue {
	return &PriorityQueue{
		queues: make(map[uint]*queue),
	}
}

func (pq *PriorityQueue) Push(priority uint, value interface{}) {
	pq.mu.Lock()
	q, ok := pq.queues[priority]
	if !ok {
		q = newQueue()
		pq.queues[priority] = q
		pq.levels = append(pq.levels, priority)
		sort.Slice(pq.levels, func(i, j int) bool {
			return pq.levels[i] > pq.levels[j] // descending
		})
	}
	q.push(value)
	pq.total++
	pq.mu.Unlock()
}

func (pq *PriorityQueue) Pop() (interface{}, error) {
	pq.mu.Lock()
	defer pq.mu.Unlock()

	for _, lvl := range pq.levels {
		q := pq.queues[lvl]
		if q.len() > 0 {
			pq.total--
			return q.pop(), nil
		}
	}
	return nil, ErrEmptyPQueue
}

func (pq *PriorityQueue) PopByPriority(priority uint) (interface{}, error) {
	pq.mu.Lock()
	defer pq.mu.Unlock()

	q, ok := pq.queues[priority]
	if !ok || q.len() == 0 {
		return nil, ErrEmptyPQueue
	}
	pq.total--
	return q.pop(), nil
}

func (pq *PriorityQueue) Len() int {
	pq.mu.Lock()
	defer pq.mu.Unlock()
	return pq.total
}

func (pq *PriorityQueue) LenByPriority(priority uint) int {
	pq.mu.Lock()
	defer pq.mu.Unlock()
	q, ok := pq.queues[priority]
	if !ok {
		return 0
	}
	return q.len()
}

func (pq *PriorityQueue) IsEmpty() bool {
	pq.mu.Lock()
	defer pq.mu.Unlock()
	return pq.total == 0
}

func (pq *PriorityQueue) Clear() {
	pq.mu.Lock()
	pq.queues = make(map[uint]*queue)
	pq.levels = pq.levels[:0]
	pq.total = 0
	pq.mu.Unlock()
}

func (pq *PriorityQueue) RemoveIf(predicate func(v interface{}) bool) (bool, interface{}) {
	pq.mu.Lock()
	defer pq.mu.Unlock()

	for _, lvl := range pq.levels {
		q := pq.queues[lvl]
		if found, val := q.removeIf(predicate); found {
			pq.total--
			return true, val
		}
	}
	return false, nil
}

func (pq *PriorityQueue) FindIf(predicate func(v interface{}) bool) (bool, interface{}) {
	pq.mu.Lock()
	defer pq.mu.Unlock()

	for _, lvl := range pq.levels {
		q := pq.queues[lvl]
		if found, val := q.findIf(predicate); found {
			return true, val
		}
	}
	return false, nil
}

type queueNode struct {
	value interface{}
	next  *queueNode
}

type queue struct {
	head *queueNode
	tail *queueNode
	size int
}

func newQueue() *queue {
	return &queue{}
}

func (q *queue) push(value interface{}) {
	node := &queueNode{value: value}
	if q.tail != nil {
		q.tail.next = node
	} else {
		q.head = node
	}
	q.tail = node
	q.size++
}

func (q *queue) pop() interface{} {
	node := q.head
	q.head = node.next
	if q.head == nil {
		q.tail = nil
	}
	q.size--
	return node.value
}

func (q *queue) len() int {
	return q.size
}

func (q *queue) removeIf(predicate func(v interface{}) bool) (bool, interface{}) {
	var prev *queueNode
	for cur := q.head; cur != nil; prev, cur = cur, cur.next {
		if predicate(cur.value) {
			if prev == nil {
				q.head = cur.next
			} else {
				prev.next = cur.next
			}
			if cur == q.tail {
				q.tail = prev
			}
			q.size--
			return true, cur.value
		}
	}
	return false, nil
}

func (q *queue) findIf(predicate func(v interface{}) bool) (bool, interface{}) {
	for cur := q.head; cur != nil; cur = cur.next {
		if predicate(cur.value) {
			return true, cur.value
		}
	}
	return false, nil
}
