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

func (ts *Teamserver) groupWouldCreateCycle(groupId, newParentId int64) bool {
	if newParentId == 0 {
		return false
	}
	if newParentId == groupId {
		return true
	}
	current := newParentId
	seen := make(map[int64]struct{}, 16)
	for current != 0 {
		if current == groupId {
			return true
		}
		if _, ok := seen[current]; ok {
			break
		}
		seen[current] = struct{}{}
		g, ok := ts.groups.Get(current)
		if !ok {
			break
		}
		current = g.ParentGroupId
	}
	return false
}

func groupContainsMember(members []int64, agentId int64) bool {
	for _, m := range members {
		if m == agentId {
			return true
		}
	}
	return false
}

func filterMember(members []int64, agentId int64) []int64 {
	out := make([]int64, 0, len(members))
	for _, m := range members {
		if m != agentId {
			out = append(out, m)
		}
	}
	return out
}

func (ts *Teamserver) groupApplyMembers(groupId int64, newMembers []int64, add, remove []int64) error {
	if err := ts.DBMS.DbGroupSetMembers(groupId, newMembers); err != nil {
		return fmt.Errorf("failed to update group members: %w", err)
	}
	g, ok := ts.groups.Get(groupId)
	if !ok {
		return fmt.Errorf("group %d does not exist", groupId)
	}
	g.Members = newMembers
	ts.groups.Put(groupId, g)
	ts.TsSyncAllClients(CreateSpGroupMembers(groupId, add, remove))
	return nil
}

