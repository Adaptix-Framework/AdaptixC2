package server

import (
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/utils/std"
	"encoding/json"
	"fmt"
	"math/rand/v2"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsTargetsList() (string, error) {
	dbTargets := ts.DBMS.DbTargetsAll()
	targets := make([]adaptix.TargetData, 0, len(dbTargets))
	for _, t := range dbTargets {
		targets = append(targets, *t)
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

		existing, _ := ts.DBMS.DbTargetFindByMatch(target.Address, target.Computer, target.Domain)
		if existing != nil {
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
		inputTargets = preEvent.Targets /// can be modified by hooks
		newTargets = make([]*adaptix.TargetData, 0, len(inputTargets))
		for i := range inputTargets {
			newTargets = append(newTargets, &inputTargets[i])
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

	existing, _ := ts.DBMS.DbTargetFindByMatch(target.Address, target.Computer, target.Domain)
	if existing != nil {
		existing.Agents = append(existing.Agents, agentData.Id)

		_ = ts.DBMS.DbTargetUpdate(existing)

		packet := CreateSpTargetUpdate(*existing)
		ts.TsSyncStateWithCategory(packet, "target:"+existing.TargetId, SyncCategoryTargetsRealtime)

		return existing.TargetId, nil
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
	modified := preEvent.Target /// can be modified by hooks
	// ----------------

	target, err := ts.DBMS.DbTargetById(targetId)
	if err != nil {
		return fmt.Errorf("target %s not exists", targetId)
	}

	if target.Computer == modified.Computer && target.Domain == modified.Domain && target.Address == modified.Address && target.Os == modified.Os && target.OsDesk == modified.OsDesk && target.Tag == modified.Tag && target.Info == modified.Info && target.Alive == modified.Alive {
		return nil
	}

	target.Computer = modified.Computer
	target.Domain = modified.Domain
	target.Address = modified.Address
	target.Os = modified.Os
	target.OsDesk = modified.OsDesk
	target.Tag = modified.Tag
	target.Info = modified.Info
	target.Alive = modified.Alive
	target.Date = time.Now().Unix()

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
	go func(ids []string, t string) {
		_ = ts.DBMS.DbTargetSetTagBatch(ids, t)
	}(targetsId, tag)

	packet := CreateSpTargetSetTag(targetsId, tag)
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

	for targetId := range targetsIdSet {
		t, err := ts.DBMS.DbTargetById(targetId)
		if err != nil {
			continue
		}
		t.Agents = std.DifferenceStringsArray(t.Agents, agentsId)
		updatedTargets = append(updatedTargets, t)
		packets = append(packets, CreateSpTargetUpdate(*t))
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
