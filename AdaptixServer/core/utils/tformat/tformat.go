package tformat

import (
	"bytes"
	"fmt"
	"image"
	_ "image/gif"
	_ "image/jpeg"
	_ "image/png"

	_ "golang.org/x/image/bmp"
)

const (
	Reset = "\033[0m"

	Red    = "\033[31m"
	Green  = "\033[32m"
	Yellow = "\033[33m"
	Blue   = "\033[34m"
	Cyan   = "\033[36m"
	White  = "\033[37m"

	Bold = "\033[1m"
)

func SetColor(color string, text string) string {
	return color + text + Reset
}

func SetBold(text string) string {
	return Bold + text + Reset
}

func SizeBytesToFormat(bytes uint64) string {
	const (
		KB = 1024.0
		MB = KB * 1024
		GB = MB * 1024
	)

	size := float64(bytes)

	if size >= GB {
		return fmt.Sprintf("%.2f Gb", size/GB)
	} else if size >= MB {
		return fmt.Sprintf("%.2f Mb", size/MB)
	} else if size >= 1000 {
		return fmt.Sprintf("%.2f Kb", size/KB)
	}
	return fmt.Sprintf("%v bytes", size)
}

func DetectImageFormat(data []byte) (string, error) {
	reader := bytes.NewReader(data)
	_, format, err := image.Decode(reader)
	if err != nil {
		return "", err
	}
	return format, nil
}
