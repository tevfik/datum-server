package logger

import "os"

// ClearLogFile truncates the on-disk log file if one is configured. It is a
// no-op when LogFilePath is empty (file logging disabled). Returns nil if the
// log file does not exist yet.
func ClearLogFile() error {
	if LogFilePath == "" {
		return nil
	}
	f, err := os.OpenFile(LogFilePath, os.O_WRONLY|os.O_TRUNC, 0o644)
	if err != nil {
		if os.IsNotExist(err) {
			return nil
		}
		return err
	}
	return f.Close()
}
