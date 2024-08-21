package isvalid

import "regexp"

func IsValidUriString(s string) bool {
	re := regexp.MustCompile(`^/(?:[a-zA-Z0-9]+(?:/[a-zA-Z0-9]+)*)?$`)
	return re.MatchString(s)
}
