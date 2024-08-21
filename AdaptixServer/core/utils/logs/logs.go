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

func logMessage(symbol string, color string, format string, a ...interface{}) {
	timestamp := tformat.SetBold(time.Now().Format("2006-01-02 15:04:05"))
	message := fmt.Sprintf(format, a...)
	mark := tformat.SetColor(symbol, color)
	fmt.Printf("%s %s (%s)\n", mark, message, timestamp)
}

func Info(format string, a ...interface{}) {
	logMessage("[*]", tformat.Green, format, a...)
}

func Success(format string, a ...interface{}) {
	logMessage("[+]", tformat.Blue, format, a...)
}

func Warn(format string, a ...interface{}) {
	logMessage("[!]", tformat.Yellow, format, a...)
}

func Error(format string, a ...interface{}) {
	logMessage("[-]", tformat.Red, format, a...)
}

func Debug(format string, a ...interface{}) {
	if PrintLog.debug {
		logMessage("[#]", tformat.Cyan, format, a...)
	}
}
