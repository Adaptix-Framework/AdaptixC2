package server

import (
	"fmt"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsGroupList(scope string) []map[string]interface{} {
	var result []map[string]interface{}

	ts.groups.ForEachFast(func(key int64, g adaptix.GroupData) bool {
		if scope != "" && g.Scope != scope {
			return true
		}
		m := map[string]interface{}{
			"group_id":        g.GroupId,
			"parent_group_id": g.ParentGroupId,
			"group_name":      g.GroupName,
			"scope":           g.Scope,
			"members":         g.Members,
		}
		result = append(result, m)
		return true
	})

	if result == nil {
		result = []map[string]interface{}{}
	}
	return result
}

func (ts *Teamserver) TsGroupCreate(parentId int64, name string, scope string) error {
	if parentId != 0 {
		if !ts.groups.Contains(parentId) {
			return fmt.Errorf("parent group %d does not exist", parentId)
		}
	}

	groupId, err := ts.DBMS.DbGroupCreate(parentId, name, scope)
	if err != nil {
		return fmt.Errorf("failed to create group: %w", err)
	}

	g := adaptix.GroupData{
		GroupId:       groupId,
		ParentGroupId: parentId,
		GroupName:     name,
		Scope:         scope,
		Members:       []int64{},
	}
	ts.groups.Put(groupId, g)

	ts.TsLogAdd(adaptix.LogStatusInfo, 0, "server:group", "Group %d '%s' created (scope: %s)", groupId, name, scope)

	packet := CreateSpGroupCreate(groupId, parentId, name, scope, g.Members)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsGroupRename(groupId int64, name string) error {
	g, ok := ts.groups.Get(groupId)
	if !ok {
		return fmt.Errorf("group %d does not exist", groupId)
	}

	if err := ts.DBMS.DbGroupRename(groupId, name); err != nil {
		return fmt.Errorf("failed to rename group: %w", err)
	}

	g.GroupName = name
	ts.groups.Put(groupId, g)

	ts.TsLogAdd(adaptix.LogStatusInfo, 0, "server:group", "Group %d renamed to '%s'", groupId, name)

	packet := CreateSpGroupRename(groupId, name)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsGroupDelete(groupId int64) error {
	if _, ok := ts.groups.Get(groupId); !ok {
		return fmt.Errorf("group %d does not exist", groupId)
	}

	if err := ts.DBMS.DbGroupDelete(groupId); err != nil {
		return fmt.Errorf("failed to delete group: %w", err)
	}

	ts.groups.Delete(groupId)

	ts.TsLogAdd(adaptix.LogStatusInfo, 0, "server:group", "Group %d deleted", groupId)

	packet := CreateSpGroupDelete(groupId)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsGroupReparent(groupId int64, newParentId int64) error {
	g, ok := ts.groups.Get(groupId)
	if !ok {
		return fmt.Errorf("group %d does not exist", groupId)
	}
	if newParentId != 0 {
		if _, ok := ts.groups.Get(newParentId); !ok {
			return fmt.Errorf("parent group %d does not exist", newParentId)
		}
	}
	if err := ts.DBMS.DbGroupReparent(groupId, newParentId); err != nil {
		return fmt.Errorf("failed to reparent group: %w", err)
	}
	g.ParentGroupId = newParentId
	ts.groups.Put(groupId, g)
	ts.TsLogAdd(adaptix.LogStatusInfo, 0, "server:group", "Group %d reparented to %d", groupId, newParentId)
	packet := CreateSpGroupReparent(groupId, newParentId)
	ts.TsSyncAllClients(packet)
	return nil
}

func (ts *Teamserver) TsGroupMembers(groupId int64, add []int64, remove []int64) error {
	g, ok := ts.groups.Get(groupId)
	if !ok {
		return fmt.Errorf("group %d does not exist", groupId)
	}

	removeSet := make(map[int64]bool, len(remove))
	for _, id := range remove {
		removeSet[id] = true
	}

	newMembers := make([]int64, 0, len(g.Members))
	for _, id := range g.Members {
		if !removeSet[id] {
			newMembers = append(newMembers, id)
		}
	}

	addSet := make(map[int64]bool, len(g.Members)+len(add))
	for _, id := range newMembers {
		addSet[id] = true
	}
	for _, id := range add {
		if !addSet[id] {
			newMembers = append(newMembers, id)
			addSet[id] = true
		}
	}

	if err := ts.DBMS.DbGroupSetMembers(groupId, newMembers); err != nil {
		return fmt.Errorf("failed to update group members: %w", err)
	}

	g.Members = newMembers
	ts.groups.Put(groupId, g)

	packet := CreateSpGroupMembers(groupId, add, remove)
	ts.TsSyncAllClients(packet)

	return nil
}
