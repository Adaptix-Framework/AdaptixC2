// Ref: https://github.com/RIscRIpt/pecoff

package binutil

import (
	"bufio"
	"encoding/binary"
	"io"
)

type readerAt struct {
	io.ReaderAt
	offset int64
}

type ReaderAtInto interface {
	io.Reader
	io.ReaderAt
	ReadAtInto(p interface{}, off int64) error
	ReadStringAt(off int64, maxlen int) (string, error)
}

func WrapReaderAt(r io.ReaderAt) ReaderAtInto {
	return readerAt{r, 0}
}

func (r readerAt) Read(p []byte) (n int, err error) {
	return r.ReaderAt.ReadAt(p, r.offset)
}

func (r readerAt) ReadAt(p []byte, off int64) (n int, err error) {
	return r.ReaderAt.ReadAt(p, off)
}

func (r readerAt) ReadAtInto(p interface{}, off int64) error {
	if off != 0 {
		return readerAt{r.ReaderAt, off}.ReadAtInto(p, 0)
	}
	return binary.Read(r, binary.LittleEndian, p)
}

func (r readerAt) ReadStringAt(off int64, maxlen int) (string, error) {
	if off != 0 {
		return readerAt{r.ReaderAt, off}.ReadStringAt(0, maxlen)
	}
	line, err := bufio.NewReader(r).ReadString(0)
	if err != nil {
		return "", err
	}
	if maxlen < 0 {
		maxlen = len(line) - 1
	} else if maxlen > len(line)-1 {
		maxlen = len(line) - 1
	}
	return line[:maxlen], nil
}
