package main

import (
	"crypto/rc4"
	"errors"
	"fmt"
	"net"
	"regexp"
	"strconv"
	"strings"
)

const (
	COMMAND_CAT          = 24
	COMMAND_COPY         = 12
	COMMAND_CD           = 8
	COMMAND_DISKS        = 15
	COMMAND_DOWNLOAD     = 32
	COMMAND_EXEC_BOF     = 50
	COMMAND_EXEC_BOF_OUT = 51
	COMMAND_EXFIL        = 35
	COMMAND_GETUID       = 22
	COMMAND_JOBS_KILL    = 47
	COMMAND_JOB_LIST     = 46
	COMMAND_LINK         = 38
	COMMAND_LS           = 14
	COMMAND_MV           = 18
	COMMAND_MKDIR        = 27
	COMMAND_PIVOT_EXEC   = 37
	COMMAND_PS_LIST      = 41
	COMMAND_PS_KILL      = 42
	COMMAND_PS_RUN       = 43
	COMMAND_PROFILE      = 21
	COMMAND_PWD          = 4
	COMMAND_REV2SELF     = 23
	COMMAND_RM           = 17
	COMMAND_TERMINATE    = 10
	COMMAND_UNLINK       = 39
	COMMAND_UPLOAD       = 33

	COMMAND_TUNNEL_START_TCP = 62
	COMMAND_TUNNEL_START_UDP = 63
	COMMAND_TUNNEL_WRITE_TCP = 64
	COMMAND_TUNNEL_WRITE_UDP = 65
	COMMAND_TUNNEL_CLOSE     = 66
	COMMAND_TUNNEL_REVERSE   = 67
	COMMAND_TUNNEL_ACCEPT    = 68

	COMMAND_SHELL_START  = 71
	COMMAND_SHELL_WRITE  = 72
	COMMAND_SHELL_CLOSE  = 73
	COMMAND_SHELL_ACCEPT = 74

	COMMAND_JOB        = 0x8437
	COMMAND_SAVEMEMORY = 0x2321
	COMMAND_ERROR      = 0x1111ffff
)

const (
	DOWNLOAD_START    = 0x1
	DOWNLOAD_CONTINUE = 0x2
	DOWNLOAD_FINISH   = 0x3
)

const (
	JOB_TYPE_LOCAL   = 0x1
	JOB_TYPE_REMOTE  = 0x2
	JOB_TYPE_PROCESS = 0x3
	JOB_TYPE_SHELL   = 0x4
)

const (
	JOB_STATE_STARTING = 0x0
	JOB_STATE_RUNNING  = 0x1
	JOB_STATE_FINISHED = 0x2
	JOB_STATE_KILLED   = 0x3
)

const (
	CALLBACK_OUTPUT      = 0x0
	CALLBACK_OUTPUT_OEM  = 0x1e
	CALLBACK_OUTPUT_UTF8 = 0x20
	CALLBACK_ERROR       = 0x0d
	CALLBACK_CUSTOM      = 0x1000
	CALLBACK_CUSTOM_LAST = 0x13ff

	CALLBACK_AX_SCREENSHOT   = 0x81
	CALLBACK_AX_DOWNLOAD_MEM = 0x82

	BOF_ERROR_PARSE     = 0x101
	BOF_ERROR_SYMBOL    = 0x102
	BOF_ERROR_MAX_FUNCS = 0x103
	BOF_ERROR_ENTRY     = 0x104
	BOF_ERROR_ALLOC     = 0x105
)

func GetOsVersion(majorVersion uint8, minorVersion uint8, buildNumber uint, isServer bool, systemArch string) (int, string) {
	var (
		desc string
		os   = OS_UNKNOWN
	)

	osVersion := "unknown"
	if majorVersion == 10 && minorVersion == 0 && isServer && buildNumber >= 26100 {
		osVersion = "Win 2025 Serv"
	} else if majorVersion == 10 && minorVersion == 0 && isServer && buildNumber >= 19045 {
		osVersion = "Win 2022 Serv"
	} else if majorVersion == 10 && minorVersion == 0 && isServer && buildNumber >= 17763 {
		osVersion = "Win 2019 Serv"
	} else if majorVersion == 10 && minorVersion == 0 && !isServer && buildNumber >= 22000 {
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
	if strings.Contains(osVersion, "Win") {
		os = OS_WINDOWS
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

func SizeBytesToFormat(bytes int64) string {
	const (
		KB = 1024.0
		MB = KB * 1024
		GB = MB * 1024
	)

	size := float64(bytes)

	if size >= GB {
		return fmt.Sprintf("%.2f Gb", size/GB)
	} else if size >= MB {
		return fmt.Sprintf("%.2f Mb", size/MB)
	} else {
		return fmt.Sprintf("%.2f Kb", size/KB)
	}
}

func RC4Crypt(data []byte, key []byte) ([]byte, error) {
	rc4crypt, errcrypt := rc4.NewCipher(key)
	if errcrypt != nil {
		return nil, errors.New("rc4 crypt error")
	}
	decryptData := make([]byte, len(data))
	rc4crypt.XORKeyStream(decryptData, data)
	return decryptData, nil
}

func parseDurationToSeconds(input string) (int, error) {
	re := regexp.MustCompile(`(\d+)(h|m|s)`)
	matches := re.FindAllStringSubmatch(input, -1)

	totalSeconds := 0
	for _, match := range matches {
		value, err := strconv.Atoi(match[1])
		if err != nil {
			return 0, err
		}

		switch match[2] {
		case "h":
			totalSeconds += value * 3600
		case "m":
			totalSeconds += value * 60
		case "s":
			totalSeconds += value
		}
	}

	return totalSeconds, nil
}

func parseStringToWorkingTime(WorkingTime string) (int, error) {
	IntWorkingTime := 0
	if WorkingTime != "" {
		match, err := regexp.MatchString("^[012]?[0-9]:[0-6][0-9]-[012]?[0-9]:[0-6][0-9]$", WorkingTime)
		if err != nil || match == false {
			return IntWorkingTime, errors.New("Failed to parse working time: Invalid format")
		}

		startAndEnd := strings.Split(WorkingTime, "-")
		startHourandMinutes := strings.Split(startAndEnd[0], ":")
		endHourandMinutes := strings.Split(startAndEnd[1], ":")

		startHour, _ := strconv.Atoi(startHourandMinutes[0])
		startMin, _ := strconv.Atoi(startHourandMinutes[1])
		endHour, _ := strconv.Atoi(endHourandMinutes[0])
		endMin, _ := strconv.Atoi(endHourandMinutes[1])

		if startHour < 0 || startHour > 24 || endHour < 0 || endHour > 24 || startMin < 0 || startMin > 60 || endMin < 0 || endMin > 60 {
			return IntWorkingTime, errors.New("Failed to parse working time: Incorrectly defined time")
		}

		if endHour < startHour || (startHour == endHour && endMin <= startMin) {
			return IntWorkingTime, errors.New("Failed to parse working time: The end hour cannot be earlier than the start hour")
		}

		IntWorkingTime |= startHour << 24
		IntWorkingTime |= startMin << 16
		IntWorkingTime |= endHour << 8
		IntWorkingTime |= endMin << 0
	}

	return IntWorkingTime, nil
}
