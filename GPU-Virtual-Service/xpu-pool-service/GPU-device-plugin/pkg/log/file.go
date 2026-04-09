/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package log provides a rolling FileLogger.
package log

import (
	"compress/gzip"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"syscall"
	"time"
)

const (
	backupTimeFormat  = "20060102T150405.000"
	compressSuffix    = ".gz"
	defaultLogPath    = "/var/log"
	defaultLogMaxSize = 20

	fileModeUserRoForArchiveLog = 0440
	fileModeUserRwForWritingLog = 0640
	dirModeUserRwxForLogDir     = 0750
	hourPerDay                  = 24
)

var _ io.WriteCloser = (*FileLogger)(nil)

// FileLogger implements the function of writing logs into files.
// maxSize specifies the maximum size of a single log file, in MB. The recommended value is
// greater than or equal to 10.
// maxBackups specifies the maximum number of compressed backup files that can be retained.
// You are advised to set this parameter based on product requirements.
// maxAge specifies the maximum duration for storing the backup log files generated
// before the timepoint, and the expired files are automatically deleted.
// If maxBackups and maxAge are both 0, no old log files will be deleted. This may cause the log space to be full.
// Avoid entering this state.
type FileLogger struct {
	// Filename is the file to write logs to.
	fileName string

	// maximum size of a log file, in MB.
	maxSize int

	// maxAge Number of days for which logs are retained
	maxAge int

	// maxBackups the maximum number of old log files to retain.
	maxBackups int

	// localTime whether the log timestamp uses the local time. The default is to use UTC time.
	localTime bool

	// Length of the current log file that has been written
	writtenLen int64

	// current log file
	file *os.File

	mutex sync.Mutex

	staleLogCh          chan bool
	startHandleStaleLog sync.Once
}

// variable so tests can mock it out and not need to write megabytes of data
// to disk.
var (
	currentTime = time.Now
	megabyte    = 1024 * 1024
)

// Write implements io.Writer.
func (l *FileLogger) Write(bytes []byte) (int, error) {
	l.mutex.Lock()
	defer l.mutex.Unlock()

	writtenLen := int64(len(bytes))
	if writtenLen > l.maxLogContextLen() {
		return 0, fmt.Errorf("write length %d exceeds maximum file writtenLen %d", writtenLen, l.maxLogContextLen())
	}

	if l.file == nil {
		if err := l.openExistingOrNew(len(bytes)); err != nil {
			return 0, err
		}
	}

	if l.writtenLen+writtenLen >= l.maxLogContextLen() {
		if err := l.rotate(); err != nil {
			return 0, err
		}
	}

	n, err := l.file.Write(bytes)
	l.writtenLen += int64(n)
	return n, err
}

// Close implements io.Closer, and closes the current logfile.
func (l *FileLogger) Close() error {
	l.mutex.Lock()
	defer l.mutex.Unlock()
	return l.close()
}

// close closes the file if it is open.
func (l *FileLogger) close() error {
	if l.file == nil {
		return nil
	}
	err := l.file.Close()
	l.file = nil
	return err
}

// Rotate close the existing log file and immediately create a new one.
func (l *FileLogger) Rotate() error {
	l.mutex.Lock()
	defer l.mutex.Unlock()
	return l.rotate()
}

// rotate closes the current file, moves it aside with a timestamp in the name,
// (if it exists), opens a new file with the original filename, and then runs
// post rotation compression.
func (l *FileLogger) rotate() error {
	if err := l.close(); err != nil {
		return err
	}
	if err := l.openNew(); err != nil {
		return err
	}
	l.postRotateCompressLog()
	return nil
}

// openNew opens a new log file for writing, moving any old log file out of the
// way. This method assumes the file has already been closed.
func (l *FileLogger) openNew() error {
	err := os.MkdirAll(l.dir(), dirModeUserRwxForLogDir)
	if err != nil {
		return fmt.Errorf("can't make directories for new logfile: %s", err)
	}

	name := l.filename()
	info, err := os.Stat(name)
	if err == nil {
		newName := backupName(name, l.localTime)
		if err := os.Rename(name, newName); err != nil {
			return fmt.Errorf("can't rename log file: %s", err)
		}

		err := os.Chmod(newName, os.FileMode(fileModeUserRoForArchiveLog))
		if err != nil {
			return fmt.Errorf("can't chmod log file: %v", err)
		}
		if err := createAndChown(name, info); err != nil {
			return err
		}
	}
	// Rename and archive the log file and create a new log file.
	// we use truncate here because this should only get called when we've moved
	// the file ourselves. if someone else creates the file in the meantime,
	// just wipe out the contents.
	f, err := os.OpenFile(name, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, os.FileMode(fileModeUserRwForWritingLog))
	if err != nil {
		return fmt.Errorf("can't open new logfile: %s", err)
	}
	l.file = f
	l.writtenLen = 0
	return nil
}

