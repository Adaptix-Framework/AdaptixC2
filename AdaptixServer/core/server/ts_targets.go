package server

import (
	"AdaptixServer/core/utils/std"
	"fmt"
	adaptix "github.com/Adaptix-Framework/axc2"
	"math/rand"
	"strings"
	"time"
)

func (ts *Teamserver) TsTargetsAdd(targets []map[string]interface{}) error {
	var newTargets []adaptix.TargetData

	for _, value := range targets {
		var target adaptix.TargetData
		if v, ok := value["computer"].(string); ok {
			target.Computer = v
		}
		if v, ok := value["domain"].(string); ok {
			target.Domain = v
		}
		if v, ok := value["address"].(string); ok {
			target.Address = v
		}
		if v, ok := value["os"].(float64); ok {
			target.Os = int(v)
		}
		if v, ok := value["os_desk"].(string); ok {
			target.OsDesk = v
		}
		if v, ok := value["tag"].(string); ok {
			target.Tag = v
		}
		if v, ok := value["info"].(string); ok {
			target.Info = v
		}
		if v, ok := value["alive"].(bool); ok {
			target.Alive = v
		}

		found := false

		for t_value := range ts.targets.Iterator() {
			t := t_value.Item.(adaptix.TargetData)
			if (t.Address == target.Address && t.Address != "") || (strings.EqualFold(t.Computer, target.Computer) && std.DomainsEqual(t.Domain, target.Domain)) {
				found = true
				break
			}
		}
		if found {
			continue
		}

		target.TargetId = fmt.Sprintf("%08x", rand.Uint32())
		target.Date = time.Now().Unix()
		if target.OsDesk == "" {
			if target.Os == 1 {
				target.OsDesk = "Windows"
			} else if target.Os == 2 {
				target.OsDesk = "Linux"
			} else if target.Os == 3 {
				target.OsDesk = "MacOS"
			}
		}

		newTargets = append(newTargets, target)
		ts.targets.Put(target)
	}

	_ = ts.DBMS.DbTargetsAdd(newTargets)

	packet := CreateSpTargetsAdd(newTargets)
	ts.TsSyncAllClients(packet)

	return nil
}
