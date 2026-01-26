package server

import (
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/utils/std"
	"encoding/json"
	"fmt"
	"math/rand/v2"
	"strings"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsTargetsList() (string, error) {
	var targets []adaptix.TargetData

	for value := range ts.targets.Iterator() {
		target := *value.Item.(*adaptix.TargetData)
		targets = append(targets, target)
	}

	jsonTarget, err := json.Marshal(targets)
	if err != nil {
		return "", err
	}
	return string(jsonTarget), nil
}

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

	if len(newTargets) > 0 {
		// --- PRE HOOK ---
		var inputTargets []adaptix.TargetData
		for _, t := range newTargets {
			inputTargets = append(inputTargets, *t)
		}
		preEvent := &eventing.EventDataTargetAdd{Targets: inputTargets}
		if !ts.EventManager.Emit(eventing.EventTargetAdd, eventing.HookPre, preEvent) {
			if preEvent.Error != nil {
				return preEvent.Error
			}
			return fmt.Errorf("operation cancelled by hook")
		}
		// ----------------

		_ = ts.DBMS.DbTargetsAdd(newTargets)

		packet := CreateSpTargetsAdd(newTargets)
		ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTargetsRealtime)

		// --- POST HOOK ---
		postEvent := &eventing.EventDataTargetAdd{Targets: inputTargets}
		ts.EventManager.EmitAsync(eventing.EventTargetAdd, postEvent)
		// -----------------
	}

	return nil
}

func (ts *Teamserver) TsTargetsCreateAlive(agentData adaptix.AgentData) (string, error) {
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
	target.Agents = append(target.Agents, agentData.Id)

	for t_value := range ts.targets.Iterator() {
		t := t_value.Item.(*adaptix.TargetData)
		if (t.Address == target.Address && t.Address != "") || (strings.EqualFold(t.Computer, target.Computer) && std.DomainsEqual(t.Domain, target.Domain)) {
			t.Agents = append(t.Agents, agentData.Id)

			_ = ts.DBMS.DbTargetUpdate(t)

			packet := CreateSpTargetUpdate(*t)
			ts.TsSyncStateWithCategory(packet, "target:"+t.TargetId, SyncCategoryTargetsRealtime)

			return t.TargetId, nil
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
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTargetsRealtime)

	return target.TargetId, nil
}

func (ts *Teamserver) TsTargetsEdit(targetId string, computer string, domain string, address string, os int, osDesk string, tag string, info string, alive bool) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataTargetEdit{Target: adaptix.TargetData{
		TargetId: targetId,
		Computer: computer,
		Domain:   domain,
		Address:  address,
		Os:       os,
		OsDesk:   osDesk,
		Tag:      tag,
		Info:     info,
		Alive:    alive,
	}}
	if !ts.EventManager.Emit(eventing.EventTargetEdit, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

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
	ts.TsSyncStateWithCategory(packet, "target:"+target.TargetId, SyncCategoryTargetsRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataTargetEdit{Target: *target}
	ts.EventManager.EmitAsync(eventing.EventTargetEdit, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsTargetDelete(targetsId []string) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataTargetRemove{TargetIds: targetsId}
	if !ts.EventManager.Emit(eventing.EventTargetRemove, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	targetsId = preEvent.TargetIds
	// ----------------

	deleteSet := make(map[string]struct{}, len(targetsId))
	for _, id := range targetsId {
		deleteSet[id] = struct{}{}
	}

	for i := ts.targets.Len(); i > 0; i-- {
		valueTarget, ok := ts.targets.Get(i - 1)
		if ok {
			if _, exists := deleteSet[valueTarget.(*adaptix.TargetData).TargetId]; exists {
				ts.targets.Delete(i - 1)
			}
		}
	}

	go func(ids []string) {
		_ = ts.DBMS.DbTargetDeleteBatch(ids)
	}(targetsId)

	packet := CreateSpTargetDelete(targetsId)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTargetsRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataTargetRemove{TargetIds: targetsId}
	ts.EventManager.EmitAsync(eventing.EventTargetRemove, postEvent)
	// -----------------

	return nil
}

/// Setters

func (ts *Teamserver) TsTargetSetTag(targetsId []string, tag string) error {
	updateSet := make(map[string]struct{}, len(targetsId))
	for _, id := range targetsId {
		updateSet[id] = struct{}{}
	}

	var updatedTargets []*adaptix.TargetData
	for valueTarget := range ts.targets.Iterator() {
		target := valueTarget.Item.(*adaptix.TargetData)
		if _, exists := updateSet[target.TargetId]; exists {
			target.Tag = tag
			updatedTargets = append(updatedTargets, target)
		}
	}

	go func(targets []*adaptix.TargetData) {
		_ = ts.DBMS.DbTargetUpdateBatch(targets)
	}(updatedTargets)

	var ids []string
	for _, t := range updatedTargets {
		ids = append(ids, t.TargetId)
	}

	packet := CreateSpTargetSetTag(ids, tag)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTargetsRealtime)

	return nil
}

func (ts *Teamserver) TsTargetRemoveSessions(agentsId []string) error {
	targetsIdSet := make(map[string]struct{})

	for _, agentId := range agentsId {
		value, ok := ts.Agents.Get(agentId)
		if !ok {
			continue
		}
		agent, ok := value.(*Agent)
		if !ok {
			continue
		}

		agentData := agent.GetData()
		if agentData.TargetId != "" {
			targetsIdSet[agentData.TargetId] = struct{}{}
		}
	}

	var updatedTargets []*adaptix.TargetData
	var packets []interface{}

	for t_value := range ts.targets.Iterator() {
		t := t_value.Item.(*adaptix.TargetData)
		if _, exists := targetsIdSet[t.TargetId]; exists {
			t.Agents = std.DifferenceStringsArray(t.Agents, agentsId)
			updatedTargets = append(updatedTargets, t)
			packets = append(packets, CreateSpTargetUpdate(*t))
		}
	}

	go func(targets []*adaptix.TargetData) {
		_ = ts.DBMS.DbTargetUpdateBatch(targets)
	}(updatedTargets)

	for _, packet := range packets {
		if upd, ok := packet.(SyncPackerTargetUpdate); ok {
			ts.TsSyncStateWithCategory(upd, "target:"+upd.TargetId, SyncCategoryTargetsRealtime)
		} else {
			ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTargetsRealtime)
		}
	}

	return nil
}