func (ts *Teamserver) TsGroupCreate(parentId int64, name string, scope string) error {
	if parentId != 0 {
		parent, ok := ts.groups.Get(parentId)
		if !ok {
			return fmt.Errorf("parent group %d does not exist", parentId)
		}
		if parent.Scope != scope {
			return fmt.Errorf("parent group scope mismatch (parent=%s, requested=%s)", parent.Scope, scope)
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

	ts.TsLogAdd(adaptix.LogStatusInfo, 0, "server", "group", "Group %d '%s' created (scope: %s)", groupId, name, scope)

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

	ts.TsLogAdd(adaptix.LogStatusInfo, 0, "server", "group", "Group %d renamed to '%s'", groupId, name)

	packet := CreateSpGroupRename(groupId, name)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsGroupDelete(groupId int64) error {
	g, ok := ts.groups.Get(groupId)
	if !ok {
		return fmt.Errorf("group %d does not exist", groupId)
	}

	reparentTo := g.ParentGroupId
	var childIds []int64
	ts.groups.ForEachFast(func(key int64, cg adaptix.GroupData) bool {
		if cg.ParentGroupId == groupId {
			childIds = append(childIds, key)
		}
		return true
	})
	for _, cid := range childIds {
		if err := ts.TsGroupReparent(cid, reparentTo); err != nil {
			return fmt.Errorf("failed to reparent child group %d: %w", cid, err)
		}
	}

	if err := ts.DBMS.DbGroupDelete(groupId); err != nil {
		return fmt.Errorf("failed to delete group: %w", err)
	}

	ts.groups.Delete(groupId)

	ts.TsLogAdd(adaptix.LogStatusInfo, 0, "server", "group", "Group %d deleted", groupId)

	packet := CreateSpGroupDelete(groupId)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsGroupReparent(groupId int64, newParentId int64) error {
	if newParentId < 0 {
		newParentId = 0
	}

	g, ok := ts.groups.Get(groupId)
	if !ok {
		return fmt.Errorf("group %d does not exist", groupId)
	}
	if newParentId != 0 {
		parent, ok := ts.groups.Get(newParentId)
		if !ok {
			return fmt.Errorf("parent group %d does not exist", newParentId)
		}
		if parent.Scope != g.Scope {
			return fmt.Errorf("parent group scope mismatch")
		}
	}
	if ts.groupWouldCreateCycle(groupId, newParentId) {
		return fmt.Errorf("reparent would create a cycle")
	}
	if g.ParentGroupId == newParentId {
		return nil
	}

	if err := ts.DBMS.DbGroupReparent(groupId, newParentId); err != nil {
		return fmt.Errorf("failed to reparent group: %w", err)
	}
	g.ParentGroupId = newParentId
	ts.groups.Put(groupId, g)
	ts.TsLogAdd(adaptix.LogStatusInfo, 0, "server", "group", "Group %d reparented to %d", groupId, newParentId)
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

	newMembers := make([]int64, 0, len(g.Members)+len(add))
	for _, id := range g.Members {
		if !removeSet[id] {
			newMembers = append(newMembers, id)
		}
	}

	addSet := make(map[int64]bool, len(newMembers)+len(add))
	for _, id := range newMembers {
		addSet[id] = true
	}
	var actuallyAdded []int64
	for _, id := range add {
		if id == 0 {
			continue
		}
		if !addSet[id] {
			newMembers = append(newMembers, id)
			addSet[id] = true
			actuallyAdded = append(actuallyAdded, id)
		}
	}

	if len(actuallyAdded) > 0 {
		addLookup := make(map[int64]bool, len(actuallyAdded))
		for _, id := range actuallyAdded {
			addLookup[id] = true
		}
		type scrub struct {
			groupId int64
			members []int64
			remove  []int64
		}
		var scrubs []scrub
		ts.groups.ForEachFast(func(key int64, og adaptix.GroupData) bool {
			if key == groupId || og.Scope != g.Scope {
				return true
			}
			out := make([]int64, 0, len(og.Members))
			var rem []int64
			for _, m := range og.Members {
				if addLookup[m] {
					rem = append(rem, m)
					continue
				}
				out = append(out, m)
			}
			if len(rem) > 0 {
				scrubs = append(scrubs, scrub{key, out, rem})
			}
			return true
		})
		for _, s := range scrubs {
			if err := ts.groupApplyMembers(s.groupId, s.members, nil, s.remove); err != nil {
				return err
			}
		}
	}

	return ts.groupApplyMembers(groupId, newMembers, actuallyAdded, remove)
}

func (ts *Teamserver) TsGroupMoveMember(agentId, fromGroupId, toGroupId int64) error {
	if agentId == 0 {
		return fmt.Errorf("agent_id is required")
	}
	if fromGroupId < 0 {
		fromGroupId = 0
	}
	if toGroupId < 0 {
		toGroupId = 0
	}

	var targetScope string
	if toGroupId > 0 {
		tg, ok := ts.groups.Get(toGroupId)
		if !ok {
			return fmt.Errorf("group %d does not exist", toGroupId)
		}
		targetScope = tg.Scope
	} else if fromGroupId > 0 {
		fg, ok := ts.groups.Get(fromGroupId)
		if !ok {
			return fmt.Errorf("group %d does not exist", fromGroupId)
		}
		targetScope = fg.Scope
	}

	type planned struct {
		groupId int64
		members []int64
		add     []int64
		remove  []int64
	}
	var plan []planned

	ts.groups.ForEachFast(func(key int64, g adaptix.GroupData) bool {
		if targetScope != "" && g.Scope != targetScope {
			return true
		}
		has := groupContainsMember(g.Members, agentId)
		want := toGroupId > 0 && key == toGroupId
		if has && !want {
			plan = append(plan, planned{
				groupId: key,
				members: filterMember(g.Members, agentId),
				remove:  []int64{agentId},
			})
		} else if !has && want {
			nm := make([]int64, len(g.Members)+1)
			copy(nm, g.Members)
			nm[len(g.Members)] = agentId
			plan = append(plan, planned{
				groupId: key,
				members: nm,
				add:     []int64{agentId},
			})
		}
		return true
	})

	if toGroupId > 0 && !ts.groups.Contains(toGroupId) {
		return fmt.Errorf("group %d does not exist", toGroupId)
	}

	for _, p := range plan {
		if err := ts.groupApplyMembers(p.groupId, p.members, p.add, p.remove); err != nil {
			return err
		}
	}
	return nil
}

func (ts *Teamserver) TsGroupRemoveAgentFromAll(agentId int64) {
	if agentId == 0 {
		return
	}
	_ = ts.TsGroupMoveMember(agentId, 0, 0)
}
