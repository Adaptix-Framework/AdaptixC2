package tformat

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
