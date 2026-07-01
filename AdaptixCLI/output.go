package main

import (
	"encoding/json"
	"fmt"
	"os"
	"text/tabwriter"
)

func printJSON(v interface{}) {
	data, _ := json.MarshalIndent(v, "", "  ")
	fmt.Println(string(data))
}

func newTabWriter() *tabwriter.Writer {
	return tabwriter.NewWriter(os.Stdout, 2, 4, 2, ' ', 0)
}

func osName(osType int) string {
	switch osType {
	case 1:
		return "Windows"
	case 2:
		return "Linux"
	case 3:
		return "macOS"
	default:
		return fmt.Sprintf("? (%d)", osType)
	}
}

func stateName(state int) string {
	switch state {
	case 1:
		return "RUNNING"
	case 2:
		return "STOPPED"
	case 3:
		return "FINISHED"
	case 4:
		return "CANCELED"
	default:
		return fmt.Sprintf("? (%d)", state)
	}
}

func taskTypeName(t int) string {
	switch t {
	case 0:
		return "LOCAL"
	case 1:
		return "TASK"
	case 2:
		return "BROWSER"
	case 3:
		return "JOB"
	case 4:
		return "TUNNEL"
	default:
		return fmt.Sprintf("? (%d)", t)
	}
}
