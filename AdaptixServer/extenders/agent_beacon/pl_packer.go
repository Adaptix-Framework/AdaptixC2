package main

import (
	"bytes"
	"encoding/binary"
)

type Packer struct {
	buffer []byte
}

func CreatePacker(buffer []byte) *Packer {
	return &Packer{
		buffer: buffer,
	}
}

func (p *Packer) Size() uint {
	return uint(len(p.buffer))
}

func (p *Packer) ParseInt8() uint8 {
	var value = make([]byte, 1)

	if p.Size() >= 1 {
		if p.Size() == 1 {
			copy(value, p.buffer[:p.Size()])
			p.buffer = []byte{}
		} else {
			copy(value, p.buffer[:1])
			p.buffer = p.buffer[1:]
		}
	}
	return uint8(value[0])
}

func (p *Packer) ParseInt16() uint16 {
	var value = make([]byte, 2)

	if p.Size() >= 2 {
		if p.Size() == 2 {
			copy(value, p.buffer[:p.Size()])
			p.buffer = []byte{}
		} else {
			copy(value, p.buffer[:2])
			p.buffer = p.buffer[2:]
		}
	}

	return uint16(binary.BigEndian.Uint16(value))
}

func (p *Packer) ParseInt32() uint {
	var value = make([]byte, 4)

	for i := range value {
		value[i] = 0
	}

	if p.Size() >= 4 {
		if p.Size() == 4 {
			copy(value, p.buffer[:p.Size()])
			p.buffer = []byte{}
		} else {
			copy(value, p.buffer[:4])
			p.buffer = p.buffer[4:]
		}
	}

	return uint(binary.BigEndian.Uint32(value))
}

func (p *Packer) ParseBytes() []byte {
	size := p.ParseInt32()

	if p.Size() < size {
		return make([]byte, 0)
	} else {
		b := p.buffer[:size]
		p.buffer = p.buffer[size:]
		return b
	}
}

func (p *Packer) ParseString() string {
	size := p.ParseInt32()

	if p.Size() < size {
		return ""
	} else {
		b := p.buffer[:size]
		p.buffer = p.buffer[size:]
		return string(bytes.Trim(b, "\x00"))
	}
}