// backupName creates a backup name from the given name, inserting a timestamp
// between the filename and the extension, using the local time if requested
// by the localTime yield configuration Whether the UTC time or local time is used is determined
// by the localTime yield configuration
func backupName(name string, local bool) string {
	dir := filepath.Dir(name)
	fileName := filepath.Base(name)
	ext := filepath.Ext(fileName)
	prefix := fileName[:len(fileName)-len(ext)]
	t := currentTime()
	if !local {
		t = t.UTC()
	}
	timestamp := t.Format(backupTimeFormat)
	return filepath.Join(dir, fmt.Sprintf("%s-%s%s", prefix, timestamp, ext))
}

// openExistingOrNew opens the logfile if it exists and if the current write
// would not put it over maxSize.  If there is no such file or write would
// put it over the maxSize, the file is renamed and a new logfile created.
func (l *FileLogger) openExistingOrNew(writeLen int) error {
	l.postRotateCompressLog()
	filename := l.filename()
	info, err := os.Stat(filename)
	if os.IsNotExist(err) {
		return l.openNew()
	}
	if err != nil {
		return fmt.Errorf("error getting log file info: %s", err)
	}

	if info.Size()+int64(writeLen) >= l.maxLogContextLen() {
		return l.rotate()
	}

	file, err := os.OpenFile(filename, os.O_APPEND|os.O_WRONLY, os.FileMode(fileModeUserRwForWritingLog))
	if err != nil {
		// if we fail to open the old log file for some reason, just ignore
		// it and open a new log file.
		return l.openNew()
	}
	l.file = file
	l.writtenLen = info.Size()
	return nil
}

// Filename generates path-safe log file names
func (l *FileLogger) filename() string {
	l.fileName = filepath.Clean(l.fileName)
	isAbsolute := filepath.IsAbs(l.fileName)
	if !isAbsolute {
		l.fileName = filepath.Join(defaultLogPath, filepath.Base(l.fileName))
	}
	return l.fileName
}

// handleStaleLogRun performs compression and removal of stale log files.
func (l *FileLogger) handleStaleLogRunOnce() error {
	if l.maxBackups == 0 && l.maxAge == 0 {
		return nil
	}

	files, err := l.oldLogFiles()
	if err != nil {
		return err
	}

	preserved := make(map[string]bool)
	cutoff := time.Now().Add(-time.Duration(l.maxAge) * hourPerDay * time.Hour)
	var toCompress, toRemove []logInfo
	for _, f := range files {
		baseName := f.Name()
		isCompressed := strings.HasSuffix(baseName, compressSuffix)
		if isCompressed {
			baseName = baseName[:len(baseName)-len(compressSuffix)]
		}

		// Check if file should be removed due to max backups or age. if maxBackups==0 or maxAge==0,
		// it is considered that recycling and clearing function is not enabled
		if (l.maxBackups != 0 && len(preserved) >= l.maxBackups) || (l.maxAge != 0 && f.timestamp.Before(cutoff)) {
			toRemove = append(toRemove, f)
		} else {
			preserved[baseName] = true
			if !isCompressed {
				toCompress = append(toCompress, f)
			}
		}
	}

	// Remove files toRemove
	for _, f := range toRemove {
		if errRemove := os.Remove(filepath.Join(l.dir(), f.Name())); errRemove != nil {
			err = errRemove
		}
	}

	// Compress files toCompress
	for _, f := range toCompress {
		fn := filepath.Join(l.dir(), f.Name())
		if errCompress := compressLogFile(fn, fn+compressSuffix); errCompress != nil {
			err = errCompress
		}
	}
	return err
}

// handleStaleLogRun runs in a goroutine to manage post-rotation compression and removal of stale log files.
func (l *FileLogger) handleStaleLogRun() {
	for range l.staleLogCh {
		if err := l.handleStaleLogRunOnce(); err != nil {
			fmt.Errorf("handle stale log err: %v", err)
		}
	}
}

func (l *FileLogger) postRotateCompressLog() {
	l.startHandleStaleLog.Do(func() {
		l.staleLogCh = make(chan bool, 1)
		go l.handleStaleLogRun()
	})
	select {
	case l.staleLogCh <- true:
	default:
	}
}

