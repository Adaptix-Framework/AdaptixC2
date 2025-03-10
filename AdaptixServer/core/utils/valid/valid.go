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
