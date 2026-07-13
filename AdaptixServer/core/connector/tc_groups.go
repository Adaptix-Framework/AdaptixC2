package connector

import (
	"encoding/json"
	"net/http"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcGroupList(ctx *gin.Context) {
	scope := ctx.Query("scope")
	groups := tc.teamserver.TsGroupList(scope)

	data, err := json.Marshal(groups)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", data)
}

type GroupCreateRequest struct {
	Scope    string `json:"scope"`
	Name     string `json:"name"`
	ParentId int64  `json:"parent_id"`
}

func (tc *TsConnector) TcGroupCreate(ctx *gin.Context) {
	var req GroupCreateRequest
	err := ctx.ShouldBindJSON(&req)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	if req.Name == "" || req.Scope == "" {
		respondError(ctx, http.StatusOK, "name and scope are required")
		return
	}

	err = tc.teamserver.TsGroupCreate(req.ParentId, req.Name, req.Scope)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

type GroupRenameRequest struct {
	GroupId int64  `json:"group_id"`
	Name    string `json:"name"`
}

func (tc *TsConnector) TcGroupRename(ctx *gin.Context) {
	var req GroupRenameRequest
	err := ctx.ShouldBindJSON(&req)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	if req.GroupId == 0 || req.Name == "" {
		respondError(ctx, http.StatusOK, "group_id and name are required")
		return
	}

	err = tc.teamserver.TsGroupRename(req.GroupId, req.Name)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

type GroupDeleteRequest struct {
	GroupId int64 `json:"group_id"`
}

func (tc *TsConnector) TcGroupDelete(ctx *gin.Context) {
	var req GroupDeleteRequest
	err := ctx.ShouldBindJSON(&req)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	if req.GroupId == 0 {
		respondError(ctx, http.StatusOK, "group_id is required")
		return
	}

	err = tc.teamserver.TsGroupDelete(req.GroupId)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

type GroupMembersRequest struct {
	GroupId int64   `json:"group_id"`
	Add     []int64 `json:"add"`
	Remove  []int64 `json:"remove"`
}

func (tc *TsConnector) TcGroupMembers(ctx *gin.Context) {
	var req GroupMembersRequest
	err := ctx.ShouldBindJSON(&req)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	if req.GroupId == 0 {
		respondError(ctx, http.StatusOK, "group_id is required")
		return
	}

	err = tc.teamserver.TsGroupMembers(req.GroupId, req.Add, req.Remove)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

type GroupReparentRequest struct {
	GroupId     int64 `json:"group_id"`
	NewParentId int64 `json:"new_parent_id"`
}

func (tc *TsConnector) TcGroupReparent(ctx *gin.Context) {
	var req GroupReparentRequest
	err := ctx.ShouldBindJSON(&req)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	if req.GroupId == 0 {
		respondError(ctx, http.StatusOK, "group_id is required")
		return
	}
	err = tc.teamserver.TsGroupReparent(req.GroupId, req.NewParentId)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondOK(ctx)
}
