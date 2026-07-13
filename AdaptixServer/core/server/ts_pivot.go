package server

import (
	"AdaptixServer/core/eventing"
	"fmt"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsGetPivotInfoByName(pivotName string) (string, int64, int64) {
	for value := range ts.pivots.Iterator() {
		pivot, ok := value.Item.(*adaptix.PivotData)
		if !ok {
			continue
		}
		if pivot.PivotName == pivotName {
			return pivot.PivotId, pivot.ParentAgentId, pivot.ChildAgentId
		}
	}
	return "", 0, 0
}

func (ts *Teamserver) TsGetPivotInfoById(pivotId string) (string, int64, int64) {
	for value := range ts.pivots.Iterator() {
		pivot, ok := value.Item.(*adaptix.PivotData)
		if !ok {
			continue
		}
		if pivot.PivotId == pivotId {
			return pivot.PivotName, pivot.ParentAgentId, pivot.ChildAgentId
		}
	}
	return "", 0, 0
}

func (ts *Teamserver) TsGetPivotByName(pivotName string) *adaptix.PivotData {
	for value := range ts.pivots.Iterator() {
		pivot, ok := value.Item.(*adaptix.PivotData)
		if !ok {
			continue
		}
		if pivot.PivotName == pivotName {
			return pivot
		}
	}
	return nil
}

func (ts *Teamserver) TsGetPivotById(pivotId string) *adaptix.PivotData {
	for value := range ts.pivots.Iterator() {
		pivot, ok := value.Item.(*adaptix.PivotData)
		if !ok {
			continue
		}
		if pivot.PivotId == pivotId {
			return pivot
		}
	}
	return nil
}

func (ts *Teamserver) TsPivotCreate(pivotId string, pAgentId int64, chAgentId int64, pivotName string, isRestore bool) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataPivotCreate{
		PivotId:       pivotId,
		ParentAgentId: pAgentId,
		ChildAgentId:  chAgentId,
		PivotName:     pivotName,
	}
	if !ts.EventManager.Emit(eventing.EventPivotCreate, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	pivotData := &adaptix.PivotData{
		PivotId:       pivotId,
		PivotName:     pivotName,
		ParentAgentId: pAgentId,
		ChildAgentId:  chAgentId,
	}

	if pivotData.PivotName == "" || ts.TsGetPivotByName(pivotData.PivotName) != nil {
		usedNames := make(map[string]bool)
		for value := range ts.pivots.Iterator() {
			pivot, ok := value.Item.(*adaptix.PivotData)
			if !ok {
				continue
			}
			usedNames[pivot.PivotName] = true
		}

		ok := false
		for i := 0; i <= 9999; i++ {
			name := fmt.Sprintf("p%d", i)
			if !usedNames[name] {
				pivotData.PivotName = name
				ok = true
				break
			}
		}
		if !ok {
			return fmt.Errorf("the number of pivots has exceeded 9999")
		}
	}

	parentAgent, ok := ts.Agents.Get(pivotData.ParentAgentId)
	if ok {
		parentAgent.PivotChilds.Put(pivotData)
	}

	childAgent, ok := ts.Agents.Get(pivotData.ChildAgentId)
	if ok {
		childAgent.SetPivotParent(pivotData)
	}

	//emptyMark := ""
	//_ = ts.TsAgentUpdateDataPartial(pivotData.ChildAgentId, struct {
	//	Mark *string `json:"mark"`
	//}{Mark: &emptyMark})

	ts.pivots.Put(pivotData)

	if !isRestore {
		err := ts.DBMS.DbPivotInsert(*pivotData)
		if err != nil {
			ts.TsLogAdd(adaptix.LogStatusError, 0, "server:pivot", "%s", err.Error())
		}
	}

	packet := CreateSpPivotCreate(*pivotData)
	ts.TsSyncAllClients(packet)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataPivotCreate{
		PivotId:       pivotData.PivotId,
		ParentAgentId: pivotData.ParentAgentId,
		ChildAgentId:  pivotData.ChildAgentId,
		PivotName:     pivotData.PivotName,
	}
	ts.EventManager.EmitAsync(eventing.EventPivotCreate, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsPivotDelete(pivotId string) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataPivotRemove{PivotId: pivotId}
	if !ts.EventManager.Emit(eventing.EventPivotRemove, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	pivotData := ts.TsGetPivotById(pivotId)
	if pivotData == nil {
		return fmt.Errorf("pivotId %s does not exist", pivotId)
	}

	if parentAgent, ok := ts.Agents.Get(pivotData.ParentAgentId); ok {
		parentAgent.PivotChilds.DeleteIf(func(i uint, value interface{}) bool {
			if pivot, ok := value.(*adaptix.PivotData); ok {
				return pivot.PivotId == pivotId
			}
			return false
		})
	}

	if childAgent, ok := ts.Agents.Get(pivotData.ChildAgentId); ok {
		childAgent.SetPivotParent(nil)
	}

	_ = ts.TsAgentUpdateDataPartial(pivotData.ChildAgentId, struct {
		Mark *string `json:"mark"`
	}{Mark: new("Unlink")})

	ts.pivots.DeleteIf(func(i uint, value interface{}) bool {
		return value.(*adaptix.PivotData).PivotId == pivotId
	})

	err := ts.DBMS.DbPivotDelete(pivotId)
	if err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server:pivot", "%s", err.Error())
	}

	packet := CreateSpPivotDelete(pivotId)
	ts.TsSyncAllClients(packet)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataPivotRemove{PivotId: pivotId}
	ts.EventManager.EmitAsync(eventing.EventPivotRemove, postEvent)
	// -----------------

	return nil
}
