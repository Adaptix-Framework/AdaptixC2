package server

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"time"

	"AdaptixServer/core/utils/krypt"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsPayloadGenID() int64 {
	return ts.IdGen.Next("payload")
}

var unsafeFilenameRe = regexp.MustCompile(`[^a-zA-Z0-9._\-]+`)

func safePayloadFilename(name string) string {
	base := filepath.Base(filepath.Clean(strings.ReplaceAll(name, `\`, `/`)))
	if base == "" || base == "." || base == "/" {
		base = "payload.bin"
	}
	base = unsafeFilenameRe.ReplaceAllString(base, "_")
	if len(base) > 120 {
		base = base[:120]
	}
	return base
}

func deriveArtifactAndArch(filename, agentType string) (artifact, arch string) {
	lower := strings.ToLower(filename)
	switch {
	case strings.HasSuffix(lower, ".exe"):
		artifact = "exe"
	case strings.HasSuffix(lower, ".dll"):
		artifact = "dll"
	case strings.HasSuffix(lower, ".bin"):
		artifact = "bin"
	case strings.HasSuffix(lower, ".so"):
		artifact = "so"
	case strings.HasSuffix(lower, ".elf"):
		artifact = "elf"
	case strings.HasSuffix(lower, ".shellcode") || strings.HasSuffix(lower, ".sc"):
		artifact = "bin"
	default:
		if i := strings.LastIndex(lower, "."); i >= 0 && i < len(lower)-1 {
			artifact = lower[i+1:]
		} else {
			artifact = "bin"
		}
	}
	arch = "unknown"
	switch {
	case strings.Contains(lower, "x64") || strings.Contains(lower, "amd64") || strings.Contains(lower, "x86_64"):
		arch = "x64"
	case strings.Contains(lower, "x86") || strings.Contains(lower, "i386") || strings.Contains(lower, "386"):
		arch = "x86"
	case strings.Contains(lower, "arm64") || strings.Contains(lower, "aarch64"):
		arch = "arm64"
	case strings.Contains(lower, "arm"):
		arch = "arm"
	}
	_ = agentType
	return artifact, arch
}

func displayNameFromFilename(filename string) string {
	base := filepath.Base(filename)
	if i := strings.LastIndex(base, "."); i > 0 {
		return base[:i]
	}
	return base
}

func (ts *Teamserver) registerPayloadBlob(name, agentType, filename string, content []byte, listeners []string, configJson, creator, buildId, watermark, description string) (adaptix.PayloadData, error) {
	if len(content) == 0 {
		return adaptix.PayloadData{}, errors.New("empty payload content")
	}
	if agentType == "" {
		return adaptix.PayloadData{}, errors.New("agent type required")
	}
	if filename == "" {
		filename = "payload.bin"
	}
	if name == "" {
		name = displayNameFromFilename(filename)
	}
	if listeners == nil {
		listeners = []string{}
	}
	if creator == "" {
		creator = "unknown"
	}

	id := ts.TsPayloadGenID()
	safeName := safePayloadFilename(filename)
	dirPath := ts.Paths.PayloadPath
	if err := os.MkdirAll(dirPath, 0o755); err != nil {
		return adaptix.PayloadData{}, fmt.Errorf("create payload path: %w", err)
	}
	localPath := filepath.Join(dirPath, fmt.Sprintf("%d_%s", id, safeName))

	if err := os.WriteFile(localPath, content, 0o644); err != nil {
		return adaptix.PayloadData{}, fmt.Errorf("write payload: %w", err)
	}

	artifact, arch := deriveArtifactAndArch(filename, agentType)
	p := adaptix.PayloadData{
		PayloadId:  id,
		Name:       name,
		AgentType:  agentType,
		Artifact:   artifact,
		Arch:       arch,
		Listeners:  listeners,
		Size:       int64(len(content)),
		Sha1:       krypt.SHA1(content),
		Sha256:     krypt.SHA256(content),
		Md5:        krypt.MD5(content),
		Creator:    creator,
		Created:    time.Now().Unix(),
		Hidden:     false,
		LocalPath:  localPath,
		ConfigJson: configJson,
		BuildId:    buildId,
		Watermark:  watermark,
		Filename:   filename,
		Notes:      strings.TrimSpace(description),
	}

	if err := ts.DBMS.DbPayloadInsert(p); err != nil {
		_ = os.Remove(localPath)
		return adaptix.PayloadData{}, err
	}

	packet := CreateSpPayloadCreate(p)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryPayloads)

	return p, nil
}

func (ts *Teamserver) TsPayloadRegister(agentType, filename string, content []byte, listeners []string, configJson, creator, buildId, watermark, description string) (adaptix.PayloadData, error) {
	return ts.registerPayloadBlob("", agentType, filename, content, listeners, configJson, creator, buildId, watermark, description)
}

func (ts *Teamserver) TsPayloadImport(name, agentType, artifact, arch, creator string, listeners []string, content []byte, configJson string) (adaptix.PayloadData, error) {
	filename := name
	if !strings.Contains(filename, ".") && artifact != "" {
		filename = name + "." + artifact
	}
	return ts.registerPayloadBlob(name, agentType, filename, content, listeners, configJson, creator, "", "", "")
}

func (ts *Teamserver) TsPayloadList(showHidden bool) ([]byte, error) {
	return ts.TsPayloadGetPage(0, 100000, showHidden, "", "Created", "desc")
}

type PayloadPage struct {
	Items  []adaptix.PayloadData `json:"items"`
	Total  int                   `json:"total"`
	Offset int                   `json:"offset"`
	Limit  int                   `json:"limit"`
}

func (ts *Teamserver) TsPayloadGetPage(offset, limit int, showHidden bool, filterExpr, sortCol, sortOrder string) ([]byte, error) {
	list, total, err := ts.DBMS.DbPayloadGetPage(offset, limit, showHidden, filterExpr, sortCol, sortOrder)
	if err != nil {
		return nil, err
	}
	if list == nil {
		list = make([]adaptix.PayloadData, 0)
	}
	for i := range list {
		if _, err := os.Stat(list[i].LocalPath); err != nil {
			list[i].Missing = true
		}
		list[i].LocalPath = ""
		list[i].ConfigJson = ""
	}
	return json.Marshal(PayloadPage{
		Items:  list,
		Total:  total,
		Offset: offset,
		Limit:  limit,
	})
}

func (ts *Teamserver) TsPayloadGet(id int64) (adaptix.PayloadData, error) {
	return ts.DBMS.DbPayloadGet(id)
}

func (ts *Teamserver) TsPayloadDownload(id int64) (string, []byte, error) {
	p, err := ts.DBMS.DbPayloadGet(id)
	if err != nil {
		return "", nil, err
	}
	data, err := os.ReadFile(p.LocalPath)
	if err != nil {
		return "", nil, fmt.Errorf("payload file missing: %w", err)
	}
	return p.Filename, data, nil
}

func (ts *Teamserver) TsPayloadHide(ids []int64, hidden bool) error {
	if len(ids) == 0 {
		return nil
	}
	if err := ts.DBMS.DbPayloadSetHidden(ids, hidden); err != nil {
		return err
	}
	packet := CreateSpPayloadUpdate(ids, hidden)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryPayloads)
	return nil
}

func (ts *Teamserver) TsPayloadSetTag(ids []int64, tag string) error {
	if len(ids) == 0 {
		return nil
	}
	go func(ids []int64, t string) {
		_ = ts.DBMS.DbPayloadSetTagBatch(ids, t)
	}(ids, tag)

	packet := CreateSpPayloadSetTag(ids, tag)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryPayloads)
	return nil
}

func (ts *Teamserver) TsPayloadSetColor(ids []int64, background, foreground string, reset bool) error {
	if len(ids) == 0 {
		return nil
	}
	color := ""
	if !reset {
		bg := strings.TrimSpace(background)
		fg := strings.TrimSpace(foreground)
		if bg != "" || fg != "" {
			for _, id := range ids {
				p, err := ts.DBMS.DbPayloadGet(id)
				if err != nil {
					continue
				}
				b, f := bg, fg
				if p.Color != "" {
					parts := strings.SplitN(p.Color, "-", 2)
					if b == "" && len(parts) > 0 {
						b = parts[0]
					}
					if f == "" && len(parts) > 1 {
						f = parts[1]
					}
				}
				c := strings.TrimSpace(b) + "-" + strings.TrimSpace(f)
				if c == "-" {
					c = ""
				}
				if err := ts.DBMS.DbPayloadSetColor([]int64{id}, c); err != nil {
					return err
				}
				if p2, err2 := ts.DBMS.DbPayloadGet(id); err2 == nil {
					p2.LocalPath = ""
					packet := CreateSpPayloadEdit(p2)
					ts.TsSyncAllClientsWithCategory(packet, SyncCategoryPayloads)
				}
			}
			return nil
		}
		color = ""
	}
	if err := ts.DBMS.DbPayloadSetColor(ids, color); err != nil {
		return err
	}
	for _, id := range ids {
		if p, err := ts.DBMS.DbPayloadGet(id); err == nil {
			p.LocalPath = ""
			packet := CreateSpPayloadEdit(p)
			ts.TsSyncAllClientsWithCategory(packet, SyncCategoryPayloads)
		}
	}
	return nil
}

func (ts *Teamserver) TsPayloadUpdateMeta(id int64, name, notes, artifact, arch string, hidden bool) (adaptix.PayloadData, error) {
	if id == 0 {
		return adaptix.PayloadData{}, errors.New("invalid payload id")
	}
	name = strings.TrimSpace(name)
	if name == "" {
		return adaptix.PayloadData{}, errors.New("name required")
	}
	notes = strings.TrimSpace(notes)
	artifact = strings.TrimSpace(artifact)
	arch = strings.TrimSpace(arch)

	if err := ts.DBMS.DbPayloadUpdateMeta(id, name, notes, artifact, arch, hidden); err != nil {
		return adaptix.PayloadData{}, err
	}
	p, err := ts.DBMS.DbPayloadGet(id)
	if err != nil {
		return adaptix.PayloadData{}, err
	}
	if _, err := os.Stat(p.LocalPath); err != nil {
		p.Missing = true
	}
	p.LocalPath = ""
	packet := CreateSpPayloadEdit(p)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryPayloads)
	return p, nil
}

func (ts *Teamserver) TsPayloadRemove(ids []int64, hard bool) error {
	if len(ids) == 0 {
		return nil
	}
	if !hard {
		return ts.TsPayloadHide(ids, true)
	}
	var paths []string
	for _, id := range ids {
		if p, err := ts.DBMS.DbPayloadGet(id); err == nil {
			paths = append(paths, p.LocalPath)
		}
	}
	if err := ts.DBMS.DbPayloadDelete(ids); err != nil {
		return err
	}
	for _, path := range paths {
		_ = os.Remove(path)
	}
	packet := CreateSpPayloadDelete(ids)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryPayloads)
	return nil
}

func (ts *Teamserver) TsPayloadSync() ([]byte, error) {
	list, err := ts.DBMS.DbPayloadList(true)
	if err != nil {
		return nil, err
	}
	type syncItem struct {
		adaptix.PayloadData
	}
	out := make([]adaptix.PayloadData, 0, len(list))
	for _, p := range list {
		if _, err := os.Stat(p.LocalPath); err != nil {
			p.Missing = true
		}
		p.LocalPath = ""
		p.ConfigJson = ""
		out = append(out, p)
	}
	return json.Marshal(out)
}

func (ts *Teamserver) TsPresyncPayloads() []interface{} {
	list, err := ts.DBMS.DbPayloadList(true)
	if err != nil {
		return nil
	}
	packets := make([]interface{}, 0, len(list))
	for _, p := range list {
		packets = append(packets, CreateSpPayloadCreate(p))
	}
	return packets
}
