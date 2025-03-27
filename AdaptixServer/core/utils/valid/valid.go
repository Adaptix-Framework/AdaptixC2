package isvalid

import "regexp"

func ValidListenerName(s string) bool {
	re := regexp.MustCompile("^[a-zA-Z0-9-_]+$")
	return re.MatchString(s)
}

func ValidUriString(s string) bool {
	re := regexp.MustCompile(`^/(?:[a-zA-Z0-9-_]+(?:/[a-zA-Z0-9-_]+)*)?$`)
	return re.MatchString(s)
}

func ValidSBString(s string) bool {
	re := regexp.MustCompile(`^[a-zA-Z]+$`)
	return re.MatchString(s)
}

func ValidSBNString(s string) bool {
	re := regexp.MustCompile(`^[a-zA-Z0-9-_]+$`)
	return re.MatchString(s)
}

func ValidColorRGB(color string) bool {
	re := regexp.MustCompile("^#[0-9A-Fa-f]{6}$")
	return re.MatchString(color)
}

func ValidHex8(s string) bool {
	match, _ := regexp.MatchString("^[0-9a-f]{8}$", s)
	return match
}
