/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package log

import (
	"fmt"
	"io"
	"os"

	"github.com/sirupsen/logrus"
)

// ConsoleHook sends log to stdout/stderr.
type ConsoleHook struct {
	formatter logrus.Formatter
}

// newConsoleHook creates a new hook for writing to stdout/stderr.
func newConsoleHook(logFormat logrus.Formatter) (*ConsoleHook, error) {
	return &ConsoleHook{formatter: logFormat}, nil
}

// Levels returns all levels.
func (hook *ConsoleHook) Levels() []logrus.Level {
	return logrus.AllLevels
}

// Fire ensure respective log entries
func (hook *ConsoleHook) Fire(logEntry *logrus.Entry) error {
	var logWriter io.Writer
	switch logEntry.Level {
	case logrus.DebugLevel, logrus.InfoLevel, logrus.WarnLevel:
		logWriter = os.Stdout
	case logrus.ErrorLevel, logrus.FatalLevel:
		logWriter = os.Stderr
	default:
		return fmt.Errorf("unknown log level: %v", logEntry.Level)
	}
	lineBytes, err := hook.formatter.Format(logEntry)
	if err != nil {
		_, printErr := fmt.Fprintf(os.Stderr, "Unable to read logEntry: %v", err)
		if printErr != nil {
			return fmt.Errorf("print os.Stderr failed: %v", printErr)
		}
		return err
	}
	if _, err := logWriter.Write(lineBytes); err != nil {
		return err
	}
	return nil
}
