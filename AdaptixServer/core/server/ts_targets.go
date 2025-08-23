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
	var newTargets []*adaptix.TargetData

	for _, value := range targets {
		target := &adaptix.TargetData{}
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
			t := t_value.Item.(*adaptix.TargetData)
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

func (ts *Teamserver) TsTargetsCreateAlive(agentData adaptix.AgentData) error {
	target := &adaptix.TargetData{
		TargetId: fmt.Sprintf("%08x", rand.Uint32()),
		Computer: agentData.Computer,
		Domain:   agentData.Domain,
		Address:  agentData.InternalIP,
		Os:       agentData.Os,
		OsDesk:   agentData.OsDesc,
		Date:     time.Now().Unix(),
		Alive:    true,
	}

	for t_value := range ts.targets.Iterator() {
		t := t_value.Item.(*adaptix.TargetData)
		if (t.Address == target.Address && t.Address != "") || (strings.EqualFold(t.Computer, target.Computer) && std.DomainsEqual(t.Domain, target.Domain)) {
			return nil
		}
	}

	if target.OsDesk == "" {
		if target.Os == 1 {
			target.OsDesk = "Windows"
		} else if target.Os == 2 {
			target.OsDesk = "Linux"
		} else if target.Os == 3 {
			target.OsDesk = "MacOS"
		}
	}

	var newTargets []*adaptix.TargetData
	newTargets = append(newTargets, target)
	ts.targets.Put(target)

	_ = ts.DBMS.DbTargetsAdd(newTargets)

	packet := CreateSpTargetsAdd(newTargets)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsTargetsEdit(targetId string, computer string, domain string, address string, os int, osDesk string, tag string, info string, alive bool) error {

	var target *adaptix.TargetData

	found := false
	for t_value := range ts.targets.Iterator() {
		target = t_value.Item.(*adaptix.TargetData)
		if target.TargetId == targetId {
			if target.Computer == computer && target.Domain == domain && target.Address == address && target.Os == os && target.OsDesk == osDesk && target.Tag == tag && target.Info == info && target.Alive == alive {
				return nil
			}

			found = true

			target.Computer = computer
			target.Domain = domain
			target.Address = address
			target.Os = os
			target.OsDesk = osDesk
			target.Tag = tag
			target.Info = info
			target.Alive = alive
			target.Date = time.Now().Unix()
			break
		}
	}

	if !found {
		return fmt.Errorf("target %s not exists", targetId)
	}

	_ = ts.DBMS.DbTargetUpdate(target)

	packet := CreateSpTargetUpdate(*target)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsTargetDelete(targetsId []string) error {

	for _, id := range targetsId {
		for i := uint(0); i < ts.targets.Len(); i++ {
			valueTarget, ok := ts.targets.Get(i)
			if ok {
				if valueTarget.(*adaptix.TargetData).TargetId == id {
					ts.targets.Delete(i)
					break
				}
			}
		}

		_ = ts.DBMS.DbTargetDelete(id)
	}

	packet := CreateSpTargetDelete(targetsId)
	ts.TsSyncAllClients(packet)

	return nil
}

/// Setters

func (ts *Teamserver) TsTargetSetTag(targetsId []string, tag string) error {

	var ids []string
	for valueTarget := range ts.targets.Iterator() {
		target := valueTarget.Item.(*adaptix.TargetData)
		found := false

		for i := len(targetsId) - 1; i >= 0; i-- {
			if target.TargetId == targetsId[i] {
				target.Tag = tag
				found = true
				_ = ts.DBMS.DbTargetUpdate(target)
				ids = append(ids, target.TargetId)
				targetsId = append(targetsId[:i], targetsId[i+1:]...)
				break
			}
		}

		if found && len(targetsId) == 0 {
			break
		}
	}

	packet := CreateSpTargetSetTag(ids, tag)
	ts.TsSyncAllClients(packet)

	return nil
}
