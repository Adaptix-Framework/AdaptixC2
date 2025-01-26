package logs

import (
	"AdaptixServer/core/utils/tformat"
	"fmt"
	"time"
)

type PrintLogger struct {
	debug bool
}

var PrintLog *PrintLogger

func NewPrintLogger(debug bool) {
	PrintLog = &PrintLogger{
		debug: debug,
	}
}

func logMessage(indent string, symbol string, color string, format string, a ...interface{}) {
	timestamp := tformat.SetBold(time.Now().Format("02/01 15:04:05"))
	message := fmt.Sprintf(format, a...)
	mark := tformat.SetColor(symbol, color)
	fmt.Printf("%s%s %s [%s]\n", indent, mark, message, timestamp)
}

func Info(indent string, format string, a ...interface{}) {
	logMessage(indent, "[*]", tformat.Green, format, a...)
}

func Success(indent string, format string, a ...interface{}) {
	logMessage(indent, "[+]", tformat.Blue, format, a...)
}

func Warn(indent string, format string, a ...interface{}) {
	logMessage(indent, "[!]", tformat.Yellow, format, a...)
}

func Error(indent string, format string, a ...interface{}) {
	logMessage(indent, "[-]", tformat.Red, format, a...)
}

func Debug(indent string, format string, a ...interface{}) {
	if PrintLog.debug {
		logMessage(indent, "[#]", tformat.Cyan, format, a...)
	}
}
