package server

import (
	"AdaptixServer/core/utils/logs"
	"fmt"

	"github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsGetPivotInfoByName(pivotName string) (string, string, string) {
	for value := range ts.pivots.Iterator() {
		pivot := value.Item.(*adaptix.PivotData)
		if pivot.PivotName == pivotName {
			return pivot.PivotId, pivot.ParentAgentId, pivot.ChildAgentId
		}
	}
	return "", "", ""
}

func (ts *Teamserver) TsGetPivotInfoById(pivotId string) (string, string, string) {
	for value := range ts.pivots.Iterator() {
		pivot := value.Item.(*adaptix.PivotData)
		if pivot.PivotId == pivotId {
			return pivot.PivotName, pivot.ParentAgentId, pivot.ChildAgentId
		}
	}
	return "", "", ""
}

func (ts *Teamserver) TsGetPivotByName(pivotName string) *adaptix.PivotData {
	for value := range ts.pivots.Iterator() {
		pivot := value.Item.(*adaptix.PivotData)
		if pivot.PivotName == pivotName {
			return pivot
		}
	}
	return nil
}

func (ts *Teamserver) TsGetPivotById(pivotId string) *adaptix.PivotData {
	for value := range ts.pivots.Iterator() {
		pivot := value.Item.(*adaptix.PivotData)
		if pivot.PivotId == pivotId {
			return pivot
		}
	}
	return nil
}

func (ts *Teamserver) TsPivotCreate(pivotId string, pAgentId string, chAgentId string, pivotName string, isRestore bool) error {
	pivotData := &adaptix.PivotData{
		PivotId:       pivotId,
		PivotName:     pivotName,
		ParentAgentId: pAgentId,
		ChildAgentId:  chAgentId,
	}

	if pivotData.PivotName == "" || ts.TsGetPivotByName(pivotData.PivotName) != nil {
		usedNames := make(map[string]bool)
		for value := range ts.pivots.Iterator() {
			pivot := value.Item.(*adaptix.PivotData)
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

	if parentAgent, err := ts.getAgent(pivotData.ParentAgentId); err == nil {
		parentAgent.PivotChilds.Put(pivotData)
	}

	if childAgent, err := ts.getAgent(pivotData.ChildAgentId); err == nil {
		childAgent.PivotParent = pivotData
	}

	emptyMark := ""
	_ = ts.TsAgentUpdateDataPartial(pivotData.ChildAgentId, struct {
		Mark *string `json:"mark"`
	}{Mark: &emptyMark})

	ts.pivots.Put(pivotData)

	if !isRestore {
		err := ts.DBMS.DbPivotInsert(*pivotData)
		if err != nil {
			logs.Error("", err.Error())
		}
	}

	packet := CreateSpPivotCreate(*pivotData)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsPivotDelete(pivotId string) error {
	pivotData := ts.TsGetPivotById(pivotId)
	if pivotData == nil {
		return fmt.Errorf("pivotId %s does not exist", pivotId)
	}

	if parentAgent, err := ts.getAgent(pivotData.ParentAgentId); err == nil {
		for i := uint(0); i < parentAgent.PivotChilds.Len(); i++ {
			valuePivot, ok := parentAgent.PivotChilds.Get(i)
			if ok {
				if pivot, ok := valuePivot.(*adaptix.PivotData); ok && pivot.PivotId == pivotId {
					parentAgent.PivotChilds.Delete(i)
					break
				}
			}
		}
	}

	if childAgent, err := ts.getAgent(pivotData.ChildAgentId); err == nil {
		childAgent.PivotParent = nil
	}

	unlinkMark := "Unlink"
	_ = ts.TsAgentUpdateDataPartial(pivotData.ChildAgentId, struct {
		Mark *string `json:"mark"`
	}{Mark: &unlinkMark})

	for i := uint(0); i < ts.pivots.Len(); i++ {
		valuePivot, ok := ts.pivots.Get(i)
		if ok {
			if valuePivot.(*adaptix.PivotData).PivotId == pivotId {
				ts.pivots.Delete(i)
				break
			}
		}
	}

	err := ts.DBMS.DbPivotDelete(pivotId)
	if err != nil {
		logs.Error("", err.Error())
	}

	packet := CreateSpPivotDelete(pivotId)
	ts.TsSyncAllClients(packet)

	return nil
}
