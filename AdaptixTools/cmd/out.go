package cmd

import (
	"io"

	"axtool/internal/ui"
)

func colorOut(w io.Writer) io.Writer {
	return ui.NewColorWriter(w)
}
