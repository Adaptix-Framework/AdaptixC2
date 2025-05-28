package safe

import (
	"bytes"
	"sync"
)

type Buffer struct {
	mu  sync.Mutex
	buf bytes.Buffer
}

func (b *Buffer) Write(p []byte) (int, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.buf.Write(p)
}

func (b *Buffer) ReadNow() []byte {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.buf.Len() == 0 {
		return nil
	}
	data := b.buf.Bytes()
	b.buf.Reset()
	out := make([]byte, len(data))
	copy(out, data)
	return out
}
