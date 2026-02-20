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

func DifferenceStringsArray(a, b []string) []string {
	remove := make(map[string]struct{}, len(b))
	for _, v := range b {
		remove[v] = struct{}{}
	}

	var result []string
	for _, v := range a {
		if _, found := remove[v]; !found {
			result = append(result, v)
		}
	}
	return result
}

func ExtractJsErrorMessage(err error) string {
	if err == nil {
		return ""
	}
	msg := err.Error()
	if idx := strings.Index(msg, "Error: "); idx != -1 {
		msg = msg[idx+7:]
	} else if idx := strings.LastIndex(msg, "GoError: "); idx != -1 {
		msg = msg[idx+9:]
	}
	if idx := strings.Index(msg, " at "); idx != -1 {
		msg = msg[:idx]
	}
	return msg
}
