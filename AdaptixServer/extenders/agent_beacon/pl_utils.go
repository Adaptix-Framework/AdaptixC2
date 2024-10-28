package main

import (
	"golang.org/x/text/encoding/charmap"
	"golang.org/x/text/transform"
	"io"
	"net"
	"strings"
)

var codePageMapping = map[int]*charmap.Charmap{
	037:   charmap.CodePage037,  // IBM EBCDIC US-Canada
	437:   charmap.CodePage437,  // OEM United States
	850:   charmap.CodePage850,  // Western European (DOS)
	852:   charmap.CodePage852,  // Central European (DOS)
	855:   charmap.CodePage855,  // OEM Cyrillic (primarily Russian)
	858:   charmap.CodePage858,  // OEM Multilingual Latin 1 + Euro
	860:   charmap.CodePage860,  // Portuguese (DOS)
	862:   charmap.CodePage862,  // Hebrew (DOS)
	863:   charmap.CodePage863,  // French Canadian (DOS)
	865:   charmap.CodePage865,  // Nordic (DOS)
	866:   charmap.CodePage866,  // Russian (DOS)
	1047:  charmap.CodePage1047, // IBM EBCDIC Latin 1/Open System
	1140:  charmap.CodePage1140, // IBM EBCDIC US-Canada with Euro
	1250:  charmap.Windows1250,  // Central European (Windows)
	1251:  charmap.Windows1251,  // Cyrillic (Windows)
	1252:  charmap.Windows1252,  // Western European (Windows)
	1253:  charmap.Windows1253,  // Greek (Windows)
	1254:  charmap.Windows1254,  // Turkish (Windows)
	1255:  charmap.Windows1255,  // Hebrew (Windows)
	1256:  charmap.Windows1256,  // Arabic (Windows)
	1257:  charmap.Windows1257,  // Baltic (Windows)
	1258:  charmap.Windows1258,  // Vietnamese (Windows)
	20866: charmap.KOI8R,        // Russian (KOI8-R)
	21866: charmap.KOI8U,        // Ukrainian (KOI8-U)
	28591: charmap.ISO8859_1,    // Western European (ISO 8859-1)
	28592: charmap.ISO8859_2,    // Central European (ISO 8859-2)
	28593: charmap.ISO8859_3,    // Latin 3 (ISO 8859-3)
	28594: charmap.ISO8859_4,    // Baltic (ISO 8859-4)
	28595: charmap.ISO8859_5,    // Cyrillic (ISO 8859-5)
	28596: charmap.ISO8859_6,    // Arabic (ISO 8859-6)
	28597: charmap.ISO8859_7,    // Greek (ISO 8859-7)
	28598: charmap.ISO8859_8,    // Hebrew (ISO 8859-8)
	28599: charmap.ISO8859_9,    // Turkish (ISO 8859-9)
	28605: charmap.ISO8859_15,   // Latin 9 (ISO 8859-15)
}

func ConvertCpToUTF8(input string, codePage int) string {
	enc, exists := codePageMapping[codePage]
	if !exists {
		return input
	}

	reader := transform.NewReader(strings.NewReader(input), enc.NewDecoder())
	utf8Text, err := io.ReadAll(reader)
	if err != nil {
		return input
	}

	return string(utf8Text)
}

func ConvertUTF8toCp(input string, codePage int) string {
	enc, exists := codePageMapping[codePage]
	if !exists {
		return input
	}

	transform.NewWriter(io.Discard, enc.NewEncoder())
	encodedText, err := io.ReadAll(transform.NewReader(strings.NewReader(input), enc.NewEncoder()))
	if err != nil {
		return input
	}

	return string(encodedText)
}

func GetOsVersion(majorVersion uint8, minorVersion uint8, buildNumber uint, isServer bool, systemArch string) (int, string) {
	var (
		desc string
		os   = 0
	)

	osVersion := "unknown"
	if majorVersion == 10 && minorVersion == 0 && isServer && buildNumber == 20348 {
		osVersion = "Win 2022 Serv"
	} else if majorVersion == 10 && minorVersion == 0 && isServer && buildNumber == 17763 {
		osVersion = "Win 2019 Serv"
	} else if majorVersion == 10 && minorVersion == 0 && !isServer && (buildNumber >= 22000 && buildNumber <= 22621) {
		osVersion = "Win 11"
	} else if majorVersion == 10 && minorVersion == 0 && isServer {
		osVersion = "Win 2016 Serv"
	} else if majorVersion == 10 && minorVersion == 0 {
		osVersion = "Win 10"
	} else if majorVersion == 6 && minorVersion == 3 && isServer {
		osVersion = "Win Serv 2012 R2"
	} else if majorVersion == 6 && minorVersion == 3 {
		osVersion = "Win 8.1"
	} else if majorVersion == 6 && minorVersion == 2 && isServer {
		osVersion = "Win Serv 2012"
	} else if majorVersion == 6 && minorVersion == 2 {
		osVersion = "Win 8"
	} else if majorVersion == 6 && minorVersion == 1 && isServer {
		osVersion = "Win Serv 2008 R2"
	} else if majorVersion == 6 && minorVersion == 1 {
		osVersion = "Win 7"
	}

	desc = osVersion + " " + systemArch
	if strings.HasSuffix(osVersion, "Win") {
		os = 1
	}
	return os, desc
}

func int32ToIPv4(ip uint) string {
	bytes := []byte{
		byte(ip),
		byte(ip >> 8),
		byte(ip >> 16),
		byte(ip >> 24),
	}
	return net.IP(bytes).String()
}
