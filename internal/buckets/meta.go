package buckets

import (
	"encoding/json"
	"os"
	"path/filepath"
	"time"
)

// metaFile is what gets persisted at <BaseDir>/.meta/<bucket>/<path>.json.
// Keep it small and explicit so it can evolve without a migration.
type metaFile struct {
	ContentType  string            `json:"content_type,omitempty"`
	OwnerID      string            `json:"owner_id,omitempty"`
	ETag         string            `json:"etag,omitempty"`
	LastModified time.Time         `json:"last_modified"`
	Size         int64             `json:"size"`
	Public       bool              `json:"public,omitempty"`
	Metadata     map[string]string `json:"metadata,omitempty"`
}

func (l *LocalFS) writeMeta(bucket, path string, obj *Object) error {
	mp := l.metaPath(bucket, path)
	if err := os.MkdirAll(filepath.Dir(mp), 0o755); err != nil {
		return err
	}
	mf := metaFile{
		ContentType:  obj.ContentType,
		OwnerID:      obj.OwnerID,
		ETag:         obj.ETag,
		LastModified: obj.LastModified,
		Size:         obj.Size,
	}
	data, err := json.Marshal(mf)
	if err != nil {
		return err
	}
	return os.WriteFile(mp, data, 0o644)
}

func (l *LocalFS) readMeta(bucket, path string) (*Object, error) {
	mp := l.metaPath(bucket, path)
	full, err := l.safe(bucket, path)
	if err != nil {
		return nil, err
	}
	stat, statErr := os.Stat(full)
	data, err := os.ReadFile(mp)
	if err != nil {
		if !os.IsNotExist(err) {
			return nil, err
		}
		// No meta — synthesise from filesystem info.
		if statErr != nil {
			return nil, ErrNotFound
		}
		return &Object{
			Bucket:       bucket,
			Path:         path,
			Size:         stat.Size(),
			LastModified: stat.ModTime(),
		}, nil
	}
	var mf metaFile
	if err := json.Unmarshal(data, &mf); err != nil {
		return nil, err
	}
	obj := &Object{
		Bucket:       bucket,
		Path:         path,
		Size:         mf.Size,
		ContentType:  mf.ContentType,
		ETag:         mf.ETag,
		LastModified: mf.LastModified,
		OwnerID:      mf.OwnerID,
	}
	if statErr == nil {
		// Trust filesystem size if metadata drifts.
		obj.Size = stat.Size()
	}
	return obj, nil
}
