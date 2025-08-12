package std

import "strings"

func DomainsEqual(d1, d2 string) bool {
	if d1 == "" || d2 == "" {
		return true
	}

	d1 = strings.ToLower(d1)
	d2 = strings.ToLower(d2)

	if d1 == d2 {
		return true
	}

	nb1 := strings.SplitN(d1, ".", 2)[0]
	nb2 := strings.SplitN(d2, ".", 2)[0]

	return nb1 == nb2
}