// oldLogFiles returns the list of backup log files stored in the same directory, sorted by ModTime
func (l *FileLogger) oldLogFiles() ([]logInfo, error) {
	l.mutex.Lock()
	defer l.mutex.Unlock()

	files, err := os.ReadDir(l.dir())
	if err != nil {
		return nil, fmt.Errorf("can't read log file directory: %s", err)
	}

	var logFiles []logInfo
	prefix, ext := l.getLogFilePrefixAndExt()
	for _, f := range files {
		fileInfo, _ := f.Info()
		if fileInfo.IsDir() {
			continue
		}
		if t, err := l.extractTimeFromFileName(f.Name(), prefix, ext); err == nil {
			logFiles = append(logFiles, logInfo{t, fileInfo})
			continue
		}
		if t, err := l.extractTimeFromFileName(f.Name(), prefix, ext+compressSuffix); err == nil {
			logFiles = append(logFiles, logInfo{t, fileInfo})
			continue
		}
	}
	sort.Sort(logInfoWithTimestamp(logFiles))
	return logFiles, nil
}

// extractTimeFromFileName If both the file name prefix and file name extension are matched,
// the timestamp is extracted from the file name. Otherwise, a failure message is returned.
func (l *FileLogger) extractTimeFromFileName(filename, prefix, ext string) (time.Time, error) {
	// check prefix
	if !strings.HasPrefix(filename, prefix) {
		return time.Time{}, errors.New("mismatched prefix")
	}
	// check suffix.
	if !strings.HasSuffix(filename, ext) {
		return time.Time{}, errors.New("mismatched extension")
	}
	ts := filename[len(prefix) : len(filename)-len(ext)]
	return time.Parse(backupTimeFormat, ts)
}

// maxLogContextLen returns the maximum writtenLen in bytes of log files before rolling.
func (l *FileLogger) maxLogContextLen() int64 {
	if l.maxSize == 0 {
		return int64(defaultMaxSize * megabyte)
	}
	return int64(l.maxSize) * int64(megabyte)
}

// dir returns the directory for the current filename.
func (l *FileLogger) dir() string {
	return filepath.Dir(l.filename())
}

// getLogFilePrefixAndExt returns the filename part and extension part from the FileLogger's filename.
func (l *FileLogger) getLogFilePrefixAndExt() (prefix, ext string) {
	filename := filepath.Base(l.filename())
	ext = filepath.Ext(filename)
	prefix = filename[:len(filename)-len(ext)] + "-"
	return prefix, ext
}

func createByOpen(name string, info os.FileInfo) error {
	f, err := os.OpenFile(name, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, info.Mode())
	if err != nil {
		return err
	}
	defer f.Close()
	return nil
}

func createAndChown(name string, info os.FileInfo) error {
	if err := createByOpen(name, info); err != nil {
		return err
	}

	stat, ok := info.Sys().(*syscall.Stat_t)
	if !ok {
		return fmt.Errorf("file info does not contain syscall.Stat_t")
	}
	return os.Chown(name, int(stat.Uid), int(stat.Gid))
}

// compressLogFile compresses the given log file and removing src log file if successful.
func compressLogFile(src, dst string) (err error) {
	srcFile, err := os.Open(src)
	if err != nil {
		return fmt.Errorf("open log file failed: %v", err)
	}
	defer srcFile.Close()

	srcFileInfo, err := os.Stat(src)
	if err != nil {
		return fmt.Errorf("stat log file failed: %v", err)
	}

	if err := createAndChown(dst, srcFileInfo); err != nil {
		return fmt.Errorf("create compressed log file failed: %v", err)
	}

	gzWriter, err := os.OpenFile(dst, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, srcFileInfo.Mode())
	if err != nil {
		return fmt.Errorf("open compressed log file failed: %v", err)
	}
	defer gzWriter.Close()

	gzFile := gzip.NewWriter(gzWriter)
	defer func() {
		if err != nil {
			if errRemove := os.Remove(dst); errRemove == nil {
				err = fmt.Errorf("compress log file failed: %v", err)
			} else {
				err = fmt.Errorf("remove log file failed: %v", err)
			}
		}
	}()

	if _, err := io.Copy(gzFile, srcFile); err != nil {
		return err
	}
	if err := gzFile.Close(); err != nil {
		return err
	}
	if err := gzWriter.Close(); err != nil {
		return err
	}
	if err := srcFile.Close(); err != nil {
		return err
	}
	if err := os.Remove(src); err != nil {
		return err
	}
	return nil
}

// logInfo is a convenience struct to return the filename and its embedded timestamp.
type logInfo struct {
	timestamp time.Time
	os.FileInfo
}

// logInfoWithTimestamp sorts by newest time formatted in the name.
type logInfoWithTimestamp []logInfo

func (b logInfoWithTimestamp) Less(i, j int) bool {
	if len(b) == 0 || i >= b.Len() || j >= b.Len() {
		return false
	}
	return b[i].timestamp.After(b[j].timestamp)
}

func (b logInfoWithTimestamp) Swap(i, j int) {
	if i >= b.Len() || j >= b.Len() {
		return
	}
	b[i], b[j] = b[j], b[i]
}

func (b logInfoWithTimestamp) Len() int {
	return len(b)
}
