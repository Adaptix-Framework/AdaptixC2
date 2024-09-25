package safe

import (
	"sync"
)

type Slice struct {
	mutex sync.RWMutex
	items []interface{}
}

type SliceItem struct {
	Index int
	Item  interface{}
}

func NewSlice() *Slice {
	return &Slice{}
}

func (sl *Slice) Put(value interface{}) {
	sl.mutex.Lock()
	defer sl.mutex.Unlock()

	sl.items = append(sl.items, value)
}

func (sl *Slice) Get(index int) (interface{}, bool) {
	sl.mutex.RLock()
	defer sl.mutex.RUnlock()

	if index < 0 || index >= len(sl.items) {
		return nil, false
	}
	return sl.items[index], true
}

func (sl *Slice) Delete(index int) {
	sl.mutex.Lock()
	defer sl.mutex.Unlock()

	if index < 0 || index >= len(sl.items) {
		return
	}
	sl.items = append(sl.items[:index], sl.items[index+1:]...)
}

func (sl *Slice) Len() int {
	sl.mutex.RLock()
	defer sl.mutex.RUnlock()
	return len(sl.items)
}

func (sl *Slice) Iterator() <-chan SliceItem {
	sl.mutex.RLock()
	defer sl.mutex.RUnlock()

	ch := make(chan SliceItem)
	go func() {
		defer close(ch)
		for i, item := range sl.items {
			ch <- SliceItem{Index: i, Item: item}
		}
	}()
	return ch
}
