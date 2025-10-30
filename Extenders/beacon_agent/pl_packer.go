package main

import (
	"bytes"
	"encoding/binary"
	"errors"
	"strings"
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

func (p *Packer) CheckPacker(types []string) bool {

	packerSize := p.Size()

	for _, t := range types {
		switch t {

		case "byte":
			if packerSize < 1 {
				return false
			}
			packerSize -= 1

		case "word":
			if packerSize < 2 {
				return false
			}
			packerSize -= 2

		case "int":
			if packerSize < 4 {
				return false
			}
			packerSize -= 4

		case "long":
			if packerSize < 8 {
				return false
			}
			packerSize -= 8

		case "array":
			if packerSize < 4 {
				return false
			}

			index := p.Size() - packerSize
			value := make([]byte, 4)
			copy(value, p.buffer[index:index+4])
			length := uint(binary.BigEndian.Uint32(value))
			packerSize -= 4

			if packerSize < length {
				return false
			}
			packerSize -= length
		}
	}
	return true
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
	} else {
		return 0
	}

	return value[0]
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
	} else {
		return 0
	}

	return binary.BigEndian.Uint16(value)
}

func (p *Packer) ParseInt32() uint {
	var value = make([]byte, 4)

	if p.Size() >= 4 {
		if p.Size() == 4 {
			copy(value, p.buffer[:p.Size()])
			p.buffer = []byte{}
		} else {
			copy(value, p.buffer[:4])
			p.buffer = p.buffer[4:]
		}
	} else {
		return 0
	}

	return uint(binary.BigEndian.Uint32(value))
}

func (p *Packer) ParseInt64() uint64 {
	var value = make([]byte, 8)

	if p.Size() >= 8 {
		if p.Size() == 8 {
			copy(value, p.buffer[:p.Size()])
			p.buffer = []byte{}
		} else {
			copy(value, p.buffer[:8])
			p.buffer = p.buffer[8:]
		}
	} else {
		return 0
	}

	return binary.BigEndian.Uint64(value)
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

func PackArray(array []interface{}) ([]byte, error) {
	var packData []byte

	for i := range array {

		switch array[i].(type) {

		case []byte:
			val := array[i].([]byte)
			packData = append(packData, val...)
			break

		case string:
			size := make([]byte, 4)
			val := array[i].(string)
			if len(val) != 0 {
				if strings.HasSuffix(val, "\x00") == false {
					val += "\x00"
				}
			}
			binary.LittleEndian.PutUint32(size, uint32(len(val)))
			packData = append(packData, size...)
			packData = append(packData, []byte(val)...)
			break

		case int:
			num := make([]byte, 4)
			val := array[i].(int)
			binary.LittleEndian.PutUint32(num, uint32(val))
			packData = append(packData, num...)
			break

		case bool:
			b := array[i].(bool)
			var bt = make([]byte, 1)
			bt[0] = 0
			if b {
				bt[0] = 1
			}
			packData = append(packData, bt...)
			break

		default:
			return nil, errors.New("PackArray unknown type")
		}
	}

	return packData, nil
}
